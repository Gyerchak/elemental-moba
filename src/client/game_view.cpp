// In-match view. See game_view.hpp.
#include "client/game_view.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "client/locale.hpp"
#include "client/modifiers.hpp"
#include "game/mobs/monsters.hpp"
#include "game/mobs/rules.hpp"
#include "game/mechanics/element_rules.hpp"
#include "game/match/layout.hpp"

namespace em {

using namespace gfx;

namespace {

constexpr std::uint32_t pack(Rgba c) {  // SDL_PIXELFORMAT_RGBA32 = bytes R,G,B,A in memory
    return std::uint32_t(c.r) | std::uint32_t(c.g) << 8 | std::uint32_t(c.b) << 16 | std::uint32_t(c.a) << 24;
}

Rgba cell_colour(Cell c) {
    Rgba col;
    switch (c.surface()) {
        case Surface::Land: col = {112, 162, 70}; break;
        case Surface::Dryland: col = {186, 160, 104}; break;
        case Surface::Glacier: col = {218, 236, 248}; break;
        case Surface::Water: col = {92, 165, 228}; break;
        case Surface::Sand: col = {228, 208, 148}; break;
        case Surface::Stone: col = {138, 134, 126}; break;
        case Surface::Wood: col = {150, 100, 60}; break;
        case Surface::DeepWater: col = {26, 64, 138}; break;
        case Surface::Magma: col = {242, 92, 22}; break;
        case Surface::Metal: col = {170, 175, 185}; break;
        case Surface::Platinum: col = {225, 228, 235}; break;
        case Surface::StoneRoad: col = {160, 152, 140}; break;
        case Surface::ForestPath: col = {128, 112, 76}; break;
        case Surface::Count: col = {255, 0, 255}; break;
    }
    if (c.tree() == Tree::Alive) col = {34, 90, 40};
    else if (c.tree() == Tree::Burned) col = {58, 46, 38};
    if (c.bush() == Bush::Alive) col = mixc(col, {60, 140, 45}, 70);
    else if (c.bush() == Bush::Burned) col = mixc(col, {80, 70, 50}, 60);
    if (c.wall() == Wall::Stone) col = {96, 88, 80};
    else if (c.wall() == Wall::Ice) col = {196, 238, 252};
    else if (c.wall() == Wall::Metal) col = {150, 156, 168};  // [U-288]
    if (c.vision_fog() == SenseBlock::Full && c.tree() != Tree::Alive && c.wall() == Wall::None)
        col = mixc(col, {215, 215, 222}, 70);  // steam / smoke cloud
    const int e = c.effective_elevation();
    if (e > 0) col = mixc(col, {255, 255, 255}, std::min(60, 18 * e));
    if (e < 0) col = mixc(col, {0, 0, 0}, std::min(60, -18 * e));
    return col;
}

const char* kSlotKeys[] = {"P", "Q", "W", "E", "R", "T", "Y", "G", "H"};  // [U-289] specials F/G show as G/H

std::string mmss(std::int64_t ms) {
    ms = std::max<std::int64_t>(0, ms);
    return std::format("{:02}:{:02}", ms / 60'000, ms / 1'000 % 60);
}

}  // namespace

GameView::GameView(SDL_Renderer* r, Settings& s, Resources& res, Audio& audio) : r_(r), set_(s), res_(res), audio_(audio) {
    sprites_.init(&res_);  // [U-258]
    // [U-257] the ground: kGroundPx pixels per cell, painted from the pack's surface textures
    map_tex_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, kGridSize * kGroundPx, kGridSize * kGroundPx);
    fog_tex_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, kGridSize, kGridSize);

    SDL_SetTextureScaleMode(map_tex_, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureScaleMode(fog_tex_, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(fog_tex_, SDL_BLENDMODE_BLEND);
    map_px_.resize(std::size_t(kGridSize) * kGridSize * kGroundPx * kGroundPx);
    map_raw_.assign(std::size_t(kGridSize) * kGridSize, 0xffffffffu);
    fog_px_.resize(std::size_t(kGridSize) * kGridSize);
}

GameView::~GameView() {
    SDL_DestroyTexture(map_tex_);
    SDL_DestroyTexture(fog_tex_);
    for (SDL_Surface* s : ground_src_)
        if (s) SDL_DestroySurface(s);
}

// [U-257] one ground texture per surface (resourcepacks/<pack>/textures/<name>.png), read once into memory
const SDL_Surface* GameView::ground_texture(int which) {
    static constexpr const char* kNames[kGroundTextures] = {
        "grass", "dirt", "glacier", "shallow_water", "sand", "structure_stone", "wood", "deep_water", "lava", "", "", "stone_road",
        "forest_path", "burned", "ash", "bush", "tree_canopy"};
    if (which < 0 || which >= kGroundTextures) return nullptr;
    if (!ground_tried_[std::size_t(which)]) {
        ground_tried_[std::size_t(which)] = true;
        const std::string f = *kNames[which] ? res_.file(std::string("textures/") + kNames[which] + ".png") : std::string();
        if (!f.empty())
            if (SDL_Surface* s = SDL_LoadPNG(f.c_str())) {
                ground_src_[std::size_t(which)] = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
                SDL_DestroySurface(s);
            }
    }
    return ground_src_[std::size_t(which)];
}

void GameView::paint_cell(int x, int y, Cell c) {
    const Rgba base = cell_colour(c);
    int which = int(c.surface());  // the surface's texture (index = Surface)
    if (c.tree() == Tree::Burned || c.tree() == Tree::Destroyed) which = 13;
    else if (c.bush() == Bush::Burned) which = 14;
    else if (c.bush() == Bush::Alive) which = 15;
    else if (c.tree() == Tree::Alive) which = 16;
    const SDL_Surface* t = c.wall() == Wall::None ? ground_texture(which) : nullptr;
    // the texture keeps 70 % of its own look; 30 % comes from the cell colour (elevation, special states)
    const int keep = t ? 70 : 0;
    for (int j = 0; j < kGroundPx; ++j)
        for (int i = 0; i < kGroundPx; ++i) {
            Rgba px = base;
            if (t) {  // 256 px of texture span 32 cells
                const int tx = ((x * kGroundPx + i) * 2) % t->w, ty = ((y * kGroundPx + j) * 2) % t->h;
                const std::uint8_t* p = static_cast<const std::uint8_t*>(t->pixels) + std::size_t(ty) * std::size_t(t->pitch) + std::size_t(tx) * 4;
                px = mixc(base, Rgba{p[0], p[1], p[2], 255}, keep);
            }
            map_px_[std::size_t(y * kGroundPx + j) * std::size_t(kGridSize * kGroundPx) + std::size_t(x * kGroundPx + i)] = pack(px);
        }
}

void GameView::reset() {
    leave = false;
    pending_ = -1;
    show_shop_ = show_score_ = chat_open_ = false;
    last_fog_ = -1;
    cam_lock_ = false;  // LoL-like: free camera, starts on your champion; Y = lock
    cam_started_ = false;
    auto_sent_ = false;
    selected_ = 0;
    // a new match: sounds, banners and animation memory start over
    snd_level_ = 0, snd_items_ = -1, snd_notice_ = -1;
    snd_recall_ = snd_welcome_ = snd_end_ = false;
    source_ice_ = false;
    announce_next_ = 0;
    announce_t_ = -100;
    sky_prev_.clear();
    sky_seen_once_ = false;
    sky_banners_.clear();
    anim_mem_.clear();
    sent_companion_ = -1;
    // [U-389] nothing of the last match may leak into the next one
    companion_last_x_.clear(), companion_last_y_.clear(), companion_dir_.clear();
    cc_max_.clear(), cc_last_.clear(), fx_born_.clear(), pings_seen_.clear();
    drawing_slot_ = -1, draw_path_.clear(), src_x_ = src_y_ = -1;
    show_recap_ = show_events_ = false;
}

Rgba GameView::team_colour(int team) const {
    if (team == 2) return hex(0x9a5ad0);
    if (set_.colourblind) return team == 0 ? hex(0xe08a1a) : hex(0x2a7ad8);  // [U-99]
    return team == 0 ? hex(0xd8402a) : hex(0x4f9a35);
}

const SkillDef* GameView::skill_of(const NetClient& net, int slot) const {
    const auto it = net.roster.find(net.my_unit);
    if (it == net.roster.end()) return nullptr;
    const RosterEntry& e = it->second;
    const ChampionDef& d = champions()[std::size_t(std::clamp<int>(e.champ, 0, int(champions().size()) - 1))];
    // [U-296] awakened at the planetary alignment: Q W E R are the new bender's set
    if (slot >= 1 && slot <= 4)
        for (const LocalAnnounce& la : net.announces)
            if (la.a.kind == std::uint8_t(AnnounceKind::Awakened) && la.a.a == net.my_unit)
                if (const SkillDef* s = awakened_skill(d, Element(std::min<std::uint32_t>(la.a.b, 4)), slot)) return s;
    switch (Slot(slot)) {
        case Slot::P: return &d.p;
        case Slot::Q: return e.q >= 0 && e.q < int(q_options().size()) ? &q_options()[std::size_t(e.q)] : nullptr;
        case Slot::W: return &d.w;
        case Slot::E: return &d.e;
        case Slot::R: return &d.r;
        case Slot::T: return &d.t;
        case Slot::Y: return &d.y;  // [U-289]
        case Slot::F: return e.f >= 0 ? &specials()[std::size_t(e.f)] : nullptr;
        case Slot::G: return e.g >= 0 ? &specials()[std::size_t(e.g)] : nullptr;
        default: return nullptr;
    }
}

// [U-303] the unit whose body is NEAREST to the cursor (inside it = distance below 0), within an 18-unit grace.
// enemies_only really skips my team now (it compared with a team number that never exists before).
const GameView::DrawUnit* GameView::unit_at(float wx, float wy, bool enemies_only, bool champions_only) const {
    const DrawUnit* best = nullptr;
    float best_d = 18.f;
    for (const DrawUnit& d : draw_) {
        if (!d.u.alive || (enemies_only && d.u.team == my_team_)) continue;
        if (champions_only && d.u.kind != std::uint8_t(UnitKind::Champion)) continue;  // [U-162]
        const Shape s = place_body(body_by_index(d.u.body), std::int32_t(d.x), std::int32_t(d.y), {d.u.fx, d.u.fy}, d.u.walk, d.u.size_pct);
        for (int i = 0; i < s.n; ++i) {
            const float dist = std::hypot(wx - float(s.c[i].x), wy - float(s.c[i].y)) - float(s.c[i].r);
            if (dist < best_d) best_d = dist, best = &d;
        }
    }
    return best;
}

void GameView::rebuild_textures(NetClient& net, std::int64_t now_ms, bool force) {
    // vision of my team, computed locally from allied units (same code as the server) [X-10]
    const SkyState sky = WorldClock::at_ms(net.start_ms + now_ms);
    const int ambient = vision_range_pct(sky);
    const bool comet = unlimited_fuel(sky);  // the comet window
    auto night_tint = [&](std::uint8_t a) -> Rgba {
        if (comet) return {50, 6, 4, a};                                   // reddish
        const int m = sky.moon_light_pct;                                  // 0 new … 100 full
        return {std::uint8_t(4 + m / 4), std::uint8_t(8 + m / 4), std::uint8_t(30 + m / 5), a};  // whiter with the moon
    };
    light_.clear();
    for (const auto& l : net.cur.lights) light_.add(LightSource{l.x, l.y, l.radius, l.strength}, net.grid);
    std::vector<Viewer> vs;
    for (const auto& u : net.cur.units)
        if (u.alive && u.team == net.my_team) {
            Viewer v{u.x, u.y, std::max<std::int32_t>(64, u.sight)};
            const UnitKind k = UnitKind(u.kind);
            if (k != UnitKind::Turret && k != UnitKind::Inhibitor && k != UnitKind::Nexus) {  // [U-150] eyes
                const std::int32_t eo = eye_offset(u.body == 4, u.size_pct);  // [U-152] see from the head
                v.x += u.fx * eo / kOne;
                v.y += u.fy * eo / kOne;
                v.fx = u.fx;
                v.fy = u.fy;
                v.fov_cos = kEyeFovCos;
                v.near = kBodySense;
            }
            vs.push_back(v);
        }
    std::vector<Cloud> cl;
    for (const auto& c : net.cur.clouds) cl.push_back({c.x, c.y, c.radius, 0, CloudKind(c.kind)});
    cloud_mask(cl, cloud_cells_);
    vis_.compute(net.grid, vs, light_, ambient, &cloud_cells_);  // [U-241] clouds block like walls
    // terrain: what I see now, what I saw before elsewhere [X-92]
    bool changed = force || net.grid_dirty;
    for (int y = 0; y < kGridSize; ++y)
        for (int x = 0; x < kGridSize; ++x) {
            const bool v = vis_.cell(x, y);
            if (v && net.seen.at(x, y) != net.grid.at(x, y)) {
                net.seen.at(x, y) = net.grid.at(x, y);
                changed = true;
            }
            const std::size_t i = std::size_t(y) * kGridSize + std::size_t(x);
            const int lit = std::max(ambient, light_.at(x, y));
            // [U-237] the comet makes the night reddish; the moon gives a whiter light
            if (!v) fog_px_[i] = pack({8, 10, 22, 165});
            else if (lit >= 100) fog_px_[i] = comet ? pack({90, 10, 0, 40}) : 0;
            else if (light_.at(x, y) > ambient) fog_px_[i] = pack({60, 30, 0, std::uint8_t((100 - lit) * 120 / 100)});
            else fog_px_[i] = pack(night_tint(std::uint8_t((100 - lit) * 135 / 100)));
        }
    net.grid_dirty = false;
    if (changed) {  // [U-257] repaint only the cells that changed; upload only that rectangle
        int x0 = kGridSize, y0 = kGridSize, x1 = -1, y1 = -1;
        for (int y = 0; y < kGridSize; ++y)
            for (int x = 0; x < kGridSize; ++x) {
                const Cell c = net.seen.at(x, y);
                std::uint32_t& old = map_raw_[std::size_t(y) * kGridSize + std::size_t(x)];
                if (old == c.raw()) continue;
                old = c.raw();
                paint_cell(x, y, c);
                x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
            }
        if (x1 >= 0) {
            const SDL_Rect rr{x0 * kGroundPx, y0 * kGroundPx, (x1 - x0 + 1) * kGroundPx, (y1 - y0 + 1) * kGroundPx};
            const int pitch = kGridSize * kGroundPx * 4;
            SDL_UpdateTexture(map_tex_, &rr, map_px_.data() + std::size_t(rr.y) * std::size_t(kGridSize * kGroundPx) + std::size_t(rr.x), pitch);
        }
    }
    // [U-204] smooth fog edges: a 3 × 3 blur of the alpha (off in the developer view)
    if (set_.smooth_fog && !set_.dev_mode) {
        std::vector<std::uint32_t> tmp = fog_px_;
        for (int y = 1; y < kGridSize - 1; ++y)
            for (int x = 1; x < kGridSize - 1; ++x) {
                int a = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) a += int(tmp[std::size_t(y + dy) * kGridSize + std::size_t(x + dx)] >> 24);
                std::uint32_t& p = fog_px_[std::size_t(y) * kGridSize + std::size_t(x)];
                p = (p & 0x00ffffffu) | (std::uint32_t(a / 9) << 24);
            }
    }
    // [U-290] the fog shown now becomes the start of the fade; the new one is its end (mixed per frame in draw_world)
    if (fog_from_px_.size() != fog_px_.size()) fog_from_px_ = fog_px_;
    else fog_from_px_.swap(fog_shown_px_);
    fog_shown_px_ = fog_from_px_;
}

