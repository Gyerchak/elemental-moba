// [U-518] em_map_maker: make or change a map without touching a file. Left: what you edit (tabs). Right: the map, always live.
// Everything a map is can be set here: every field of every cell, the bases, lanes, turrets, camps, monsters, minion waves, match
// features, textures. The result is custom_maps/<id>.emap: the game, practice and servers read it (servers send it to players).
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <print>
#include <queue>
#include <string>
#include <vector>

#include "client/locale.hpp"
#include "client/tool_shell.hpp"
#include "core/surface_rules.hpp"
#include "game/match/map_file.hpp"
#include "game/mobs/monsters.hpp"
#include "game/world.hpp"

using namespace em;
using namespace em::gfx;
namespace fs = std::filesystem;

namespace {

constexpr int N = kGridSize;
[[nodiscard]] constexpr std::size_t idx(int x, int y) { return std::size_t(y) * N + std::size_t(x); }

constexpr const char* kSurfaceNames[] = {"Land", "Dryland", "Glacier", "Shallow water", "Sand", "Stone", "Wood", "Deep water", "Magma", "Metal", "Platinum", "Stone road", "Forest path"};
constexpr Rgba kSurfaceCol[] = {{70, 124, 62}, {134, 118, 74}, {206, 228, 240}, {80, 150, 200}, {214, 196, 136}, {128, 128, 134}, {130, 90, 56}, {26, 62, 124}, {230, 90, 30}, {150, 156, 168}, {226, 226, 236}, {150, 140, 120}, {120, 100, 66}};

// ---- what the brush paints
enum class Layer : std::uint8_t { Preset, Surface, Elevation, Walk, Wall, Tree, Bush, VisionFog, SoundFog, SkillFog, SeismicFog, HeatFog, Count };
constexpr const char* kLayerNames[] = {"Ready made", "Ground", "Height", "Walking", "Wall", "Tree", "Bush", "Blocks sight", "Blocks sound", "Blocks skills", "Blocks seismic sense", "Blocks heat sense"};
constexpr const char* kWalkNames[] = {"Blocked", "Walkable", "Only flying", "Only benders"};
constexpr const char* kWallNames[] = {"None", "Ice wall", "Stone wall", "Metal wall"};
constexpr const char* kTreeNames[] = {"None", "Tree", "Burned", "Destroyed"};
constexpr const char* kBushNames[] = {"None", "Bush", "Burned"};
constexpr const char* kFogNames[] = {"None", "Partly", "Fully", "Like a bush"};

struct Preset {
    const char* name;
    const char* tip;
    Rgba col;
    void (*apply)(Cell&);
};
void ground(Cell& c, Surface s) {
    c.set_surface(s);
    c.set_tree(Tree::None), c.set_bush(Bush::None), c.set_wall(Wall::None);
    c.set_vision_fog(SenseBlock::None), c.set_skill_fog(SenseBlock::None);
    c.set_walk(s == Surface::DeepWater ? Walk::Blocked : Walk::Walk);
}
const Preset kPresets[] = {
    {"Grass", "Plain land: walkable, earth can be bent", kSurfaceCol[0], [](Cell& c) { ground(c, Surface::Land); }},
    {"Dry land", "Earth can be bent", kSurfaceCol[1], [](Cell& c) { ground(c, Surface::Dryland); }},
    {"Sand", "", kSurfaceCol[4], [](Cell& c) { ground(c, Surface::Sand); }},
    {"Stone floor", "", kSurfaceCol[5], [](Cell& c) { ground(c, Surface::Stone); }},
    {"Stone road", "", kSurfaceCol[11], [](Cell& c) { ground(c, Surface::StoneRoad); }},
    {"Forest path", "", kSurfaceCol[12], [](Cell& c) { ground(c, Surface::ForestPath); }},
    {"Shallow water", "Walkable; a source of water, earth and mud", kSurfaceCol[3], [](Cell& c) { ground(c, Surface::Water); }},
    {"Deep water", "Not walkable; swimmers only where the game allows it", kSurfaceCol[7], [](Cell& c) { ground(c, Surface::DeepWater); }},
    {"Glacier", "Water without limit for waterbenders", kSurfaceCol[2], [](Cell& c) { ground(c, Surface::Glacier); }},
    {"Lava", "Burns; lights the area", kSurfaceCol[8], [](Cell& c) { ground(c, Surface::Magma); }},
    {"Wood floor / bridge", "No earth to bend here", kSurfaceCol[6], [](Cell& c) { ground(c, Surface::Wood); }},
    {"Metal floor", "", kSurfaceCol[9], [](Cell& c) { ground(c, Surface::Metal); }},
    {"Tree", "Blocks walking, sight and skills; fire burns it", {30, 90, 40}, [](Cell& c) { c.set_bush(Bush::None), c.set_wall(Wall::None), c.set_tree(Tree::Alive), c.set_vision_fog(SenseBlock::Full), c.set_skill_fog(SenseBlock::Full); }},
    {"Bush", "Hides who stands inside", {60, 150, 60}, [](Cell& c) { c.set_tree(Tree::None), c.set_wall(Wall::None), c.set_bush(Bush::Alive), c.set_vision_fog(SenseBlock::Brush); }},
    {"Rock", "A real rock: blocks, can be broken, bendable as a source", {96, 88, 80}, [](Cell& c) { c.set_tree(Tree::None), c.set_bush(Bush::None), c.set_wall(Wall::Stone), c.set_elevation(2), c.set_vision_fog(SenseBlock::Full), c.set_skill_fog(SenseBlock::Full); }},
    {"Ice wall", "", {190, 230, 250}, [](Cell& c) { c.set_tree(Tree::None), c.set_bush(Bush::None), c.set_wall(Wall::Ice), c.set_vision_fog(SenseBlock::Full), c.set_skill_fog(SenseBlock::Full); }},
    {"Metal wall", "Attracts lightning", {170, 176, 190}, [](Cell& c) { c.set_tree(Tree::None), c.set_bush(Bush::None), c.set_wall(Wall::Metal), c.set_vision_fog(SenseBlock::Full), c.set_skill_fog(SenseBlock::Full); }},
    {"Clear objects", "Removes trees, bushes and walls, keeps the ground", {40, 40, 50}, [](Cell& c) { c.set_tree(Tree::None), c.set_bush(Bush::None), c.set_wall(Wall::None), c.set_vision_fog(SenseBlock::None), c.set_skill_fog(SenseBlock::None); }},
};

enum class Tool : std::uint8_t { Brush, Rect, Line, Fill, Scatter, Picker };
constexpr const char* kToolNames[] = {"Brush", "Rectangle", "Line", "Fill", "Scatter", "Pick from map"};
constexpr const char* kToolTips[] = {"Paint by dragging", "Drag a rectangle", "Drag a line (brush size = width)", "Fills the connected area of the same kind", "Paints only some cells: for forests and bushes", "Click a cell: its value becomes the current one"};

// ---- what can be placed
enum class Obj : std::uint8_t { None, Base, Fountain, Nexus, Pole, Portal, SpiritIsland, NexusTurret, Camp, AggroCamp, BuffCamp, DarkSpirit, Epic, Spot, Waypoint, T1, T2, T3, Inhib };
struct ObjLook { Obj kind; const char* name; const char* tip; const char* tag; Rgba col; };
constexpr ObjLook kObjs[] = {
    {Obj::Base, "Base centre", "The middle of the base of the Fire side (the Earth side is its mirror)", "BASE", {255, 190, 90}},
    {Obj::Fountain, "Fountain", "Where champions start and come back to life; the shop ring is around it", "FTN", {120, 220, 255}},
    {Obj::Nexus, "Nation Shrine", "Destroying it wins the match", "NEX", {255, 120, 90}},
    {Obj::Pole, "Pole", "Optional", "POL", {220, 240, 255}},
    {Obj::Portal, "Spirit portal", "Optional: channel here to travel to the other portal", "POR", {180, 120, 255}},
    {Obj::SpiritIsland, "Spirit island", "Optional", "ISL", {180, 255, 200}},
    {Obj::NexusTurret, "Shrine turret", "Add as many as you like", "TUR", {255, 150, 120}},
    {Obj::Camp, "Jungle camp", "Peaceful spirits and animals", "CMP", {130, 230, 130}},
    {Obj::AggroCamp, "Aggressive camp", "Attacks who comes close", "AGR", {240, 180, 80}},
    {Obj::BuffCamp, "Buff camp", "Its killer carries a buff", "BUF", {120, 200, 240}},
    {Obj::DarkSpirit, "Dark spirit", "", "DRK", {150, 90, 170}},
    {Obj::Epic, "Epic monster", "One on the map, not mirrored", "EPC", {255, 90, 200}},
    {Obj::Spot, "Any monster", "Any monster anywhere; mirrored by default", "MON", {255, 230, 120}},
};
[[nodiscard]] const ObjLook& look_of(Obj k) {
    for (const ObjLook& o : kObjs)
        if (o.kind == k) return o;
    return kObjs[0];
}

struct Pick { std::string label, value, note; };

class MapMaker {
public:
    bool init(const std::string& open);
    void frame();
    void take_drop(const std::string& path);
    void start_at(int tab) { tab_ = std::clamp(tab, 0, 4); }
    tool::Window win;

private:
    MapDoc d_;
    std::string file_id_;          // the file it came from ("" = not saved yet)
    bool from_builtin_ = false, dirty_ = false;
    std::vector<std::vector<std::uint32_t>> undo_;
    std::vector<MapDoc> undo_doc_;
    int tab_ = 0;
    std::string tip_, status_;
    float scroll_ = 0;
    // view
    SDL_Texture* tex_ = nullptr;
    std::vector<std::uint32_t> px_;
    bool tex_dirty_ = true;
    float zoom_ = 1.5f, cam_x_ = 256, cam_y_ = 256;
    float vx_ = 0, vy_ = 0, vw_ = 0, vh_ = 0;
    bool panning_ = false, fitted_ = false;
    float pan_mx_ = 0, pan_my_ = 0;
    bool grid_lines_ = false;
    // paint
    Layer layer_ = Layer::Preset;
    int preset_ = 0, value_ = 0;
    Tool tool_ = Tool::Brush;
    int size_ = 6;
    bool round_ = true, mirror_on_ = true;
    int scatter_ = 25;
    bool stroke_ = false;
    int sx_ = 0, sy_ = 0;
    // objects
    Obj place_ = Obj::None;
    std::string place_monster_;
    bool place_mirrored_ = true;
    Obj drag_ = Obj::None;
    int drag_i_ = 0, drag_j_ = 0;
    int lane_ = 0;
    // check
    struct Problem { bool error; std::string text; CellPt at; };
    std::vector<Problem> problems_;
    // dialogs
    std::string pick_title_, pick_search_;
    std::vector<Pick> pick_items_;
    std::function<void(const std::string&)> pick_done_;
    float pick_scroll_ = 0;
    void open_picker(std::string title, std::vector<Pick> items, std::function<void(const std::string&)> done);
    void draw_picker();
    bool saving_ = false;
    std::string save_name_;
    int overwrite_armed_ = 0;
    static void SDLCALL on_files(void* self, const char* const* list, int);
    std::mutex dlg_mu_;
    std::string dlg_result_, dlg_what_;
    void ask_file(const std::string& what, bool picture);
    void got_file(const std::string& what, const std::string& path);
    // work
    void new_map(int lanes);
    void open_map(const std::string& value);
    void push_undo();
    void do_undo();
    void paint_cell(int x, int y, std::uint32_t seed_hit = 0);
    void paint_op(Cell& c);
    [[nodiscard]] int value_of(Cell c) const;
    void brush(int x, int y);
    void rect_op(int x0, int y0, int x1, int y1);
    void line_op(int x0, int y0, int x1, int y1);
    void fill_op(int x, int y);
    void rebuild_texture();
    void check_map();
    void save_new(const std::string& name);
    void overwrite();
    void write_file(const std::string& id);
    void auto_structures(int lane);
    // draw
    void toolbar();
    void tab_paint(float x, float y, float w);
    void tab_objects(float x, float y, float w);
    void tab_lanes(float x, float y, float w);
    void tab_settings(float x, float y, float w);
    void tab_check(float x, float y, float w);
    void map_view();
    void draw_objects();
    bool object_at(float mx, float my, Obj& kind, int& i, int& j);
    void move_object(Obj kind, int i, int j, CellPt to);
    void remove_object(Obj kind, int i, int j);
    void place_object(CellPt at);
    [[nodiscard]] float sxf(float cx) const { return vx_ + vw_ / 2 + (cx - cam_x_) * zoom_; }
    [[nodiscard]] float syf(float cy) const { return vy_ + vh_ / 2 + (cy - cam_y_) * zoom_; }
    [[nodiscard]] float cxf(float mx) const { return (mx - vx_ - vw_ / 2) / zoom_ + cam_x_; }
    [[nodiscard]] float cyf(float my) const { return (my - vy_ - vh_ / 2) / zoom_ + cam_y_; }
    [[nodiscard]] Cell cell(int x, int y) const { return Cell(d_.cells[idx(x, y)]); }
};

// ---------------------------------------------------------------- start
void MapMaker::new_map(int lanes) {
    MapDoc d;
    d.cells.assign(std::size_t(N) * N, Cell().raw());
    d.name = "New map", d.id = "new_map";
    d.desc = "";
    d.exact_terrain = true;
    const CellPt from{62, 449}, to{449, 62};
    auto lane_via = [&](CellPt via) {
        std::vector<CellPt> l{from};
        if (via != kNoCell) l.push_back(via);
        l.push_back(to);
        return l;
    };
    if (lanes == 1) d.lanes = {lane_via(kNoCell)};
    else if (lanes == 2) d.lanes = {lane_via({62, 62}), lane_via({449, 449})};
    else d.lanes = {lane_via({62, 62}), lane_via(kNoCell), lane_via({449, 449})};
    d.lane_structs.assign(d.lanes.size(), LaneStructs{kNoCell, kNoCell, kNoCell, kNoCell});
    d_ = std::move(d);
    for (int i = 0; i < int(d_.lanes.size()); ++i) auto_structures(i);
    d_.nexus_turrets = {{60, 458}, {53, 451}};
    file_id_.clear();
    from_builtin_ = false, dirty_ = false;
    undo_.clear(), undo_doc_.clear();
    lane_ = 0;
    tex_dirty_ = true;
    check_map();
}

void MapMaker::open_map(const std::string& value) {
    for (const MapDef& m : maps())
        if (m.id == value) {
            d_ = doc_of(m);
            from_builtin_ = is_builtin_map_id(m.id);
            std::error_code ec;
            file_id_ = fs::exists(fs::path("custom_maps") / (d_.id + ".emap"), ec) || from_builtin_ ? d_.id : "";
            dirty_ = false;
            undo_.clear(), undo_doc_.clear();
            lane_ = 0;
            tex_dirty_ = true;
            status_ = trf("opened {}", d_.name);
            check_map();
            return;
        }
}

bool MapMaker::init(const std::string& open) {
    if (!win.open("Elemental MOBA - Map maker", 1600, 900)) return false;
    load_custom_maps();
    px_.resize(std::size_t(N) * N);
    tex_ = SDL_CreateTexture(win.ren(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, N, N);
    SDL_SetTextureScaleMode(tex_, SDL_SCALEMODE_NEAREST);
    new_map(3);
    if (!open.empty()) open_map(open);
    return true;
}

// ---------------------------------------------------------------- painting
void MapMaker::paint_op(Cell& c) {
    switch (layer_) {
        case Layer::Preset: kPresets[std::size_t(preset_)].apply(c); break;
        case Layer::Surface: c.set_surface(Surface(value_)); break;
        case Layer::Elevation: c.set_elevation(value_); break;
        case Layer::Walk: c.set_walk(Walk(value_)); break;
        case Layer::Wall: c.set_wall(Wall(value_)); break;
        case Layer::Tree: c.set_tree(Tree(value_)); break;
        case Layer::Bush: c.set_bush(Bush(value_)); break;
        case Layer::VisionFog: c.set_vision_fog(SenseBlock(value_)); break;
        case Layer::SoundFog: c.set_sound_fog(SenseBlock(value_)); break;
        case Layer::SkillFog: c.set_skill_fog(SenseBlock(value_)); break;
        case Layer::SeismicFog: c.set_seismic_fog(SenseBlock(value_)); break;
        case Layer::HeatFog: c.set_heat_fog(SenseBlock(value_)); break;
        default: break;
    }
}

int MapMaker::value_of(Cell c) const {
    switch (layer_) {
        case Layer::Surface: case Layer::Preset: return int(c.surface());
        case Layer::Elevation: return c.elevation();
        case Layer::Walk: return int(c.walk());
        case Layer::Wall: return int(c.wall());
        case Layer::Tree: return int(c.tree());
        case Layer::Bush: return int(c.bush());
        case Layer::VisionFog: return int(c.vision_fog());
        case Layer::SoundFog: return int(c.sound_fog());
        case Layer::SkillFog: return int(c.skill_fog());
        case Layer::SeismicFog: return int(c.seismic_fog());
        case Layer::HeatFog: return int(c.heat_fog());
        default: return 0;
    }
}

void MapMaker::paint_cell(int x, int y, std::uint32_t) {
    if (!Grid::in_bounds(x, y)) return;
    if (tool_ == Tool::Scatter) {  // the same cells every time: dragging twice does not fill it up
        const std::uint32_t h = (std::uint32_t(x) * 73856093u) ^ (std::uint32_t(y) * 19349663u);
        if ((h ^ (h >> 13)) % 100 >= std::uint32_t(scatter_)) return;
    }
    Cell c = cell(x, y);
    paint_op(c);
    d_.cells[idx(x, y)] = c.raw();
    if (mirror_on_) d_.cells[idx(N - 1 - x, N - 1 - y)] = c.raw();
}

void MapMaker::brush(int x, int y) {
    const int r = size_ / 2;
    for (int yy = y - r; yy <= y + (size_ - 1) / 2 + (size_ <= 1 ? 0 : 0); ++yy)
        for (int xx = x - r; xx <= x + (size_ - 1) / 2; ++xx) {
            if (round_ && size_ > 2 && (xx - x) * (xx - x) + (yy - y) * (yy - y) > r * r) continue;
            paint_cell(xx, yy);
        }
    tex_dirty_ = true;
}

void MapMaker::rect_op(int x0, int y0, int x1, int y1) {
    for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y)
        for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x) paint_cell(x, y);
    tex_dirty_ = true;
}

