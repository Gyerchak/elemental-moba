// [U-95..U-103][U-207][U-214][U-233] App core: window + renderer, the main loop, the screen switch.
// The screens themselves are in client/screens/*.cpp.
#include "game/versions.hpp"
#include "client/app.hpp"
#include <fstream>
#include <filesystem>

#include <algorithm>
#include <chrono>
#include <format>
#include <random>

#include "client/custom_champions.hpp"
#include "client/locale.hpp"

namespace em {

using namespace gfx;

App::App(AppOptions o) : opt_(std::move(o)) {
    set_.load();  // settings.cfg next to where the game starts [U-99]
    if (!opt_.name.empty()) set_.name = opt_.name;
    loc::load(set_.language);  // [U-239]
    if (!apply_version(set_.version).empty()) set_.version = kOfficialVersion;  // [U-95] own version (bad file → official)
    load_custom_champions(set_.custom_champions);  // [U-210] before any match — AFTER the version (they copy its numbers)
    // [U-214] Vulkan renderer first; anything else only if this machine has no Vulkan
    const SDL_WindowFlags base = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    win_ = SDL_CreateWindow("elemental-moba", opt_.width, opt_.height, base | SDL_WINDOW_VULKAN);
    if (win_) ren_ = SDL_CreateRenderer(win_, "vulkan");
    if (!ren_) {
        if (win_) SDL_DestroyWindow(win_);
        win_ = SDL_CreateWindow("elemental-moba", opt_.width, opt_.height, base);
        ren_ = SDL_CreateRenderer(win_, nullptr);
    }
    renderer_name_ = ren_ ? SDL_GetRendererName(ren_) : "none";
    SDL_SetRenderVSync(ren_, set_.vsync ? 1 : 0);
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    gfx::init_text(ren_);  // [U-250] font text
    SDL_StopTextInput(win_);  // only on while typing: with an IME (Wayland) letters would never reach the game
    res_.init(ren_, set_.resource_pack);
    set_item_icon_source([this](int i) { return res_.item_icon(kItems[std::size_t(i)].name); });  // [U-257]
    audio_.init();
    audio_.set_volumes(set_);
    view_ = std::make_unique<GameView>(ren_, set_, res_, audio_);
    apply_window();
    std::random_device rd;
    secret_ = (std::uint64_t(rd()) << 32 | rd()) | 1;
}

App::~App() {
    set_.save();
    net_.disconnect();
    stop_server();
    view_.reset();
    SDL_DestroyRenderer(ren_);
    SDL_DestroyWindow(win_);
}

// [U-207][U-233] true fullscreen (exclusive), borderless, or a window; the mouse stays inside by default;
// in a fullscreen match the keyboard is grabbed too, so desktop shortcuts don't fire while playing.
void App::apply_window() {
    if (opt_.windowed || set_.window == WindowMode::Windowed) {
        SDL_SetWindowFullscreen(win_, false);
        SDL_SetWindowBordered(win_, true);
    } else if (set_.window == WindowMode::Borderless) {
        SDL_SetWindowFullscreenMode(win_, nullptr);  // desktop-sized, no mode change
        SDL_SetWindowFullscreen(win_, true);
    } else {
        const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(win_));
        if (dm) SDL_SetWindowFullscreenMode(win_, dm);  // exclusive fullscreen
        SDL_SetWindowFullscreen(win_, true);
    }
    const bool in_match = screen_ == Screen::Game || screen_ == Screen::Loading;
    SDL_SetWindowMouseGrab(win_, set_.confine_mouse && !opt_.windowed);
    SDL_SetWindowKeyboardGrab(win_, in_match && set_.window != WindowMode::Windowed && !opt_.windowed);
}

bool App::start_server(net::ServerConfig cfg, std::string* err) {
    stop_server();
    server_ = std::make_unique<net::Server>(cfg);
    if (!server_->start(err)) {
        server_.reset();
        return false;
    }
    server_thread_ = std::jthread([s = server_.get()](std::stop_token st) { s->run(st); });
    return true;
}