// ------------------------------------------------------------------ input
void GameView::cast_at_cursor(NetClient& net, int slot) {
    const float wx = view_.wx(ui_ ? ui_->mx : 0), wy = view_.wy(ui_ ? ui_->my : 0);
    Cmd c{.kind = CmdKind::Cast, .slot = std::uint8_t(slot), .x = std::int32_t(wx), .y = std::int32_t(wy), .sx = src_x_, .sy = src_y_};
    src_x_ = src_y_ = -1;
    const SkillDef* d = skill_of(net, slot);
    if (d && d->targeted) {
        const DrawUnit* t = unit_at(wx, wy, true, champs_only());  // X held: champions only [U-162]
        if (!t || t->u.team == net.my_team) return hint("click on a visible enemy");
        c.target = t->u.id;
    }
    net.send_cmd(c);
}

// [U-340] is there water to bend at this point? (river / sea / glacier, an ice wall, a puddle / ice patch)
bool GameView::source_at(const NetClient& net, float wx, float wy, const SkillDef* d) const {
    const int cx = int(wx) / kCellUnits, cy = int(wy) / kCellUnits;
    if (d && d->kind == SkillKind::StonePush) {  // [U-406] a lifted stone (a boulder) near the cursor
        for (const auto& f : net.cur.fx)
            if (f.kind == std::uint8_t(FxKind::Boulder) && (float(f.x) - wx) * (float(f.x) - wx) + (float(f.y) - wy) * (float(f.y) - wy) <= 100.f * 100.f)
                return true;
        return false;
    }
    if (d && d->sub == SubElement::Earth)  // [U-378] earth: a rock (any stone wall)
        return wx >= 0 && wy >= 0 && Grid::in_bounds(cx, cy) && net.seen.at(cx, cy).wall() == Wall::Stone;
    if (wx >= 0 && wy >= 0 && Grid::in_bounds(cx, cy)) {
        const Cell c = net.seen.at(cx, cy);
        const Surface s = c.surface();
        if (c.wall() == Wall::Ice || s == Surface::Water || s == Surface::DeepWater || s == Surface::Glacier) return true;
    }
    for (const auto& c : net.cur.clouds)
        if ((c.kind == std::uint8_t(CloudKind::Puddle) || c.kind == std::uint8_t(CloudKind::IcePatch)) &&
            (c.x - wx) * (c.x - wx) + (c.y - wy) * (c.y - wy) <= float(c.radius) * float(c.radius))
            return true;
    return false;
}

// [U-402] steering (control 1): while the key of a steerable skill is held, the cursor goes to the server ~20× a
// second. Drawing (control 2): the mouse path is recorded while the key is held and sent on release, then cast.
void GameView::update_controls(NetClient& net, double now_s) {
    if (!ui_) return;
    const bool* ks = SDL_GetKeyboardState(nullptr);
    const float wx = view_.wx(ui_->mx), wy = view_.wy(ui_->my);
    auto held = [&](int slot) { return slot >= 1 && slot <= 8 && ks[set_.keys[std::size_t(Action(slot - 1))]] && !chat_open_; };
    if (drawing_slot_ >= 0) {
        if (held(drawing_slot_)) {
            const auto [lx, ly] = draw_path_.back();
            if ((wx - lx) * (wx - lx) + (wy - ly) * (wy - ly) >= 24.f * 24.f && draw_path_.size() < 40) draw_path_.push_back({wx, wy});
        } else {  // released: send the path, then cast at its end
            for (std::size_t i = 0; i < draw_path_.size(); ++i)
                net.send_cmd({.kind = CmdKind::DrawPoint, .slot = std::uint8_t(drawing_slot_), .item = std::int16_t(i == 0 ? 1 : 0),
                              .x = std::int32_t(draw_path_[i].first), .y = std::int32_t(draw_path_[i].second)});
            net.send_cmd({.kind = CmdKind::Cast, .slot = std::uint8_t(drawing_slot_), .x = std::int32_t(draw_path_.back().first),
                          .y = std::int32_t(draw_path_.back().second)});
            drawing_slot_ = -1;
            draw_path_.clear();
        }
    }
    if (now_s - steer_sent_ < 0.05) return;
    for (int sl = 1; sl <= 8; ++sl) {
        const SkillDef* d = skill_of(net, sl);
        if (d && d->control == 1 && held(sl)) {
            net.send_cmd({.kind = CmdKind::Steer, .slot = std::uint8_t(sl), .x = std::int32_t(wx), .y = std::int32_t(wy)});
            steer_sent_ = now_s;
            break;
        }
    }
}

