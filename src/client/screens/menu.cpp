// [U-95][U-100][U-101][U-208][U-210][U-212][U-220] Main menu.
#include <format>
#include <random>

#include "game/versions.hpp"
#include "client/app.hpp"
#include "client/locale.hpp"

namespace em {

using namespace gfx;

void App::menu() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float cx = float(w) / 2;
    text(ren_, cx - text_w("ELEMENTAL MOBA", 5) / 2, 50, "ELEMENTAL MOBA", {255, 170, 70}, 5);
    const std::string sub = std::format("{} — {} {}", tr("placeholder build"), tr("version"), set_.version);
    text(ren_, cx - text_w(sub, 1.5f) / 2, 100, sub, {170, 170, 190}, 1.5f);

    const float bw = 380, bh = 40, gap = 8;
    float y = 140;
    // left column: play
    const float lx = cx - bw - 20, rx = cx + 20;
    text(ren_, lx, y, tr("Play"), {255, 220, 140}, 2);
    text(ren_, rx, y, tr("Collection"), {255, 220, 140}, 2);
    y += 30;
    float ly = y, ry = y;
    if (button(ren_, ui_, lx, ly, bw, bh, tr("Single player (practice)"))) {  // [U-100]
        net::ServerConfig c;
        c.version = set_.version;  // [U-95]
        c.port = std::uint16_t(7790 + std::random_device{}() % 200);
        c.practice = true;
        c.ruleset = std::uint8_t(set_.ruleset);  // [U-416] Options → Game
        c.loopback_only = true;
        c.host_secret = secret_;
        std::string err;
        if (start_server(c, &err)) connect_local(c.port);
        else status_ = err;
    }
    ly += bh + gap;
    if (button(ren_, ui_, lx, ly, bw, bh, tr("Multiplayer"))) back_ = Screen::Menu, go(Screen::Multiplayer);  // [U-101][U-212]
    ly += bh + gap;
    if (button(ren_, ui_, lx, ly, bw, bh, tr("Host a match"))) back_ = Screen::Menu, go(Screen::HostMatch);   // [U-102]
    ly += bh + gap;
    if (button(ren_, ui_, lx, ly, bw, bh, tr("Host a server"))) back_ = Screen::Menu, go(Screen::HostServer); // [U-103]
    ly += bh + gap;
    if (button(ren_, ui_, lx, ly, bw, bh, tr("Options"))) back_ = Screen::Menu, go(Screen::Options);          // [U-208]
    ly += bh + gap;
    if (button(ren_, ui_, lx, ly, bw, bh, tr("Quit"))) quit_ = true;
    // right column: collection
    if (button(ren_, ui_, rx, ry, bw, bh, tr("Items and builds"))) back_ = Screen::Menu, go(Screen::Items);  // [U-220]
    ry += bh + gap;
    if (button(ren_, ui_, rx, ry, bw, bh, tr("Loadout setups (runes, trees, keystones)"))) back_ = Screen::Menu, go(Screen::Setups);  // [U-178]
    ry += bh + gap;
    if (button(ren_, ui_, rx, ry, bw, bh, tr("Champions (all numbers)"))) back_ = Screen::Menu, go(Screen::ChampionInfo);  // [U-187]
    ry += bh + gap;
    if (button(ren_, ui_, rx, ry, bw, bh, tr("Own champions"))) back_ = Screen::Menu, go(Screen::CustomChampions);  // [U-210]
    ry += bh + gap;
    if (button(ren_, ui_, rx, ry, bw, bh, tr("Interactions (every rule)"))) back_ = Screen::Menu, interactions_scroll_ = 0, go(Screen::Interactions);  // [U-397]
    ry += bh + gap;
    // [U-95] version: official or an own version (versions/<name>/version.txt); a click = the next one
    if (const auto all = list_versions();
        button(ren_, ui_, rx, ry, bw, bh, std::format("{}: {}", tr("Version"), set_.version)) && all.size() > 1) {  // only official: nothing, no message
        auto it = std::ranges::find(all, set_.version);
        const std::string next = (it == all.end() || it + 1 == all.end()) ? all.front() : *(it + 1);
        int n = 0;
        if (const std::string e = apply_version(next, &n); e.empty()) {
            set_.version = next;
            set_.save();
            status_ = next == kOfficialVersion ? tr("official game") : std::format("{}: {} {} — {}", next, n, tr("changes"), version_description(next));
        } else {
            status_ = std::format("{}: {}", next, e);
            apply_version(set_.version);
        }
    }
    ry += bh + gap;
    if (!status_.empty()) text(ren_, cx - text_w(status_, 1.5f) / 2, std::max(ly, ry) + 30, status_, {255, 160, 120}, 1.5f);
}

}  // namespace em
