// [U-511] em_server: only the server, without the game and without joining as a player.
//   em_server                 a small window with every server setting (they are kept in server.cfg)
//   em_server --nogui         no window at all, for a machine without a screen; uses server.cfg, these options override it:
//                             --port N  --map NAME  --version NAME  --tick N  --unranked  --language CODE
// The champion maker's output (custom_champions/*.champ) is sent to every player who joins.
#include <enet/enet.h>
#undef near  // (Windows headers define these words)
#undef far

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <format>
#include <print>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "client/locale.hpp"
#include "client/tool_shell.hpp"
#include "game/match/maps.hpp"
#include "game/versions.hpp"
#include "game/world.hpp"
#include "net/server.hpp"
#include "net/server_config.hpp"

using namespace em;
using namespace em::gfx;

namespace {

std::stop_source g_stop;
void on_signal(int) { g_stop.request_stop(); }

int run_console(net::ServerConfig cfg) {
    net::Server s(cfg);
    std::string err;
    if (!s.start(&err)) {
        std::println(stderr, "{}", err);
        return 1;
    }
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::println("{}", trf("Elemental MOBA server on UDP {} at {} Hz (Ctrl+C stops it)", cfg.port, cfg.tick_hz));
    std::jthread status([&](std::stop_token st) {
        while (!st.stop_requested() && !g_stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::println("{}", trf("players {}  {}  time {} s  tick load {:.1f}%", s.players.load(), s.in_match ? tr("in a match") : tr("lobby"),
                                   s.match_ms.load() / 1000, s.tick_load.load() * 100));
        }
    });
    s.run(g_stop.get_token());
    return 0;
}

struct Panel {
    net::ServerConfig cfg;
    std::string language;
    std::string port_s, tick_s, snap_s, wave_s, pick_s, queue_s, start_s;
    std::unique_ptr<net::Server> server;
    std::jthread thread;
    std::string status;
    float scroll = 0;
    std::vector<std::string> versions;