void App::stop_server() {
    if (server_thread_.joinable()) {
        server_thread_.request_stop();
        server_thread_.join();
    }
    server_.reset();
}

void App::connect_local(std::uint16_t port) {
    net_.connect("127.0.0.1", port, set_.name, secret_, set_.version, set_.show_other_skins);
    ready_ = false;
    pick_champ_ = opt_.autotest_s ? opt_.champ : -1;
    go(Screen::ChampSelect);
}

void App::connect_to(const std::string& address_in) {
    // [U-422] an invite link "em-join://host:port" works like a plain address
    const std::string address = address_in.starts_with("em-join://") ? address_in.substr(10) : address_in;
    const auto c = address.rfind(':');
    const std::string host = c == std::string::npos ? address : address.substr(0, c);
    const int port = c == std::string::npos ? net::kDefaultPort : std::atoi(address.c_str() + c + 1);
    net_.connect(host, std::uint16_t(std::clamp(port, 1, 65'535)), set_.name, 0, set_.version, set_.show_other_skins);
    pick_champ_ = -1;
    ready_ = false;
    go(Screen::ChampSelect);
}

void App::go(Screen s) {
    screen_ = s;
    ui_.focus = -1;
    bind_wait_ = -1;
    apply_window();
    // music: menu / match [U-197]
    if (s == Screen::Game) audio_.play_music(res_.music("match"));
    else if (s == Screen::Menu) audio_.play_music(res_.music("menu"));
}

int App::run() {
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    audio_.play_music(res_.music("menu"));
    if (opt_.autotest_s > 0 && !opt_.join_host.empty()) {  // automated client against a running server
        connect_to(std::format("{}:{}", opt_.join_host, opt_.join_port));
    } else if (opt_.autotest_s > 0) {  // automated practice run [U-100]
        net::ServerConfig c;
        c.version = set_.version;  // [U-95]
        c.port = std::uint16_t(7790 + std::random_device{}() % 200);
        c.practice = true;
        c.loopback_only = true;
        c.host_secret = secret_;
        c.start_ms = opt_.start_ms;
        c.first_wave_ms = opt_.first_wave_ms;
        c.players_are_bots = opt_.autoplay;
        std::string err;
        if (!start_server(c, &err)) {
            SDL_Log("server: %s", err.c_str());
            return 1;
        }
        connect_local(c.port);
    }
    while (!quit_) {
        ui_.clicked = ui_.rclicked = ui_.backspace = ui_.enter = false;
        ui_.wheel = 0;
        ui_.typed.clear();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (ren_) SDL_ConvertEventToRenderCoordinates(ren_, &e);  // [U-389] mouse in render pixels (HiDPI / scaled screens)
            if (e.type == SDL_EVENT_MOUSE_WHEEL) ui_.wheel += e.wheel.y;  // scroll lists [U-250]
            if (e.type == SDL_EVENT_QUIT) quit_ = true;
            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                ui_.mx = e.motion.x;
                ui_.my = e.motion.y;
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                ui_.mx = e.button.x;
                ui_.my = e.button.y;
                if (e.button.button == SDL_BUTTON_LEFT) ui_.clicked = true, ui_.down = true;
                if (e.button.button == SDL_BUTTON_RIGHT) ui_.rclicked = true;
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) ui_.down = false;
            if (e.type == SDL_EVENT_KEY_DOWN) {  // diagnostics: does the keyboard reach the game at all?
                ++key_count_;
                last_key_ = std::format("{} (scancode {})", SDL_GetKeyName(e.key.key), int(e.key.scancode));
            }
            if (bind_wait_ >= 0 && e.type == SDL_EVENT_KEY_DOWN) {  // Controls page: new key [U-165]
                if (e.key.scancode != SDL_SCANCODE_ESCAPE) set_.keys[std::size_t(bind_wait_)] = e.key.scancode;
                bind_wait_ = -1;
                set_.save();
                continue;
            }
            const bool game_input = screen_ == Screen::Game && !ingame_menu_;
            if (game_input) {
                view_->handle_event(e, net_);
                continue;
            }
            if (e.type == SDL_EVENT_TEXT_INPUT) ui_.typed += e.text.text;
            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_BACKSPACE) ui_.backspace = true;
                if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) ui_.enter = true;
                if (e.key.key == SDLK_ESCAPE) {
                    if (ingame_menu_) ingame_menu_ = false;  // back into the match
                    else if (screen_ != Screen::Menu && screen_ != Screen::ChampSelect && screen_ != Screen::Game) go(back_);
                }
            }
        }
        now_s_ = std::chrono::duration<double>(clock::now() - t0).count();
        // text input only while a text field / the chat is focused
        const bool want_text = screen_ == Screen::Game && !ingame_menu_ ? view_->wants_text() : ui_.focus >= 0;
        if (want_text != SDL_TextInputActive(win_)) {
            if (want_text) SDL_StartTextInput(win_);
            else SDL_StopTextInput(win_);
        }
        // [U-469] watchdog: a frame over 200 ms is written to logs/slow_frames.txt (network / drawing / present)
        const auto w0 = clock::now();
        net_.poll(now_s_);
        const auto w1 = clock::now();
        frame(now_s_);
        const auto w2 = clock::now();
        // autotest: screenshot + quit
        if (opt_.autotest_s > 0 && screen_ == Screen::Game && match_start_s_ >= 0 && now_s_ - match_start_s_ >= opt_.autotest_s && !shot_done_) {
            // [U-446] --shots N: N frames in a row (shot_00.bmp …) → frame-to-frame glitches (flicker, jumps) can be compared
            if (!opt_.shot.empty())
                if (SDL_Surface* s = SDL_RenderReadPixels(ren_, nullptr)) {
                    std::string f = opt_.shot;
                    if (opt_.shots > 1) f = std::format("{}_{:02}.bmp", f.substr(0, f.rfind('.')), shots_taken_);
                    SDL_SaveBMP(s, f.c_str());
                    SDL_DestroySurface(s);
                }
            if (++shots_taken_ >= std::max(1, opt_.shots)) shot_done_ = true, quit_ = true;
        }
        SDL_RenderPresent(ren_);
        gfx::end_frame_text();
        if (const auto w3 = clock::now(); w3 - w0 > std::chrono::milliseconds(200)) {
            auto ms = [](auto d) { return std::chrono::duration<double, std::milli>(d).count(); };
            std::error_code ec;
            std::filesystem::create_directories("logs", ec);
            if (std::ofstream lg("logs/slow_frames.txt", std::ios::app); lg)
                lg << std::format("{:.1f} s  screen {}  network {:.0f} ms  frame {:.0f} ms  present {:.0f} ms  fx {}  units {}\n", now_s_, int(screen_),
                                  ms(w1 - w0), ms(w2 - w1), ms(w3 - w2), net_.cur.fx.size(), net_.cur.units.size());
        }
    }
    return 0;
}

