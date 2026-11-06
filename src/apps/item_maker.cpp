// [U-535] The item maker: own items and changes to built-in ones. They live in a game version (versions/<name>/version.txt,
// item.<name>.<key> = value), so they reach everyone who joins that version. The official game is never changed.
#include <SDL3/SDL.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <string>
#include <vector>

#include "apps/maker_ui.hpp"
#include "apps/makers.hpp"
#include "core/items.hpp"
#include "core/stats.hpp"
#include "game/versions.hpp"

using namespace em;
using namespace em::gfx;
using loc::tr;
using loc::trf;
namespace fs = std::filesystem;

namespace {

struct NumberRow { const char* key; const char* label; int lo, hi; };
constexpr NumberRow kRows[] = {
    {"health", "Health", 0, 2000}, {"attack", "Attack", 0, 300}, {"elemental_power", "Elemental power", 0, 400}, {"defence", "Defence", 0, 200},
    {"elemental_defence", "Elemental defence", 0, 200}, {"attack_speed", "Attack speed %", 0, 150}, {"move_speed", "Move speed", 0, 150},
    {"move_speed_pct", "Move speed %", 0, 50}, {"elemental_haste", "Elemental haste", 0, 100}, {"non_elemental_haste", "Non-elemental haste", 0, 100},
    {"cast_speed", "Cast speed", 0, 100}, {"skill_speed", "Skill speed", 0, 100}, {"skill_width", "Skill width", 0, 200}, {"skill_length", "Skill length", 0, 500},
    {"skill_range", "Skill range", 0, 500}, {"skill_radius", "Skill radius", 0, 200}, {"elemental_lethality", "Elemental lethality", 0, 100},
    {"elemental_penetration", "Elemental penetration %", 0, 60}, {"tenacity", "Tenacity %", 0, 60}, {"cc_amplify", "Stronger crowd control %", 0, 50},
    {"lifesteal", "Life steal %", 0, 40}, {"omnivamp", "Omnivamp %", 0, 40}, {"spellvamp", "Spell vamp %", 0, 40}, {"crit_chance", "Critical chance %", 0, 100},
    {"crit_damage", "Critical damage %", 0, 150}, {"on_hit_dmg", "Damage on hit", 0, 200}, {"on_hit_max_hp", "On hit, % of max health", 0, 20},
    {"resist_water", "Resist water %", 0, 60}, {"resist_earth", "Resist earth %", 0, 60}, {"resist_fire", "Resist fire %", 0, 60}, {"resist_air", "Resist air %", 0, 60},
    {"slow_resist", "Resist slows %", 0, 80}, {"wall_damage", "Damage to walls", 0, 300}, {"model_size", "Model and hitbox size %", -40, 60},
};

class ItemMaker {
public:
    tool::Window win;
    bool init() {
        if (!win.embedded() && !win.open("Elemental MOBA - Item maker", 1400, 860)) return false;
        doc_.prefix = "item.";
        const auto all = list_versions();
        for (const std::string& v : all)
            if (v != kOfficialVersion && current_version() == v) doc_.load(v);
        rebuild();
        return true;
    }
    void run() {
        while (win.begin()) {
            frame();
            win.dirty = dirty_;
            win.end();
        }
    }

private:
    makers::Picker picker_;
    makers::VersionDoc doc_;
    std::string item_, search_, status_, tip_, new_version_, icon_path_;
    bool dirty_ = false, naming_ = false, new_item_ = false;
    std::string naming_text_;
    float list_scroll_ = 0, scroll_ = 0;
    int show_all_ = 1;