void MapMaker::line_op(int x0, int y0, int x1, int y1) {
    const int n = std::max({std::abs(x1 - x0), std::abs(y1 - y0), 1});
    for (int i = 0; i <= n; ++i) brush(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n);
}

void MapMaker::fill_op(int x, int y) {
    if (!Grid::in_bounds(x, y)) return;
    const int was = value_of(cell(x, y));
    std::vector<std::uint8_t> seen(std::size_t(N) * N, 0);
    std::vector<std::pair<int, int>> st{{x, y}};
    seen[idx(x, y)] = 1;
    while (!st.empty()) {
        const auto [cx, cy] = st.back();
        st.pop_back();
        for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            const int nx = cx + dx, ny = cy + dy;
            if (!Grid::in_bounds(nx, ny) || seen[idx(nx, ny)] || value_of(cell(nx, ny)) != was) continue;
            seen[idx(nx, ny)] = 1;
            st.push_back({nx, ny});
        }
    }
    const Tool keep = tool_;
    tool_ = Tool::Fill;
    for (int yy = 0; yy < N; ++yy)
        for (int xx = 0; xx < N; ++xx)
            if (seen[idx(xx, yy)]) paint_cell(xx, yy);
    tool_ = keep;
    tex_dirty_ = true;
}

void MapMaker::push_undo() {
    undo_.push_back(d_.cells);
    undo_doc_.push_back(d_);
    undo_doc_.back().cells.clear();  // (the cells are in undo_)
    if (undo_.size() > 30) undo_.erase(undo_.begin()), undo_doc_.erase(undo_doc_.begin());
    dirty_ = true;
}