void GameView::hint(std::string h) {
    hint_ = std::move(h);
    hint_t_ = last_frame_;
}

void GameView::cast_key(NetClient& net, int slot, bool from_click) {
    const SkillDef* d = skill_of(net, slot);
    if (!d) return;
    src_x_ = src_y_ = -1;
    if (d->kind == SkillKind::Passive) return hint(std::format("{} is a passive (always on)", d->name));
    const auto& me = net.cur.me;
    if (net.have_snapshot && me.rank[slot] == 0)
        return hint(std::format("{} not learned: Ctrl+{} or the + button ({} skill points)", d->name, kSlotKeys[slot], me.skill_points));
    if (net.have_snapshot && me.respawn_ms > 0) return hint("you are dead");
    if (net.have_snapshot && me.stolen_ms > 0 && me.stolen_slot == slot) return hint("the Face Stealer took this skill");
    const bool recast = d->casts > 1 && me.recast[slot] > 0;
    if (net.have_snapshot && me.cd_left_ms[slot] > 0 && !recast && !me.no_cd)
        return hint(std::format("{} on cooldown ({:.1f} s)", d->name, me.cd_left_ms[slot] / 1000.0));
    if (net.have_snapshot && me.s_cost[slot] > 0 && me.res < me.s_cost[slot] * 100 && !(me.res_max > 0 && me.res == me.res_max))
        return hint(std::format("not enough resource for {}", d->name));
    if (d->full_moon_only && !bloodbending_allowed(WorldClock::at_ms(net.start_ms + net.cur.now_ms)))
        return hint("bloodbending only works at full moon");
    const bool self = d->kind == SkillKind::Shield || d->kind == SkillKind::Haste || d->kind == SkillKind::Flight ||
                      d->kind == SkillKind::SelfFreeze || d->kind == SkillKind::WindSense ||
                      (d->kind == SkillKind::Burst && d->range == 0) || (d->kind == SkillKind::Heal && d->range == 0);
    const bool quick = set_.quick(slot) && !from_click;
    if (d->control == 2 && !from_click && ui_) {  // [U-402] hold the key and move the mouse: the wall follows the path
        drawing_slot_ = slot;
        draw_path_ = {{view_.wx(ui_->mx), view_.wy(ui_->my)}};
        pending_ = -1;
        return hint(std::format("{}: hold the key and draw with the mouse, release to raise", d->name));
    }
    if (source_first(*d) && quick && ui_ && source_at(net, view_.wx(ui_->mx), view_.wy(ui_->my), d)) {
        src_x_ = std::int32_t(view_.wx(ui_->mx)), src_y_ = std::int32_t(view_.wy(ui_->my));  // [U-340] quick cast on water: source picked
        pending_ = slot;
        hint(std::format("{}: source picked, now click where it goes", d->name));
    } else if (self || quick) {  // [U-163] quick cast per ability / all
        cast_at_cursor(net, slot);
        pending_ = -1;
    } else {
        pending_ = slot;  // normal cast: show the exact shape, click to cast
    }
}

void GameView::handle_event(const SDL_Event& e, NetClient& net) {
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        view_.zoom = std::clamp(view_.zoom * (e.wheel.y > 0 ? 1.12f : 0.89f), 0.12f, 1.6f);
        return;
    }
    if (chat_open_) {
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) {
                // [U-182] team chat by default; "/all text" = everyone; "/w name text" = friend whisper
                if (chat_text_.starts_with("/all ")) net.send_chat(net::Chat::All, chat_text_.substr(5));
                else if (chat_text_.starts_with("/w ")) {
                    const std::string rest = chat_text_.substr(3);
                    const auto sp = rest.find(' ');
                    if (sp != std::string::npos) net.send_chat(net::Chat::Whisper, rest.substr(sp + 1), rest.substr(0, sp));
                } else if (!chat_text_.empty()) net.send_chat(net::Chat::Team, chat_text_);
                chat_text_.clear();
                chat_open_ = false;
                SDL_StopTextInput(SDL_GetKeyboardFocus());
            } else if (e.key.key == SDLK_ESCAPE) {
                chat_open_ = false;
                SDL_StopTextInput(SDL_GetKeyboardFocus());
            } else if (e.key.key == SDLK_BACKSPACE && !chat_text_.empty()) {
                chat_text_.pop_back();
            }
        } else if (e.type == SDL_EVENT_TEXT_INPUT) {
            chat_text_ += e.text.text;
        }
        return;
    }
    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
        const bool ctrl = (e.key.mod & SDL_KMOD_CTRL) != 0;
        const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
        const bool alt = (e.key.mod & SDL_KMOD_ALT) != 0;
        const SDL_Scancode sc = e.key.scancode;
        // practice cheats [U-100]: Ctrl+Shift+N no cooldowns, +G gold, +L level
        if (ctrl && shift) {
            if (sc == SDL_SCANCODE_N) net.send_cmd({.kind = CmdKind::CheatNoCooldowns});
            if (sc == SDL_SCANCODE_G) net.send_cmd({.kind = CmdKind::CheatGold});
            if (sc == SDL_SCANCODE_L) net.send_cmd({.kind = CmdKind::CheatLevel});
            return;
        }
        // [U-246] Ctrl+1…6 = emotes (laugh, taunt, dance, joke, wave, cheer)
        if (ctrl && sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_6) {
            net.send_cmd({.kind = CmdKind::Emote, .slot = std::uint8_t(sc - SDL_SCANCODE_1)});
            return;
        }
        // [U-231] F1…F12 = camera on my teammates in order
        // (a key bound to an action in the options wins over this)
        if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12 && std::ranges::find(set_.keys, sc) == set_.keys.end()) {
            const int want = int(sc - SDL_SCANCODE_F1);
            int n = 0;
            for (const auto& s2 : net.cur.scores) {
                const auto r = net.roster.find(s2.id);
                if (r == net.roster.end() || r->second.team != net.my_team || s2.id == net.my_unit) continue;
                if (n++ != want) continue;
                for (const DrawUnit& d : draw_)
                    if (d.u.id == s2.id) center_on(d.x, d.y);
            }
            return;
        }
        // [U-165] key bindings; a key matches by position (scancode) or, for letters, by what the layout
        // says it is — works with any layout / input method
        const SDL_Scancode by_key = SDL_GetScancodeFromKey(e.key.key, nullptr);
        for (int i = 0; i < kActions; ++i) {
            const SDL_Scancode k = set_.keys[std::size_t(i)];
            if (!(k == sc || (k == by_key && k >= SDL_SCANCODE_A && k <= SDL_SCANCODE_Z))) continue;
            const Action a = Action(i);
            if (a == Action::Stance && !ctrl) continue;       // [U-231] Ctrl+Space = stance
            if (a == Action::CenterSelf && ctrl) continue;   //          Space = camera on me
            if (alt && a <= Action::G) {                      // [U-243] Alt + skill = self-cast
                self_cast(net, int(a) + 1);
                return;
            }
            on_action(a, ctrl && a <= Action::G, net);
            return;
        }
        if (sc == SDL_SCANCODE_ESCAPE) {
            if (pending_ >= 0 || attack_move_) pending_ = -1, attack_move_ = 0;
            else if (show_shop_ || show_score_ || selected_) show_shop_ = show_score_ = false, selected_ = 0;
            else wants_menu = true;  // [U-207] the in-game menu (live options) — not leaving
        }
    }
    if (e.type == SDL_EVENT_KEY_UP && e.key.scancode == set_.keys[std::size_t(Action::Score)]) show_score_ = false;
    // [U-246] ping wheel: hold Ctrl, press the left button, drag towards a ping, release
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT && (SDL_GetModState() & SDL_KMOD_CTRL)) {
        ping_wheel_ = true;
        ping_x_ = e.button.x;
        ping_y_ = e.button.y;
        if (ui_) ui_->clicked = false;
        swallow_click_ = true;
        return;
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT && ping_wheel_) {
        ping_wheel_ = false;
        const float dx = e.button.x - ping_x_, dy = e.button.y - ping_y_;
        PingKind k = PingKind::Generic;
        if (dx * dx + dy * dy > 30 * 30) {  // up danger, right on my way, down assist, left missing, up-left bait
            const float a = std::atan2(dy, dx);
            k = a < -2.36f ? PingKind::Missing : a < -1.96f ? PingKind::Bait : a < -0.78f ? PingKind::Danger
              : a < 0.78f ? PingKind::OnMyWay : a < 2.36f ? PingKind::AssistMe : PingKind::Missing;
        }
        net.send_cmd({.kind = CmdKind::Ping, .slot = std::uint8_t(k), .x = std::int32_t(view_.wx(ping_x_)), .y = std::int32_t(view_.wy(ping_y_))});
        return;
    }
    // [U-414] emote wheel: hold Ctrl, press the RIGHT button, drag towards an emote, release
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT && (SDL_GetModState() & SDL_KMOD_CTRL)) {
        emote_wheel_ = true;
        ping_x_ = e.button.x;
        ping_y_ = e.button.y;
        if (ui_) ui_->rclicked = false;
        return;
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_RIGHT && emote_wheel_) {
        emote_wheel_ = false;
        if (ui_) ui_->rclicked = false;
        const float dx = e.button.x - ping_x_, dy = e.button.y - ping_y_;
        if (dx * dx + dy * dy > 30 * 30) {  // 6 sectors of 60°, the first one straight up, clockwise
            const float a = std::atan2(dx, -dy);  // 0 = up
            const int k = (int(std::lround(a / 1.0472f)) % 6 + 6) % 6;
            net.send_cmd({.kind = CmdKind::Emote, .slot = std::uint8_t(k)});
        }
        return;
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {  // [U-164] left + right together = camera on me
        const SDL_MouseButtonFlags b = SDL_GetMouseState(nullptr, nullptr);
        if (set_.both_buttons_center && (b & SDL_BUTTON_LMASK) && (b & SDL_BUTTON_RMASK)) both_down_ = true;
    }
}