    void rebuild() {  // the table = the version as saved + what is being edited
        std::string e = doc_.name.empty() ? apply_version(kOfficialVersion) : std::string();
        if (!doc_.name.empty() && fs::exists(doc_.file())) e = apply_version(doc_.name);
        else if (!doc_.name.empty()) e = apply_version(kOfficialVersion);
        if (!e.empty()) status_ = e;
        restore_items();
        for (const makers::VersionDoc::Line& l : doc_.lines) {
            const auto d2 = l.key.rfind('.');
            if (d2 == std::string::npos || d2 <= 5) continue;
            if (const std::string er = apply_item_line(l.key.substr(5, d2 - 5), l.key.substr(d2 + 1), l.value); !er.empty()) status_ = er;
        }
    }
    void set(const std::string& key, const std::string& value) {
        if (doc_.name.empty()) {  // the official game stays: the first change asks for a version
            naming_ = true, new_item_ = false;
            naming_text_ = "my-items";
            win.ui.focus = 98;
            return;
        }
        doc_.set("item." + item_ + "." + key, value);
        dirty_ = true;
        rebuild();
    }
    const ItemDef* cur() const {
        const int i = item_index(item_);
        return i >= 0 ? &kItems[std::size_t(i)] : nullptr;
    }
    void choose_version() {
        std::vector<makers::Pick> items;
        items.push_back({tr("New version"), "+", tr("A copy of the official game with your changes")});
        for (const std::string& v : list_versions())
            if (v != kOfficialVersion) items.push_back({v, v, version_description(v)});
        picker_.open(win, tr("Which game version do these items belong to"), std::move(items), [this](const std::string& v) {
            if (v == "+") {
                naming_ = true, new_item_ = false;
                naming_text_ = "my-items";
                win.ui.focus = 98;
            } else {
                doc_.load(v);
                dirty_ = false;
                rebuild();
            }
        });
    }
    void save() {
        std::string e;
        if (doc_.name.empty()) return choose_version();
        if (doc_.save(&e)) {
            dirty_ = false;
            status_ = trf("Saved in versions/{}: choose this version in the menu, or press Use it in the game", doc_.name);
        } else {
            status_ = e;
        }
        rebuild();
    }
    void take_icon() {
        if (doc_.name.empty() || item_.empty() || icon_path_.empty()) return;
        std::string id;
        for (char c : item_) {
            if (c == '\'') continue;
            id += c == ' ' || c == '-' ? '_' : char(std::tolower(static_cast<unsigned char>(c)));
        }
        const fs::path dir = fs::path("versions") / doc_.name / "pack" / "icons" / "items";
        std::error_code ec;
        fs::create_directories(dir, ec);
        fs::copy_file(icon_path_, dir / (id + ".png"), fs::copy_options::overwrite_existing, ec);
        status_ = ec ? ec.message() : trf("The picture is the icon of {}", item_);
        icon_path_.clear();
    }

    void toolbar() {
        SDL_Renderer* r = win.ren();
        Ui& ui = win.ui;
        text(r, 14, 12, tr("Item maker"), {255, 200, 120}, 2);
        float x = 14 + text_w(tr("Item maker"), 2) + 16;
        if (button(r, ui, x, 8, 320, 28, fit_text(std::format("{}: {}", tr("Game version"), doc_.name.empty() ? tr("(choose one)") : doc_.name), 308), doc_.name.empty())) choose_version();
        x += 324;
        if (button(r, ui, x, 8, 110, 28, tr("New item"))) {
            naming_ = true, new_item_ = true;
            naming_text_ = "New Item";
            ui.focus = 98;
        }
        x += 114;
        if (button(r, ui, x, 8, 110, 28, dirty_ ? tr("Save *") : tr("Save"), dirty_)) save();
        x += 114;
        const bool can_use = !doc_.name.empty();
        if (can_use && hover(ui, x, 8, 170, 28)) tip_ = tr("Practice and hosting then run this version; players who join get it");
        if (button(r, ui, x, 8, 170, 28, tr("Use it in the game"), false, can_use) && can_use) {
            save();
            win.exit.use_version = doc_.name;
            if (!win.embedded()) status_ = trf("Choose the version {} in the menu of the game", doc_.name);
            else win.leave();
        }
        if (button(r, ui, win.w - 120, 8, 110, 28, std::format("{}: {}", tr("Language"), loc::current()))) loc::load(tool::next_language(loc::current()));
    }