void MapMaker::do_undo() {
    if (undo_.empty()) return;
    d_ = std::move(undo_doc_.back());
    d_.cells = std::move(undo_.back());
    undo_.pop_back(), undo_doc_.pop_back();
    tex_dirty_ = true;
    lane_ = std::clamp(lane_, 0, std::max(0, int(d_.lanes.size()) - 1));
    check_map();
}

void MapMaker::rebuild_texture() {
    const bool fog_layer = layer_ >= Layer::VisionFog && layer_ < Layer::Count;
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            const Cell c = cell(x, y);
            Rgba col = kSurfaceCol[std::min<std::size_t>(std::size_t(c.surface()), std::size(kSurfaceCol) - 1)];
            const int shade = (c.elevation()) * 14;
            auto add = [](std::uint8_t v, int d) { return std::uint8_t(std::clamp(int(v) + d, 0, 255)); };
            col = {add(col.r, shade), add(col.g, shade), add(col.b, shade), 255};
            if (c.bush() == Bush::Alive) col = mixc(col, {60, 160, 60, 255}, 70);
            if (c.tree() == Tree::Alive) col = {24, 78, 36, 255};
            if (c.tree() == Tree::Burned || c.bush() == Bush::Burned) col = {50, 42, 36, 255};
            if (c.wall() == Wall::Stone) col = {92, 84, 76, 255};
            if (c.wall() == Wall::Ice) col = {190, 230, 250, 255};
            if (c.wall() == Wall::Metal) col = {170, 176, 190, 255};
            if (layer_ == Layer::Walk) {
                const Walk w = c.walk();
                col = mixc(col, w == Walk::Walk ? Rgba{0, 200, 0, 255} : w == Walk::Blocked ? Rgba{220, 0, 0, 255} : w == Walk::FlyOnly ? Rgba{0, 120, 255, 255} : Rgba{240, 200, 0, 255}, 40);
            }
            if (layer_ == Layer::Elevation) {
                const int e = c.elevation();
                col = mixc(col, e < 0 ? Rgba{0, 60, 200, 255} : Rgba{std::uint8_t(40 + e * 35), std::uint8_t(40 + e * 35), std::uint8_t(40 + e * 35), 255}, 60);
            }
            if (fog_layer) {
                const int v = value_of(c);
                if (v) col = mixc(col, v == 1 ? Rgba{240, 220, 60, 255} : v == 2 ? Rgba{230, 40, 40, 255} : Rgba{60, 230, 60, 255}, 60);
            }
            px_[idx(x, y)] = std::uint32_t(col.r) | std::uint32_t(col.g) << 8 | std::uint32_t(col.b) << 16 | 0xff000000u;
        }
    SDL_UpdateTexture(tex_, nullptr, px_.data(), N * 4);
    tex_dirty_ = false;
}

// ---------------------------------------------------------------- objects
bool MapMaker::object_at(float mx, float my, Obj& kind, int& i, int& j) {
    const float r = std::max(8.f, 4 * zoom_);
    auto near = [&](CellPt c) { return c != kNoCell && std::hypot(sxf(float(c.first)) - mx, syf(float(c.second)) - my) <= r; };
    if (tab_ == 2 && lane_ < int(d_.lanes.size())) {
        const LaneStructs& s = d_.lane_structs[std::size_t(lane_)];
        if (near(s.t1)) return kind = Obj::T1, i = lane_, true;
        if (near(s.t2)) return kind = Obj::T2, i = lane_, true;
        if (near(s.t3)) return kind = Obj::T3, i = lane_, true;
        if (near(s.inhib)) return kind = Obj::Inhib, i = lane_, true;
        const auto& l = d_.lanes[std::size_t(lane_)];
        for (int k = 0; k < int(l.size()); ++k)
            if (near(l[std::size_t(k)])) return kind = Obj::Waypoint, i = lane_, j = k, true;
        return false;
    }
    if (near(d_.base)) return kind = Obj::Base, true;
    if (near(d_.fountain)) return kind = Obj::Fountain, true;
    if (near(d_.nexus)) return kind = Obj::Nexus, true;
    if (near(d_.pole)) return kind = Obj::Pole, true;
    if (near(d_.portal)) return kind = Obj::Portal, true;
    if (near(d_.spirit_island)) return kind = Obj::SpiritIsland, true;
    auto list = [&](const std::vector<CellPt>& v, Obj k) {
        for (int n = 0; n < int(v.size()); ++n)
            if (near(v[std::size_t(n)])) return kind = k, i = n, true;
        return false;
    };
    if (list(d_.nexus_turrets, Obj::NexusTurret) || list(d_.camps, Obj::Camp) || list(d_.aggro_camps, Obj::AggroCamp) || list(d_.buff_camps, Obj::BuffCamp) || list(d_.dark_spirits, Obj::DarkSpirit)) return true;
    for (int n = 0; n < int(d_.epics.size()); ++n)
        if (near(d_.epics[std::size_t(n)].first)) return kind = Obj::Epic, i = n, true;
    for (int n = 0; n < int(d_.spots.size()); ++n)
        if (near(d_.spots[std::size_t(n)].cell)) return kind = Obj::Spot, i = n, true;
    return false;
}

void MapMaker::move_object(Obj kind, int i, int j, CellPt to) {
    switch (kind) {
        case Obj::Base: d_.base = to; break;
        case Obj::Fountain: d_.fountain = to; break;
        case Obj::Nexus: d_.nexus = to; break;
        case Obj::Pole: d_.pole = to; break;
        case Obj::Portal: d_.portal = to; break;
        case Obj::SpiritIsland: d_.spirit_island = to; break;
        case Obj::NexusTurret: d_.nexus_turrets[std::size_t(i)] = to; break;
        case Obj::Camp: d_.camps[std::size_t(i)] = to; break;
        case Obj::AggroCamp: d_.aggro_camps[std::size_t(i)] = to; break;
        case Obj::BuffCamp: d_.buff_camps[std::size_t(i)] = to; break;
        case Obj::DarkSpirit: d_.dark_spirits[std::size_t(i)] = to; break;
        case Obj::Epic: d_.epics[std::size_t(i)].first = to; break;
        case Obj::Spot: d_.spots[std::size_t(i)].cell = to; break;
        case Obj::Waypoint: d_.lanes[std::size_t(i)][std::size_t(j)] = to; break;
        case Obj::T1: d_.lane_structs[std::size_t(i)].t1 = to; break;
        case Obj::T2: d_.lane_structs[std::size_t(i)].t2 = to; break;
        case Obj::T3: d_.lane_structs[std::size_t(i)].t3 = to; break;
        case Obj::Inhib: d_.lane_structs[std::size_t(i)].inhib = to; break;
        default: break;
    }
    dirty_ = true;
}

void MapMaker::remove_object(Obj kind, int i, int j) {
    push_undo();
    auto del = [&](std::vector<CellPt>& v) { v.erase(v.begin() + i); };
    switch (kind) {
        case Obj::Pole: d_.pole = kNoCell; break;
        case Obj::Portal: d_.portal = kNoCell; break;
        case Obj::SpiritIsland: d_.spirit_island = kNoCell; break;
        case Obj::NexusTurret: del(d_.nexus_turrets); break;
        case Obj::Camp: del(d_.camps); break;
        case Obj::AggroCamp: del(d_.aggro_camps); break;
        case Obj::BuffCamp: del(d_.buff_camps); break;
        case Obj::DarkSpirit: del(d_.dark_spirits); break;
        case Obj::Epic: d_.epics.erase(d_.epics.begin() + i); break;
        case Obj::Spot: d_.spots.erase(d_.spots.begin() + i); break;
        case Obj::Waypoint:
            if (d_.lanes[std::size_t(i)].size() > 2) d_.lanes[std::size_t(i)].erase(d_.lanes[std::size_t(i)].begin() + j);
            else status_ = tr("a lane needs at least two points");
            break;
        case Obj::T3: d_.lane_structs[std::size_t(i)].t3 = kNoCell; break;
        default: status_ = tr("this one is required: drag it instead"); break;
    }
    check_map();
}

void MapMaker::place_object(CellPt at) {
    push_undo();
    switch (place_) {
        case Obj::Base: d_.base = at; break;
        case Obj::Fountain: d_.fountain = at; break;
        case Obj::Nexus: d_.nexus = at; break;
        case Obj::Pole: d_.pole = at; break;
        case Obj::Portal: d_.portal = at; break;
        case Obj::SpiritIsland: d_.spirit_island = at; break;
        case Obj::NexusTurret: d_.nexus_turrets.push_back(at); break;
        case Obj::Camp: d_.camps.push_back(at); break;
        case Obj::AggroCamp: d_.aggro_camps.push_back(at); break;
        case Obj::BuffCamp: d_.buff_camps.push_back(at); break;
        case Obj::DarkSpirit: d_.dark_spirits.push_back(at); break;
        case Obj::Epic: d_.epics.push_back({at, place_monster_}); break;
        case Obj::Spot: d_.spots.push_back({at, place_monster_, place_mirrored_}); break;
        default: break;
    }
    // one of a kind: done; lists stay ready for the next click
    if (place_ <= Obj::SpiritIsland) place_ = Obj::None;
    check_map();
}