// [U-243] Alt + skill key: cast on myself (my position / me as the target)
void GameView::self_cast(NetClient& net, int slot) {
    const DrawUnit* me = nullptr;
    for (const DrawUnit& d : draw_)
        if (d.u.id == net.my_unit) me = &d;
    if (!me) return;
    net.send_cmd({.kind = CmdKind::Cast, .slot = std::uint8_t(slot), .target = net.my_unit, .x = std::int32_t(me->x), .y = std::int32_t(me->y)});
}

bool GameView::champs_only() const {  // [U-162] X held (or switched on)
    if (set_.champs_only_toggle) return champs_toggle_on_;
    const bool* ks = SDL_GetKeyboardState(nullptr);
    return ks[set_.keys[std::size_t(Action::ChampsOnly)]];
}

void GameView::on_action(Action a, bool ctrl, NetClient& net) {
    const float wx = view_.wx(ui_ ? ui_->mx : 0), wy = view_.wy(ui_ ? ui_->my : 0);
    if (a <= Action::G) {
        const int sl = int(a) + 1;  // Q = slot 1
        if (ctrl) {
            // [U-301] only this skill: blocked → a notice, never another skill instead
            const SkillDef* sd = skill_of(net, sl);
            const DrawUnit* mine = me_unit();
            if (net.have_snapshot && net.cur.me.skill_points <= 0) hint(tr("no skill points (one per level)"));
            else if (sd && mine && net.cur.me.rank[sl] >= skill_max_rank(*sd, mine->u.level))
                hint(std::format("{}: {}", sd->name, tr("cannot level up now — pick another skill")));
            else net.send_cmd({.kind = CmdKind::LevelUp, .slot = std::uint8_t(sl)});
        } else {
            cast_key(net, sl);
        }
        return;
    }
    if (a >= Action::Item1 && a <= Action::Item9) {  // keys 1…9 = slots 0…8 (the boots slot has no key) [U-322]
        net.send_cmd({.kind = CmdKind::UseItem, .slot = std::uint8_t(int(a) - int(Action::Item1)), .x = std::int32_t(wx), .y = std::int32_t(wy)});
        return;
    }
    switch (a) {
        case Action::Stop: net.send_cmd({.kind = CmdKind::Stop}); pending_ = -1; break;  // [U-265] the cast goes on
        case Action::CancelCast: net.send_cmd({.kind = CmdKind::CancelCast}); pending_ = -1; break;  // [U-265] Z
        case Action::Stance: net.send_cmd({.kind = CmdKind::Stance}); break;            // [U-72]
        case Action::Recall: net.send_cmd({.kind = CmdKind::Recall}); hint("recalling (8 s, moving cancels)"); break;
        case Action::Portal: net.send_cmd({.kind = CmdKind::Portal}); break;            // [X-54]
        case Action::Shop: show_shop_ = !show_shop_; break;
        case Action::Score: show_score_ = true; break;
        case Action::CamLock:
            cam_lock_ = !cam_lock_;
            hint(cam_lock_ ? "camera locked on you" : "camera free: mouse at the edge / arrows");
            break;
        case Action::CenterSelf: cam_started_ = false; break;  // [U-231] Space
        case Action::ExpandStats: expand_stats_ = !expand_stats_; break;  // [U-238]
        case Action::SourceForm:  // [U-293] waterbenders: the form of water taken from sources
            source_ice_ = !source_ice_;
            net.send_cmd({.kind = CmdKind::SourceForm, .slot = std::uint8_t(source_ice_)});
            hint(source_ice_ ? tr("water sources → ICE") : tr("water sources → WATER"));
            break;
        case Action::AttackRange:  // [U-289] Ctrl + ` = always on / off (holding alone is read every frame)
            if (SDL_GetModState() & SDL_KMOD_CTRL) {
                set_.show_attack_range = !set_.show_attack_range;
                set_.save();
                hint(set_.show_attack_range ? tr("attack range: always shown") : tr("attack range: hold ` to see it"));
            }
            break;
        case Action::AttackMoveChamp:  // [U-148] fires at the cursor at once, champions only
            net.send_cmd({.kind = CmdKind::AttackMove, .slot = std::uint8_t(1 | (set_.attack_move_cursor ? 2 : 0)), .x = std::int32_t(wx), .y = std::int32_t(wy)});
            click_x_ = wx, click_y_ = wy, click_t_ = last_frame_;
            pending_ = -1;
            break;
        case Action::AttackMoveAny:
            net.send_cmd({.kind = CmdKind::AttackMove, .slot = std::uint8_t(set_.attack_move_cursor ? 2 : 0), .x = std::int32_t(wx), .y = std::int32_t(wy)});
            click_x_ = wx, click_y_ = wy, click_t_ = last_frame_;
            pending_ = -1;
            break;
        case Action::ChampsOnly:
            if (set_.champs_only_toggle) {
                champs_toggle_on_ = !champs_toggle_on_;
                hint(champs_toggle_on_ ? "targeting champions only: ON" : "targeting champions only: off");
            }
            break;
        case Action::ToggleAutoAttack:  // [U-158]
            set_.auto_attack = !set_.auto_attack;
            net.send_cmd({.kind = CmdKind::SetAutoAttack, .slot = std::uint8_t(set_.auto_attack)});
            hint(set_.auto_attack ? "autoattack: ON" : "autoattack: OFF");
            break;
        case Action::Chat:
            chat_open_ = true;
            SDL_StartTextInput(SDL_GetKeyboardFocus());
            break;
        default: break;
    }
}

void GameView::center_on(float x, float y) {
    cam_lock_ = false;
    cam_started_ = true;
    view_.cx = x;
    view_.cy = y;
}


