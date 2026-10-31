// Drawing primitives. See draw.hpp.
#include "client/draw.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3_ttf/SDL_ttf.h>

namespace em::gfx {

void set_color(SDL_Renderer* r, Rgba c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a); }

Rgba resource_colour(std::uint8_t kind, std::uint64_t t) {
    switch (kind) {
        case 0: return {60, 150, 255, 255};   // water
        case 1: return {255, 175, 60, 255};   // calm flame (fuel)
        case 5: {                              // rage: chaotic flame
            const float f = 0.5f + 0.5f * std::sin(float(t % 100'000) * 0.023f) * std::sin(float(t % 100'000) * 0.0071f);
            return mixc({200, 20, 40, 255}, {255, 110, 0, 255}, int(f * 100));
        }
        default: return {235, 225, 90, 255};  // energy / ammo
    }
}

// [U-467] whole-screen glitches: ONE triangle with a NaN / infinite / gigantic corner (a bad position, a zoom of 0, a
// huge radius) is stretched over the whole screen by the GPU. Every primitive checks its numbers first and skips such
// a shape (a missing circle for one frame is invisible; a screen-wide smear is not).
namespace {
constexpr float kSaneLimit = 60'000.f;
[[nodiscard]] bool sane(float v) { return std::isfinite(v) && std::abs(v) < kSaneLimit; }
[[nodiscard]] bool sane(float x, float y, float r = 0) { return sane(x) && sane(y) && sane(r) && r < kSaneLimit / 4; }
}  // namespace

void fill_circle(SDL_Renderer* r, float x, float y, float rad, Rgba c, int seg) {
    if (!sane(x, y, rad)) return;
    if (rad <= 0.3f) return;
    // [U-389] reused buffers: no heap allocation per shape (hundreds per frame used to hitch)
    thread_local std::vector<SDL_Vertex> v;
    thread_local std::vector<int> idx;
    v.resize(std::size_t(seg) + 1);
    idx.clear();
    const SDL_FColor fc{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f};
    v[0] = {{x, y}, fc, {0, 0}};
    for (int i = 0; i < seg; ++i) {
        const float a = float(i) * 6.2831853f / float(seg);
        v[std::size_t(i) + 1] = {{x + std::cos(a) * rad, y + std::sin(a) * rad}, fc, {0, 0}};
        idx.insert(idx.end(), {0, i + 1, (i + 1) % seg + 1});
    }
    SDL_RenderGeometry(r, nullptr, v.data(), int(v.size()), idx.data(), int(idx.size()));
}

void circle(SDL_Renderer* r, float x, float y, float rad, Rgba c, int seg) {
    if (!sane(x, y, rad)) return;  // [U-467]
    thread_local std::vector<SDL_FPoint> p;  // [U-389] reused
    p.resize(std::size_t(seg) + 1);
    for (int i = 0; i <= seg; ++i) {
        const float a = float(i) * 6.2831853f / float(seg);
        p[std::size_t(i)] = {x + std::cos(a) * rad, y + std::sin(a) * rad};
    }
    set_color(r, c);
    SDL_RenderLines(r, p.data(), int(p.size()));
}

void fill_ellipse(SDL_Renderer* r, float x, float y, float rx, float ry, Rgba c, int seg) {
    if (rx <= 0.3f || ry <= 0.2f) return;
    if (!sane(x, y, rx) || !sane(ry) || ry > kSaneLimit / 4) return;  // [U-467]
    // [U-389] reused buffers: no heap allocation per shape (hundreds per frame used to hitch)
    thread_local std::vector<SDL_Vertex> v;
    thread_local std::vector<int> idx;
    v.resize(std::size_t(seg) + 1);
    idx.clear();
    const SDL_FColor fc{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f};
    v[0] = {{x, y}, fc, {0, 0}};
    for (int i = 0; i < seg; ++i) {
        const float a = float(i) * 6.2831853f / float(seg);
        v[std::size_t(i) + 1] = {{x + std::cos(a) * rx, y + std::sin(a) * ry}, fc, {0, 0}};
        idx.insert(idx.end(), {0, i + 1, (i + 1) % seg + 1});
    }
    SDL_RenderGeometry(r, nullptr, v.data(), int(v.size()), idx.data(), int(idx.size()));
}

