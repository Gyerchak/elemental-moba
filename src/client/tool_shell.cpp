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
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
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

std::string public_address(std::string* error) {
    auto fail = [&](const char* why) { if (error) *error = why; return std::string(); };
#if defined(_WIN32)
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return fail("no network");
    using Sock = SOCKET;
    const Sock bad = INVALID_SOCKET;
#else
    using Sock = int;
    const Sock bad = -1;
#endif
    addrinfo hints{};
    hints.ai_family = AF_INET, hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    if (getaddrinfo("stun.l.google.com", "19302", &hints, &res) != 0 || !res) return fail("no answer: no internet or no name lookup");
    const Sock s = socket(AF_INET, SOCK_DGRAM, 0);
    std::string found;
    if (s != bad) {
#if defined(_WIN32)
        const DWORD ms = 2000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof ms);
#else
        timeval tv{2, 0};
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif
        unsigned char req[20] = {0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xA4, 0x42};  // a binding request, magic cookie
        for (int i = 8; i < 20; ++i) req[i] = static_cast<unsigned char>(SDL_rand(256));
        if (sendto(s, reinterpret_cast<const char*>(req), 20, 0, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen)) == 20) {
            unsigned char in[512];
            const int n = int(recvfrom(s, reinterpret_cast<char*>(in), sizeof in, 0, nullptr, nullptr));
            for (int i = 20; n >= 20 && i + 4 <= n;) {
                const int type = in[i] << 8 | in[i + 1], len = in[i + 2] << 8 | in[i + 3];
                if ((type == 0x0020 || type == 0x0001) && len >= 8 && i + 12 <= n && in[i + 5] == 1) {
                    const unsigned char x = type == 0x0020 ? 0xFF : 0;  // XOR-MAPPED: with the magic cookie
                    const unsigned char k[4] = {0x21, 0x12, 0xA4, 0x42};
                    found = std::format("{}.{}.{}.{}", in[i + 8] ^ (k[0] & x), in[i + 9] ^ (k[1] & x), in[i + 10] ^ (k[2] & x), in[i + 11] ^ (k[3] & x));
                    break;
                }
                i += 4 + ((len + 3) & ~3);
            }
        }
#if defined(_WIN32)
        closesocket(s);
#else
        close(s);
#endif
    }
    freeaddrinfo(res);
    if (found.empty()) return fail("no answer: no internet, or the firewall stops it");
    return found;
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

void Window::attach(SDL_Window* w, SDL_Renderer* r) {
    win_ = w, ren_ = r, embedded_ = true;
    t0_ = SDL_GetTicks();
    SDL_StopTextInput(win_);
}

Window::~Window() {
    if (embedded_) {
        SDL_StopTextInput(win_);
        return;
    }
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
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: open_ = false, exit.quit = true; break;
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
    if (embedded_) {  // [U-529] the way back to the menu of the game
        SDL_SetRenderClipRect(ren_, nullptr);
        if (back_armed_ > 0) --back_armed_;
        if (gfx::button(ren_, ui, w - 280, 8, 150, 28, back_armed_ > 0 ? loc::tr("Click again: not saved") : loc::tr("Back to the game"), back_armed_ > 0)) {
            if (!dirty || back_armed_ > 0) closing_ = true;
            else back_armed_ = 240;
        }
    }
    gfx::end_frame_text();
    SDL_RenderPresent(ren_);
}

}  // namespace em::tool
