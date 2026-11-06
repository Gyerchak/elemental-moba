// [U-546] The unit maker: minions, structures and monsters set like champions: their numbers, their body and skills they cast by themselves.
// They are bots that use the game's mechanics. It lives in a game version (unit.<name>.<key> = value), so it reaches everyone who joins it.
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
#include "game/content.hpp"
#include "game/mobs/unit_tweaks.hpp"
#include "game/versions.hpp"

using namespace em;
using namespace em::gfx;
using loc::tr;
using loc::trf;
namespace fs = std::filesystem;

namespace {

struct Row { const char* key; const char* label; int UnitTweak::*field; };
constexpr Row kRows[] = {
    {"hp", "Health %", &UnitTweak::hp}, {"attack", "Attack %", &UnitTweak::attack}, {"defence", "Defence %", &UnitTweak::defence},
    {"elemental_defence", "Elemental defence %", &UnitTweak::elemental_defence}, {"move_speed", "Move speed %", &UnitTweak::move_speed},
    {"attack_speed", "Attack speed %", &UnitTweak::attack_speed}, {"attack_range", "Attack range %", &UnitTweak::attack_range}, {"size", "Model and hitbox size %", &UnitTweak::size},
};

class UnitMaker {
public:
    tool::Window win;
    bool init() {
        if (!win.embedded() && !win.open("Elemental MOBA - Unit maker", 1400, 860)) return false;
        doc_.prefix = "unit.";
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
    std::string chosen_, status_, tip_, naming_text_;
    bool dirty_ = false, naming_ = false;
    float scroll_ = 0;

    void rebuild() {
        const std::string e = !doc_.name.empty() && fs::exists(doc_.file()) ? apply_version(doc_.name) : apply_version(kOfficialVersion);
        if (!e.empty()) status_ = e;
        unit_tweaks().clear();
        for (const makers::VersionDoc::Line& l : doc_.lines) {
            const auto d2 = l.key.rfind('.');
            if (d2 == std::string::npos || d2 <= 5) continue;
            if (const std::string er = apply_unit_line(l.key.substr(5, d2 - 5), l.key.substr(d2 + 1), l.value); !er.empty()) status_ = er;
        }
    }
    void put(const std::string& key, const std::string& value) {
        if (doc_.name.empty()) {
            naming_ = true;
            naming_text_ = "my-units";
            win.ui.focus = 98;
            return;
        }
        doc_.set("unit." + chosen_ + "." + key, value);
        dirty_ = true;
        rebuild();
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
    void choose_version() {
        std::vector<makers::Pick> items;
        items.push_back({tr("New version"), "+", tr("A copy of the official game with your changes")});
        for (const std::string& v : list_versions())
            if (v != kOfficialVersion) items.push_back({v, v, version_description(v)});
        picker_.open(win, tr("Which game version do these belong to"), std::move(items), [this](const std::string& v) {
            if (v == "+") {
                naming_ = true;
                naming_text_ = "my-units";
                win.ui.focus = 98;
            } else {
                doc_.load(v);
                dirty_ = false;
                rebuild();
            }
        });
    }
    void pick_skill(int slot) {
        std::vector<makers::Pick> items{{tr("(none)"), "", ""}};
        std::vector<std::string> seen;
        auto add = [&](const SkillDef& s, std::string_view from) {
            if (s.name.empty() || s.kind == SkillKind::Passive || s.kind == SkillKind::None || std::ranges::find(seen, std::string(s.name)) != seen.end()) return;
            seen.emplace_back(s.name);
            items.push_back({std::string(s.name), std::string(s.name), std::format("{} - {}", from, tr(s.desc))});
        };
        for (const ChampionDef& c : champions())
            for (const SkillDef* s : {&c.w, &c.e, &c.r, &c.t, &c.y}) add(*s, c.name);
        for (const SkillDef& s : q_options()) add(s, "Q");
        for (const SkillDef& s : specials()) add(s, "F / G");
        picker_.open(win, tr("A skill it casts by itself"), std::move(items), [this, slot](const std::string& v) { put(std::format("skill{}", slot + 1), v); });
    }

    void frame() {
        SDL_Renderer* r = win.ren();
        Ui& ui = win.ui;
        tip_.clear();
        const bool modal = picker_.active() || naming_;
        const bool real_clicked = ui.clicked;
        if (modal) ui.clicked = false;
        text(r, 14, 12, tr("Unit maker"), {255, 200, 120}, 2);
        float tx = 14 + text_w(tr("Unit maker"), 2) + 16;
        if (button(r, ui, tx, 8, 320, 28, fit_text(std::format("{}: {}", tr("Game version"), doc_.name.empty() ? tr("(choose one)") : doc_.name), 308), doc_.name.empty())) choose_version();
        tx += 324;
        if (button(r, ui, tx, 8, 110, 28, dirty_ ? tr("Save *") : tr("Save"), dirty_)) save();
        tx += 114;
        const bool can_use = !doc_.name.empty();
        if (can_use && hover(ui, tx, 8, 170, 28)) tip_ = tr("Practice and hosting then run this version; players who join get it");
        if (button(r, ui, tx, 8, 170, 28, tr("Use it in the game"), false, can_use) && can_use) {
            save();
            win.exit.use_version = doc_.name;
            if (!win.embedded()) status_ = trf("Choose the version {} in the menu of the game", doc_.name);
            else win.leave();
        }
        if (button(r, ui, win.w - 120, 8, 110, 28, std::format("{}: {}", tr("Language"), loc::current()))) loc::load(tool::next_language(loc::current()));
        // ---- the list
        const float lw = std::min(380.f, win.w * 0.3f), top = 48, bottom = win.h - 56;
        const SDL_Rect clip{14, int(top), int(lw), int(bottom - top)};
        SDL_SetRenderClipRect(r, &clip);
        if (hover(ui, 14, top, lw, bottom - top)) scroll_ = std::max(0.f, scroll_ - ui.wheel * 44);
        float ly = top - scroll_;
        for (const std::string& n : tweakable_units()) {
            if (ly > top - 30 && ly < bottom) {
                const std::string label = std::format("{}{}", tweak_of(n) ? "* " : "", tr(n));
                if (button(r, ui, 14, ly, lw - 4, 26, fit_text(label, lw - 16), chosen_ == n) && hover(ui, 14, top, lw, bottom - top)) chosen_ = n;
            }
            ly += 30;
        }
        SDL_SetRenderClipRect(r, nullptr);
        // ---- the chosen one
        const float x = 14 + lw + 16, w = win.w - x - 14;
        float y = top;
        if (chosen_.empty()) {
            text(r, x, y + 6, fit_text(tr("Choose a unit on the left."), w), {200, 200, 215});
        } else {
            static const UnitTweak kUsual{};
            const UnitTweak* t = tweak_of(chosen_);
            const UnitTweak& v = t ? *t : kUsual;
            text(r, x, y, fit_text(tr(chosen_), w, 2), {255, 220, 150}, 2);
            y += 32;
            int id = 100;
            for (const Row& row : kRows) {
                int n = v.*row.field;
                const int was = n;
                if (slider(r, ui, id++, x, y, w, n, 10, 500, tr(row.label)) && n != was) put(row.key, std::to_string(n));
                y += 28;
            }
            y += 6;
            const std::string body = !v.body ? "Usual" : v.body == &kHumanoidBody ? "Humanoid" : v.body == &kSlimBody ? "Slim" : v.body == &kBroadBody ? "Broad" : "Beast";
            if (button(r, ui, x, y, w, 26, std::format("{}: {}", tr("Hitbox of the body"), tr(body)))) {
                std::vector<makers::Pick> items;
                for (const char* n : {"Usual", "Humanoid", "Slim", "Broad", "Beast"}) items.push_back({tr(n), n, ""});
                picker_.open(win, tr("Hitbox of the body"), std::move(items), [this](const std::string& val) { put("body", val); });
            }
            y += 38;
            text(r, x, y, tr("Skills it casts by itself"), {255, 220, 150}, 2);
            y += 28;
            for (int i = 0; i < 3; ++i) {
                const SkillDef* s = v.skills[std::size_t(i)];
                if (button(r, ui, x, y, w, 28, s ? std::string(s->name) : tr("(none)"))) pick_skill(i);
                y += 32;
            }
            y += 6;
            text(r, x, y, fit_text(tr("It casts what is ready at the nearest enemy it can reach, without paying for it. A monster casts only at who it fights."), w), {170, 170, 190});
            y += 26;
            if (t && button(r, ui, x, y, 240, 28, tr("Back to the usual"))) {
                std::erase_if(doc_.lines, [&](const auto& l) { return l.key.starts_with("unit." + chosen_ + "."); });
                dirty_ = true;
                rebuild();
            }
        }
        fill_rect(r, 0, win.h - 50, win.w, 50, {16, 17, 24, 255});
        text(r, 14, win.h - 42, fit_text(tip_, win.w - 28), {230, 230, 200});
        text(r, 14, win.h - 24, fit_text(status_.empty() ? tr("Minions, structures and monsters are bots that use the game's mechanics. It lives in a game version; players who join it get it.") : status_, win.w - 28), {170, 240, 180});
        if (naming_) {
            ui.clicked = real_clicked;
            const float dw = 560, dx = (win.w - dw) / 2, dy = win.h / 2 - 90;
            fill_rect(r, 0, 0, win.w, win.h, {0, 0, 0, 170});
            fill_rect(r, dx, dy, dw, 170, {30, 32, 44, 255});
            rect(r, dx, dy, dw, 170, {255, 190, 110, 255});
            text(r, dx + 16, dy + 12, tr("Name of the new game version"), {255, 220, 150}, 2);
            text_field(r, ui, 98, dx + 16, dy + 62, dw - 32, naming_text_, tr("Name"));
            if (button(r, ui, dx + 16, dy + 110, 200, 36, tr("OK")) || ui.enter) {
                naming_ = false;
                ui.focus = -1;
                doc_.name = makers::version_folder_name(naming_text_);
                doc_.others.clear(), doc_.lines.clear();
                if (fs::exists(doc_.file())) doc_.load(doc_.name);
                else doc_.others = {"base = official-0.1"};
                dirty_ = true;
                rebuild();
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

int unit_main(int, char**) {
    tool::go_to_game_folder();
    loc::load(tool::language_of_game());
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "SDL_Init: {}", SDL_GetError());
        return 1;
    }
    {
        auto maker = std::make_unique<UnitMaker>();
        if (!maker->init()) {
            SDL_Quit();
            return 1;
        }
        maker->run();
    }
    SDL_Quit();
    return 0;
}

tool::ToolExit unit_embedded(SDL_Window* win, SDL_Renderer* ren) {
    auto maker = std::make_unique<UnitMaker>();
    maker->win.attach(win, ren);
    if (!maker->init()) return {};
    maker->run();
    return maker->win.exit;
}

}  // namespace em::makers