// ------------------------------------------------------------------ frame
void GameView::frame(NetClient& net, double now_s, Ui& ui) {
    ui_ = &ui;
    frame_ms_ = (now_s - last_frame_) * 1000;
    last_frame_ = now_s;
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(r_, &w, &h);
    view_.sw = float(w);
    view_.sh = float(h);
    if (!net.have_snapshot) {
        fill_rect(r_, 0, 0, float(w), float(h), {10, 10, 16, 255});
        text(r_, float(w) / 2 - 160, float(h) / 2, "Waiting for the server...", {230, 230, 230}, 2);
        return;
    }
    update_controls(net, now_s);  // [U-402]
    if (sent_companion_ != set_.companion) {  // [U-407] my travel companion (again whenever the option changes)
        net.send_cmd({.kind = CmdKind::SetCompanion, .slot = std::uint8_t(set_.companion)});
        sent_companion_ = set_.companion;
    }
    const Snapshot& cur = net.cur;
    // [X-02][U-338] interpolation: positions are drawn 50 ms in the past, between the two snapshots around that moment.
    // A late packet then never shows as a freeze-and-jump (the old way blended only the last two and waited).
    draw_.clear();
    my_team_ = net.my_team;
    const double render_t = now_s - 0.1;  // [U-389] 2 snapshot intervals: one late packet never freezes the picture
    const NetClient::PosFrame* fa = nullptr;
    const NetClient::PosFrame* fb = nullptr;
    for (std::size_t i = 0; i + 1 < net.pos_hist.size(); ++i)
        if (net.pos_hist[i].arrived <= render_t && render_t <= net.pos_hist[i + 1].arrived) fa = &net.pos_hist[i], fb = &net.pos_hist[i + 1];
    if (!fa && !net.pos_hist.empty()) fa = fb = render_t < net.pos_hist.front().arrived ? &net.pos_hist.front() : &net.pos_hist.back();
    const float alpha = fa && fb && fb->arrived > fa->arrived ? float(std::clamp((render_t - fa->arrived) / (fb->arrived - fa->arrived), 0.0, 1.0)) : 1.f;
    for (const auto& u : cur.units) {
        float x = float(u.x), y = float(u.y);
        if (fa && fb) {
            const auto ia = fa->pos.find(u.id), ib = fb->pos.find(u.id);
            if (ia != fa->pos.end() && ib != fb->pos.end() && std::abs(ia->second.first - ib->second.first) < 400 &&
                std::abs(ia->second.second - ib->second.second) < 400) {
                x = float(ia->second.first) + float(ib->second.first - ia->second.first) * alpha;
                y = float(ia->second.second) + float(ib->second.second - ia->second.second) * alpha;
            } else if (ib != fb->pos.end()) {
                x = float(ib->second.first), y = float(ib->second.second);
            }
        }
        draw_.push_back({u, x, y});
    }
    // [U-389] effects + companions on the same delayed timeline as the units (they ran ahead of their casters)
    auto lerp_map = [&](const auto& ma, const auto& mb, std::uint32_t id, std::int32_t& x, std::int32_t& y) {
        const auto ia = ma.find(id), ib = mb.find(id);
        if (ib == mb.end()) return;
        if (ia == ma.end()) {
            x = ib->second.first, y = ib->second.second;
            return;
        }
        x = ia->second.first + std::int32_t(float(ib->second.first - ia->second.first) * alpha);
        y = ia->second.second + std::int32_t(float(ib->second.second - ia->second.second) * alpha);
    };
    fx_draw_ = cur.fx;
    comp_draw_ = cur.companions;
    if (fa && fb) {
        for (auto& f : fx_draw_) lerp_map(fa->fx, fb->fx, f.id, f.x, f.y);
        for (auto& c : comp_draw_) lerp_map(fa->comp, fb->comp, c.owner, c.x, c.y);
    }
    if (!auto_sent_) {  // [U-158] my autoattack setting → server, once per match
        net.send_cmd({.kind = CmdKind::SetAutoAttack, .slot = std::uint8_t(set_.auto_attack)});
        auto_sent_ = true;
    }
    if (net.notice_seq != notice_seen_) {  // the server says why an order did nothing [X-103]
        notice_seen_ = net.notice_seq;
        hint(net.notice);
    }
    const std::int64_t now_ms = cur.now_ms + std::int64_t((now_s - cur.arrived) * 1000);
    local_tick_ = std::uint32_t(cur.tick + std::uint64_t(std::max(0.0, now_s - cur.arrived) * net.tick_hz));
    // camera [U-174] my base is always bottom-left; [U-175] tilted (game) or top-down (debug) view
    view_.flip = net.my_team == 1;
    view_.gy = set_.tilted() ? 0.78f : 1.f;
    view_.hz = set_.tilted() ? 0.62f : 0.f;
    const DrawUnit* mine = nullptr;
    for (const DrawUnit& d : draw_)
        if (d.u.id == net.my_unit) mine = &d;
    if (mine && !cam_started_) {
        view_.cx = mine->x;
        view_.cy = mine->y;
        cam_started_ = true;
    }
    // [U-251] holding Space (or both mouse buttons) keeps the camera on me
    const bool* held = SDL_GetKeyboardState(nullptr);
    const bool space_held = held[set_.keys[std::size_t(Action::CenterSelf)]] && !(SDL_GetModState() & SDL_KMOD_CTRL) && !chat_open_;
    const SDL_MouseButtonFlags mb = SDL_GetMouseState(nullptr, nullptr);
    const bool both_held = set_.both_buttons_center && (mb & SDL_BUTTON_LMASK) && (mb & SDL_BUTTON_RMASK);
    if ((cam_lock_ || space_held || both_held) && mine) {
        view_.cx = mine->x;
        view_.cy = mine->y;
    } else {
        const float pan = 1400.f * float(std::min(frame_ms_, 50.0)) / 1000.f / std::max(0.2f, view_.zoom * 1.6f);
        const bool* ks = SDL_GetKeyboardState(nullptr);
        const float sg = view_.flip ? -1.f : 1.f;  // pan in SCREEN directions
        if (ui.mx < 6 || ks[SDL_SCANCODE_LEFT]) view_.cx -= pan * sg;
        if (ui.mx > float(w) - 6 || ks[SDL_SCANCODE_RIGHT]) view_.cx += pan * sg;
        if (ui.my < 6 || ks[SDL_SCANCODE_UP]) view_.cy -= pan * sg / view_.gy;
        if (ui.my > float(h) - 6 || ks[SDL_SCANCODE_DOWN]) view_.cy += pan * sg / view_.gy;
        view_.cx = std::clamp(view_.cx, 0.f, float(kMapUnits));
        view_.cy = std::clamp(view_.cy, 0.f, float(kMapUnits));
    }
    // [U-389] terrain changes are shown quickly, but at most 30× a second (fire / wall fights rebuilt every frame → hitches)
    if (now_s - last_fog_ > 0.1 || (net.grid_dirty && now_s - last_fog_ > 0.033)) {
        rebuild_textures(net, now_ms, last_fog_ < 0);
        fog_fade_from_ = now_s;
        fog_fade_t_ = -1;
        last_fog_ = now_s;
    }
    draw_world(net, now_s);
    play_sounds(net);
    draw_indicator(net);
    draw_hud(net, now_s, ui);

    // mouse orders (after the HUD had its chance to eat the click)
    if (mouse_on_hud_) ui.clicked = ui.rclicked = false;
    const float wx = view_.wx(ui.mx), wy = view_.wy(ui.my);
    if (both_down_) {  // [U-164]
        both_down_ = false;
        cam_started_ = false;
        ui.clicked = ui.rclicked = false;
    }
    if (ui.clicked) {
        const SkillDef* pd = pending_ >= 0 ? skill_of(net, pending_) : nullptr;
        if (pd && source_first(*pd) && src_x_ < 0 && source_at(net, wx, wy, pd)) {  // [U-340] first click: the source
            src_x_ = std::int32_t(wx), src_y_ = std::int32_t(wy);
            hint(std::format("{}: source picked, now click where it goes", pd->name));
        } else if (pending_ >= 0) {
            cast_at_cursor(net, pending_);
            pending_ = -1;
        } else if (!attack_move_) {  // [U-161] left click = select → info panel
            const DrawUnit* t = unit_at(wx, wy, false);
            selected_ = t ? t->u.id : 0;
        } else if (attack_move_) {
            net.send_cmd({.kind = CmdKind::AttackMove, .slot = std::uint8_t((attack_move_ == 2 ? 1 : 0) | (set_.attack_move_cursor ? 2 : 0)), .x = std::int32_t(wx), .y = std::int32_t(wy)});
            attack_move_ = 0;
            click_x_ = wx, click_y_ = wy, click_t_ = now_s;
        }
        ui.clicked = false;
    }
    if (ui.rclicked) {  // [U-73] right click = attack / move
        if (pending_ >= 0 || attack_move_) {
            pending_ = -1;
            attack_move_ = 0;
        } else if (const DrawUnit* t = unit_at(wx, wy, false, champs_only()); t && t->u.team != net.my_team && t->u.alive) {
            net.send_cmd({.kind = CmdKind::Attack, .target = t->u.id});
        } else if (const auto mine = net.roster.find(net.my_unit);  // [U-393] a wall under the cursor + autos that hit walls
                   mine != net.roster.end() && mine->second.champ >= 0 && champions()[std::size_t(mine->second.champ)].autos.hits_walls &&
                   wx >= 0 && wy >= 0 && Grid::in_bounds(int(wx) / kCellUnits, int(wy) / kCellUnits) &&
                   net.seen.at(int(wx) / kCellUnits, int(wy) / kCellUnits).wall() != Wall::None) {
            net.send_cmd({.kind = CmdKind::AttackWall, .x = std::int32_t(wx), .y = std::int32_t(wy)});
        } else {
            net.send_cmd({.kind = CmdKind::Move, .x = std::int32_t(wx), .y = std::int32_t(wy)});
            click_x_ = wx, click_y_ = wy, click_t_ = now_s;
        }
        ui.rclicked = false;
    }
}