void App::frame(double now_s) {
    if (net_.reload_own_champions) {  // [U-415] back from a server that brought its own champions: mine again
        net_.reload_own_champions = false;
        reset_custom_champions();
        load_custom_champions(set_.custom_champions);
    }
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    if (screen_ != Screen::Game) {
        fill_rect(ren_, 0, 0, float(w), float(h), {14, 16, 26, 255});
        // [U-257] the menu picture behind every menu screen (dimmed so the buttons stay readable)
        if (SDL_Texture* bg = res_.texture("screens/menu.png"); bg && screen_ != Screen::Loading) {
            float tw = 0, th = 0;
            SDL_GetTextureSize(bg, &tw, &th);
            const float sc = std::max(float(w) / tw, float(h) / th);
            const SDL_FRect d{(float(w) - tw * sc) / 2, (float(h) - th * sc) / 2, tw * sc, th * sc};
            SDL_SetTextureColorMod(bg, 110, 110, 120);
            SDL_RenderTexture(ren_, bg, nullptr, &d);
            SDL_SetTextureColorMod(bg, 255, 255, 255);
        }
    }
    // ---- connection state → screens
    if (net_.state == NetClient::State::Match && (screen_ == Screen::ChampSelect || screen_ == Screen::PostGame) && net_.winner < 0) {
        go(Screen::Loading);
        loading_since_ = now_s;
    }
    if (screen_ == Screen::Loading && net_.have_snapshot && now_s - loading_since_ > 2.0) {  // [U-199]
        go(Screen::Game);
        view_->reset();
        match_start_s_ = now_s;
        over_since_ = -1;
    }
    if (screen_ == Screen::Game && net_.winner >= 0 && !net_.final_stats.empty()) {
        // the result banner shows in the game view for a moment, then the statistics [U-179]
        if (over_since_ < 0) over_since_ = now_s;
        if (now_s - over_since_ > 3) {
            ingame_menu_ = false;
            go(Screen::PostGame);
        }
    }
    if (net_.state == NetClient::State::Rejected && screen_ != Screen::Menu && screen_ != Screen::Multiplayer) {
        // [U-95] auto-fallback: the server runs another version we also have → switch and retry
        const auto have = list_versions();
        if (net_.server_version != set_.version && std::ranges::find(have, net_.server_version) != have.end() &&
            apply_version(net_.server_version).empty()) {
            set_.version = net_.server_version;
            set_.save();
            status_ = std::format("{} {} — {}", tr("switched to the server's version"), net_.server_version, tr("join again"));
            stop_server();
            go(Screen::Menu);
        } else {
            status_ = std::format("{}: {}", tr("rejected"), net_.reject_reason);
            stop_server();
            go(Screen::Menu);
        }
    }
    if (net_.state == NetClient::State::Disconnected && screen_ != Screen::Menu && screen_ != Screen::ServerRunning) {
        status_ = tr("disconnected");
        stop_server();
        go(Screen::Menu);
    }
    switch (screen_) {
        case Screen::Menu: menu(); break;
        case Screen::Options: options(false); break;
        case Screen::Multiplayer: multiplayer(); break;
        case Screen::ServerEdit: server_edit(); break;
        case Screen::HostMatch: host(false); break;
        case Screen::HostServer: host(true); break;
        case Screen::ServerRunning: server_running(); break;
        case Screen::ChampSelect: champ_select(); break;
        case Screen::Loading: loading(now_s); break;
        case Screen::PostGame: postgame(); break;
        case Screen::Items: items_screen(); break;
        case Screen::Setups: setups_screen(); break;
        case Screen::ChampionInfo: champion_info(); break;
        case Screen::CustomChampions: custom_champions(); break;
        case Screen::Interactions: interactions(); break;
        case Screen::Game: {
            gfx::Ui dead = ui_;  // the match keeps running under the menu, but clicks belong to the menu
            dead.clicked = dead.rclicked = false;
            view_->frame(net_, now_s, ingame_menu_ ? dead : ui_);
            if (view_->wants_menu) {
                view_->wants_menu = false;
                ingame_menu_ = true;
            }
            if (ingame_menu_) ingame_menu();
            if (set_.dev_mode) text(ren_, 8, 80, std::format("keys: {}  last: {}  renderer: {}", key_count_, last_key_, renderer_name_), {150, 255, 150});
            if (view_->leave) {
                net_.disconnect();
                stop_server();
                go(Screen::Menu);
                status_.clear();
            }
            break;
        }
    }
}

// [U-207] Esc in a match: the game keeps running; live options; leave only on purpose
void App::ingame_menu() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    fill_rect(ren_, 0, 0, float(w), float(h), {0, 0, 0, 140});
    const float x = float(w) / 2 - 330, y = 40;
    fill_rect(ren_, x - 10, y - 10, 680, float(h) - 80, {14, 16, 26, 245});
    rect(ren_, x - 10, y - 10, 680, float(h) - 80, {150, 130, 90});
    text(ren_, x, y, tr("Game menu"), {255, 200, 120}, 3);
    if (button(ren_, ui_, x + 420, y, 220, 34, tr("Resume (Esc)"))) ingame_menu_ = false;
    options(true);
    if (button(ren_, ui_, x, float(h) - 80, 260, 36, tr("Leave the match"))) {
        ingame_menu_ = false;
        net_.disconnect();
        stop_server();
        go(Screen::Menu);
    }
}

}  // namespace em