// [U-518] turrets along the Fire half of a lane: from the base out (tier 3, tier 2, tier 1) + the inhibitor closest to the base
void MapMaker::auto_structures(int lane) {
    const auto& l = d_.lanes[std::size_t(lane)];
    std::vector<double> len{0};
    for (std::size_t i = 1; i < l.size(); ++i) len.push_back(len.back() + std::hypot(double(l[i].first - l[i - 1].first), double(l[i].second - l[i - 1].second)));
    auto at = [&](double f) {
        const double want = len.back() * f;
        for (std::size_t i = 1; i < l.size(); ++i)
            if (want <= len[i]) {
                const double t = (want - len[i - 1]) / std::max(1.0, len[i] - len[i - 1]);
                return CellPt{int(l[i - 1].first + (l[i].first - l[i - 1].first) * t), int(l[i - 1].second + (l[i].second - l[i - 1].second) * t)};
            }
        return l.back();
    };
    d_.lane_structs[std::size_t(lane)] = {at(0.36), at(0.24), at(0.13), at(0.05)};
    dirty_ = true;
}

// ---------------------------------------------------------------- is it playable?
void MapMaker::check_map() {
    problems_.clear();
    auto err = [&](std::string t, CellPt at = kNoCell) { problems_.push_back({true, std::move(t), at}); };
    auto warn = [&](std::string t, CellPt at = kNoCell) { problems_.push_back({false, std::move(t), at}); };
    auto open_cell = [&](CellPt c) {
        if (!Grid::in_bounds(c.first, c.second)) return false;
        const Cell k = cell(c.first, c.second);
        return k.walk() == Walk::Walk && k.wall() == Wall::None && k.tree() != Tree::Alive;
    };
    if (!open_cell(d_.fountain)) err(tr("The fountain stands on a cell nobody can walk on"), d_.fountain);
    if (!open_cell(d_.nexus)) warn(tr("The shrine stands on a blocked cell"), d_.nexus);
    if (d_.lanes.empty()) err(tr("A map needs at least one lane"));
    // can a champion walk from my fountain to every waypoint and to the enemy fountain?
    std::vector<std::uint8_t> reached(std::size_t(N) * N, 0);
    if (open_cell(d_.fountain)) {
        std::vector<std::pair<int, int>> st{d_.fountain};
        reached[idx(d_.fountain.first, d_.fountain.second)] = 1;
        while (!st.empty()) {
            const auto [x, y] = st.back();
            st.pop_back();
            const Cell here = cell(x, y);
            for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                const int nx = x + dx, ny = y + dy;
                if (!Grid::in_bounds(nx, ny) || reached[idx(nx, ny)]) continue;
                const Cell to = cell(nx, ny);
                if (to.walk() != Walk::Walk || to.wall() != Wall::None || to.tree() == Tree::Alive || std::abs(to.elevation() - here.elevation()) > 1) continue;
                reached[idx(nx, ny)] = 1;
                st.push_back({nx, ny});
            }
        }
        const CellPt other = mirror(d_.fountain);
        if (!reached[idx(other.first, other.second)]) err(tr("You cannot walk from one fountain to the other"), other);
    }
    for (std::size_t i = 0; i < d_.lanes.size(); ++i) {
        for (const CellPt& p : d_.lanes[i])
            if (!reached[idx(p.first, p.second)]) {
                err(trf("Lane {}: a point cannot be reached on foot", i + 1), p);
                break;
            }
        const LaneStructs& s = d_.lane_structs[i];
        if (s.t1 == kNoCell || s.t2 == kNoCell || s.inhib == kNoCell) err(trf("Lane {}: it needs two turrets and an inhibitor", i + 1));
    }
    if (d_.lanes.size() > 1)
        for (std::size_t i = 0; i < d_.lanes.size(); ++i) {
            const auto& a = d_.lanes[i];
            const auto& b = d_.lanes[d_.lanes.size() - 1 - i];
            if (a.front() != mirror(b.back()) && std::hypot(double(a.front().first - mirror(b.back()).first), double(a.front().second - mirror(b.back()).second)) > 40)
                warn(trf("Lane {}: it does not mirror its opposite lane: the sides are not equal", i + 1), a.front());
        }
    bool symmetric = true;
    for (int y = 0; y < N && symmetric; ++y)
        for (int x = 0; x < N; ++x)
            if (d_.cells[idx(x, y)] != d_.cells[idx(N - 1 - x, N - 1 - y)]) {
                symmetric = false;
                warn(tr("The terrain is not mirrored: the two sides are not equal"), {x, y});
                break;
            }
    if (d_.exact_terrain)  // (otherwise the game clears a pit or an island for them)
        for (const auto& [c, n] : d_.epics) if (!open_cell(c)) warn(trf("{} stands on a blocked cell", n), c);
    if (d_.fountain != kNoCell && d_.nexus != kNoCell && std::hypot(double(d_.fountain.first - d_.nexus.first), double(d_.fountain.second - d_.nexus.second)) > 120)
        warn(tr("The fountain is far from the shrine"), d_.fountain);
}

// ---------------------------------------------------------------- files
void MapMaker::write_file(const std::string& id) {
    d_.id = id;
    if (d_.pack.empty() || (d_.pack.starts_with("map_") && !is_builtin_map_id(id) && !fs::exists(fs::path("resourcepacks") / d_.pack))) d_.pack = "map_" + id;
    const std::string blob = doc_to_blob(d_);
    MapDoc test;
    if (const std::string e = doc_from_blob(blob, test); !e.empty()) {
        status_ = trf("It cannot be saved: {}", tr(e));
        return;
    }
    std::error_code ec;
    fs::create_directories("custom_maps", ec);
    const fs::path file = fs::path("custom_maps") / (id + ".emap");
    {
        std::ofstream out(file.string() + ".tmp", std::ios::binary);
        out << blob;
        if (!out) {
            status_ = trf("could not write {}", file.string());
            return;
        }
    }
    fs::rename(file.string() + ".tmp", file, ec);
    file_id_ = id;
    dirty_ = false;
    load_custom_maps();
    from_builtin_ = false;
    status_ = trf("saved custom_maps/{}.emap: choose it as the map when you play or host", id);
}

void MapMaker::save_new(const std::string& name) {
    d_.name = name.empty() ? d_.name : name;
    std::string id = map_id_of(d_.name), base = id;
    std::error_code ec;
    for (int n = 2; is_builtin_map_id(id) || fs::exists(fs::path("custom_maps") / (id + ".emap"), ec); ++n) id = std::format("{}_{}", base, n);  // never over another map
    d_.pack.clear();
    write_file(id);
}

void MapMaker::overwrite() {
    if (file_id_.empty()) return save_new(d_.name);
    write_file(file_id_);
}

void SDLCALL MapMaker::on_files(void* self, const char* const* list, int) {
    auto* m = static_cast<MapMaker*>(self);
    if (!list || !*list) return;
    const std::scoped_lock lock(m->dlg_mu_);
    m->dlg_result_ = *list;
}

void MapMaker::ask_file(const std::string& what, bool picture) {
    {
        const std::scoped_lock lock(dlg_mu_);
        dlg_what_ = what;
        dlg_result_.clear();
    }
    static const SDL_DialogFileFilter kPic[] = {{"PNG", "png"}};
    static const SDL_DialogFileFilter kWav[] = {{"WAV", "wav"}};
    SDL_ShowOpenFileDialog(on_files, this, win.win(), picture ? kPic : kWav, 1, nullptr, false);
}

// A texture or music file goes into the map's own resource pack (tried first while the map is played).
void MapMaker::got_file(const std::string& what, const std::string& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        status_ = trf("file not found: {}", path);
        return;
    }
    if (d_.pack.empty()) d_.pack = "map_" + map_id_of(d_.name);
    const fs::path root = fs::path("resourcepacks") / d_.pack;
    const fs::path to = root / what;
    fs::create_directories(to.parent_path(), ec);
    fs::copy_file(path, to, fs::copy_options::overwrite_existing, ec);
    status_ = ec ? trf("could not write {}", to.string()) : trf("picked {}", fs::path(path).filename().string());
    dirty_ = true;
}

void MapMaker::take_drop(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    for (char& c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".emap") {
        std::ifstream in(path, std::ios::binary);
        const std::string blob((std::istreambuf_iterator<char>(in)), {});
        MapDoc m;
        const std::string e = doc_from_blob(blob, m);
        if (!e.empty()) {
            status_ = tr(e);
            return;
        }
        d_ = std::move(m);
        file_id_.clear();
        tex_dirty_ = true;
        check_map();
    }
}

void MapMaker::open_picker(std::string title, std::vector<Pick> items, std::function<void(const std::string&)> done) {
    pick_title_ = std::move(title), pick_items_ = std::move(items), pick_done_ = std::move(done);
    pick_search_.clear();
    pick_scroll_ = 0;
    win.ui.focus = 99;
}

