// [U-503] See particles.hpp.
#include "client/particles.hpp"

#include <algorithm>
#include <cmath>

namespace em {

using namespace gfx;

float Particles::rnd() {
    seed_ = seed_ * 1664525u + 1013904223u;
    return float((seed_ >> 8) & 0xFFFFF) / float(0xFFFFF);
}

// The look of each kind of bending: how a particle of it moves and fades.
void Particles::spawn(Element el, SubElement sub, float x, float y, float spread, float power, Rgba col) {
    static constexpr std::size_t kCap[] = {0, 400, 1000, 2500};
    if (p_.size() >= kCap[std::clamp(density_, 0, 3)]) return;
    const float a = rnd() * 6.2831853f, d = spread * std::sqrt(rnd());
    Particle q{};
    q.x = x + std::cos(a) * d, q.y = y + std::sin(a) * d;
    const float ang = rnd() * 6.2831853f, sp = (0.3f + rnd()) * power;
    q.vx = std::cos(ang) * sp, q.vy = std::sin(ang) * sp;
    q.c = col;
    if (sub == SubElement::Lightning) {  // sparks: fast, short, white-hot
        q.h = 20 + rnd() * 40, q.vx *= 5, q.vy *= 5, q.vh = (rnd() - 0.3f) * 240, q.max = 0.18f + rnd() * 0.22f, q.size = 3 + rnd() * 3, q.gravity = 300, q.add = true;
        q.c = mixc(col, {255, 255, 255, 255}, 60);
    } else if (sub == SubElement::Ice) {  // ice crystals: glitter, fall
        q.h = 25 + rnd() * 50, q.vh = 40 + rnd() * 70, q.max = 0.5f + rnd() * 0.5f, q.size = 3 + rnd() * 3, q.gravity = 260, q.add = true;
        q.c = mixc(col, {255, 255, 255, 255}, 50);
    } else if (sub == SubElement::Steam) {  // steam: big soft puffs
        q.h = 10 + rnd() * 30, q.vh = 30 + rnd() * 35, q.max = 0.8f + rnd() * 0.8f, q.size = 10 + rnd() * 12, q.grow = 18;
        q.c = with_alpha(col, 90);
    } else if (el == Element::Fire) {  // embers: rise, glow, shrink
        q.h = 8 + rnd() * 30, q.vh = 60 + rnd() * 110, q.max = 0.45f + rnd() * 0.7f, q.size = 4 + rnd() * 5, q.grow = -4, q.add = true;
        q.c = mixc(col, {255, 210, 90, 255}, int(rnd() * 70));
    } else if (el == Element::Water) {  // droplets: thrown up, fall back
        q.h = 8 + rnd() * 25, q.vh = 70 + rnd() * 110, q.max = 0.45f + rnd() * 0.4f, q.size = 3 + rnd() * 3, q.gravity = 420;
        q.c = with_alpha(mixc(col, {230, 245, 255, 255}, 35), 220);
    } else if (el == Element::Earth) {  // dust + small stones
        const bool stone = rnd() < 0.35f;
        q.h = 4 + rnd() * 12, q.vh = stone ? 90 + rnd() * 110 : 12 + rnd() * 25, q.max = stone ? 0.5f + rnd() * 0.3f : 0.7f + rnd() * 0.7f;
        q.size = stone ? 3 + rnd() * 3 : 10 + rnd() * 10, q.grow = stone ? 0 : 14, q.gravity = stone ? 480 : 0;
        q.c = stone ? mixc(col, {60, 50, 40, 255}, 50) : with_alpha(mixc(col, {150, 130, 100, 255}, 60), 95);
    } else if (el == Element::Air) {  // wisps: drift, pale
        q.h = 10 + rnd() * 45, q.vx *= 1.8f, q.vy *= 1.8f, q.vh = 10 + rnd() * 20, q.max = 0.5f + rnd() * 0.6f, q.size = 6 + rnd() * 8, q.grow = 8, q.add = true;
        q.c = with_alpha(mixc(col, {255, 255, 255, 255}, 70), 110);
    } else {  // physical: blood-red / grey sparks
        q.h = 25 + rnd() * 30, q.vx *= 2, q.vy *= 2, q.vh = 40 + rnd() * 80, q.max = 0.25f + rnd() * 0.3f, q.size = 3 + rnd() * 3, q.gravity = 380, q.add = true;
    }
    q.life = q.max;
    p_.push_back(q);
}

void Particles::trail(Element el, SubElement sub, float x, float y, float radius, float dt, Rgba col, float boost) {
    if (density_ <= 0) return;
    // about 26 / 60 / 110 per second for a skill the size of a bolt; big areas emit more
    static constexpr float kRate[] = {0, 26, 60, 110};
    float want = kRate[std::clamp(density_, 0, 3)] * dt * boost * std::clamp(radius / 40.f, 0.6f, 5.f);
    for (; want > 0; want -= 1)
        if (want >= 1 || rnd() < want) spawn(el, sub, x, y, radius, 26, col);
}

void Particles::burst(Element el, SubElement sub, float x, float y, float radius, Rgba col, float amount) {
    if (density_ <= 0) return;
    const int n = int(float(density_ * 8) * amount * std::clamp(radius / 40.f, 0.7f, 4.f));
    for (int i = 0; i < n; ++i) spawn(el, sub, x, y, radius * 0.6f, 150, col);
}

void Particles::step(float dt) {
    for (Particle& q : p_) {
        q.life -= dt;
        q.x += q.vx * dt, q.y += q.vy * dt;
        q.h = std::max(0.f, q.h + q.vh * dt);
        q.vh -= q.gravity * dt;
        q.size = std::max(0.5f, q.size + q.grow * dt);
        q.vx *= 1.f - 1.2f * dt, q.vy *= 1.f - 1.2f * dt;
    }
    std::erase_if(p_, [](const Particle& q) { return q.life <= 0; });
}

void Particles::draw(SDL_Renderer* r, const View& v) const {
    for (int pass = 0; pass < 2; ++pass)  // soft smoke first, glowing ones over it
        for (const Particle& q : p_) {
            if (q.add != (pass == 1)) continue;
            const float sx = v.sx(q.x), sy = v.sy_h(q.y, q.h), rr = q.size * v.zoom;
            if (sx < -60 || sy < -60 || sx > v.sw + 60 || sy > v.sh + 60) continue;
            const float f = std::clamp(q.life / q.max, 0.f, 1.f);
            soft_disc(r, sx, sy, rr, rr, with_alpha(q.c, std::uint8_t(float(q.c.a) * f)), q.add);
        }
}

}  // namespace em