    void load_text() {
        port_s = std::to_string(cfg.port), tick_s = std::to_string(cfg.tick_hz), snap_s = std::to_string(cfg.snap_div);
        wave_s = std::to_string(cfg.first_wave_ms / 1000), pick_s = std::to_string(cfg.pick_seconds), queue_s = std::to_string(cfg.queue_min);
        start_s = std::to_string(cfg.start_ms / 60'000);
    }
    void apply_text() {
        cfg.port = std::uint16_t(std::clamp(std::atoi(port_s.c_str()), 1024, 65'535));
        cfg.tick_hz = std::uint32_t(std::clamp(std::atoi(tick_s.c_str()), 16, 256));
        cfg.snap_div = std::clamp(std::atoi(snap_s.c_str()), 1, 16);
        cfg.first_wave_ms = std::int64_t(std::clamp(std::atoi(wave_s.c_str()), 0, 600)) * 1000;
        cfg.pick_seconds = std::clamp(std::atoi(pick_s.c_str()), 0, 600);
        cfg.queue_min = std::clamp(std::atoi(queue_s.c_str()), 1, 10);
        cfg.start_ms = std::int64_t(std::clamp(std::atoi(start_s.c_str()), 0, 600)) * 60'000;
    }
    void stop() {
        if (thread.joinable()) {
            thread.request_stop();
            thread.join();
        }
        server.reset();
    }
    void start() {
        apply_text();
        save_server_cfg(cfg, language);
        stop();
        server = std::make_unique<net::Server>(cfg);
        std::string err;
        if (!server->start(&err)) {
            status = err;
            server.reset();
            return;
        }
        status = tr("Server running");
        thread = std::jthread([this](std::stop_token st) { server->run(st); });
    }
};

void draw_panel(tool::Window& win, Panel& p) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    const float W = win.w, H = win.h;
    const float x = 24, colw = std::min(W - 48, 900.f);
    float y = 16;
    text(r, x, y, tr("Elemental MOBA server"), {255, 200, 120}, 3);
    if (button(r, ui, W - 150, 14, 126, 28, std::format("{}: {}", tr("Language"), p.language))) {
        p.language = tool::next_language(p.language);
        loc::load(p.language);
        p.apply_text();
        save_server_cfg(p.cfg, p.language);
    }
    if (button(r, ui, W - 340, 14, 180, 28, tr("Champion maker"))) {
        std::string err;
        if (!tool::open_program("em_champion_maker", &err)) p.status = err;
    }
    y += 40;
    // ---- running or not, always on top
    const bool on = p.server != nullptr;
    fill_rect(r, x, y, colw, 78, {30, 34, 46, 255});
    rect(r, x, y, colw, 78, on ? Rgba{120, 220, 130, 255} : Rgba{110, 110, 140, 255});
    if (on) {
        text(r, x + 12, y + 10, std::format("{} {}   {} {}   {}", tr("port"), p.server->port(), tr("players"), p.server->players.load(),
                                            p.server->in_match ? tr("in a match") : tr("lobby")), {160, 240, 170}, 2);
        text(r, x + 12, y + 34, std::format("{} {} s   {} {:.0f}%", tr("match time"), p.server->match_ms.load() / 1000, tr("tick load"),
                                            p.server->tick_load.load() * 100), {200, 200, 200});
    } else {
        text(r, x + 12, y + 12, tr("The server is stopped"), {230, 200, 170}, 2);
    }
    const std::string addr = std::format("{}:{}", tool::lan_address(), p.port_s);
    text(r, x + 12, y + 56, std::format("{}: {}", tr("Your LAN address"), addr), {230, 225, 170});
    if (button(r, ui, x + 420, y + 50, 70, 22, tr("Copy"))) {
        SDL_SetClipboardText(addr.c_str());
        p.status = std::format("{} {}", tr("copied:"), addr);
    }
    if (button(r, ui, x + colw - 200, y + 10, 188, 56, on ? tr("Stop server") : tr("Start server"), on)) {
        if (on) p.stop(), p.status.clear();
        else p.start();
    }
    y += 88;
    if (!p.status.empty()) text(r, x, y, fit_text(p.status, colw), {255, 170, 130}), y += 16;
    // ---- settings (they count from the next start)
    const float top = y;
    const SDL_Rect clip{0, int(top), int(W), int(H - top)};
    SDL_SetRenderClipRect(r, &clip);
    p.scroll = std::clamp(p.scroll - ui.wheel * 36, 0.f, 760.f);
    y += 14 - p.scroll;
    auto head = [&](std::string_view t) {
        text(r, x, y + 4, t, {255, 220, 140}, 2);
        y += 28;
    };
    auto toggle = [&](std::string_view label, bool& v) {
        if (button(r, ui, x, y, colw, 24, std::format("{}: {}", label, v ? tr("ON") : tr("OFF")), v)) v = !v;
        y += 28;
    };
    auto choice = [&](std::string_view label, std::string_view value, auto&& next) {
        if (button(r, ui, x, y, colw, 24, std::format("{}: {}", label, value))) next();
        y += 28;
    };
    head(tr("Identity"));
    y += 12;
    text_field(r, ui, 1, x, y, 420, p.cfg.name, tr("Name in the server list"));
    text_field(r, ui, 2, x + 440, y, 120, p.port_s, tr("UDP port"));
    y += 42;
    text_field(r, ui, 3, x, y, colw, p.cfg.motd, tr("Message of the day"));
    y += 42;
    text_field(r, ui, 10, x, y, 420, p.cfg.password, tr("Password (empty = anyone can join)"));
    y += 38;
    head(tr("Game"));
    {
        auto vit = std::ranges::find(p.versions, p.cfg.version);
        choice(tr("Game version"), p.cfg.version, [&] {
            if (p.versions.empty()) return;
            p.cfg.version = vit == p.versions.end() || vit + 1 == p.versions.end() ? p.versions.front() : *(vit + 1);
        });
    }
    const int nm = int(maps().size());
    choice(tr("Map"), maps()[std::size_t(std::clamp(p.cfg.map, 0, nm - 1))].name, [&] { p.cfg.map = (std::clamp(p.cfg.map, 0, nm - 1) + 1) % nm; });
    choice(tr("Mode"), tr(kGameModeNames[std::min<int>(p.cfg.mode, int(std::size(kGameModeNames)) - 1)]), [&] { p.cfg.mode = std::uint8_t((p.cfg.mode + 1) % std::size(kGameModeNames)); });
    choice(tr("Systems"), tr(kRuleSetNames[std::min<int>(p.cfg.ruleset, 2)]), [&] { p.cfg.ruleset = std::uint8_t((p.cfg.ruleset + 1) % 3); });
    toggle(tr("Soft level cap after 30"), p.cfg.soft_cap);
    toggle(tr("Practice rules (cheats on, bots fill the teams)"), p.cfg.practice);
    head(tr("Match features (off by default)"));
    for (int i = 0; i < kFeatureCount; ++i) {
        bool f = (p.cfg.features >> i) & 1;
        const bool before = f;
        toggle(tr(kFeatureNames[i]), f);
        if (f != before) p.cfg.features = std::uint16_t(p.cfg.features ^ (1u << i));
    }
    head(tr("Players"));
    toggle(tr("Ban phase (draft)"), p.cfg.bans);
    toggle(tr("Ranked: MMR + LP after every match"), p.cfg.ranked);
    toggle(tr("Skill-based matchmaking (balanced teams)"), p.cfg.matchmaking);
    toggle(tr("Matchmaking queue: players queue for map + mode, random teams"), p.cfg.queue_mode);
    toggle(tr("Allow duplicate nicknames"), p.cfg.allow_duplicate_names);
    toggle(tr("Unique nicknames with registration + login"), p.cfg.registration);
    toggle(tr("Force server-only textures"), p.cfg.force_server_textures);
    toggle(tr("Own champions: their files are sent to everyone who joins"), p.cfg.allow_custom_champions);
    toggle(tr("Players' own emotes are passed on (separate from skins)"), p.cfg.allow_own_emotes);
    toggle(tr("People may join to watch"), p.cfg.allow_spectators);
    toggle(tr("Save a replay of every match (replays folder)"), p.cfg.record_replays);
    text(r, x, y + 6, tr("Allowed systems:"), {200, 200, 210});
    {
        float px = x + 170;
        for (std::uint8_t b : {net::kLinux, net::kWindows, net::kMacOS}) {
            const bool sel = p.cfg.allowed_platforms & b;
            if (button(r, ui, px, y, 120, 24, net::platform_name(b), sel)) p.cfg.allowed_platforms ^= b;
            px += 126;
        }
    }
    y += 34;
    head(tr("Numbers"));
    y += 12;
    text_field(r, ui, 4, x, y, 130, p.tick_s, tr("Tick rate (Hz)"));
    text_field(r, ui, 5, x + 150, y, 130, p.snap_s, tr("Snapshot every N ticks"));
    text_field(r, ui, 6, x + 300, y, 130, p.wave_s, tr("First wave (s)"));
    text_field(r, ui, 7, x + 450, y, 130, p.pick_s, tr("Pick timer (s, 0 = off)"));
    text_field(r, ui, 8, x + 600, y, 130, p.queue_s, tr("Queue: players"));
    y += 44;
    text_field(r, ui, 9, x, y, 130, p.start_s, tr("Sky starts at (min)"));
    y += 42;
    text(r, x, y, tr("Settings count from the next start. They are kept in server.cfg (also used by elemental_moba --server)."), {150, 150, 170});
    SDL_SetRenderClipRect(r, nullptr);
    if (ui.enter) ui.focus = -1;
}

}  // namespace

int main(int argc, char** argv) {
    tool::go_to_game_folder();
    if (enet_initialize() != 0) {
        std::println(stderr, "enet init failed");
        return 1;
    }
    std::atexit(enet_deinitialize);
    net::ServerConfig cfg;
    cfg.pick_seconds = 90;  // a server without a host: the lobby timer is on
    std::string language = tool::language_of_game();
    net::load_server_cfg(cfg, &language);
    bool console = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--nogui") console = true;
        else if (a == "--port") cfg.port = std::uint16_t(std::atoi(next().c_str()));
        else if (a == "--map") cfg.map = std::max(0, map_index(next()));
        else if (a == "--version") cfg.version = next();
        else if (a == "--tick") cfg.tick_hz = std::uint32_t(std::atoi(next().c_str()));
        else if (a == "--unranked") cfg.ranked = false;
        else if (a == "--language") language = next();
        else {
            std::println("unknown option {}", a);
            return 2;
        }
    }
    loc::load(language);
    if (console) return run_console(cfg);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "SDL_Init: {}", SDL_GetError());
        return 1;
    }
    int rc = 0;
    {
        tool::Window win;
        if (!win.open("Elemental MOBA server", 960, 780)) rc = 1;
        else {
            Panel p;
            p.cfg = cfg, p.language = language;
            p.versions = list_versions();
            p.load_text();
            while (win.begin()) {
                draw_panel(win, p);
                win.end();
            }
            p.stop();
            p.apply_text();
            save_server_cfg(p.cfg, p.language);
        }
    }
    SDL_Quit();
    return rc;
}