void MapMaker::draw_picker() {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    const float pw = std::min(760.f, win.w - 80), ph = win.h - 120, x = (win.w - pw) / 2, y = 60;
    fill_rect(r, 0, 0, win.w, win.h, {0, 0, 0, 170});
    fill_rect(r, x, y, pw, ph, {30, 32, 44, 255});
    rect(r, x, y, pw, ph, {255, 190, 110, 255});
    text(r, x + 14, y + 10, fit_text(pick_title_, pw - 28, 2), {255, 220, 150}, 2);
    text_field(r, ui, 99, x + 14, y + 52, pw - 140, pick_search_, tr("Search"));
    if (button(r, ui, x + pw - 110, y + 50, 96, 30, tr("Close"))) {
        pick_done_ = {};
        ui.focus = -1;
        return;
    }
    std::string low = pick_search_;
    for (char& c : low) c = char(std::tolower(static_cast<unsigned char>(c)));
    const float top = y + 92, bottom = y + ph - 10;
    const SDL_Rect clip{int(x), int(top), int(pw), int(bottom - top)};
    SDL_SetRenderClipRect(r, &clip);
    pick_scroll_ = std::max(0.f, pick_scroll_ - ui.wheel * 40);
    float ry = top - pick_scroll_;
    std::function<void(const std::string&)> chosen;
    std::string value;
    for (const Pick& p : pick_items_) {
        std::string l = p.label + " " + p.note;
        for (char& c : l) c = char(std::tolower(static_cast<unsigned char>(c)));
        if (!low.empty() && l.find(low) == std::string::npos) continue;
        if (ry > top - 30 && ry < bottom) {
            if (button(r, ui, x + 14, ry, 300, 26, p.label) && hover(ui, x, top, pw, bottom - top)) chosen = pick_done_, value = p.value;
            text(r, x + 324, ry + 8, fit_text(p.note, pw - 340), {180, 180, 195});
        }
        ry += 30;
    }
    SDL_SetRenderClipRect(r, nullptr);
    if (chosen) {
        pick_done_ = {};
        ui.focus = -1;
        chosen(value);
    }
}

// ---------------------------------------------------------------- tab: terrain
void MapMaker::tab_paint(float x, float y0, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    float y = y0;
    const float tw = (w - 8) / 3;
    for (int i = 0; i < 6; ++i) {
        const float bx = x + tw * float(i % 3), by = y + 28 * float(i / 3);
        if (hover(ui, bx, by, tw, 26)) tip_ = tr(kToolTips[i]);
        if (button(r, ui, bx, by, tw - 4, 24, tr(kToolNames[i]), tool_ == Tool(i))) tool_ = Tool(i);
    }
    y += 62;
    if (slider(r, ui, 40, x, y, w, size_, 1, 80, tr("Brush size"))) {}
    y += 26;
    if (button(r, ui, x, y, w / 2 - 4, 24, round_ ? tr("Round brush") : tr("Square brush"))) round_ = !round_;
    if (hover(ui, x + w / 2, y, w / 2, 26)) tip_ = tr("Whatever you paint also appears at the mirror point: both sides stay equal");
    if (button(r, ui, x + w / 2, y, w / 2 - 4, 24, std::format("{}: {}", tr("Mirror the other side"), mirror_on_ ? tr("ON") : tr("OFF")), mirror_on_)) mirror_on_ = !mirror_on_;
    y += 30;
    if (tool_ == Tool::Scatter) {
        slider(r, ui, 41, x, y, w, scatter_, 1, 100, tr("How many cells %"));
        y += 26;
    }
    y += 4;
    text(r, x, y, tr("What it paints"), {255, 220, 150}, 2);
    y += 26;
    const float lw = (w - 8) / 4;
    for (int i = 0; i < int(Layer::Count); ++i) {
        const float bx = x + lw * float(i % 4), by = y + 26 * float(i / 4);
        if (button(r, ui, bx, by, lw - 3, 24, tr(kLayerNames[i]), layer_ == Layer(i))) {
            layer_ = Layer(i);
            value_ = layer_ == Layer::Elevation ? 0 : layer_ == Layer::Walk ? 1 : 1;
            tex_dirty_ = true;
        }
    }
    y += 26 * 3 + 6;
    auto values = [&](const char* const* names, int n) {
        const float vw = (w - 8) / 4;
        for (int i = 0; i < n; ++i) {
            const float bx = x + vw * float(i % 4), by = y + 28 * float(i / 4);
            if (button(r, ui, bx, by, vw - 3, 26, tr(names[i]), value_ == i)) value_ = i;
        }
        y += 28 * float((n + 3) / 4) + 4;
    };
    switch (layer_) {
        case Layer::Preset: {
            const float pw = (w - 8) / 3;
            for (std::size_t i = 0; i < std::size(kPresets); ++i) {
                const float bx = x + pw * float(i % 3), by = y + 34 * float(i / 3);
                if (hover(ui, bx, by, pw, 32) && *kPresets[i].tip) tip_ = tr(kPresets[i].tip);
                if (button(r, ui, bx, by, pw - 3, 30, "", preset_ == int(i))) preset_ = int(i);
                fill_rect(r, bx + 5, by + 7, 16, 16, kPresets[i].col);
                rect(r, bx + 5, by + 7, 16, 16, {220, 220, 230, 255});
                text(r, bx + 26, by + 11, fit_text(tr(kPresets[i].name), pw - 34), {235, 235, 240});
            }
            y += 34 * float((std::size(kPresets) + 2) / 3);
            break;
        }
        case Layer::Surface: {
            const float pw = (w - 8) / 3;
            for (int i = 0; i < int(std::size(kSurfaceNames)); ++i) {
                const float bx = x + pw * float(i % 3), by = y + 34 * float(i / 3);
                if (button(r, ui, bx, by, pw - 3, 30, "", value_ == i)) value_ = i;
                fill_rect(r, bx + 5, by + 7, 16, 16, kSurfaceCol[i]);
                text(r, bx + 26, by + 11, fit_text(tr(kSurfaceNames[i]), pw - 34), {235, 235, 240});
            }
            y += 34 * 5;
            text(r, x, y, fit_text(tr("Only the ground: walking stays as it was. Ready made sets both."), w), {150, 150, 170});
            y += 18;
            break;
        }
        case Layer::Elevation:
            slider(r, ui, 42, x, y, w, value_, kMinElevation, kMaxElevation, tr("Height"));
            y += 26;
            text(r, x, y, fit_text(tr("Height differences of 2 or more cannot be walked: that makes cliffs."), w), {150, 150, 170});
            y += 18;
            break;
        case Layer::Walk: values(kWalkNames, 4); break;
        case Layer::Wall: values(kWallNames, 4); break;
        case Layer::Tree: values(kTreeNames, 4); break;
        case Layer::Bush: values(kBushNames, 3); break;
        default: values(kFogNames, 4); break;
    }
    y += 6;
    if (button(r, ui, x, y, w / 2 - 4, 26, tr("Copy this half to the other"))) {
        push_undo();
        for (int yy = 0; yy < N; ++yy)
            for (int xx = 0; xx < N; ++xx)
                if (xx + yy < N - 1 || (xx + yy == N - 1 && xx < yy)) d_.cells[idx(N - 1 - xx, N - 1 - yy)] = d_.cells[idx(xx, yy)];
        tex_dirty_ = true;
        check_map();
    }
    if (hover(ui, x, y, w / 2, 26)) tip_ = tr("The Fire side (top left of the diagonal) is copied over the Earth side");
    if (button(r, ui, x + w / 2, y, w / 2 - 4, 26, std::format("{}: {}", tr("Cell grid"), grid_lines_ ? tr("ON") : tr("OFF")), grid_lines_)) grid_lines_ = !grid_lines_;
}

// ---------------------------------------------------------------- tab: objects
void MapMaker::tab_objects(float x, float y0, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    float y = y0;
    text(r, x, y, fit_text(tr("Pick one, click on the map. Drag to move, right click removes."), w), {255, 220, 150});
    y += 20;
    text(r, x, y, fit_text(tr("You place the Fire side: the Earth side is always its mirror (dim marks)."), w), {150, 150, 170});
    y += 22;
    const float bw = (w - 8) / 2;
    auto count_of = [&](Obj k) -> int {
        switch (k) {
            case Obj::NexusTurret: return int(d_.nexus_turrets.size());
            case Obj::Camp: return int(d_.camps.size());
            case Obj::AggroCamp: return int(d_.aggro_camps.size());
            case Obj::BuffCamp: return int(d_.buff_camps.size());
            case Obj::DarkSpirit: return int(d_.dark_spirits.size());
            case Obj::Epic: return int(d_.epics.size());
            case Obj::Spot: return int(d_.spots.size());
            default: return -1;
        }
    };
    int i = 0;
    for (const ObjLook& o : kObjs) {
        const float bx = x + bw * float(i % 2), by = y + 34 * float(i / 2);
        if (hover(ui, bx, by, bw, 32) && *o.tip) tip_ = tr(o.tip);
        if (button(r, ui, bx, by, bw - 4, 30, "", place_ == o.kind)) {
            if (o.kind == Obj::Epic || o.kind == Obj::Spot) {
                std::vector<Pick> items;
                for (const MonsterDef& m : kMonsters) items.push_back({std::string(m.name), std::string(m.name), m.boss ? tr("epic") : ""});
                const Obj k = o.kind;
                open_picker(tr("Which monster"), std::move(items), [this, k](const std::string& v) {
                    place_ = k;
                    place_monster_ = v;
                });
            } else {
                place_ = place_ == o.kind ? Obj::None : o.kind;
            }
        }
        fill_circle(r, bx + 14, by + 15, 7, o.col, 12);
        const int n = count_of(o.kind);
        text(r, bx + 28, by + 11, fit_text(n >= 0 ? std::format("{} ({})", tr(o.name), n) : tr(o.name), bw - 36), {235, 235, 240});
        ++i;
    }
    y += 34 * float((std::size(kObjs) + 1) / 2) + 6;
    if (place_ == Obj::Spot) {
        if (button(r, ui, x, y, w, 26, std::format("{}: {}", tr("Also at the mirror point"), place_mirrored_ ? tr("ON") : tr("OFF")), place_mirrored_)) place_mirrored_ = !place_mirrored_;
        y += 32;
    }
    if (place_ == Obj::Epic || place_ == Obj::Spot) {
        text(r, x, y, fit_text(trf("Click on the map to place: {}", place_monster_), w), {170, 240, 180});
        y += 20;
    }
    if (place_ != Obj::None && button(r, ui, x, y, 160, 26, tr("Stop placing"))) place_ = Obj::None;
    y += 34;
    if (hover(ui, x, y, w, 26)) tip_ = tr("Off: camps get a clearing, the base gets ponds, epics get fords. On: exactly what you painted");
    if (button(r, ui, x, y, w, 26, std::format("{}: {}", tr("Terrain exactly as painted"), d_.exact_terrain ? tr("ON") : tr("OFF")), d_.exact_terrain)) {
        push_undo();
        d_.exact_terrain = !d_.exact_terrain;
    }
    y += 30;
    if (!d_.exact_terrain && button(r, ui, x, y, w, 26, std::format("{}: {}", tr("Epics on sea islands with a ford"), d_.epic_islands ? tr("ON") : tr("OFF")), d_.epic_islands)) {
        push_undo();
        d_.epic_islands = !d_.epic_islands;
    }
}