void GameView::draw_indicator(NetClient& net) {
    if (drawing_slot_ >= 0)  // [U-402] the wall being drawn
        for (std::size_t i = 1; i < draw_path_.size(); ++i)
            line(r_, view_.sx(draw_path_[i - 1].first), view_.sy(draw_path_[i - 1].second), view_.sx(draw_path_[i].first),
                 view_.sy(draw_path_[i].second), {255, 230, 140, 230});
    // [U-289] my autoattack range like LoL: hold ` ; Ctrl + ` keeps it on. Edge-to-edge range → from my body's edge.
    {
        const bool* ks = SDL_GetKeyboardState(nullptr);
        const bool held = ks[set_.keys[std::size_t(Action::AttackRange)]] && !(SDL_GetModState() & SDL_KMOD_CTRL) && !chat_open_;
        if ((held || set_.show_attack_range) && net.have_snapshot) {
            const bool water_user = [&] {
                const auto it = net.roster.find(net.my_unit);
                if (it == net.roster.end()) return false;
                const ChampionDef& c = champions()[std::size_t(std::max<int>(0, it->second.champ))];
                return c.element == Element::Water || c.avatar;
            }();
            for (const DrawUnit& du : draw_) {
                if (!du.u.alive) continue;
                const float reach = float(body_reach(body_by_index(du.u.body), du.u.size_pct));
                const UnitKind k = UnitKind(du.u.kind);
                if (du.u.id == net.my_unit) {  // my autoattack range (edge to edge → from my body's edge)
                    const float r = float(net.cur.me.attack_range) + reach;
                    ground_ring(r_, view_, du.x, du.y, r, {255, 255, 255, 170}, 72);
                    ground_ring(r_, view_, du.x, du.y, r + 2, {80, 80, 80, 90}, 72);
                    // [U-354] XP range: enemies dying inside it give me experience (dashed look: a thin gold ring)
                    ground_ring(r_, view_, du.x, du.y, float(rules::kXpRange), {235, 200, 90, 110}, 96);
                    if (water_user)  // [U-348] how far I can draw water from a puddle / ice / a water source
                        ground_ring(r_, view_, du.x, du.y, float(elements::kWaterSourceCells * kCellUnits), {90, 170, 255, 150}, 64);
                } else if (k == UnitKind::Champion && du.u.team == net.my_team) {  // [U-307] allies
                    ground_ring(r_, view_, du.x, du.y, float(du.u.range) + reach, {120, 190, 255, 90}, 48);
                } else if (k == UnitKind::Turret || k == UnitKind::Nexus) {  // [U-307] turrets (both teams)
                    ground_ring(r_, view_, du.x, du.y, float(du.u.range) + reach,
                                du.u.team == net.my_team ? Rgba{120, 190, 255, 120} : Rgba{255, 90, 80, 160}, 80);
                } else if (k == UnitKind::Monster || k == UnitKind::Boss) {  // [U-307] how far it chases (patience)
                    const int mi = monster_index(net::kMobNames[std::min<int>(du.u.name_id, int(std::size(net::kMobNames)) - 1)]);
                    const float patience = float(mi >= 0 ? kMonsters[std::size_t(mi)].patience : rules::kMonsterLeash);  // [U-308]
                    ground_ring(r_, view_, du.x, du.y, patience, {255, 220, 90, 110}, 72);
                }
            }
        }
    }
    if (pending_ < 0) src_x_ = src_y_ = -1;
    if (pending_ < 0 || !ui_) return;
    const SkillDef* d = skill_of(net, pending_);
    if (d && source_first(*d)) {  // [U-340] highlight the picked source / a source under the cursor
        const float cxw = view_.wx(ui_->mx), cyw = view_.wy(ui_->my);
        if (src_x_ >= 0) ground_ring(r_, view_, float(src_x_), float(src_y_), 48.f, {120, 210, 255, 230}, 32);
        else if (source_at(net, cxw, cyw, d)) ground_ring(r_, view_, cxw, cyw, 40.f, {120, 210, 255, 140}, 32);
    }
    const DrawUnit* mine = nullptr;
    for (const DrawUnit& du : draw_)
        if (du.u.id == net.my_unit) mine = &du;
    if (!d || !mine) return;
    const auto& me = net.cur.me;
    const float range = float(me.s_range[pending_]), width = float(me.s_width[pending_]), radius = float(me.s_radius[pending_]);
    const float mx = view_.wx(ui_->mx), my = view_.wy(ui_->my);
    const std::int32_t ux = std::int32_t(mine->x), uy = std::int32_t(mine->y);
    const Dir dir = dir_of(std::int64_t(mx) - ux, std::int64_t(my) - uy);
    const float dist = std::hypot(mx - mine->x, my - mine->y);
    const float clamped = range > 0 ? std::min(dist, range) : 0;
    const std::int32_t px = ux + std::int32_t(float(dir.x) / kOne * clamped), py = uy + std::int32_t(float(dir.y) / kOne * clamped);
    const Flow flow = flow_of(d->sub);
    const Rgba edge{255, 255, 255, 230};
    if (range > 0) ground_ring(r_, view_, mine->x, mine->y, range, {255, 255, 255, 90}, 72);
    Shape s;
    switch (d->kind) {
        case SkillKind::Bolt: case SkillKind::Dash: case SkillKind::Tornado: case SkillKind::Lantern: {
            const float len = d->kind == SkillKind::Lantern || d->kind == SkillKind::Dash ? clamped : range;
            const std::int32_t ex = ux + std::int32_t(float(dir.x) / kOne * len), ey = uy + std::int32_t(float(dir.y) / kOne * len);
            const float hw = d->kind == SkillKind::Tornado ? radius : std::max(12.f, width / 2);
            const Dir side{-dir.y, dir.x};
            for (float sg : {-1.f, 1.f})
                line(r_, view_.sx(float(ux) + sg * hw * float(side.x) / kOne), view_.sy(float(uy) + sg * hw * float(side.y) / kOne),
                     view_.sx(float(ex) + sg * hw * float(side.x) / kOne), view_.sy(float(ey) + sg * hw * float(side.y) / kOne), {255, 255, 255, 140});
            s = fx_shape(d->kind == SkillKind::Tornado ? FxKind::Tornado : FxKind::Bolt, ex, ey, std::int32_t(hw), 0, 0, dir, flow, 1, local_tick_);
            break;
        }
        case SkillKind::Wave: {
            const std::int32_t ex = ux + std::int32_t(float(dir.x) / kOne * range), ey = uy + std::int32_t(float(dir.y) / kOne * range);
            s = fx_shape(FxKind::Wave, ex, ey, 0, std::int32_t(width), 0, dir, flow, 1, local_tick_);
            const Shape start = fx_shape(FxKind::Wave, ux, uy, 0, std::int32_t(width), 0, dir, flow, 1, local_tick_);
            outline_shape(r_, view_, start, {255, 255, 255, 120});
            break;
        }
        case SkillKind::Cone: s = fx_shape(FxKind::Cone, ux, uy, 0, std::int32_t(width), std::int32_t(range), dir, flow, 1, local_tick_); break;
        case SkillKind::Burst: case SkillKind::Field: case SkillKind::Heal: case SkillKind::Pillar:
            s = fx_shape(FxKind::Field, range > 0 ? px : ux, range > 0 ? py : uy, std::int32_t(radius), 0, 0, dir, flow, 1, local_tick_);
            break;
        case SkillKind::Boulder: s = fx_shape(FxKind::Boulder, px, py, std::int32_t(width / 2), 0, 0, dir, flow, 1, local_tick_); break;
        case SkillKind::Wall: {
            const Dir along{-dir.y, dir.x};
            const float L = radius > 0 ? radius : width;
            if (radius > 0) ground_ring(r_, view_, float(px), float(py), L, edge, 48);
            else line(r_, view_.sx(float(px) - float(along.x) / kOne * L / 2), view_.sy(float(py) - float(along.y) / kOne * L / 2),
                      view_.sx(float(px) + float(along.x) / kOne * L / 2), view_.sy(float(py) + float(along.y) / kOne * L / 2), edge);
            break;
        }
        case SkillKind::Blink: ground_ring(r_, view_, float(px), float(py), 30, edge); break;
        case SkillKind::Bloodbend: text(r_, ui_->mx + 12, ui_->my, "click an enemy", edge); break;
        default: break;
    }
    if (!s.empty()) {
        fill_shape(r_, view_, s, {255, 255, 255, 40});
        outline_shape(r_, view_, s, edge, 2);  // exact edges of the skill [U-131]
    }
}