void ellipse(SDL_Renderer* r, float x, float y, float rx, float ry, Rgba c, int seg) {
    if (!sane(x, y, rx) || !sane(ry) || ry > kSaneLimit / 4) return;  // [U-467]
    thread_local std::vector<SDL_FPoint> p;  // [U-389] reused
    p.resize(std::size_t(seg) + 1);
    for (int i = 0; i <= seg; ++i) {
        const float a = float(i) * 6.2831853f / float(seg);
        p[std::size_t(i)] = {x + std::cos(a) * rx, y + std::sin(a) * ry};
    }
    set_color(r, c);
    SDL_RenderLines(r, p.data(), int(p.size()));
}

void cooldown_sweep(SDL_Renderer* r, float x, float y, float s, float fraction, Rgba c) {
    fraction = std::clamp(fraction, 0.f, 1.f);
    if (fraction <= 0) return;
    // a fan from the centre: the cooled-down part (from 12 o'clock clockwise) is open, the rest is shaded — clipped to
    // the square by projecting each ray onto the square's border
    const float cx = x + s / 2, cy = y + s / 2, h = s / 2;
    const int steps = std::max(4, int(64 * fraction));
    thread_local std::vector<SDL_Vertex> v;
    thread_local std::vector<int> idx;
    v.clear(), idx.clear();
    const SDL_FColor fc{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f};
    v.push_back({{cx, cy}, fc, {0, 0}});
    const float start = (1 - fraction) * 6.2831853f;  // the shaded part ends at 12 o'clock (it shrinks clockwise)
    for (int i = 0; i <= steps; ++i) {
        const float a = start + float(i) / float(steps) * fraction * 6.2831853f;
        const float dx = std::sin(a), dy = -std::cos(a);
        const float k = h / std::max(std::abs(dx), std::abs(dy));  // to the square's border
        v.push_back({{cx + dx * k, cy + dy * k}, fc, {0, 0}});
        if (i > 0) idx.insert(idx.end(), {0, i, i + 1});
    }
    SDL_RenderGeometry(r, nullptr, v.data(), int(v.size()), idx.data(), int(idx.size()));
}

void ring_arc(SDL_Renderer* r, float cx, float cy, float rad, float progress, Rgba c, float thick) {
    progress = std::clamp(progress, 0.f, 1.f);
    const int steps = std::max(3, int(40 * progress));
    set_color(r, c);
    for (float t = 0; t < thick; t += 1)
        for (int i = 0; i < steps; ++i) {
            const float a0 = float(i) / float(steps) * progress * 6.2831853f, a1 = float(i + 1) / float(steps) * progress * 6.2831853f;
            SDL_RenderLine(r, cx + std::sin(a0) * (rad + t), cy - std::cos(a0) * (rad + t), cx + std::sin(a1) * (rad + t), cy - std::cos(a1) * (rad + t));
        }
}

void fill_rect(SDL_Renderer* r, float x, float y, float w, float h, Rgba c) {
    if (!sane(x, y) || !sane(w, h)) return;  // [U-467]
    set_color(r, c);
    const SDL_FRect f{x, y, w, h};
    SDL_RenderFillRect(r, &f);
}
void rect(SDL_Renderer* r, float x, float y, float w, float h, Rgba c) {
    set_color(r, c);
    const SDL_FRect f{x, y, w, h};
    SDL_RenderRect(r, &f);
}
void line(SDL_Renderer* r, float x0, float y0, float x1, float y1, Rgba c) {
    if (!sane(x0, y0) || !sane(x1, y1)) return;  // [U-467]
    set_color(r, c);
    SDL_RenderLine(r, x0, y0, x1, y1);
}

namespace {
struct Cached {
    TTF_Text* t = nullptr;
    std::uint32_t used = 0;
};
struct TextState {
    TTF_TextEngine* engine = nullptr;
    std::unordered_map<int, TTF_Font*> fonts;  // pixel size → font
    std::unordered_map<std::string, Cached> cache;  // size + '\x1f' + string
    std::uint32_t frame = 0;
    bool ok = false;
} g_text;

int px_of(float scale) { return std::max(6, int(std::lround(11.f * scale))); }
TTF_Font* font_px(int px) {
    if (auto it = g_text.fonts.find(px); it != g_text.fonts.end()) return it->second;
    TTF_Font* f = TTF_OpenFont("resourcepacks/default/fonts/DejaVuSansMono.ttf", float(px));
    if (f) TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
    g_text.fonts[px] = f;
    return f;
}
TTF_Text* cached(std::string_view s, float scale) {
    const int px = px_of(scale);
    std::string key = std::to_string(px);
    key += '\x1f';
    key += s;
    auto it = g_text.cache.find(key);
    if (it == g_text.cache.end()) {
        TTF_Font* f = font_px(px);
        if (!f) return nullptr;
        TTF_Text* t = TTF_CreateText(g_text.engine, f, s.data(), s.size());
        it = g_text.cache.emplace(std::move(key), Cached{t, g_text.frame}).first;
    }
    it->second.used = g_text.frame;
    return it->second.t;
}
}  // namespace

