// [U-178][U-187][U-210][U-220] Collection: items + own builds, loadout setups, champion wiki, own champions.
#include <algorithm>
#include <filesystem>
#include <format>

#include "client/app.hpp"
#include "game/reference/interactions.hpp"
#include "client/custom_champions.hpp"
#include "client/item_browser.hpp"
#include "client/locale.hpp"
#include "client/skill_text.hpp"

namespace em {

using namespace gfx;

// ---------------------------------------------------------------- items and own builds [U-220]
void App::items_screen() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float W = float(w), H = float(h);
    text(ren_, 20, 14, tr("Items and builds"), {255, 200, 120}, 3);
    Build* b = build_selected_ >= 0 && build_selected_ < int(set_.builds.size()) ? &set_.builds[std::size_t(build_selected_)] : nullptr;
    const int added = shop_.draw(ren_, ui_, 20, 56, W - 380, H - 120, b ? tr("Add to the build") : "", [&](int) {
        return b && std::ranges::find(b->items, std::int16_t(-1)) != b->items.end();
    });
    if (b && added >= 0)
        if (auto it = std::ranges::find(b->items, std::int16_t(-1)); it != b->items.end()) *it = std::int16_t(added), set_.save();
    // builds list
    const float bx = W - 350;
    text(ren_, bx, 60, tr("My builds"), {255, 220, 140}, 2);
    float y = 90;
    for (int i = 0; i < int(set_.builds.size()); ++i, y += 30)
        if (button(ren_, ui_, bx, y, 330, 26, set_.builds[std::size_t(i)].name, build_selected_ == i)) build_selected_ = i;
    if (button(ren_, ui_, bx, y, 160, 26, tr("New build"))) {
        set_.builds.push_back({std::format("{} {}", tr("Build"), set_.builds.size() + 1), "", {}});
        set_.builds.back().items.fill(-1);
        build_selected_ = int(set_.builds.size()) - 1;
        set_.save();
    }
    y += 40;
    if (b) {
        if (text_field(ren_, ui_, 80, bx, y + 12, 330, b->name, tr("Name"))) set_.save();
        y += 52;
        // champion of the build ("" = any)
        text(ren_, bx, y, std::format("{}: {}", tr("Champion"), b->champ.empty() ? tr("any") : b->champ), {200, 200, 210});
        y += 16;
        for (int c = 0; c < int(champions().size()); ++c) {
            if (button(ren_, ui_, bx + float(c % 2) * 166, y + float(c / 2) * 24, 160, 22, champions()[std::size_t(c)].name,
                       b->champ == champions()[std::size_t(c)].name))
                b->champ = b->champ == champions()[std::size_t(c)].name ? "" : std::string(champions()[std::size_t(c)].name), set_.save();
        }
        y += float((champions().size() + 1) / 2) * 24 + 10;
        int total = 0;
        for (int i = 0; i < 8; ++i) {
            const float ix = bx + float(i % 4) * 82, iy = y + float(i / 4) * 64;
            fill_rect(ren_, ix, iy, 56, 56, {30, 30, 40, 255});
            if (b->items[std::size_t(i)] >= 0) {
                draw_item_icon(ren_, b->items[std::size_t(i)], ix, iy, 56);
                total += kItems[std::size_t(b->items[std::size_t(i)])].cost;
                if (hover(ui_, ix, iy, 56, 56) && ui_.rclicked) b->items[std::size_t(i)] = -1, ui_.rclicked = false, set_.save();
            }
        }
        y += 132;
        text(ren_, bx, y, std::format("{}: {}   ({})", tr("Total"), total, tr("right click removes")), {255, 215, 80});
        y += 22;
        if (button(ren_, ui_, bx, y, 160, 26, tr("Delete build"))) {
            set_.builds.erase(set_.builds.begin() + build_selected_);
            build_selected_ = -1;
            set_.save();
        }
    }
    if (button(ren_, ui_, 20, H - 52, 200, 36, tr("Back"))) go(Screen::Menu);
}

// ---------------------------------------------------------------- loadout setups [U-178]
void App::setups_screen() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float H = float(h);
    text(ren_, 20, 14, tr("Setups — runes, masteries, keystone, build, levelling"), {255, 200, 120}, 2.5f);
    float y = 60;
    for (int i = 0; i < int(set_.setups.size()); ++i, y += 30)
        if (button(ren_, ui_, 20, y, 300, 26, set_.setups[std::size_t(i)].name, setup_selected_ == i)) setup_selected_ = i;
    if (button(ren_, ui_, 20, y, 300, 26, tr("New setup"))) {
        set_.setups.push_back({std::format("{} {}", tr("Setup"), set_.setups.size() + 1), {}});
        setup_selected_ = int(set_.setups.size()) - 1;
        set_.save();
    }
    if (setup_selected_ >= 0 && setup_selected_ < int(set_.setups.size())) {
        SavedSetup& s = set_.setups[std::size_t(setup_selected_)];
        if (text_field(ren_, ui_, 81, 360, 72, 340, s.name, tr("Name"))) set_.save();
        setup_editor(360, 110, float(w) - 380, H - 110 - 70, s);  // [U-318][U-319][U-323][U-324]
        if (button(ren_, ui_, 360, H - 52, 200, 30, tr("Delete setup"))) {
            set_.setups.erase(set_.setups.begin() + setup_selected_);
            setup_selected_ = 0;
            set_.save();
        }
    }
    if (button(ren_, ui_, 20, H - 52, 200, 36, tr("Back"))) go(Screen::Menu);
}