void GameView::draw_hud(NetClient& net, double now_s, Ui& ui) {
    const Snapshot& cur = net.cur;
    const auto& me = cur.me;
    const float W = view_.sw, H = view_.sh;
    const double since = now_s - cur.arrived;
    const std::int64_t now_ms = cur.now_ms + std::int64_t(since * 1000);
    const SkyState sky = WorldClock::at_ms(net.start_ms + now_ms);
    const auto it = net.roster.find(net.my_unit);
    const ChampionDef* cd = it != net.roster.end() ? &champions()[std::size_t(it->second.champ)] : nullptr;
    const Element my_el = cd ? cd->element : Element::None;

    sky_mods_ = env_modifiers(my_el, sky);
    // top-right: time, sky, score [U-30..U-35][U-107][U-122..U-126][U-206]
    const float bx0 = W - 480;
    fill_rect(r_, bx0, 0, 480, 64, {0, 0, 0, 160});
    text(r_, bx0 + 8, 6, std::format("{}   {} {} : {} {}", mmss(now_ms), tr("Fire"), cur.kills[0], cur.kills[1], tr("Earth")), {255, 255, 255}, 2);
    // [U-253] sky: clock, sight, the next event with a countdown, and every active sky effect as a chip.
    // Hover a chip → what it does to MY champion (numbers). Chips with a gold frame change my power.
    const NextEvent nx = next_sky_event(net.start_ms + now_ms);
    text(r_, bx0 + 8, 28, std::format("{} {:02}:{:02}   {} {}%{}", sky.is_day ? tr("day") : tr("night"), sky.minute_of_day / 60,
                                      sky.minute_of_day % 60, tr("sight"), vision_range_pct(sky),
                                      nx.name.empty() ? std::string() : std::format("   {}: {} {}", tr("next"), nx.name, mmss(nx.in_ms))),
         {220, 220, 160});
    {  // [U-389] right end of the clock line (it used to sit in the chip row)
        const std::string pf = std::format("{:.0f} ms  {:.0f} fps", net.ping_ms, frame_ms_ > 0 ? 1000.0 / frame_ms_ : 0.0);
        text(r_, bx0 + 472 - text_w(pf), 10, pf, {150, 160, 180});
    }
    {
        float cx = bx0 + 8;
        const ModView* hov = nullptr;
        float hov_x = 0;
        for (const ModView& m : sky_mods_) {
            const float cw = text_w(m.name) + 10;
            fill_rect(r_, cx, 43, cw, 15, mixc(m.colour, {0, 0, 0, 255}, m.minor ? 80 : 55));
            rect(r_, cx, 43, cw, 15, m.minor ? Rgba{110, 110, 120} : Rgba{255, 210, 90});
            text(r_, cx + 5, 47, m.name, m.minor ? Rgba{180, 180, 180} : Rgba{255, 255, 255});
            if (hover(ui_ ? *ui_ : ui, cx, 43, cw, 15)) hov = &m, hov_x = cx;
            cx += cw + 4;
        }
        if (hov) {
            const float tw = text_w(hov->detail) + 12;
            const float tx = std::min(hov_x, W - tw - 4);
            fill_rect(r_, tx, 62, tw, 18, {0, 0, 0, 235});
            rect(r_, tx, 62, tw, 18, hov->colour);
            text(r_, tx + 6, 67, hov->detail, {255, 255, 255});
        }
    }
    // kill feed under it
    float fy = 72;
    for (auto itf = net.feed.rbegin(); itf != net.feed.rend() && fy < 220; ++itf, fy += 14) {
        const float tw = text_w(itf->text);
        fill_rect(r_, W - tw - 16, fy - 2, tw + 8, 12, {0, 0, 0, 140});
        text(r_, W - tw - 12, fy, itf->text, {255, 230, 200});
    }
    // chat (bottom-left)
    float cy = H - 230;
    if (set_.show_chat)
        for (std::size_t i = net.chat.size() > 8 ? net.chat.size() - 8 : 0; i < net.chat.size(); ++i) {
            const ChatLine& c = net.chat[i];
            if (now_s - c.born > 20 && !chat_open_) continue;
            const Rgba col = c.channel == net::Chat::Team ? Rgba{140, 200, 255} : c.channel == net::Chat::All ? Rgba{255, 255, 255}
                           : c.channel == net::Chat::Whisper ? Rgba{230, 140, 255} : c.channel == net::Chat::System ? Rgba{255, 220, 120}
                                                                                                                  : Rgba{200, 255, 200};
            text(r_, 10, cy, fit_text(std::format("[{}] {}: {}", net::chat_name(c.channel), c.from, c.text), W - 250), col);  // [U-389]
            cy += 12;
        }
    if (chat_open_) {
        fill_rect(r_, 8, H - 250, 420, 18, {0, 0, 0, 200});
        text(r_, 12, H - 245, "> " + chat_text_ + "_", {255, 255, 255});
    }

    // bottom panel: icon, stats, skills, bars, items, buttons → client/hud.cpp [U-166..U-170]
    const float py = H - 110;
    const net::NetUnit* mu = nullptr;
    for (const auto& u : cur.units)
        if (u.id == net.my_unit) mu = &u;
    draw_bottom_panel(net, now_s, ui);
    {  // [U-366] my jungle buff, [U-339] the Avatar's carried water — one line over the minimap
        std::string extra;
        static const char* kBuff[] = {"", "Ember (autos burn + slow)", "Wan Shi Tong's Wisdom (regen +50 %, cooldowns -10 %)"};
        if (me.jbuff > 0 && me.jbuff < 3 && me.jbuff_ms > 0) extra += std::format("{} {}:{:02}   ", tr(kBuff[me.jbuff]), me.jbuff_ms / 60'000, me.jbuff_ms / 1000 % 60);
        if (me.water_max > 0) extra += std::format("{} {}/{}", tr("water"), me.water / 100, me.water_max / 100);
        if (!extra.empty()) text(r_, W - text_w(extra) - 12, H - 200 - 150 - 24, extra, {170, 220, 255});
    }

    // minimap [U-99]
    if (set_.minimap) {
        const float ms = 200, mx = W - ms - 8, my = H - ms - 150;
        const SDL_FRect md{mx, my, ms, ms};
        const double rot = view_.flip ? 180.0 : 0.0;  // [U-174] my base bottom-left on the minimap too
        SDL_RenderTextureRotated(r_, map_tex_, nullptr, &md, rot, nullptr, SDL_FLIP_NONE);
        SDL_RenderTextureRotated(r_, fog_tex_, nullptr, &md, rot, nullptr, SDL_FLIP_NONE);
        const float sc = ms / float(kMapUnits);
        auto mmx = [&](float wx) { return mx + view_.fx(wx) * sc; };
        auto mmy = [&](float wy) { return my + view_.fx(wy) * sc; };
        for (const DrawUnit& du : draw_) {
            if (!du.u.alive || du.u.kind == std::uint8_t(UnitKind::Champion)) continue;
            const float rr = du.u.kind == std::uint8_t(UnitKind::Minion) ? 1.5f : 3.f;
            fill_circle(r_, mmx(du.x), mmy(du.y), rr, team_colour(du.u.team), 8);
        }
        if (set_.show_paths)  // [U-203]
            for (const auto& p : cur.paths)
                for (int i = 0; i + 1 < p.n; ++i)
                    line(r_, mmx(float(p.pts[i][0] * kCellUnits)), mmy(float(p.pts[i][1] * kCellUnits)), mmx(float(p.pts[i + 1][0] * kCellUnits)),
                         mmy(float(p.pts[i + 1][1] * kCellUnits)), p.unit == net.my_unit ? Rgba{255, 255, 255, 160} : Rgba{110, 220, 120, 140});
        for (const auto& g : cur.ghosts)  // [U-235] falcon report: grey border enemies, green allies
            if (g.kind == std::uint8_t(UnitKind::Champion) && g.champ >= 0) {
                draw_icon(g.champ, mmx(float(g.x)) - 7, mmy(float(g.y)) - 7, 14, true);
                rect(r_, mmx(float(g.x)) - 8, mmy(float(g.y)) - 8, 16, 16, g.team == net.my_team ? Rgba{90, 220, 110} : Rgba{150, 150, 150});
            }
        for (const DrawUnit& du : draw_) {  // [U-236] champion icons at a sensible size
            if (!du.u.alive || du.u.kind != std::uint8_t(UnitKind::Champion) || du.u.champ < 0) continue;
            const float s2 = 18;
            draw_icon(du.u.champ, mmx(du.x) - s2 / 2, mmy(du.y) - s2 / 2, s2, false);
            rect(r_, mmx(du.x) - s2 / 2 - 1, mmy(du.y) - s2 / 2 - 1, s2 + 2, s2 + 2,
                 du.u.id == net.my_unit ? Rgba{255, 230, 60} : du.u.team == net.my_team ? Rgba{90, 200, 255} : Rgba{230, 70, 60});
        }
        for (const auto& c : cur.camps) {  // [U-365][U-411] camp timers on the minimap only, and only while Tab is held
            if (!show_score_) break;
            const float cx = mmx(float(c.x * kCellUnits + 16)), cy = mmy(float(c.y * kCellUnits + 16));
            if (c.respawn_s == 0) {
                if (c.epic) circle(r_, cx, cy, 5, {255, 210, 80, 230}, 12);
                continue;
            }
            circle(r_, cx, cy, c.epic ? 5.f : 3.f, {150, 150, 150, 200}, 10);
            const std::string t = c.respawn_s >= 60 ? std::format("{}:{:02}", c.respawn_s / 60, c.respawn_s % 60) : std::format("{}", c.respawn_s);
            text(r_, cx - text_w(t, 0.75f) / 2, cy + 4, t, c.epic ? Rgba{255, 210, 80} : Rgba{210, 210, 210}, 0.75f);
        }
        for (const auto& p : cur.pings)  // [U-246]
            if (cur.now_ms - p.ms < 4'500) circle(r_, mmx(float(p.x)), mmy(float(p.y)), 6 + float((cur.now_ms - p.ms) % 1'000) / 100.f, {255, 230, 90, 220}, 12);
        SDL_SetRenderClipRect(r_, nullptr);
        {  // [U-389] the camera frame stays inside the minimap (far zoom made it huge)
            const SDL_Rect clip{int(mx), int(my), int(ms), int(ms)};
            SDL_SetRenderClipRect(r_, &clip);
            rect(r_, std::min(mmx(view_.wx(0)), mmx(view_.wx(view_.sw))), std::min(mmy(view_.wy(0)), mmy(view_.wy(view_.sh))),
                 view_.sw / view_.zoom * sc, view_.sh / (view_.zoom * view_.gy) * sc, {255, 255, 255, 180});
            SDL_SetRenderClipRect(r_, nullptr);
        }
        rect(r_, mx, my, ms, ms, {140, 140, 160});
        // [U-389] the shop window lies over the minimap on small screens: its clicks belong to the shop
        const float shop_w = std::min(W - 80, 1100.f), shop_h = std::min(H - 260, 620.f);
        const bool over_shop = show_shop_ && hover(ui, shop_x_, shop_y_ - 24, shop_w, shop_h + 24);
        if (!over_shop && hover(ui, mx, my, ms, ms)) {
            const float wx = view_.fx((ui.mx - mx) / sc), wy = view_.fx((ui.my - my) / sc);
            if (ui.clicked) {
                cam_lock_ = false;
                view_.cx = wx;
                view_.cy = wy;
                ui.clicked = false;
            }
            if (ui.rclicked) {
                net.send_cmd({.kind = CmdKind::Move, .x = std::int32_t(wx), .y = std::int32_t(wy)});
                ui.rclicked = false;
            }
        }
    }

    // shop [U-80][U-81][U-113][U-136][U-220][U-245]: the LoL-like browser; buy with the button under the tooltip
    if (show_shop_) {
        const float sw = std::min(W - 80, 1100.f), sh = std::min(H - 260, 620.f);
        // [U-342] a title bar: drag it to move the shop (like LoL), X closes it; the window stays on the screen
        if (shop_x_ < 0) shop_x_ = 40, shop_y_ = 80;
        shop_x_ = std::clamp(shop_x_, 0.f, std::max(0.f, W - sw)), shop_y_ = std::clamp(shop_y_, 24.f, std::max(24.f, H - sh));
        const float bar_y = shop_y_ - 24;
        fill_rect(r_, shop_x_, bar_y, sw, 24, {40, 34, 22, 245});
        rect(r_, shop_x_, bar_y, sw, 24, {150, 130, 90});
        text(r_, shop_x_ + 8, bar_y + 8, tr("Shop — drag to move"), {255, 225, 150});
        if (button(r_, ui, shop_x_ + sw - 26, bar_y + 2, 22, 20, "X")) show_shop_ = false;
        if (ui.clicked && hover(ui, shop_x_, bar_y, sw - 30, 24)) shop_drag_ = true, shop_dx_ = ui.mx - shop_x_, shop_dy_ = ui.my - shop_y_, ui.clicked = false;
        if (!ui.down) shop_drag_ = false;
        if (shop_drag_) shop_x_ = ui.mx - shop_dx_, shop_y_ = ui.my - shop_dy_;
        if (hover(ui, shop_x_, bar_y, sw, 24)) mouse_on_hud_ = true;
    }
    if (show_shop_) {
        const float sw = std::min(W - 80, 1100.f), sh = std::min(H - 260, 620.f);
        Bag bag{};
        for (int i = 0; i < kItemSlots; ++i) bag[std::size_t(i)] = me.items[i];
        shop_.bag = &bag;
        const int bought = shop_.draw(r_, ui, shop_x_, shop_y_, sw, sh, me.can_shop ? tr("Buy") : tr("Stand in the shop ring to buy"),
                                      [&](int it2) { return me.can_shop && me.gold >= plan_buy(bag, it2).price; }, me.gold);
        shop_.bag = nullptr;
        if (bought >= 0) net.send_cmd({.kind = CmdKind::Buy, .item = std::int16_t(bought)});
        if (hover(ui, shop_x_, shop_y_, sw, sh)) mouse_on_hud_ = true;
    }
    // scoreboard
    if (show_score_ && net.winner < 0) {  // [U-389] (at the end the victory screen + post-game stats take over)
        const float sx = W / 2 - 360, sy = 90;
        // [U-411] epic monster timers at the top (like other MOBAs); Lion Turtle stacks per team
        std::string epics;
        for (const auto& c : cur.camps)
            if (c.epic) {
                const char* nm = net::kMobNames[std::min<int>(c.name_id, int(std::size(net::kMobNames)) - 1)];
                epics += c.respawn_s == 0 ? std::format("{}: {}    ", tr(nm), tr("up")) : std::format("{}: {}:{:02}    ", tr(nm), c.respawn_s / 60, c.respawn_s % 60);
            }
        epics += std::format("{} {} / {}", tr("Lion Turtle stacks"), me.lion_stacks[0], me.lion_stacks[1]);
        fill_rect(r_, sx, sy - 26, 720, 22, {0, 0, 0, 220});
        text(r_, sx + 10, sy - 22, fit_text(epics, 700), {255, 210, 80});
        fill_rect(r_, sx, sy, 720, 40 + 10 * 22.f, {0, 0, 0, 220});
        text(r_, sx + 10, sy + 8, tr("Champion            Lvl  K / D / A   CS   items   bounty"), {255, 255, 255});
        float ry = sy + 28;
        for (int team = 0; team < 2; ++team)
            for (const auto& s : cur.scores) {
                const auto r = net.roster.find(s.id);
                if (r == net.roster.end() || r->second.team != team) continue;
                int items = 0;
                for (auto i : s.items) items += i >= 0;
                text(r_, sx + 10, ry, fit_text(r->second.name, 150), team_colour(team));  // [U-389] the name in its own column
                text(r_, sx + 166, ry, std::format("{:>3}  {:>2} / {:>2} / {:>2}  {:>4}   {:>2}", s.level, s.kills, s.deaths, s.assists, s.cs, items),
                     team_colour(team));
                // [U-413] the bounty: gold when it is a shutdown (above 300), grey when lowered by deaths
                const Rgba bc = s.bounty > 300 ? Rgba{255, 200, 60} : s.bounty < 300 ? Rgba{150, 150, 150} : Rgba{220, 220, 220};
                const std::string bs = s.bounty > 300 ? std::format("{} g  SHUTDOWN", s.bounty) : std::format("{} g", s.bounty);
                if (s.bounty > 300) fill_rect(r_, sx + 486, ry - 2, text_w(bs) + 8, 16, {90, 60, 0, 200});
                text(r_, sx + 490, ry, bs, bc);
                if (!s.alive) text(r_, sx + 630, ry, std::format("{} {}s", tr("respawn"), s.respawn_ms / 1000), {200, 200, 200});
                ry += 20;
            }
    }
    // death / end
    if (mu && !mu->alive && net.winner < 0) {
        fill_rect(r_, 0, 0, W, H, {40, 40, 40, 90});
        const std::string rs = std::format("{} {:.0f} s", tr("Respawn in"), std::max(0.0, me.respawn_ms / 1000.0 - since));
        text(r_, (W - text_w(rs, 3)) / 2, H / 2 - 60, rs, {255, 255, 255}, 3);
        draw_death_recap(net, now_s);  // [U-262]
    } else {
        show_recap_ = false;
    }
    draw_announcements(net, now_s);  // [U-263]
    if (net.winner >= 0) {
        const bool won = net.winner == net.my_team;
        if (SDL_Texture* bg = res_.texture(won ? "screens/victory.png" : "screens/defeat.png")) {  // [U-257]
            const SDL_FRect d{W / 2 - 300, H / 2 - 260, 600, 410};
            SDL_RenderTexture(r_, bg, nullptr, &d);
        }
        fill_rect(r_, 0, H / 2 - 50, W, 100, {0, 0, 0, 200});
        const std::string big = won ? tr("VICTORY") : tr("DEFEAT");
        const std::string line = std::format("{} {}. {}", net.winner == 0 ? tr("Fire Nation") : tr("Earth Nation"), tr("wins"), tr("Esc = back to menu"));
        text(r_, (W - text_w(big, 5)) / 2, H / 2 - 30, big, won ? Rgba{255, 220, 100} : Rgba{255, 90, 90}, 5);
        text(r_, (W - text_w(line, 2)) / 2, H / 2 + 20, line, {255, 255, 255}, 2);
    }
    if (pending_ >= 0) {
        const SkillDef* pd = skill_of(net, pending_);
        const char* how = src_x_ >= 0                     ? "source picked - left click: where it goes  right click: cancel"
                          : pd && source_first(*pd)        ? "left click: cast (on water / a rock: pick the source first)  right click: cancel"
                                                           : "left click: cast  right click: cancel";
        text(r_, ui.mx + 14, ui.my + 14, how, {255, 255, 255});
    }
    if (champs_only()) text(r_, ui.mx + 14, ui.my + 26, "CHAMPIONS ONLY", {255, 120, 120});
    draw_portraits(net, ui);
    draw_tooltip(net, ui);
    if (attack_move_) text(r_, ui.mx + 14, ui.my + 14, attack_move_ == 2 ? "attack-move CHAMPIONS ONLY: left click" : "attack-move: left click", {255, 200, 200});
    if (last_frame_ - hint_t_ < 2.5) {
        const float hs = W < 1400 ? 1.5f : 2.f;  // [U-389] never under the minimap / chat
        const std::string hint_fit = fit_text(hint_, W - 460, hs);
        const float tw = text_w(hint_fit, hs);
        fill_rect(r_, (W - tw) / 2 - 8, py - 104, tw + 16, 24, {0, 0, 0, 190});
        text(r_, (W - tw) / 2, py - 100, hint_fit, {255, 230, 140}, hs);
    }
    // [X-25] debug: cell under the cursor
    {
        const int cx = int(view_.wx(ui.mx)) / kCellUnits, cy = int(view_.wy(ui.my)) / kCellUnits;
        if (Grid::in_bounds(cx, cy) && ((SDL_GetModState() & SDL_KMOD_ALT) != 0) && !mouse_on_hud_) {
            const Cell c = net.seen.at(cx, cy);
            text(r_, ui.mx + 14, ui.my - 14, std::format("cell {},{} surf {} elev {} wall {} tree {} bush {} raw {:08x}", cx, cy, int(c.surface()),
                                                          c.effective_elevation(), int(c.wall()), int(c.tree()), int(c.bush()), c.raw()), {255, 255, 0});
        }
    }
}

