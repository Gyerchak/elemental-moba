#pragma once
// [U-03][X-17] Placeholder renderer primitives on SDL3's 2D renderer: coloured circles, precise
// union outlines for fluid skills [U-131], font text. Immediate-mode widgets for menus.
#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "core/hitbox.hpp"

namespace em::gfx {

struct Rgba {
    std::uint8_t r = 255, g = 255, b = 255, a = 255;
};
[[nodiscard]] constexpr Rgba hex(std::uint32_t c, std::uint8_t a = 255) {
    return {std::uint8_t(c >> 16), std::uint8_t(c >> 8), std::uint8_t(c), a};
}
[[nodiscard]] constexpr Rgba with_alpha(Rgba c, std::uint8_t a) { return {c.r, c.g, c.b, a}; }
[[nodiscard]] constexpr Rgba mixc(Rgba a, Rgba b, int pct) {
    auto m = [&](std::uint8_t x, std::uint8_t y) { return std::uint8_t((x * (100 - pct) + y * pct) / 100); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}

void set_color(SDL_Renderer* r, Rgba c);
// [U-268] resource bar colours: calm flame (steady warm orange), rage (chaotic flame: flickers red ↔ orange)
[[nodiscard]] Rgba resource_colour(std::uint8_t kind, std::uint64_t ticks_ms);
void fill_circle(SDL_Renderer* r, float x, float y, float rad, Rgba c, int segments = 24);
void circle(SDL_Renderer* r, float x, float y, float rad, Rgba c, int segments = 32);
void fill_rect(SDL_Renderer* r, float x, float y, float w, float h, Rgba c);
void rect(SDL_Renderer* r, float x, float y, float w, float h, Rgba c);
void line(SDL_Renderer* r, float x0, float y0, float x1, float y1, Rgba c);

// [U-250] UTF-8 text in DejaVu Sans Mono (resourcepacks/default/fonts) with a thin dark outline, so Polish letters
// and symbols (− × → …) show and text stays readable on the map. `scale` 1 ≈ the old 8 px debug font.
// Falls back to the SDL debug font when the font file is missing. Strings are cached between frames.
void text(SDL_Renderer* r, float x, float y, std::string_view s, Rgba c = {}, float scale = 1);
[[nodiscard]] float text_w(std::string_view s, float scale = 1);
void init_text(SDL_Renderer* r);  // once, after the renderer exists
// Cut `s` (UTF-8 safe) so it fits in `max_w` pixels, ending with "…" when cut [U-250] (no text over other text).
[[nodiscard]] std::string fit_text(std::string s, float max_w, float scale = 1);
// [U-444] a cooldown sweep: the part of the square [x, y, s] still on cooldown (fraction 0 … 1), clockwise from 12 o'clock
void cooldown_sweep(SDL_Renderer* r, float x, float y, float s, float fraction, Rgba c);
// a ring arc (progress 0 … 1, clockwise from 12 o'clock) around a centre
void ring_arc(SDL_Renderer* r, float cx, float cy, float rad, float progress, Rgba c, float thick = 2);
// [U-389] the first n CHARACTERS (UTF-8 safe: never half a Polish letter)
[[nodiscard]] inline std::string utf8_cut(std::string_view s, std::size_t n) {
    std::size_t i = 0, chars = 0;
    while (i < s.size() && chars < n) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        i += c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        ++chars;
    }
    return std::string(s.substr(0, std::min(i, s.size())));
}
void end_frame_text();            // once per frame: drops strings not drawn for a while

// World → screen transform [U-174][U-175].
//  flip: the Earth Nation sees the map turned 180°, so EVERY team has its base bottom-left.
//  gy:   ground squash of the tilted LoL-like camera (1 = top-down debug view).
//  hz:   how much height (units above the ground) moves things up the screen (0 = top-down).
inline constexpr float kViewMap = 16'384.f;
struct View {
    float cx = 0, cy = 0;  // world units at screen centre
    float zoom = 0.85f;    // pixels per unit
    float sw = 1280, sh = 720;
    bool flip = false;
    float gy = 1.f, hz = 0.f;
    [[nodiscard]] float fx(float w) const { return flip ? kViewMap - w : w; }
    [[nodiscard]] float sx(float wx) const { return (fx(wx) - fx(cx)) * zoom + sw / 2; }
    [[nodiscard]] float sy(float wy) const { return (fx(wy) - fx(cy)) * zoom * gy + sh / 2; }
    [[nodiscard]] float sy_h(float wy, float height) const { return sy(wy) - height * zoom * hz; }
    [[nodiscard]] float wx(float px) const { const float t = (px - sw / 2) / zoom + fx(cx); return flip ? kViewMap - t : t; }
    [[nodiscard]] float wy(float py) const { const float t = (py - sh / 2) / (zoom * gy) + fx(cy); return flip ? kViewMap - t : t; }
    [[nodiscard]] float rx(float r) const { return r * zoom; }
    [[nodiscard]] float ry(float r) const { return r * zoom * gy; }
};

void fill_ellipse(SDL_Renderer* r, float x, float y, float rx, float ry, Rgba c, int segments = 24);
void ellipse(SDL_Renderer* r, float x, float y, float rx, float ry, Rgba c, int segments = 32);
// a circle lying on the ground (an ellipse when the camera is tilted)
inline void ground_fill(SDL_Renderer* r, const View& v, float wx, float wy, float wr, Rgba c, int seg = 24) {
    fill_ellipse(r, v.sx(wx), v.sy(wy), v.rx(wr), v.ry(wr), c, seg);
}
inline void ground_ring(SDL_Renderer* r, const View& v, float wx, float wy, float wr, Rgba c, int seg = 48) {
    ellipse(r, v.sx(wx), v.sy(wy), v.rx(wr), v.ry(wr), c, seg);
}

// Filled union of circles + the exact outer edge (only arc parts not inside another circle) [U-131].
void fill_shape(SDL_Renderer* r, const View& v, const Shape& s, Rgba fill);
void outline_shape(SDL_Renderer* r, const View& v, const Shape& s, Rgba edge, float thickness = 1.5f);

// ---- tiny immediate-mode UI
struct Ui {
    float mx = 0, my = 0;
    bool clicked = false, rclicked = false;  // this frame
    bool down = false;
    std::string typed;   // text typed this frame
    bool backspace = false, enter = false;
    int focus = -1;      // focused text field id
    int drag = -1;       // slider being dragged
    float wheel = 0;     // mouse wheel this frame (+ = up)
};
bool button(SDL_Renderer* r, Ui& ui, float x, float y, float w, float h, std::string_view label, bool active = false,
            bool enabled = true);
// Drag a value between lo and hi; returns true while it changes.
bool slider(SDL_Renderer* r, Ui& ui, int id, float x, float y, float w, int& value, int lo, int hi, std::string_view label);
// Returns true when Enter is pressed in the field.
bool text_field(SDL_Renderer* r, Ui& ui, int id, float x, float y, float w, std::string& value, std::string_view label);
[[nodiscard]] inline bool hover(const Ui& ui, float x, float y, float w, float h) {
    return ui.mx >= x && ui.mx < x + w && ui.my >= y && ui.my < y + h;
}

}  // namespace em::gfx
