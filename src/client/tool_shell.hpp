#pragma once
// [U-510][U-511] What the small apps (champion maker, server) share with the game: the game folder, a Vulkan window, the
// immediate-mode UI input of client/draw.hpp, the language, opening another program of the build.
#include <SDL3/SDL.h>

#include <functional>
#include <string>

#include "client/draw.hpp"

namespace em::tool {

void go_to_game_folder();          // the game may be started from anywhere: work from where resourcepacks/ and lang/ are
[[nodiscard]] std::string lan_address();
[[nodiscard]] std::string language_of_game();          // `language=` of settings.cfg ("en" if none)
[[nodiscard]] std::string next_language(const std::string& code);  // the next one of lang/ (cycles)
bool open_program(const char* name, std::string* error = nullptr);  // starts em_… next to this program; it runs on its own

class Window {
public:
    bool open(const char* title, int w, int h);
    ~Window();
    // One frame: reads the input; `extra` sees every event first. Returns false when the window is closed.
    bool begin(const std::function<void(const SDL_Event&)>& extra = {});
    void end();
    [[nodiscard]] SDL_Renderer* ren() const { return ren_; }
    [[nodiscard]] SDL_Window* win() const { return win_; }
    gfx::Ui ui;
    float w = 0, h = 0;
    double now_s = 0;

private:
    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    std::uint64_t t0_ = 0;
    int frames_ = 0;
    bool closing_ = false;
};

}  // namespace em::tool