    void list_panel(float x, float y, float w, float h) {
        SDL_Renderer* r = win.ren();
        Ui& ui = win.ui;
        text_field(r, ui, 1, x, y + 14, w, search_, tr("Search"));
        const float top = y + 56, bottom = y + h;
        const SDL_Rect clip{int(x), int(top), int(w), int(bottom - top)};
        SDL_SetRenderClipRect(r, &clip);
        if (hover(ui, x, top, w, bottom - top)) list_scroll_ = std::max(0.f, list_scroll_ - ui.wheel * 44);
        std::string low = search_;
        for (char& c : low) c = char(std::tolower(static_cast<unsigned char>(c)));
        float ly = top - list_scroll_;
        for (const ItemDef& d : kItems) {
            std::string nm(d.name);
            for (char& c : nm) c = char(std::tolower(static_cast<unsigned char>(c)));
            if (!low.empty() && nm.find(low) == std::string::npos) continue;
            if (ly > top - 30 && ly < bottom) {
                const bool own = std::ranges::any_of(doc_.lines, [&](const auto& l) { return l.key.starts_with("item." + std::string(d.name) + "."); });
                if (button(r, ui, x, ly, w - 4, 26, fit_text(std::format("{}{}   {} g", own ? "* " : "", d.name, d.cost), w - 16), item_ == d.name) && hover(ui, x, top, w, bottom - top)) item_ = std::string(d.name), scroll_ = 0;
            }
            ly += 30;
        }
        SDL_SetRenderClipRect(r, nullptr);
    }