// ---------------------------------------------------------------- champion wiki [U-187]
void App::champion_info() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float W = float(w), H = float(h);
    text(ren_, 20, 14, tr("Champions — every number"), {255, 200, 120}, 2.5f);
    for (int c = 0; c < int(champions().size()); ++c)
        if (button(ren_, ui_, 20, 56 + float(c) * 30, 200, 26, champions()[std::size_t(c)].name, info_champ_ == c)) info_champ_ = c;
    const ChampionDef& c = champions()[std::size_t(info_champ_)];
    view_->draw_icon(info_champ_, 240, 56, 72, false);
    text(ren_, 324, 60, std::format("{} — {} · {}", c.name, tr(role_name(c.role)), tr(element_name(c.element))), hex(c.colour), 2);
    float y = 92;
    for (const std::string& l : champion_lines(c)) text(ren_, 324, y, l, {225, 225, 235}), y += 13;
    y = std::max(y, 140.f) + 10;
    const float col2 = W / 2 + 40;
    float y2 = 60;
    auto block = [&](float x, float& yy, const SkillDef& s, std::string_view key) {
        bool first = true;
        for (const std::string& l : skill_lines(s)) {
            text(ren_, x, yy, first ? std::format("[{}] {}", key, l) : "    " + l, first ? Rgba{255, 230, 150} : Rgba{215, 215, 225});
            yy += 13;
            first = false;
        }
        yy += 6;
    };
    block(240, y, c.p, "P");
    block(240, y, c.w, "W");
    block(240, y, c.e, "E");
    block(col2, y2, c.r, "R");
    block(col2, y2, c.t, "T");
    block(col2, y2, c.y, "Y");  // [U-289]
    text(ren_, col2, y2, tr("Q options of this element:"), {255, 220, 140});
    y2 += 15;
    for (int q = 0; q < int(q_options().size()); ++q)
        if (q_allowed(info_champ_, q)) block(col2, y2, q_options()[std::size_t(q)], "Q");
    if (y2 < H - 200) {
        text(ren_, col2, y2, tr("Special skills (F / G) of this element:"), {255, 220, 140});
        y2 += 15;
        for (int s = 0; s < int(specials().size()); ++s)
            if (special_allowed(s, c) && y2 < H - 70) text(ren_, col2, y2, std::format("{} — {}", specials()[std::size_t(s)].name, specials()[std::size_t(s)].desc), {200, 200, 215}), y2 += 13;
    }
    if (button(ren_, ui_, 20, H - 52, 200, 36, tr("Back"))) go(Screen::Menu);
}

// ---------------------------------------------------------------- own champions [U-210]
void App::custom_champions() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float H = float(h);
    text(ren_, 20, 14, tr("Own champions"), {255, 200, 120}, 3);
    text(ren_, 20, 52, tr("Files in custom_champions/*.champ. Picked ones join the practice roster; with ANY error a file is ignored."), {170, 170, 190});
    float y = 80;
    // [U-353] the files are read once a second (not every frame); a change applies AT ONCE (no restart)
    if (now_s_ - custom_scan_t_ > 1.0) custom_scan_ = scan_custom_champions(), custom_scan_t_ = now_s_;
    // never while a server of this game runs (it shares the roster) or while I am connected to one (champion numbers)
    const bool can_change = !server_ && net_.state == NetClient::State::Idle;
    for (const CustomChampionFile& f : custom_scan_) {
        const bool on = std::ranges::find(set_.custom_champions, f.file) != set_.custom_champions.end();
        if (button(ren_, ui_, 20, y, 300, 26, std::format("{}  {}", f.ok ? "" : "[!]", f.file), on, f.ok && can_change)) {
            if (on) std::erase(set_.custom_champions, f.file);
            else set_.custom_champions.push_back(f.file);
            set_.save();
            reset_custom_champions();
            load_custom_champions(set_.custom_champions);
        }
        text(ren_, 330, y + 8, f.ok ? std::format("{} — {}", f.name, tr("valid")) : f.error, f.ok ? Rgba{140, 230, 140} : Rgba{255, 120, 120});
        y += 30;
    }
    if (y == 80) text(ren_, 20, y, tr("No files yet. Copy custom_champions/example.champ and change it."), {200, 200, 210});
    if (!can_change) text(ren_, 20, H - 90, tr("Stop the running server to change own champions."), {255, 180, 120});
    if (button(ren_, ui_, 20, H - 52, 200, 36, tr("Back"))) go(Screen::Menu);
}

}  // namespace em

namespace em {
// [U-397] every interaction of the systems, generated from the game's own tables (game/reference/interactions.cpp)
void App::interactions() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float W = float(w), H = float(h);
    static const std::vector<std::string> lines = interaction_lines();
    text(ren_, 20, 14, tr("Interactions"), {255, 200, 120}, 3);
    text(ren_, 20, 52, tr("Every rule of the systems, straight from the game's tables. Mouse wheel = scroll."), {170, 170, 190});
    const float top = 80, bottom = H - 64, row = 18;
    interactions_scroll_ = std::clamp(interactions_scroll_ - ui_.wheel * 60, 0.f, std::max(0.f, float(lines.size()) * row - (bottom - top)));
    const SDL_Rect clip{0, int(top), int(W), int(bottom - top)};
    SDL_SetRenderClipRect(ren_, &clip);
    float y = top - interactions_scroll_;
    for (const std::string& ln : lines) {
        if (y > top - row && y < bottom) {
            if (ln.starts_with("## ")) text(ren_, 20, y + 2, ln.substr(3), {255, 210, 120}, 1.2f);
            else text(ren_, 36, y, fit_text(ln, W - 60), {225, 225, 235});
        }
        y += ln.starts_with("## ") ? row + 8 : row;
    }
    SDL_SetRenderClipRect(ren_, nullptr);
    if (button(ren_, ui_, 20, H - 52, 200, 36, tr("Back"))) go(Screen::Menu);
}
}  // namespace em
