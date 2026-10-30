// elemental-moba — game client + server in one binary [U-01][U-04].
//   elemental_moba                         menu (practice, join, host match, host server, settings)
//   elemental_moba --server [--port N] [--tick N] [--snap-div N]   dedicated headless server [U-103]
//                  [--pick-seconds N (default 90, 0 = off)] [--unranked] [--no-matchmaking]           [U-276]
//                  [--version <name>]  own game version from versions/<name>/version.txt                  [U-95]
//   elemental_moba --autotest S --shot f.bmp [--champ i] [--start-min m] [--first-wave s] [--manual]
#include "game/match/maps.hpp"
#include <SDL3/SDL.h>
#include <enet/enet.h>

#include <csignal>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <print>
#include <string>
#include <thread>

#include "client/app.hpp"
#include "net/server.hpp"

namespace {
std::stop_source g_stop;
void on_signal(int) { g_stop.request_stop(); }

// The game data (resourcepacks/, lang/, custom_champions/) sits in the game folder. The game may be started
// from anywhere (from build/, by a desktop shortcut…) → find that folder and work from it. A settings.cfg
// left next to the executable by older versions is copied over once.
void go_to_game_folder() {
    namespace fs = std::filesystem;
    std::error_code ec;
    auto is_root = [&](const fs::path& p) { return fs::is_directory(p / "resourcepacks", ec) && fs::is_directory(p / "lang", ec); };
    const fs::path cwd = fs::current_path(ec);
    if (is_root(cwd)) return;
    const char* base = SDL_GetBasePath();
    if (!base) return;
    const fs::path exe = fs::path(base);
    for (fs::path p : {exe, exe.parent_path(), exe.parent_path().parent_path()}) {
        if (!is_root(p)) continue;
        if (!fs::exists(p / "settings.cfg", ec) && fs::exists(cwd / "settings.cfg", ec)) fs::copy_file(cwd / "settings.cfg", p / "settings.cfg", ec);
        fs::current_path(p, ec);
        return;
    }
}
}  // namespace

int main(int argc, char** argv) {
    go_to_game_folder();
    if (enet_initialize() != 0) {
        std::println(stderr, "enet init failed");
        return 1;
    }
    std::atexit(enet_deinitialize);
    em::net::ServerConfig scfg;
    em::AppOptions opt;
    bool dedicated = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--server") dedicated = true, scfg.pick_seconds = 90;  // [U-276] dedicated: lobby timer on
        else if (a == "--pick-seconds") scfg.pick_seconds = std::atoi(next().c_str());
        else if (a == "--map") scfg.map = std::max(0, em::map_index(next()));        // [U-420] e.g. --map twisted_grove
        else if (a == "--queue") scfg.queue_mode = true;                              // [U-423] matchmaking queue
        else if (a == "--queue-min") scfg.queue_min = std::max(1, std::atoi(next().c_str()));
        else if (a == "--unranked") scfg.ranked = false;            // [U-276] no MMR / LP
        else if (a == "--version") scfg.version = next();          // [U-95] own game version (versions/<name>/)
        else if (a == "--no-matchmaking") scfg.matchmaking = false;
        else if (a == "--port") scfg.port = std::uint16_t(std::atoi(next().c_str()));
        else if (a == "--tick") scfg.tick_hz = std::uint32_t(std::atoi(next().c_str()));
        else if (a == "--snap-div") scfg.snap_div = std::atoi(next().c_str());
        else if (a == "--practice") scfg.practice = true;
        else if (a == "--autotest") opt.autotest_s = std::atoi(next().c_str());
        else if (a == "--shot") opt.shot = next();
        else if (a == "--shots") opt.shots = std::max(1, std::atoi(next().c_str()));  // [U-446]
        else if (a == "--champ") opt.champ = std::atoi(next().c_str());
        else if (a == "--start-min") opt.start_ms = std::int64_t(std::atof(next().c_str()) * 60'000);
        else if (a == "--first-wave") opt.first_wave_ms = std::int64_t(std::atof(next().c_str()) * 1'000);
        else if (a == "--manual") opt.autoplay = false;
        else if (a == "--shot-select") opt.shot_select = true;  // autotest: picture of champion select
        else if (a == "--join") {
            const std::string hp = next();
            const auto c = hp.find(':');
            opt.join_host = hp.substr(0, c);
            if (c != std::string::npos) opt.join_port = std::uint16_t(std::atoi(hp.c_str() + c + 1));
        } else if (a == "--wait") opt.wait_players = std::atoi(next().c_str());
        else if (a == "--name") opt.name = next();
        else if (a == "--no-start") opt.starter = false;
        else if (a == "--windowed") opt.windowed = true;  // [U-207] e.g. for two screens
        else if (a == "--size") {
            opt.width = std::atoi(next().c_str());
            opt.height = std::atoi(next().c_str());
        } else {
            std::println("unknown option {}", a);
            return 2;
        }
    }
    if (dedicated) {  // [U-103] headless
        em::net::Server s(scfg);
        std::string err;
        if (!s.start(&err)) {
            std::println(stderr, "{}", err);
            return 1;
        }
        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);
        std::println("elemental-moba server {} on UDP {} at {} Hz (Ctrl+C to stop)", em::net::kVersion, scfg.port, scfg.tick_hz);
        std::jthread status([&](std::stop_token st) {
            while (!st.stop_requested() && !g_stop.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                std::println("players {}  {}  time {} s  tick load {:.1f}%", s.players.load(), s.in_match ? "in match" : "lobby",
                             s.match_ms.load() / 1000, s.tick_load.load() * 100);
            }
        });
        s.run(g_stop.get_token());
        return 0;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "SDL_Init: {}", SDL_GetError());
        return 1;
    }
    int rc = 0;
    {
        em::App app(opt);
        rc = app.run();
    }
    SDL_Quit();
    return rc;
}