// ---------------------------------------------------------------- tab: lanes
void MapMaker::tab_lanes(float x, float y0, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    float y = y0;
    text(r, x, y, fit_text(tr("A lane goes from the Fire base to the Earth base: minions walk it."), w), {255, 220, 150});
    y += 20;
    text(r, x, y, fit_text(tr("Click on the map = a new point at the end. Drag points and turrets. Right click removes."), w), {150, 150, 170});
    y += 24;
    const float bw = 70;
    for (int i = 0; i < int(d_.lanes.size()); ++i)
        if (button(r, ui, x + (bw + 4) * float(i), y, bw, 30, trf("Lane {}", i + 1), lane_ == i)) lane_ = i;
    y += 38;
    if (d_.lanes.size() < 8 && button(r, ui, x, y, 150, 28, tr("Add a lane"))) {
        push_undo();
        d_.lanes.push_back({{62, 449}, {449, 62}});
        d_.lane_structs.push_back({kNoCell, kNoCell, kNoCell, kNoCell});
        lane_ = int(d_.lanes.size()) - 1;
        auto_structures(lane_);
        check_map();
    }
    if (d_.lanes.size() > 1 && button(r, ui, x + 156, y, 170, 28, tr("Remove this lane"))) {
        push_undo();
        d_.lanes.erase(d_.lanes.begin() + lane_);
        d_.lane_structs.erase(d_.lane_structs.begin() + lane_);
        lane_ = std::clamp(lane_, 0, int(d_.lanes.size()) - 1);
        check_map();
    }
    y += 36;
    if (hover(ui, x, y, w, 28)) tip_ = tr("Puts the turrets and the inhibitor along this lane, evenly on the Fire half");
    if (button(r, ui, x, y, 240, 28, tr("Place turrets automatically"))) {
        push_undo();
        auto_structures(lane_);
        check_map();
    }
    if (hover(ui, x + 246, y, w - 246, 28)) tip_ = tr("The opposite lane becomes this lane's mirror, so both teams have the same way");
    if (d_.lanes.size() > 1 && button(r, ui, x + 246, y, 240, 28, tr("Mirror to the opposite lane"))) {
        push_undo();
        const std::size_t other = d_.lanes.size() - 1 - std::size_t(lane_);
        std::vector<CellPt> m;
        for (auto it = d_.lanes[std::size_t(lane_)].rbegin(); it != d_.lanes[std::size_t(lane_)].rend(); ++it) m.push_back(mirror(*it));
        d_.lanes[other] = std::move(m);
        check_map();
    }
    y += 40;
    const auto& l = d_.lanes[std::size_t(lane_)];
    const LaneStructs& s = d_.lane_structs[std::size_t(lane_)];
    text(r, x, y, trf("Points: {}", l.size()), {230, 230, 235});
    y += 18;
    for (const auto& [name, c] : {std::pair{"Outer turret", s.t1}, {"Middle turret", s.t2}, {"Base turret (optional)", s.t3}, {"Inhibitor", s.inhib}}) {
        text(r, x, y, c == kNoCell ? std::format("{}: {}", tr(name), tr("none")) : std::format("{}: {}, {}", tr(name), c.first, c.second), {200, 200, 210});
        y += 18;
    }
    if (s.t3 == kNoCell && button(r, ui, x, y + 4, 240, 26, tr("Add the base turret"))) {
        push_undo();
        d_.lane_structs[std::size_t(lane_)].t3 = {(s.t2.first + s.inhib.first) / 2, (s.t2.second + s.inhib.second) / 2};
    }
}

// ---------------------------------------------------------------- tab: settings
void MapMaker::tab_settings(float x, float y0, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    const SDL_Rect clip{int(x - 8), int(y0), int(w + 16), int(win.h - 56 - y0)};
    SDL_SetRenderClipRect(r, &clip);
    if (hover(ui, x, y0, w, win.h - 56 - y0)) scroll_ = std::max(0.f, scroll_ - ui.wheel * 44);
    float y = y0 + 14 - scroll_;
    {
        const std::string before = d_.name;
        text_field(r, ui, 1, x, y, w, d_.name, tr("Map name"));
        if (d_.name != before) dirty_ = true;
    }
    y += 40;
    text_field(r, ui, 2, x, y, w, d_.desc, tr("Description (shown when choosing the map)"));
    y += 38;
    int ts = d_.team_size;
    if (slider(r, ui, 43, x, y, w, ts, 1, 10, tr("Players per team"))) d_.team_size = ts, dirty_ = true;
    y += 30;
    text(r, x, y, tr("Minion waves"), {255, 220, 150}, 2);
    y += 26;
    auto num = [&](int id, const char* label, int v, int lo, int hi, auto&& set) {
        if (slider(r, ui, id, x, y, w, v, lo, hi, tr(label))) set(v), dirty_ = true;
        y += 26;
    };
    num(44, "Melee minions", d_.minions.melee, 0, 12, [&](int v) { d_.minions.melee = v; });
    num(45, "Ranged minions", d_.minions.ranged, 0, 12, [&](int v) { d_.minions.ranged = v; });
    num(46, "A siege minion every N waves (0 = never)", d_.minions.siege_every, 0, 20, [&](int v) { d_.minions.siege_every = v; });
    num(47, "First wave (s)", int(d_.minions.first_ms / 1000), 1, 600, [&](int v) { d_.minions.first_ms = v * 1000LL; });
    num(48, "Between waves (s)", int(d_.minions.period_ms / 1000), 5, 300, [&](int v) { d_.minions.period_ms = v * 1000LL; });
    y += 6;
    text(r, x, y, tr("Match features this map turns on"), {255, 220, 150}, 2);
    y += 26;
    for (int i = 0; i < kFeatureCount; ++i) {
        const bool on = (d_.features >> i) & 1;
        const float bx = x + (w / 2) * float(i % 2);
        if (hover(ui, bx, y, w / 2, 26)) tip_ = tr(kFeatureDescs[i]);
        if (button(r, ui, bx, y, w / 2 - 4, 24, std::format("{}: {}", tr(kFeatureNames[i]), on ? tr("ON") : tr("OFF")), on)) d_.features = std::uint16_t(d_.features ^ (1u << i)), dirty_ = true;
        if (i % 2) y += 28;
    }
    y += 34;
    text(r, x, y, tr("The map's own look"), {255, 220, 150}, 2);
    y += 26;
    text(r, x, y, fit_text(trf("Pack: {} (these files are used first while this map is played)", d_.pack.empty() ? "map_" + map_id_of(d_.name) : d_.pack), w), {150, 150, 170});
    y += 20;
    struct Tex { const char* label; const char* file; bool pic; };
    static constexpr Tex kTex[] = {
        {"Grass texture", "textures/grass.png", true}, {"Dry land texture", "textures/dirt.png", true}, {"Sand texture", "textures/sand.png", true}, {"Stone texture", "textures/structure_stone.png", true},
        {"Road texture", "textures/stone_road.png", true}, {"Path texture", "textures/forest_path.png", true}, {"Glacier texture", "textures/glacier.png", true}, {"Wood texture", "textures/wood.png", true},
        {"Shallow water texture", "textures/shallow_water.png", true}, {"Deep water texture", "textures/deep_water.png", true}, {"Lava texture", "textures/lava.png", true},
        {"Tree picture", "textures/tree_canopy.png", true}, {"Bush picture", "textures/bush.png", true}, {"Music of the match", "music/match.wav", false},
    };
    for (const Tex& t : kTex) {
        if (button(r, ui, x, y, 240, 24, tr(t.label))) ask_file(t.file, t.pic);
        std::error_code ec;
        const bool has = !d_.pack.empty() && fs::exists(fs::path("resourcepacks") / d_.pack / t.file, ec);
        text(r, x + 250, y + 8, has ? tr("own file") : tr("the game's usual one"), has ? Rgba{170, 240, 180} : Rgba{150, 150, 170});
        y += 28;
    }
    SDL_SetRenderClipRect(r, nullptr);
    if (ui.enter) ui.focus = -1;
}

// ---------------------------------------------------------------- tab: check
void MapMaker::tab_check(float x, float y0, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    float y = y0;
    if (button(r, ui, x, y, 200, 30, tr("Check again"))) check_map();
    y += 40;
    if (problems_.empty()) {
        text(r, x, y, tr("Everything is fine: this map can be played."), {160, 240, 170}, 2);
        return;
    }
    for (const Problem& p : problems_) {
        if (button(r, ui, x, y, w, 26, fit_text(p.text, w - 20), false) && p.at != kNoCell) cam_x_ = float(p.at.first), cam_y_ = float(p.at.second), zoom_ = std::max(zoom_, 3.f);
        fill_rect(r, x, y, 5, 26, p.error ? Rgba{255, 90, 80, 255} : Rgba{255, 210, 90, 255});
        y += 30;
    }
    text(r, x, y + 4, fit_text(tr("Click a line to see the place. Red = it will not work, yellow = it works but is not fair."), w), {150, 150, 170});
}