    void edit_panel(float x, float y, float w, float h) {
        SDL_Renderer* r = win.ren();
        Ui& ui = win.ui;
        const ItemDef* dp = cur();
        if (!dp) {
            text(r, x, y + 10, fit_text(tr("Choose an item on the left, or press New item."), w), {200, 200, 215});
            return;
        }
        const ItemDef& d = *dp;
        const SDL_Rect clip{int(x - 4), int(y), int(w + 8), int(h)};
        SDL_SetRenderClipRect(r, &clip);
        if (hover(ui, x, y, w, h)) scroll_ = std::max(0.f, scroll_ - ui.wheel * 44);
        float py = y + 6 - scroll_;
        text(r, x, py, fit_text(std::string(d.name), w, 2), {255, 220, 150}, 2);
        py += 28;
        const ItemTier tier = item_tier(d);
        text(r, x, py, fit_text(trf("Price {} g, recipe {} g, {}", d.cost, recipe_cost(d), tr(tier == ItemTier::Basic ? "Basic" : tier == ItemTier::Epic ? "Epic" : "Legendary")), w), {190, 190, 205});
        py += 22;
        auto number = [&](const char* key, const char* label, int v, int lo, int hi, int id) {
            if (py > y - 30 && py < y + h) {
                int nv = v;
                if (slider(r, ui, id, x, py, w, nv, lo, hi, tr(label)) && nv != v) set(key, std::to_string(nv));
            }
            py += 26;
        };
        number("cost", "Price", d.cost, 0, 6000, 1000);
        auto choice = [&](const char* key, const char* label, const char* shown, std::vector<const char*> names) {
            if (py > y - 30 && py < y + h && button(r, ui, x, py, w, 24, std::format("{}: {}", tr(label), tr(shown)))) {
                std::vector<makers::Pick> items;
                for (const char* n : names) items.push_back({tr(n), n, ""});
                const std::string k = key;
                picker_.open(win, tr(label), std::move(items), [this, k](const std::string& v) { set(k, v); });
            }
            py += 28;
        };
        constexpr const char* kGroups[] = {"None", "Boots", "ElementWard", "Special"};
        constexpr const char* kLimits[] = {"Many", "One", "OnePerGroup"};
        constexpr const char* kActives[] = {"None", "LotusTile", "SpiritFlask", "Falcon", "Shadow"};
        choice("group", "Group", kGroups[std::size_t(d.group)], {kGroups, kGroups + 4});
        choice("limit", "How many one champion may carry", kLimits[std::size_t(d.limit)], {kLimits, kLimits + 3});
        choice("active", "Use (key)", kActives[std::size_t(d.active)], {kActives, kActives + 5});
        if (d.active != ItemActive::None) number("active_cd_ms", "Use: cooldown (ms)", d.active_cd_ms, 0, 300000, 1001);
        const float half = w / 2;
        if (py > y - 30 && py < y + h) {
            if (button(r, ui, x, py, half - 4, 24, std::format("{}: {}", tr("Walks through units"), d.ghost ? tr("ON") : tr("OFF")), d.ghost)) set("ghost", d.ghost ? "0" : "1");
            if (button(r, ui, x + half, py, half - 4, 24, std::format("{}: {}", tr("Taken off the shop"), d.mod == "removed" ? tr("ON") : tr("OFF")), d.mod == "removed")) set("removed", d.mod == "removed" ? "0" : "1");
        }
        py += 30;
        // ---- the parts
        text(r, x, py, tr("Made of (parts)"), {255, 220, 150});
        py += 18;
        for (std::size_t i = 0; i < d.from.size(); ++i) {
            const float pw = w / 3;
            if (py > y - 30 && py < y + h && button(r, ui, x + pw * float(i), py, pw - 4, 24, d.from[i].empty() ? tr("(none)") : fit_text(std::string(d.from[i]), pw - 12))) {
                std::vector<makers::Pick> items{{tr("(none)"), "", ""}};
                for (const ItemDef& o : kItems)
                    if (o.name != d.name) items.push_back({std::string(o.name), std::string(o.name), std::format("{} g", o.cost)});
                picker_.open(win, tr("Made of (parts)"), std::move(items), [this, i](const std::string& v) {
                    const ItemDef* c = cur();
                    if (!c) return;
                    std::array<std::string, 3> parts;
                    for (std::size_t k = 0; k < 3; ++k) parts[k] = std::string(c->from[k]);
                    parts[i] = v;
                    std::string joined;
                    for (const std::string& p : parts)
                        if (!p.empty()) joined += (joined.empty() ? "" : ", ") + p;
                    set("from", joined);
                });
            }
        }
        py += 30;
        // ---- the numbers
        text(r, x, py, tr("What it gives"), {255, 220, 150}, 2);
        py += 26;
        int id = 1100;
        for (const NumberRow& row : kRows) number(row.key, row.label, item_number(d, row.key), row.lo, row.hi, id++);
        py += 6;
        // ---- any other stat
        text(r, x, py, tr("Any other stat"), {255, 220, 150});
        py += 18;
        for (const ItemDef::More& m : d.more) {
            if (m.key == StatKey::Count) continue;
            const std::string k = std::string(m.pct ? "pct_" : "") + std::string(kStatKeyNames[int(m.key)]);
            if (py > y - 30 && py < y + h) {
                int v = m.value / 100;
                const int before = v;
                if (slider(r, ui, 1300 + int(m.key), x, py, w - 30, v, 0, 500, std::format("{}{}", kStatKeyNames[int(m.key)], m.pct ? " %" : "")) && v != before) set(k, std::to_string(v));
                if (button(r, ui, x + w - 24, py, 22, 22, "x")) set(k, "0");
            }
            py += 26;
        }
        if (py > y - 30 && py < y + h && button(r, ui, x, py, half - 4, 24, tr("Add a stat"))) {
            std::vector<makers::Pick> items;
            for (int k = 0; k < int(StatKey::Count); ++k) {
                items.push_back({std::string(kStatKeyNames[k]), std::string(kStatKeyNames[k]), ""});
                items.push_back({std::format("{} %", kStatKeyNames[k]), std::format("pct_{}", kStatKeyNames[k]), tr("A percentage of that stat")});
            }
            picker_.open(win, tr("Add a stat"), std::move(items), [this](const std::string& v) { set(v, "10"); });
        }
        py += 34;
        // ---- the icon
        text(r, x, py, tr("Icon (PNG), goes with the version"), {255, 220, 150});
        py += 18;
        if (py > y - 30 && py < y + h) {
            text_field(r, ui, 2, x, py + 12, w - 90, icon_path_, tr("Paste a file path, or drop a file on this window"));
            if (button(r, ui, x + w - 80, py + 12, 80, 28, tr("Load"))) take_icon();
        }
        SDL_SetRenderClipRect(r, nullptr);
    }