void init_text(SDL_Renderer* r) {
    if (g_text.engine) return;
    if (!TTF_WasInit() && !TTF_Init()) return;
    g_text.engine = TTF_CreateRendererTextEngine(r);
    g_text.ok = g_text.engine && font_px(11);
    // [U-338] open the usual sizes now: opening a font mid-game reads the disk → a visible hitch
    for (float sc : {1.f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 2.f, 2.5f, 3.f, 3.9f, 5.f}) (void)font_px(px_of(sc));
}

void end_frame_text() {
    ++g_text.frame;
    if (g_text.frame % 300 != 0) return;
    std::erase_if(g_text.cache, [](auto& kv) {
        if (g_text.frame - kv.second.used < 300) return false;
        if (kv.second.t) TTF_DestroyText(kv.second.t);
        return true;
    });
}

float text_w(std::string_view s, float scale) {
    if (!g_text.ok) return float(s.size()) * 8 * scale;
    int w = 0, h = 0;
    if (TTF_Text* t = cached(s, scale)) TTF_GetTextSize(t, &w, &h);
    return float(w);
}

std::string fit_text(std::string s, float max_w, float scale) {
    if (text_w(s, scale) <= max_w) return s;
    while (!s.empty()) {
        // drop one whole UTF-8 character: its continuation bytes, then its first byte
        while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) s.pop_back();
        if (!s.empty()) s.pop_back();
        if (text_w(s + "…", scale) <= max_w) return s + "…";
    }
    return s;
}

void text(SDL_Renderer* r, float x, float y, std::string_view s, Rgba c, float scale) {
    if (s.empty()) return;
    if (!g_text.ok) {  // fallback: SDL debug font (ASCII only)
        float sx = 1, sy = 1;
        SDL_GetRenderScale(r, &sx, &sy);
        SDL_SetRenderScale(r, scale, scale);
        set_color(r, c);
        std::string str(s);
        SDL_RenderDebugText(r, x / scale, y / scale, str.c_str());
        SDL_SetRenderScale(r, sx, sy);
        return;
    }
    TTF_Text* t = cached(s, scale);
    if (!t) return;
    // the old debug font drew 8 px glyphs from y; this font's caps start ~3 px below its top → lift it
    const float ty = std::round(y - 3.f * scale), tx = std::round(x);
    TTF_SetTextColor(t, 0, 0, 0, std::uint8_t(c.a * 3 / 4));  // outline for contrast on the map
    TTF_DrawRendererText(t, tx + 1, ty + 1);
    TTF_SetTextColor(t, c.r, c.g, c.b, c.a);
    TTF_DrawRendererText(t, tx, ty);
}

void fill_shape(SDL_Renderer* r, const View& v, const Shape& s, Rgba fill) {
    // overlapping alpha circles would look patchy → draw at full alpha into the union by
    // drawing each circle with the same colour (alpha stacks a little, which reads as "depth")
    for (int i = 0; i < s.n; ++i) fill_ellipse(r, v.sx(float(s.c[i].x)), v.sy(float(s.c[i].y)), v.rx(float(s.c[i].r)), v.ry(float(s.c[i].r)), fill);
}