// ---------------------------------------------------------------- the map
void MapMaker::draw_objects() {
    SDL_Renderer* r = win.ren();
    auto mark = [&](CellPt c, const ObjLook& o, float rad = 5, std::string_view label = {}) {
        if (c == kNoCell) return;
        const float sx = sxf(float(c.first)), sy = syf(float(c.second));
        const CellPt m = mirror(c);
        fill_circle(r, sxf(float(m.first)), syf(float(m.second)), rad, with_alpha(o.col, 90), 12);
        fill_circle(r, sx, sy, rad, o.col, 12);
        circle(r, sx, sy, rad, {0, 0, 0, 255}, 12);
        const std::string_view t = label.empty() ? std::string_view(o.tag) : label;
        if (zoom_ > 0.7f) text(r, sx + rad + 2, sy - 4, t, {255, 255, 255});
    };
    // lanes under everything
    for (std::size_t i = 0; i < d_.lanes.size(); ++i) {
        const bool sel = tab_ == 2 && int(i) == lane_;
        const Rgba c = sel ? Rgba{255, 240, 120, 255} : Rgba{255, 255, 255, 130};
        const auto& l = d_.lanes[i];
        for (std::size_t k = 1; k < l.size(); ++k) {
            line(r, sxf(float(l[k - 1].first)), syf(float(l[k - 1].second)), sxf(float(l[k].first)), syf(float(l[k].second)), c);
            if (sel) line(r, sxf(float(l[k - 1].first)) + 1, syf(float(l[k - 1].second)), sxf(float(l[k].first)) + 1, syf(float(l[k].second)), c);
        }
        if (sel)
            for (std::size_t k = 0; k < l.size(); ++k) {
                fill_circle(r, sxf(float(l[k].first)), syf(float(l[k].second)), 5, {255, 240, 120, 255}, 10);
                text(r, sxf(float(l[k].first)) + 7, syf(float(l[k].second)) - 4, std::to_string(k + 1), {255, 255, 200});
            }
        const LaneStructs& s = d_.lane_structs[i];
        static constexpr ObjLook kT{Obj::T1, "", "", "T", {255, 110, 90}};
        static constexpr ObjLook kI{Obj::Inhib, "", "", "I", {255, 170, 60}};
        mark(s.t1, kT, sel ? 6.f : 4.f, "T1"), mark(s.t2, kT, sel ? 6.f : 4.f, "T2"), mark(s.t3, kT, sel ? 6.f : 4.f, "T3"), mark(s.inhib, kI, sel ? 6.f : 4.f, "I");
    }
    mark(d_.base, look_of(Obj::Base), 6), mark(d_.fountain, look_of(Obj::Fountain), 7), mark(d_.nexus, look_of(Obj::Nexus), 8);
    mark(d_.pole, look_of(Obj::Pole)), mark(d_.portal, look_of(Obj::Portal)), mark(d_.spirit_island, look_of(Obj::SpiritIsland));
    for (const CellPt& c : d_.nexus_turrets) mark(c, look_of(Obj::NexusTurret));
    for (const CellPt& c : d_.camps) mark(c, look_of(Obj::Camp));
    for (const CellPt& c : d_.aggro_camps) mark(c, look_of(Obj::AggroCamp));
    for (const CellPt& c : d_.buff_camps) mark(c, look_of(Obj::BuffCamp));
    for (const CellPt& c : d_.dark_spirits) mark(c, look_of(Obj::DarkSpirit));
    for (const auto& [c, n] : d_.epics) {  // not mirrored
        fill_circle(r, sxf(float(c.first)), syf(float(c.second)), 7, look_of(Obj::Epic).col, 12);
        if (zoom_ > 0.7f) text(r, sxf(float(c.first)) + 9, syf(float(c.second)) - 4, n, {255, 255, 255});
    }
    for (const MapDoc::Spot& s : d_.spots) {
        if (s.mirrored) mark(s.cell, look_of(Obj::Spot), 5, s.monster);
        else {
            fill_circle(r, sxf(float(s.cell.first)), syf(float(s.cell.second)), 5, look_of(Obj::Spot).col, 12);
            if (zoom_ > 0.7f) text(r, sxf(float(s.cell.first)) + 7, syf(float(s.cell.second)) - 4, s.monster, {255, 255, 255});
        }
    }
}

void MapMaker::map_view() {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    const SDL_Rect clip{int(vx_), int(vy_), int(vw_), int(vh_)};
    fill_rect(r, vx_, vy_, vw_, vh_, {12, 14, 22, 255});
    SDL_SetRenderClipRect(r, &clip);
    if (tex_dirty_) rebuild_texture();
    const SDL_FRect dst{sxf(0), syf(0), N * zoom_, N * zoom_};
    SDL_RenderTexture(r, tex_, nullptr, &dst);
    rect(r, dst.x, dst.y, dst.w, dst.h, {230, 230, 240, 255});
    if (grid_lines_ && zoom_ >= 4)
        for (int i = 0; i <= N; i += 1) {
            if (sxf(float(i)) >= vx_ && sxf(float(i)) <= vx_ + vw_) line(r, sxf(float(i)), std::max(vy_, dst.y), sxf(float(i)), std::min(vy_ + vh_, dst.y + dst.h), {0, 0, 0, 50});
            if (syf(float(i)) >= vy_ && syf(float(i)) <= vy_ + vh_) line(r, std::max(vx_, dst.x), syf(float(i)), std::min(vx_ + vw_, dst.x + dst.w), syf(float(i)), {0, 0, 0, 50});
        }
    // the mirror axis: the two sides
    line(r, sxf(0), syf(float(N)), sxf(float(N)), syf(0), {255, 255, 255, 40});
    draw_objects();
    const bool over = hover(ui, vx_, vy_, vw_, vh_) && !pick_done_ && !saving_;
    const int cx = int(std::floor(cxf(ui.mx))), cy = int(std::floor(cyf(ui.my)));
    if (over) {
        if (ui.wheel != 0) {  // zoom towards the cursor
            const float wx = cxf(ui.mx), wy = cyf(ui.my);
            zoom_ = std::clamp(zoom_ * (1.f + ui.wheel * 0.15f), 0.4f, 24.f);
            cam_x_ += wx - cxf(ui.mx), cam_y_ += wy - cyf(ui.my);
        }
        const bool* ks = SDL_GetKeyboardState(nullptr);
        const SDL_MouseButtonFlags mb = SDL_GetMouseState(nullptr, nullptr);
        const bool mid = (mb & SDL_BUTTON_MMASK) || (ks[SDL_SCANCODE_SPACE] && ui.down);
        if (mid && !panning_) panning_ = true, pan_mx_ = ui.mx, pan_my_ = ui.my;
        if (panning_) {
            cam_x_ -= (ui.mx - pan_mx_) / zoom_, cam_y_ -= (ui.my - pan_my_) / zoom_;
            pan_mx_ = ui.mx, pan_my_ = ui.my;
            ui.clicked = false;
        }
        if (Grid::in_bounds(cx, cy)) {
            const Cell c = cell(cx, cy);
            tip_ = std::format("{}, {}: {}, {} {}, {}", cx, cy, tr(kSurfaceNames[std::min<std::size_t>(std::size_t(c.surface()), std::size(kSurfaceNames) - 1)]), tr("height"), c.elevation(), tr(kWalkNames[std::size_t(c.walk())]));
        }
    }
    if (!(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MMASK) && !(SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_SPACE] && ui.down)) panning_ = false;
    // ---- what a click does depends on the tab
    if (tab_ == 0 && over && !panning_) {
        const int sz = (tool_ == Tool::Brush || tool_ == Tool::Scatter || tool_ == Tool::Line) ? size_ : 1;
        const float pr = float(sz) / 2 * zoom_;
        if (round_) circle(r, ui.mx, ui.my, std::max(2.f, pr), {255, 255, 255, 220}, 28);
        else rect(r, ui.mx - pr, ui.my - pr, pr * 2, pr * 2, {255, 255, 255, 220});
        if (mirror_on_) {
            const CellPt m = mirror({cx, cy});
            circle(r, sxf(float(m.first)), syf(float(m.second)), std::max(2.f, pr), {255, 255, 255, 90}, 24);
        }
        if (ui.clicked) {
            ui.clicked = false;
            if (tool_ == Tool::Picker) {
                if (Grid::in_bounds(cx, cy)) {
                    value_ = value_of(cell(cx, cy));
                    if (layer_ == Layer::Preset) {
                        layer_ = Layer::Surface;
                        tex_dirty_ = true;
                    }
                    tool_ = Tool::Brush;
                }
            } else if (tool_ == Tool::Fill) {
                push_undo();
                fill_op(cx, cy);
                check_map();
            } else {
                push_undo();
                stroke_ = true;
                sx_ = cx, sy_ = cy;
                if (tool_ == Tool::Brush || tool_ == Tool::Scatter) brush(cx, cy);
            }
        }
    }
    if (tab_ == 0 && stroke_) {
        if (ui.down && (tool_ == Tool::Brush || tool_ == Tool::Scatter)) {
            line_op(sx_, sy_, cx, cy);
            sx_ = cx, sy_ = cy;
        }
        if (tool_ == Tool::Rect) rect(r, sxf(float(std::min(sx_, cx))), syf(float(std::min(sy_, cy))), float(std::abs(cx - sx_) + 1) * zoom_, float(std::abs(cy - sy_) + 1) * zoom_, {255, 255, 255, 230});
        if (tool_ == Tool::Line) line(r, sxf(float(sx_)), syf(float(sy_)), sxf(float(cx)), syf(float(cy)), {255, 255, 255, 230});
        if (!ui.down) {
            if (tool_ == Tool::Rect) rect_op(sx_, sy_, cx, cy);
            if (tool_ == Tool::Line) line_op(sx_, sy_, cx, cy);
            stroke_ = false;
            check_map();
        }
    }
    if ((tab_ == 1 || tab_ == 2) && !panning_) {
        const CellPt at{std::clamp(cx, 0, N - 1), std::clamp(cy, 0, N - 1)};
        if (over && ui.rclicked) {
            Obj k = Obj::None;
            int i = 0, j = 0;
            if (object_at(ui.mx, ui.my, k, i, j)) remove_object(k, i, j);
            ui.rclicked = false;
        }
        if (over && ui.clicked) {
            ui.clicked = false;
            Obj k = Obj::None;
            int i = 0, j = 0;
            if (tab_ == 1 && place_ != Obj::None) {
                place_object(at);
            } else if (object_at(ui.mx, ui.my, k, i, j)) {
                push_undo();
                drag_ = k, drag_i_ = i, drag_j_ = j;
            } else if (tab_ == 2) {
                push_undo();
                d_.lanes[std::size_t(lane_)].push_back(at);
                check_map();
            }
        }
        if (drag_ != Obj::None) {
            if (ui.down) move_object(drag_, drag_i_, drag_j_, at);
            else {
                drag_ = Obj::None;
                check_map();
            }
        }
    }
    SDL_SetRenderClipRect(r, nullptr);
    rect(r, vx_, vy_, vw_, vh_, {90, 90, 110, 255});
    if (button(r, ui, vx_ + vw_ - 96, vy_ + 8, 88, 24, tr("Whole map"))) {
        cam_x_ = cam_y_ = float(N) / 2;
        zoom_ = std::min(vw_, vh_) / float(N) * 0.96f;
    }
    text(r, vx_ + 10, vy_ + 10, tr("Wheel = zoom. Middle button or Space + drag = move."), {230, 230, 235});
}

