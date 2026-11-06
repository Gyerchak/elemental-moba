// [U-535] The loadout maker: what players can choose in their setups: keystones, trees and their nodes, shards, classic runes,
// quintessences, masteries. Changes and new ones live in a game version (versions/<name>/version.txt: setup.<kind>.<name>.<key> = …),
// so they reach everyone who joins that version. The official game is never changed. A new keystone gives a stat (its own rule needs code).
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
#include "core/stats.hpp"
#include "game/loadout/loadout.hpp"
#include "game/versions.hpp"

using namespace em;
using namespace em::gfx;
using loc::tr;
using loc::trf;
namespace fs = std::filesystem;

namespace {

struct Entry {
    std::string name, group, desc;
    StatMod mod{StatKey::Count, 0, false};
    int max_rank = 0;   // masteries
    bool has_stat = true, has_desc = false;
};

constexpr const char* kKinds[] = {"keystone", "tree", "node", "shard", "rune", "quint", "mastery"};
constexpr const char* kKindNames[] = {"Keystones", "Trees", "Tree nodes", "Shards", "Classic runes", "Quintessences", "Masteries"};

class LoadoutMaker {
public:
    tool::Window win;
    bool init() {
        if (!win.embedded() && !win.open("Elemental MOBA - Loadout maker", 1400, 860)) return false;
        doc_.prefix = "setup.";
        for (const std::string& v : list_versions())
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
    int kind_ = 0;
    std::string chosen_, status_, tip_, name_edit_, desc_edit_, naming_text_, pending_new_;
    bool dirty_ = false, naming_ = false, naming_new_ = false;
    float scroll_ = 0;

    void rebuild() {
        std::string e;
        if (!doc_.name.empty() && fs::exists(doc_.file())) e = apply_version(doc_.name);
        else e = apply_version(kOfficialVersion);
        if (!e.empty()) status_ = e;
        restore_setup();
        for (const makers::VersionDoc::Line& l : doc_.lines) {  // setup.<kind>.<name>.<key>, in order
            const auto d1 = l.key.find('.', 6), d2 = l.key.rfind('.');
            if (d1 == std::string::npos || d2 <= d1) continue;
            if (const std::string er = apply_setup_text(l.key.substr(6, d1 - 6), l.key.substr(d1 + 1, d2 - d1 - 1), l.key.substr(d2 + 1), l.value); !er.empty()) status_ = er;
        }
    }
    [[nodiscard]] std::vector<Entry> entries() const {
        std::vector<Entry> out;
        const std::string_view k = kKinds[kind_];
        if (k == "keystone") {
            for (const KeystoneDef& d : keystones())
                if (!d.name.empty()) out.push_back({std::string(d.name), "", std::string(d.desc), d.mod, 0, true, true});
        } else if (k == "tree") {
            for (const SpecTree& t : spec_trees())
                if (!t.name.empty()) out.push_back({std::string(t.name), "", std::string(t.theme), {StatKey::Count, 0, false}, 0, false, true});
        } else if (k == "node") {
            for (const SpecTree& t : spec_trees())
                if (!t.name.empty())
                    for (std::size_t i = 0; i < t.tiers.size(); ++i)
                        for (const SpecNode& n : t.tiers[i])
                            if (!n.name.empty()) out.push_back({std::string(n.name), std::format("{} {}", t.name, i + 1), "", n.mod});
        } else if (k == "shard") {
            for (std::size_t r = 0; r < shards().size(); ++r)
                for (const RuneDef& d : shards()[r])
                    if (!d.name.empty()) out.push_back({std::string(d.name), kRuneSlotNames[r], "", d.mod});
        } else if (k == "rune") {
            for (std::size_t r = 0; r < classic_runes().size(); ++r)
                for (const RuneDef& d : classic_runes()[r])
                    if (!d.name.empty()) out.push_back({std::string(d.name), kClassicSectionNames[r], "", d.mod});
        } else if (k == "quint") {
            for (const RuneDef& d : quints())
                if (!d.name.empty()) out.push_back({std::string(d.name), "", "", d.mod});
        } else {
            for (const MasteryTree& t : mastery_trees())
                for (std::size_t i = 0; i < t.tiers.size(); ++i)
                    for (const MasteryDef& m : t.tiers[i])
                        if (m.max_rank) out.push_back({std::string(m.name), std::format("{} {}", t.name, i + 1), "", m.mod, m.max_rank});
        }
        return out;
    }
    void put(const std::string& name, const std::string& key, const std::string& value) {
        if (doc_.name.empty()) {
            naming_ = true, naming_new_ = false;
            naming_text_ = "my-loadouts";
            win.ui.focus = 98;
            return;
        }
        doc_.set(std::format("setup.{}.{}.{}", kKinds[kind_], name, key), value);
        dirty_ = true;
        rebuild();
    }
    void choose_version() {
        std::vector<makers::Pick> items;
        items.push_back({tr("New version"), "+", tr("A copy of the official game with your changes")});
        for (const std::string& v : list_versions())
            if (v != kOfficialVersion) items.push_back({v, v, version_description(v)});
        picker_.open(win, tr("Which game version do these belong to"), std::move(items), [this](const std::string& v) {
            if (v == "+") {
                naming_ = true, naming_new_ = false;
                naming_text_ = "my-loadouts";
                win.ui.focus = 98;
            } else {
                doc_.load(v);
                dirty_ = false;
                rebuild();
            }
        });
    }
    void save() {
        if (doc_.name.empty()) return choose_version();
        std::string e;
        if (doc_.save(&e)) {
            dirty_ = false;
            status_ = trf("Saved in versions/{}: choose this version in the menu, or press Use it in the game", doc_.name);
        } else {
            status_ = e;
        }
        rebuild();
    }
    void add_new(const std::string& name, const std::string& place) {
        chosen_ = name;
        put(name, "place", place);
        if (std::string_view(kKinds[kind_]) != "tree") put(name, "stat", "elemental_power");
    }
    void ask_place() {  // where a new one goes
        const std::string_view k = kKinds[kind_];
        std::vector<makers::Pick> items;
        if (k == "keystone") items.push_back({tr("A new keystone"), "new", tr("It gives a stat")});
        else if (k == "tree" || k == "quint") items.push_back({tr("A new one"), "new", ""});
        else if (k == "shard")
            for (const char* n : kRuneSlotNames) items.push_back({n, n, ""});
        else if (k == "rune")
            for (const char* n : {"Marks", "Seals", "Glyphs"}) items.push_back({n, n, ""});
        else if (k == "node") {
            for (const SpecTree& t : spec_trees())
                if (!t.name.empty())
                    for (int tier = 1; tier <= 3; ++tier)
                        for (int side = 1; side <= 2; ++side) {
                            const std::string p = std::format("{} {} {}", t.name, tier, side);
                            items.push_back({p, p, ""});
                        }
        } else {
            for (const MasteryTree& t : mastery_trees())
                for (int tier = 1; tier <= kMasteryTiers; ++tier) {
                    const std::string p = std::format("{} {}", t.name, tier);
                    items.push_back({p, p, ""});
                }
        }
        const std::string nm = naming_text_;
        picker_.open(win, tr("Where does it go"), std::move(items), [this, nm](const std::string& v) { add_new(nm, v); });
    }

    void toolbar() {
        SDL_Renderer* r = win.ren();
        Ui& ui = win.ui;
        text(r, 14, 12, tr("Loadout maker"), {255, 200, 120}, 2);
        float x = 14 + text_w(tr("Loadout maker"), 2) + 16;
        if (button(r, ui, x, 8, 320, 28, fit_text(std::format("{}: {}", tr("Game version"), doc_.name.empty() ? tr("(choose one)") : doc_.name), 308), doc_.name.empty())) choose_version();
        x += 324;
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
        float tx = 14;
        const float tw = (win.w - 28) / float(std::size(kKinds));
        for (std::size_t i = 0; i < std::size(kKinds); ++i) {
            if (button(r, ui, tx, 44, tw - 4, 32, fit_text(tr(kKindNames[i]), tw - 14), kind_ == int(i))) kind_ = int(i), chosen_.clear(), scroll_ = 0;
            tx += tw;
        }
    }

    void frame() {
        SDL_Renderer* r = win.ren();
        Ui& ui = win.ui;
        tip_.clear();
        const bool modal = picker_.active() || naming_;
        const bool real_clicked = ui.clicked;
        if (modal) ui.clicked = false;
        toolbar();
        const std::vector<Entry> all = entries();
        const float lw = std::min(460.f, win.w * 0.36f), top = 84, bottom = win.h - 56;
        // ---- the list
        if (button(r, ui, 14, top, lw - 4, 28, tr("Add a new one"))) {
            naming_ = true, naming_new_ = true;
            naming_text_ = tr("New");
            ui.focus = 98;
        }
        const float lt = top + 34;
        const SDL_Rect clip{14, int(lt), int(lw), int(bottom - lt)};
        SDL_SetRenderClipRect(r, &clip);
        if (hover(ui, 14, lt, lw, bottom - lt)) scroll_ = std::max(0.f, scroll_ - ui.wheel * 44);
        float ly = lt - scroll_;
        for (const Entry& e : all) {
            if (ly > lt - 30 && ly < bottom) {
                const std::string label = e.group.empty() ? e.name : std::format("{}  ({})", e.name, e.group);
                if (button(r, ui, 14, ly, lw - 4, 26, fit_text(label, lw - 16), chosen_ == e.name) && hover(ui, 14, lt, lw, bottom - lt)) chosen_ = e.name, name_edit_ = e.name, desc_edit_ = e.desc;
            }
            ly += 30;
        }
        SDL_SetRenderClipRect(r, nullptr);
        // ---- the chosen one
        const float x = 14 + lw + 16, w = win.w - x - 14;
        float y = top;
        const auto it = std::ranges::find_if(all, [&](const Entry& e) { return e.name == chosen_; });
        if (it == all.end()) {
            text(r, x, y + 6, fit_text(tr("Choose one on the left, or add a new one."), w), {200, 200, 215});
        } else {
            const Entry& e = *it;
            text(r, x, y, fit_text(e.name, w, 2), {255, 220, 150}, 2);
            y += 34;
            const std::string before = name_edit_;
            text_field(r, ui, 1, x, y + 12, w - 110, name_edit_, tr("Name"));
            if (button(r, ui, x + w - 100, y + 12, 100, 28, tr("Rename"), false, name_edit_ != e.name && !name_edit_.empty()) && name_edit_ != e.name && !name_edit_.empty()) {
                std::erase(name_edit_, '.');
                put(e.name, "name", name_edit_);
                chosen_ = name_edit_;
            }
            (void)before;
            y += 54;
            if (e.has_desc) {
                text_field(r, ui, 2, x, y + 12, w - 110, desc_edit_, std::string_view(kKinds[kind_]) == "tree" ? tr("Theme") : tr("What it does (text)"));
                if (button(r, ui, x + w - 100, y + 12, 100, 28, tr("Set"))) put(e.name, std::string_view(kKinds[kind_]) == "tree" ? "theme" : "desc", desc_edit_);
                y += 54;
            }
            if (e.has_stat) {
                const std::string stat = e.mod.key == StatKey::Count ? tr("(none)") : std::string(kStatKeyNames[int(e.mod.key)]);
                if (button(r, ui, x, y, w, 26, std::format("{}: {}", tr("Stat it gives"), stat))) {
                    std::vector<makers::Pick> items;
                    for (int k = 0; k < int(StatKey::Count); ++k) items.push_back({std::string(kStatKeyNames[k]), std::string(kStatKeyNames[k]), ""});
                    const std::string nm = e.name;
                    picker_.open(win, tr("Stat it gives"), std::move(items), [this, nm](const std::string& v) { put(nm, "stat", v); });
                }
                y += 32;
                int v = e.mod.value / 100;
                const int was = v;
                if (slider(r, ui, 10, x, y, w, v, 0, 500, tr("How much")) && v != was) put(e.name, "value", std::to_string(v));
                y += 30;
                if (button(r, ui, x, y, w, 24, std::format("{}: {}", tr("A percentage of that stat"), e.mod.pct ? tr("ON") : tr("OFF")), e.mod.pct)) put(e.name, "pct", e.mod.pct ? "0" : "1");
                y += 32;
            }
            if (e.max_rank) {
                int v = e.max_rank;
                const int was = v;
                if (slider(r, ui, 11, x, y, w, v, 1, 9, tr("Ranks")) && v != was) put(e.name, "max_rank", std::to_string(v));
                y += 30;
            }
            if (button(r, ui, x, y + 10, 200, 30, tr("Remove it"))) {
                put(e.name, "removed", "1");
                chosen_.clear();
            }
        }
        fill_rect(r, 0, win.h - 50, win.w, 50, {16, 17, 24, 255});
        text(r, 14, win.h - 42, fit_text(tip_, win.w - 28), {230, 230, 200});
        text(r, 14, win.h - 24, fit_text(status_.empty() ? tr("What players can choose in their setups. It lives in a game version; players who join it get it. A new keystone gives a stat.") : status_, win.w - 28), {170, 240, 180});
        if (naming_) {
            ui.clicked = real_clicked;
            const float dw = 560, dx = (win.w - dw) / 2, dy = win.h / 2 - 90;
            fill_rect(r, 0, 0, win.w, win.h, {0, 0, 0, 170});
            fill_rect(r, dx, dy, dw, 170, {30, 32, 44, 255});
            rect(r, dx, dy, dw, 170, {255, 190, 110, 255});
            text(r, dx + 16, dy + 12, naming_new_ ? tr("Name of the new one") : tr("Name of the new game version"), {255, 220, 150}, 2);
            text_field(r, ui, 98, dx + 16, dy + 62, dw - 32, naming_text_, tr("Name"));
            if (button(r, ui, dx + 16, dy + 110, 200, 36, tr("OK")) || ui.enter) {
                naming_ = false;
                ui.focus = -1;
                if (naming_new_) {
                    std::erase(naming_text_, '.');
                    if (!naming_text_.empty()) {
                        if (doc_.name.empty()) {
                            pending_new_ = naming_text_;
                            naming_ = true, naming_new_ = false;
                            naming_text_ = "my-loadouts";
                            ui.focus = 98;
                        } else {
                            ask_place();
                        }
                    }
                } else {
                    doc_.name = makers::version_folder_name(naming_text_);
                    doc_.others.clear(), doc_.lines.clear();
                    if (fs::exists(doc_.file())) doc_.load(doc_.name);
                    else doc_.others = {"base = official-0.1"};
                    dirty_ = true;
                    rebuild();
                    if (!pending_new_.empty()) {
                        naming_text_ = pending_new_;
                        pending_new_.clear();
                        ask_place();
                    }
                }
            }
            if (button(r, ui, dx + 226, dy + 110, 140, 36, tr("Close"))) naming_ = false, ui.focus = -1;
        } else if (picker_.active()) {
            ui.clicked = real_clicked;
            picker_.draw(win);
        }
    }
};

}  // namespace

namespace em::makers {

int loadout_main(int, char**) {
    tool::go_to_game_folder();
    loc::load(tool::language_of_game());
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "SDL_Init: {}", SDL_GetError());
        return 1;
    }
    {
        auto maker = std::make_unique<LoadoutMaker>();
        if (!maker->init()) {
            SDL_Quit();
            return 1;
        }
        maker->run();
    }
    SDL_Quit();
    return 0;
}

tool::ToolExit loadout_embedded(SDL_Window* win, SDL_Renderer* ren) {
    auto maker = std::make_unique<LoadoutMaker>();
    maker->win.attach(win, ren);
    if (!maker->init()) return {};
    maker->run();
    return maker->win.exit;
}

}  // namespace em::makers