void outline_shape(SDL_Renderer* r, const View& v, const Shape& s, Rgba edge, float thickness) {
    set_color(r, edge);
    for (int i = 0; i < s.n; ++i) {
        const Circle& c = s.c[i];
        constexpr int kSeg = 48;
        SDL_FPoint prev{};
        bool prev_out = false;
        for (int k = 0; k <= kSeg; ++k) {
            const float a = float(k) * 6.2831853f / kSeg;
            const float wx = float(c.x) + std::cos(a) * float(c.r), wy = float(c.y) + std::sin(a) * float(c.r);
            bool out = true;  // edge point not inside any other circle = a real edge of the skill
            for (int j = 0; j < s.n && out; ++j) {
                if (j == i) continue;
                const float dx = wx - float(s.c[j].x), dy = wy - float(s.c[j].y);
                out = dx * dx + dy * dy > float(s.c[j].r) * float(s.c[j].r);
            }
            const SDL_FPoint p{v.sx(wx), v.sy(wy)};
            if (!sane(p.x, p.y)) {  // [U-467]
                prev_out = false;
                continue;
            }
            if (out && prev_out) {
                SDL_RenderLine(r, prev.x, prev.y, p.x, p.y);
                if (thickness > 1.2f) SDL_RenderLine(r, prev.x + 1, prev.y, p.x + 1, p.y);
            }
            prev = p;
            prev_out = out;
        }
    }
}

bool button(SDL_Renderer* r, Ui& ui, float x, float y, float w, float h, std::string_view label, bool active, bool enabled) {
    const bool hv = enabled && hover(ui, x, y, w, h);
    fill_rect(r, x, y, w, h, active ? Rgba{120, 70, 30, 235} : hv ? Rgba{70, 70, 90, 235} : Rgba{38, 38, 50, 225});
    rect(r, x, y, w, h, active ? Rgba{255, 170, 80} : Rgba{110, 110, 140});
    // [U-250] the label always fits: big text if there is room, else normal size, else cut with "…"
    float sc = h >= 34 ? 2.f : 1.f;
    if (sc > 1 && text_w(label, sc) > w - 10) sc = 1;
    const std::string fitted = fit_text(std::string(label), w - 8, sc);
    text(r, x + (w - text_w(fitted, sc)) / 2, y + (h - 8 * sc) / 2, fitted, enabled ? Rgba{235, 235, 240} : Rgba{120, 120, 120}, sc);
    if (hv && ui.clicked) {
        ui.clicked = false;
        return true;
    }
    return false;
}

bool slider(SDL_Renderer* r, Ui& ui, int id, float x, float y, float w, int& value, int lo, int hi, std::string_view label) {
    const float h = 22;
    text(r, x, y + 7, label, {210, 210, 220});
    const float sx = x + 300, sw = w - 360;
    fill_rect(r, sx, y + 9, sw, 4, {60, 60, 80, 255});
    const float f = float(value - lo) / float(std::max(1, hi - lo));
    fill_rect(r, sx, y + 9, sw * f, 4, {255, 190, 90, 255});
    fill_rect(r, sx + sw * f - 5, y + 3, 10, 16, {240, 240, 240, 255});
    text(r, sx + sw + 12, y + 7, std::to_string(value), {255, 255, 255});
    if (ui.clicked && hover(ui, sx - 6, y, sw + 12, h)) {
        ui.drag = id;
        ui.clicked = false;
    }
    if (!ui.down && ui.drag == id) ui.drag = -1;
    if (ui.drag == id) {
        const int v = lo + int(std::lround(std::clamp((ui.mx - sx) / sw, 0.f, 1.f) * float(hi - lo)));
        if (v != value) {
            value = v;
            return true;
        }
    }
    return false;
}

bool text_field(SDL_Renderer* r, Ui& ui, int id, float x, float y, float w, std::string& value, std::string_view label) {
    const float h = 28;
    text(r, x, y - 12, label, {200, 200, 210});
    const bool hv = hover(ui, x, y, w, h);
    if (ui.clicked && hv) {
        ui.focus = id;
        ui.clicked = false;
    }
    const bool f = ui.focus == id;
    fill_rect(r, x, y, w, h, f ? Rgba{30, 30, 45, 240} : Rgba{20, 20, 28, 220});
    rect(r, x, y, w, h, f ? Rgba{255, 200, 120} : Rgba{100, 100, 120});
    bool enter = false;
    if (f) {
        value += ui.typed;
        if (ui.backspace && !value.empty()) value.pop_back();
        if (value.size() > 40) value.resize(40);
        enter = ui.enter;
    }
    const bool blink = (SDL_GetTicks() / 500) % 2 == 0;
    text(r, x + 6, y + 6, value + (f && blink ? "_" : ""), {240, 240, 240}, 2);
    return enter;
}

}  // namespace em::gfx