// ---------------------------------------------------------------- the bar on top
void MapMaker::toolbar() {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    text(r, 14, 12, tr("Map maker"), {255, 200, 120}, 2);
    float x = std::max(170.f, 14 + text_w(tr("Map maker"), 2) + 16);
    if (button(r, ui, x, 8, 80, 28, tr("New"))) {
        open_picker(tr("New map"), {{tr("Three lanes"), "3", tr("Empty land, ready to paint")}, {tr("Two lanes"), "2", ""}, {tr("One lane"), "1", ""}},
                    [this](const std::string& v) { new_map(std::atoi(v.c_str())); });
    }
    x += 84;
    if (button(r, ui, x, 8, 80, 28, tr("Open"))) {
        std::vector<Pick> items;
        for (const MapDef& m : maps()) items.push_back({std::string(m.name), std::string(m.id), std::format("{}{}", is_builtin_map_id(m.id) ? tr("built in") + " - " : "", tr(m.desc))});
        open_picker(tr("Open a map (you can change it and save it as new)"), std::move(items), [this](const std::string& v) { open_map(v); });
    }
    x += 84;
    if (hover(ui, x, 8, 170, 28)) tip_ = tr("A new map beside the old one: nothing is overwritten");
    if (button(r, ui, x, 8, 170, 28, dirty_ || file_id_.empty() ? tr("Save as new *") : tr("Save as new"), dirty_)) {
        saving_ = true;
        save_name_ = file_id_.empty() ? d_.name : d_.name + " 2";
        ui.focus = 98;
    }
    x += 174;
    const bool can_over = !file_id_.empty();
    if (can_over && hover(ui, x, 8, 170, 28)) tip_ = from_builtin_ ? tr("Replaces the built-in map on this computer and on servers you host; remove the file to get the original back") : tr("Writes over the file you opened");
    if (button(r, ui, x, 8, 170, 28, overwrite_armed_ > 0 ? tr("Click again to overwrite") : tr("Overwrite"), overwrite_armed_ > 0, can_over)) {
        if (overwrite_armed_ > 0) overwrite_armed_ = 0, overwrite();
        else overwrite_armed_ = 240;
    }
    if (overwrite_armed_ > 0) --overwrite_armed_;
    x += 174;
    if (button(r, ui, x, 8, 170, 28, tr("Test in practice"))) {
        if (dirty_ || file_id_.empty()) {
            if (file_id_.empty()) save_new(d_.name);
            else overwrite();
        }
        const char* base = SDL_GetBasePath();
        const std::string exe = (fs::path(base ? base : "") / "elemental_moba").string();
        const char* args[] = {exe.c_str(), "--practice-map", file_id_.c_str(), nullptr};
        if (SDL_Process* p = SDL_CreateProcess(args, false)) SDL_DestroyProcess(p);
        else status_ = SDL_GetError();
    }
    x += 174;
    if (button(r, ui, x, 8, 80, 28, tr("Undo"), false, !undo_.empty())) do_undo();
    if (button(r, ui, win.w - 150, 8, 140, 28, std::format("{}: {}", tr("Language"), loc::current()))) loc::load(tool::next_language(loc::current()));
    const char* const tabs[] = {"Terrain", "Objects", "Lanes", "Settings", "Check"};
    float tx = 14;
    for (int i = 0; i < 5; ++i) {
        std::string lab = tr(tabs[i]);
        if (i == 4 && !problems_.empty()) lab += std::format(" ({})", problems_.size());
        if (button(r, ui, tx, 44, 118, 32, lab, tab_ == i)) {
            tab_ = i, scroll_ = 0;
            if (i == 4) check_map();
        }
        tx += 122;
    }
}

void MapMaker::frame() {
    Ui& ui = win.ui;
    tip_.clear();
    {
        std::string got, what;
        {
            const std::scoped_lock lock(dlg_mu_);
            if (!dlg_result_.empty()) got = std::move(dlg_result_), what = dlg_what_, dlg_result_.clear();
        }
        if (!got.empty()) got_file(what, got);
    }
    const bool modal = pick_done_ != nullptr || saving_;
    const bool real_clicked = ui.clicked, real_r = ui.rclicked;
    if (modal) ui.clicked = false, ui.rclicked = false;
    const float panel = std::min(640.f, win.w * 0.42f);
    if (!fitted_) {  // the whole map at the start
        fitted_ = true;
        zoom_ = std::min(win.w - panel - 6, win.h - 100) / float(N) * 0.96f;
    }
    toolbar();
    const float x = 14, w = panel - 28, y = 84;
    vx_ = panel, vy_ = 44, vw_ = win.w - panel - 6, vh_ = win.h - 44 - 56;
    if (hover(ui, vx_, vy_, vw_, vh_)) {
        map_view();
        if (tab_ == 0) tab_paint(x, y, w);
        else if (tab_ == 1) tab_objects(x, y, w);
        else if (tab_ == 2) tab_lanes(x, y, w);
        else if (tab_ == 3) tab_settings(x, y, w);
        else tab_check(x, y, w);
    } else {
        if (tab_ == 0) tab_paint(x, y, w);
        else if (tab_ == 1) tab_objects(x, y, w);
        else if (tab_ == 2) tab_lanes(x, y, w);
        else if (tab_ == 3) tab_settings(x, y, w);
        else tab_check(x, y, w);
        map_view();
    }
    SDL_Renderer* r = win.ren();
    fill_rect(r, 0, win.h - 50, win.w, 50, {16, 17, 24, 255});
    text(r, 14, win.h - 42, fit_text(tip_, win.w - 28), {230, 230, 200});
    text(r, 14, win.h - 24, fit_text(status_.empty() ? trf("Map: {}   Saved as: {}", d_.name, file_id_.empty() ? tr("not saved yet") : file_id_) : status_, win.w - 28), {170, 220, 170});
    if (saving_) {
        ui.clicked = real_clicked;
        const float dw = 520, dx = (win.w - dw) / 2, dy = win.h / 2 - 90;
        fill_rect(r, 0, 0, win.w, win.h, {0, 0, 0, 170});
        fill_rect(r, dx, dy, dw, 170, {30, 32, 44, 255});
        rect(r, dx, dy, dw, 170, {255, 190, 110, 255});
        text(r, dx + 16, dy + 12, tr("Save as a new map"), {255, 220, 150}, 2);
        text_field(r, ui, 98, dx + 16, dy + 62, dw - 32, save_name_, tr("Name of the new map"));
        if (button(r, ui, dx + 16, dy + 110, 200, 36, tr("Save")) || ui.enter) {
            saving_ = false;
            ui.focus = -1;
            save_new(save_name_);
        }
        if (button(r, ui, dx + 226, dy + 110, 140, 36, tr("Close"))) {
            saving_ = false;
            ui.focus = -1;
        }
    } else if (pick_done_) {
        ui.clicked = real_clicked, ui.rclicked = real_r;
        draw_picker();
    }
}

}  // namespace

int main(int argc, char** argv) {
    tool::go_to_game_folder();
    std::string open;
    int start_tab = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--tab" && i + 1 < argc) start_tab = std::atoi(argv[++i]);
        else open = a;
    }
    loc::load(tool::language_of_game());
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "SDL_Init: {}", SDL_GetError());
        return 1;
    }
    {
        auto maker = std::make_unique<MapMaker>();
        if (!maker->init(open)) {
            SDL_Quit();
            return 1;
        }
        maker->start_at(start_tab);
        std::string drop;
        while (maker->win.begin([&](const SDL_Event& e) {
            if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) drop = e.drop.data;
        })) {
            if (!drop.empty()) maker->take_drop(drop), drop.clear();
            maker->frame();
            maker->win.end();
        }
    }
    SDL_Quit();
    return 0;
}
