// [U-95][U-100][U-101][U-208][U-210][U-212][U-220] Main menu.
#include <format>
#include <random>

#include "game/versions.hpp"
#include "client/app.hpp"
#include "game/match/maps.hpp"
#include "client/locale.hpp"
#include "client/tool_shell.hpp"

namespace em {

using namespace gfx;

void App::menu() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    if (!version_offer_.empty()) {  // [U-374] agree or stay out
        const float dw = 640, dx = (float(w) - dw) / 2, dy = float(h) / 2 - 90;
        fill_rect(ren_, dx, dy, dw, 180, {30, 32, 44, 255});
        rect(ren_, dx, dy, dw, 180, {255, 190, 110, 255});
        text(ren_, dx + 16, dy + 14, fit_text(trf("This server plays the game version {}", version_offer_), dw - 32, 2), {255, 220, 150}, 2);
        text(ren_, dx + 16, dy + 52, fit_text(tr("Download it and join? It is saved as a new version with the name of the server, a number and the date."), dw - 32), {220, 220, 230});
        if (button(ren_, ui_, dx + 16, dy + 110, 300, 44, tr("Download and join"))) {
            version_offer_.clear();
            net_.fetch_version = true;
            connect_to(last_address_);
            return;
        }
        if (button(ren_, ui_, dx + 330, dy + 110, 200, 44, tr("Stay out"))) version_offer_.clear();
        return;
    }
    const float cx = float(w) / 2;
    text(ren_, cx - text_w("ELEMENTAL MOBA", 5) / 2, 50, "ELEMENTAL MOBA", {255, 170, 70}, 5);
    const std::string sub = std::format("{} — {} {} ({})", tr("placeholder build"), tr("version"), set_.version, version_id(set_.version));  // [U-375]
    text(ren_, cx - text_w(sub, 1.5f) / 2, 100, sub, {170, 170, 190}, 1.5f);

    // [U-530] three equal columns of five buttons, the same gaps everywhere; a row of three under them
    const float gap = 10, col_gap = 24, bh = 44;
    const float bw = std::clamp((float(w) - 80 - 2 * col_gap) / 3, 240.f, 380.f);
    const float x0 = cx - (3 * bw + 2 * col_gap) / 2, x1 = x0 + bw + col_gap, x2 = x1 + bw + col_gap;
    const float head_y = std::max(130.f, (float(h) - (28 + 6 * bh + 5 * gap + 2 * gap + 40)) / 2 + 40);
    for (auto [hx, title] : {std::pair{x0, tr("Play")}, std::pair{x1, tr("Collection")}, std::pair{x2, tr("Create")}})
        text(ren_, hx + bw / 2 - text_w(title, 2) / 2, head_y, title, {255, 220, 140}, 2);
    const float y0 = head_y + 34;
    auto row = [&](int i) { return y0 + float(i) * (bh + gap); };
    // ---- play
    if (button(ren_, ui_, x0, row(0), bw, bh, tr("Single player (practice)"))) {  // [U-100]
        net::ServerConfig c;
        c.version = set_.version;  // [U-95]
        c.port = std::uint16_t(7790 + std::random_device{}() % 200);
        c.practice = true;
        c.ruleset = std::uint8_t(set_.ruleset);  // [U-416] Options → Game
        c.map = std::clamp(set_.map, 0, int(maps().size()) - 1);  // [U-420]
        c.loopback_only = true;
        c.host_secret = secret_;
        std::string err;
        if (start_server(c, &err)) connect_local(c.port);
        else status_ = err;
    }
    if (button(ren_, ui_, x0, row(1), bw, bh, tr("Multiplayer"))) back_ = Screen::Menu, go(Screen::Multiplayer);  // [U-101][U-212]
    if (button(ren_, ui_, x0, row(2), bw, bh, tr("Host a match"))) back_ = Screen::Menu, go(Screen::HostMatch);   // [U-102]
    if (button(ren_, ui_, x0, row(3), bw, bh, tr("Host a server"))) back_ = Screen::Menu, go(Screen::HostServer); // [U-103]
    if (button(ren_, ui_, x0, row(4), bw, bh, tr("Replays"))) back_ = Screen::Menu, go(Screen::Replays);          // [X-19]
    // ---- collection
    if (button(ren_, ui_, x1, row(0), bw, bh, tr("Champions (all numbers)"))) back_ = Screen::Menu, go(Screen::ChampionInfo);  // [U-187]
    if (button(ren_, ui_, x1, row(1), bw, bh, tr("Items and builds"))) back_ = Screen::Menu, go(Screen::Items);  // [U-220]
    if (button(ren_, ui_, x1, row(2), bw, bh, tr("Loadout setups"))) back_ = Screen::Menu, go(Screen::Setups);  // [U-178]
    if (button(ren_, ui_, x1, row(3), bw, bh, tr("Own champions"))) back_ = Screen::Menu, go(Screen::CustomChampions);  // [U-210]
    if (button(ren_, ui_, x1, row(4), bw, bh, tr("Interactions (every rule)"))) back_ = Screen::Menu, interactions_scroll_ = 0, go(Screen::Interactions);  // [U-397]
    // ---- create: they open inside this window [U-529]
    if (button(ren_, ui_, x2, row(0), bw, bh, tr("Champion maker"))) open_maker_ = 1;  // [U-510]
    if (button(ren_, ui_, x2, row(1), bw, bh, tr("Map maker"))) open_maker_ = 2;       // [U-518]
    if (button(ren_, ui_, x2, row(2), bw, bh, tr("Item maker"))) open_maker_ = 3;      // [U-535]
    if (button(ren_, ui_, x2, row(3), bw, bh, tr("Loadout maker"))) open_maker_ = 4;   // [U-535]
    if (button(ren_, ui_, x2, row(4), bw, bh, tr("Server app"))) {                     // [U-511] its own program: it keeps running without the game
        std::string err;
        if (!tool::open_program("em_server", &err)) status_ = err;
    }
    // ---- the row under the columns
    const float by = row(5) + gap;
    if (button(ren_, ui_, x0, by, bw, bh, tr("Options"))) back_ = Screen::Menu, go(Screen::Options);          // [U-208]
    // [U-95] version: official or an own version (versions/<name>/version.txt); a click = the next one
    if (const auto all = list_versions();
        button(ren_, ui_, x1, by, bw, bh, fit_text(std::format("{}: {}", tr("Version"), set_.version), bw - 16)) && all.size() > 1) {  // only official: nothing, no message
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
    if (button(ren_, ui_, x2, by, bw, bh, tr("Quit"))) quit_ = true;
    const float ly = by + bh, ry = ly;
    if (!status_.empty()) text(ren_, cx - text_w(status_, 1.5f) / 2, std::max(ly, ry) + 14, status_, {255, 160, 120}, 1.5f);
}

}  // namespace em