// [U-201] sounds from what happened since the last frame. Positional: only what my champion can HEAR
// (the same ranges as the sound waves) is played, quieter with distance, panned left / right.
const GameView::DrawUnit* GameView::me_unit() const {
    for (const DrawUnit& d : draw_)
        if (d.u.id == me_id_) return &d;
    return nullptr;
}

void GameView::play_sounds(NetClient& net) {
    me_id_ = net.my_unit;
    const DrawUnit* me = nullptr;
    for (const DrawUnit& d : draw_)
        if (d.u.id == net.my_unit) me = &d;
    const float lx = me ? me->x : view_.cx, ly = me ? me->y : view_.cy;
    auto at = [&](const std::string& name, float x, float y, float range) {
        const float dx = (x - lx) * (view_.flip ? -1.f : 1.f), dy = y - ly;
        audio_.play_at(res_.sound(name), dx, dy, range);
    };
    for (const auto& f : net.cur.fx) {  // a new skill = its sound by shape + element [U-259] (old: cast_<element>)
        if (fx_seen_.contains(f.id)) continue;
        fx_seen_[f.id] = true;
        const FxKind k = FxKind(f.kind);
        static constexpr const char* kShape[] = {"bolt", "wave", "burst", "field", "cone", "tornado", "lantern", "auto", "towershot",
                                                 "boulder", "heal", "steam"};
        // lightning has its own sounds (fx/<shape>_Lightning) [U-295]
        const std::string el = SubElement(f.sub) == SubElement::Lightning ? std::string("Lightning")
                                                                           : std::string(element_name(base_element(SubElement(f.sub))));
        const std::string name = std::string("fx/") + kShape[std::min<int>(f.kind, int(std::size(kShape)) - 1)] + "_" + el;
        if (res_.sound(name)) at(name, float(f.x), float(f.y), float(kHearLoud));
        else if (k != FxKind::Steam) at(k == FxKind::Auto || k == FxKind::TowerShot ? "auto_ranged" : "cast_" + el, float(f.x), float(f.y), float(kHearLoud));
    }
    // [U-259] my own events: level up, buying / selling, gold, refused orders
    const auto& mine_s = net.cur.me;
    if (net.have_snapshot) {
        const DrawUnit* mine = me_unit();
        const int lvl = mine ? int(mine->u.level) : 0;
        if (snd_level_ > 0 && lvl > snd_level_) audio_.play(res_.sound("ui/level_up"), SoundCat::Ui);
        snd_level_ = lvl;
        int items = 0;
        for (int i = 0; i < kItemSlots; ++i) items += mine_s.items[i] >= 0 ? mine_s.items[i] + 1 : 0;
        if (snd_items_ >= 0 && items != snd_items_) audio_.play(res_.sound(mine_s.gold < snd_gold_ ? "ui/buy" : "ui/sell"), SoundCat::Ui);
        snd_items_ = items;
        snd_gold_ = mine_s.gold;
        if (net.notice_seq != snd_notice_) {
            if (snd_notice_ >= 0) audio_.play(res_.sound("ui/error"), SoundCat::Ui, 0.6f);
            snd_notice_ = net.notice_seq;
        }
        const bool recalling = mine_s.channel_left_ms > 0;
        if (recalling && !snd_recall_) audio_.play(res_.sound("world/recall"), SoundCat::Effects, 0.8f);
        snd_recall_ = recalling;
        if (!snd_welcome_) {  // [U-263] the announcer greets at the start …
            snd_welcome_ = true;
            play_voice("welcome");
        }
        if (net.winner >= 0 && !snd_end_) {  // … and calls the end
            snd_end_ = true;
            play_voice(net.winner == net.my_team ? "victory" : "defeat");
        }
    }
    if (fx_seen_.size() > 4'000) fx_seen_.clear();
    for (const LocalHit& h : net.hits)
        if (h.born == net.cur.arrived)
            for (const DrawUnit& d : draw_)
                if (d.u.id == h.h.dst) at(h.h.is_auto ? "hit_auto" : "hit_" + std::string(element_name(base_element(SubElement(h.h.sub)))), d.x, d.y, float(kHearLoud));
    for (const DrawUnit& d : draw_) {  // deaths
        auto [it, fresh] = alive_seen_.try_emplace(d.u.id, d.u.alive != 0);
        const bool structure = d.u.kind >= std::uint8_t(UnitKind::Turret) && d.u.kind <= std::uint8_t(UnitKind::Nexus);
        if (!fresh && it->second && !d.u.alive)
            at(structure ? "world/structure_down" : d.u.kind == std::uint8_t(UnitKind::Champion) ? "death_champion" : "death", d.x, d.y,
               structure ? 99'999.f : float(kHearLoud));
        it->second = d.u.alive != 0;
    }
}

}  // namespace em
