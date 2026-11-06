// [U-510][U-511] See tool_shell.hpp.
#include "client/tool_shell.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#undef near
#undef far
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "client/locale.hpp"

namespace em::tool {

namespace fs = std::filesystem;

void go_to_game_folder() {
    std::error_code ec;
    auto is_root = [&](const fs::path& p) { return fs::is_directory(p / "resourcepacks", ec) && fs::is_directory(p / "lang", ec); };
    if (is_root(fs::current_path(ec))) return;
    const char* base = SDL_GetBasePath();
    if (!base) return;
    const fs::path exe = fs::path(base);
    for (const fs::path& p : {exe, exe.parent_path(), exe.parent_path().parent_path()})
        if (is_root(p)) {
            fs::current_path(p, ec);
            return;
        }
}

std::string lan_address() {
    std::string best = "127.0.0.1";
#if !defined(_WIN32)
    ifaddrs* all = nullptr;
    if (getifaddrs(&all) == 0) {
        for (ifaddrs* i = all; i; i = i->ifa_next)
            if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET && !(i->ifa_flags & IFF_LOOPBACK)) {
                char buf[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(i->ifa_addr)->sin_addr, buf, sizeof buf);
                best = buf;
                if (std::string_view(i->ifa_name) != "docker0" && !std::string_view(i->ifa_name).starts_with("br-")) break;
            }
        freeifaddrs(all);
    }
#else
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
        char host[256] = {};
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        if (gethostname(host, sizeof host) == 0 && getaddrinfo(host, nullptr, &hints, &res) == 0) {
            for (addrinfo* a = res; a; a = a->ai_next) {
                char buf[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(a->ai_addr)->sin_addr, buf, sizeof buf);
                if (std::string_view(buf).starts_with("127.")) continue;
                best = buf;
                break;
            }
            freeaddrinfo(res);
        }
        WSACleanup();
    }
#endif
    return best;
}

std::string language_of_game() {
    std::ifstream in("settings.cfg");
    std::string line;
    while (std::getline(in, line))
        if (line.starts_with("language=")) return line.substr(9);
    return "en";
}

std::string next_language(const std::string& code) {
    std::vector<std::string> all = loc::languages();
    std::ranges::sort(all);
    const auto it = std::ranges::find(all, code);
    return it == all.end() || it + 1 == all.end() ? all.front() : *(it + 1);
}

bool open_program(const char* name, std::string* error) {
    const char* base = SDL_GetBasePath();
    fs::path p = fs::path(base ? base : "") / name;
#if defined(_WIN32)
    p += ".exe";
#endif
    std::error_code ec;
    if (!fs::exists(p, ec)) {
        if (error) *error = std::format("{}: {}", loc::tr("not found"), p.string());
        return false;
    }
    const std::string path = p.string();
    const char* args[] = {path.c_str(), nullptr};
    SDL_Process* proc = SDL_CreateProcess(args, false);
    if (!proc) {
        if (error) *error = SDL_GetError();
        return false;
    }
    SDL_DestroyProcess(proc);  // (it keeps running)
    return true;
}

bool Window::open(const char* title, int width, int height) {
    SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");  // [U-496] Vulkan only
    win_ = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (win_) ren_ = SDL_CreateRenderer(win_, "gpu");
    if (!ren_) {
        const std::string why = std::format("{}\n\nSDL: {}", loc::tr("This program needs Vulkan (a Vulkan driver for your GPU)."), SDL_GetError());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, why.c_str(), win_);
        return false;
    }
    SDL_SetRenderVSync(ren_, 1);
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    gfx::init_text(ren_);
    SDL_StopTextInput(win_);
    t0_ = SDL_GetTicks();
    return true;
}

Window::~Window() {
    if (ren_) SDL_DestroyRenderer(ren_);
    if (win_) SDL_DestroyWindow(win_);
}

bool Window::begin(const std::function<void(const SDL_Event&)>& extra) {
    ui.clicked = ui.rclicked = ui.backspace = ui.enter = ui.copy = false;
    ui.wheel = 0;
    ui.typed.clear();
    SDL_Event e;
    bool open_ = true;
    while (SDL_PollEvent(&e)) {
        SDL_ConvertEventToRenderCoordinates(ren_, &e);
        if (extra) extra(e);
        switch (e.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: open_ = false; break;
            case SDL_EVENT_MOUSE_WHEEL: ui.wheel += e.wheel.y; break;
            case SDL_EVENT_MOUSE_MOTION: ui.mx = e.motion.x, ui.my = e.motion.y; break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                ui.mx = e.button.x, ui.my = e.button.y;
                if (e.button.button == SDL_BUTTON_LEFT) ui.clicked = true, ui.down = true;
                if (e.button.button == SDL_BUTTON_RIGHT) ui.rclicked = true;
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_LEFT) ui.down = false;
                break;
            case SDL_EVENT_TEXT_INPUT: ui.typed += e.text.text; break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_BACKSPACE) ui.backspace = true;
                if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) ui.enter = true;
                if ((e.key.mod & SDL_KMOD_CTRL) && e.key.key == SDLK_C) ui.copy = true;
                if ((e.key.mod & SDL_KMOD_CTRL) && e.key.key == SDLK_V)
                    if (char* clip = SDL_GetClipboardText()) {
                        for (const char* c = clip; *c; ++c)
                            if (*c != '\n' && *c != '\r') ui.typed += *c;
                        SDL_free(clip);
                    }
                break;
            default: break;
        }
    }
    if (closing_) open_ = false;
    const bool want_text = ui.focus >= 0;
    if (want_text != SDL_TextInputActive(win_)) {
        if (want_text) SDL_StartTextInput(win_);
        else SDL_StopTextInput(win_);
    }
    int pw = 0, ph = 0;
    SDL_GetRenderOutputSize(ren_, &pw, &ph);
    w = float(pw), h = float(ph);
    now_s = double(SDL_GetTicks() - t0_) / 1000.0;
    gfx::set_color(ren_, {20, 22, 30, 255});
    SDL_RenderClear(ren_);
    return open_;
}

void Window::end() {
    // EM_TOOL_SHOT=<file.bmp>: a picture of the window after a moment, then quit (to check a tool without a person)
    if (const char* shot = SDL_getenv("EM_TOOL_SHOT"); shot && ++frames_ == 30) {
        if (SDL_Surface* s = SDL_RenderReadPixels(ren_, nullptr)) {
            SDL_SaveBMP(s, shot);
            SDL_DestroySurface(s);
        }
        closing_ = true;
    }
    gfx::end_frame_text();
    SDL_RenderPresent(ren_);
}

}  // namespace em::tool