    void frame() {
        SDL_Renderer* r = win.ren();
        Ui& ui = win.ui;
        tip_.clear();
        const bool modal = picker_.active() || naming_;
        const bool real_clicked = ui.clicked;
        if (modal) ui.clicked = false;
        toolbar();
        const float list_w = std::min(380.f, win.w * 0.3f);
        list_panel(14, 44, list_w, win.h - 44 - 56);
        edit_panel(14 + list_w + 16, 48, win.w - list_w - 44, win.h - 48 - 56);
        fill_rect(r, 0, win.h - 50, win.w, 50, {16, 17, 24, 255});
        text(r, 14, win.h - 42, fit_text(tip_, win.w - 28), {230, 230, 200});
        text(r, 14, win.h - 24, fit_text(status_.empty() ? tr("The official game never changes: items live in a game version. Players who join that version get them.") : status_, win.w - 28), {170, 240, 180});
        if (naming_) {
            ui.clicked = real_clicked;
            const float dw = 560, dx = (win.w - dw) / 2, dy = win.h / 2 - 90;
            fill_rect(r, 0, 0, win.w, win.h, {0, 0, 0, 170});
            fill_rect(r, dx, dy, dw, 170, {30, 32, 44, 255});
            rect(r, dx, dy, dw, 170, {255, 190, 110, 255});
            text(r, dx + 16, dy + 12, new_item_ ? tr("Name of the new item") : tr("Name of the new game version"), {255, 220, 150}, 2);
            text_field(r, ui, 98, dx + 16, dy + 62, dw - 32, naming_text_, tr("Name"));
            if (button(r, ui, dx + 16, dy + 110, 200, 36, tr("OK")) || ui.enter) {
                naming_ = false;
                ui.focus = -1;
                if (new_item_) {
                    std::string nm = naming_text_;
                    std::erase(nm, '.');
                    if (doc_.name.empty()) {
                        pending_item_ = nm;
                        naming_ = true, new_item_ = false;
                        naming_text_ = "my-items";
                        ui.focus = 98;
                    } else if (!nm.empty()) {
                        item_ = nm;
                        set("cost", "500");
                    }
                } else {
                    doc_.name = makers::version_folder_name(naming_text_);
                    doc_.others.clear(), doc_.lines.clear();
                    if (fs::exists(doc_.file())) doc_.load(doc_.name);
                    else doc_.others = {"base = official-0.1"};
                    dirty_ = true;
                    if (!pending_item_.empty()) {
                        item_ = pending_item_;
                        pending_item_.clear();
                        set("cost", "500");
                    }
                    rebuild();
                }
            }
            if (button(r, ui, dx + 226, dy + 110, 140, 36, tr("Close"))) naming_ = false, ui.focus = -1;
        } else if (picker_.active()) {
            ui.clicked = real_clicked;
            picker_.draw(win);
        }
    }
    std::string pending_item_;
};

}  // namespace

namespace em::makers {

int item_main(int, char**) {
    tool::go_to_game_folder();
    loc::load(tool::language_of_game());
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "SDL_Init: {}", SDL_GetError());
        return 1;
    }
    {
        auto maker = std::make_unique<ItemMaker>();
        if (!maker->init()) {
            SDL_Quit();
            return 1;
        }
        maker->run();
    }
    SDL_Quit();
    return 0;
}

tool::ToolExit item_embedded(SDL_Window* win, SDL_Renderer* ren) {
    auto maker = std::make_unique<ItemMaker>();
    maker->win.attach(win, ren);
    if (!maker->init()) return {};
    maker->run();
    return maker->win.exit;
}

}  // namespace em::makers
