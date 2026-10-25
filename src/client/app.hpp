#pragma once
// [U-95..U-103][U-207] The game application: window (Vulkan renderer [U-214]), main loop, and the screens
// from MENU.txt. Every screen is one member function in its own file under client/screens/.
#include <SDL3/SDL.h>

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "client/audio.hpp"
#include "client/game_view.hpp"
#include "client/custom_champions.hpp"
#include "client/item_browser.hpp"
#include "client/net_client.hpp"
#include "client/resources.hpp"
#include "net/server.hpp"

namespace em {

struct AppOptions {
    // automated run (used to test without a human): practice, auto pick, screenshot, quit
    int autotest_s = 0;
    std::string shot;          // BMP path
    int champ = 0;
    std::int64_t start_ms = 0; // sky clock offset for the practice server
    bool autoplay = true;      // my champion is driven by a bot in autotest
    std::int64_t first_wave_ms = 65'000;
    int width = 1600, height = 900;
    std::string join_host;     // autotest against a running server (dedicated) instead of a local one
    std::uint16_t join_port = 7777;
    int wait_players = 1;      // autotest: start when this many players are in the lobby
    std::string name;          // nickname override
    bool starter = true;       // autotest: this client presses Start
    bool windowed = false;     // force a window (tests / two screens)
    bool shot_select = false;  // autotest: screenshot of champion select (loadout open) instead of the match, then quit
};

class App {
public:
    explicit App(AppOptions o);
    ~App();
    int run();

private:
    enum class Screen {
        Menu, Options, Multiplayer, ServerEdit, HostMatch, HostServer, ServerRunning,
        ChampSelect, Loading, Game, PostGame,
        Items, Setups, ChampionInfo, CustomChampions,
    };
    // ---- app.cpp
    void frame(double now_s);
    void apply_window();
    bool start_server(net::ServerConfig cfg, std::string* err);
    void stop_server();
    void connect_local(std::uint16_t port);
    void connect_to(const std::string& address);  // host:port
    void go(Screen s);
    // ---- screens/menu.cpp
    void menu();
    // ---- screens/options.cpp
    void options(bool in_game);
    // ---- screens/multiplayer.cpp
    void multiplayer();
    void server_edit();
    void host(bool dedicated);
    void server_running();
    // ---- screens/champ_select.cpp
    void champ_select();
    void apply_champion_defaults(int champ);  // [U-178] last setup / Q / F / G of that champion
    void send_pick();
    // returns the height used; rows wrap inside `w` (no button ever goes past x + w) [U-250]
    float draw_loadout_editor(float x, float y, Loadout& lo, bool editable, float w = 760);
    float pick_scroll_ = 0;
    double autotest_select_t_ = -1;
    int filter_avatar_ = 0;        // champion select: 0 any, 1 Avatars, 2 Dark Avatars [U-07]
    int filter_origin_ = 0;        // champion select: 0 all, 1 built-in, 2 own champions [U-305]
    std::vector<CustomChampionFile> custom_scan_;  // [U-353] the own-champion files, read once a second
    double custom_scan_t_ = -10;        // champion select: scroll of the picks panel
    bool show_loadout_ = false;    // champion select: the loadout editor is folded until opened
    void draw_chat_box(float x, float y, float w, float h, net::Chat channel);
    // ---- screens/postgame.cpp
    void loading(double now_s);
    void postgame();
    // ---- screens/collection.cpp
    void items_screen();
    void setups_screen();
    void champion_info();
    void custom_champions();
    // ---- shared
    void ingame_menu();  // Esc in a match: live options, leave [U-207]

    AppOptions opt_;
    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    std::string renderer_name_;
    Settings set_;
    gfx::Ui ui_;
    NetClient net_;
    Resources res_;
    Audio audio_;
    std::unique_ptr<GameView> view_;
    std::unique_ptr<net::Server> server_;
    std::jthread server_thread_;
    std::uint64_t secret_ = 0;
    Screen screen_ = Screen::Menu, back_ = Screen::Menu;
    std::string status_;
    double now_s_ = 0;
    // ---- menu fields
    std::string port_ = "7777", tick_ = "128", host_name_ = "My elemental-moba game", host_motd_ = "Have fun!";
    bool force_tex_ = false, dup_names_ = false, registration_ = false, soft_cap_ = false, bans_ = false;
    bool ranked_ = true, matchmaking_ = true, pick_timer_ = true;  // [U-276]
    std::uint8_t platforms_ = net::kAllPlatforms;
    std::string direct_addr_ = "127.0.0.1:7777", edit_name_, edit_addr_;
    int edit_index_ = -1, selected_server_ = -1;
    std::vector<std::future<ServerInfo>> pings_;
    std::vector<ServerInfo> infos_;
    // ---- champion select
    int pick_team_ = 0, pick_champ_ = -1, pick_q_ = -1, pick_f_ = -1, pick_g_ = -1, pick_setup_ = -1;
    Loadout pick_loadout_;
    std::string search_;
    int filter_role_ = -1, filter_element_ = -1;
    bool filter_fav_ = false, filter_non_mage_ = false;
    int ban_pick_ = -1;
    std::string chat_text_;
    net::Chat chat_channel_ = net::Chat::Team;
    bool ready_ = false;
    // ---- collection
    int item_filter_ = -1, item_sort_ = 0, item_selected_ = -1, build_selected_ = -1, setup_selected_ = 0;
    std::string item_search_;
    int info_champ_ = 0;
    ItemBrowser shop_;
    // ---- state
    double match_start_s_ = -1, loading_since_ = -1, over_since_ = -1;
    bool quit_ = false, shot_done_ = false, ingame_menu_ = false;
    int options_page_ = 0;
    int bind_wait_ = -1;             // Controls page: waiting for a key for this action
    std::string last_key_ = "none";  // keyboard diagnostics
    int key_count_ = 0;
    bool autotest_picked_ = false, autotest_started_ = false;
};

}  // namespace em
