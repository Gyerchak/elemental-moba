#pragma once
// [U-503] Skill particles (client only, purely visual): embers, droplets, dust, wisps, sparks. World positions + a height,
// drawn inside the world image with soft discs (so they follow the quality level and the bloom like everything else).
#include <vector>

#include "client/draw.hpp"
#include "core/elements.hpp"

namespace em {

struct Particle {
    float x, y, h, vx, vy, vh, life, max, size, grow, gravity;
    gfx::Rgba c;
    bool add;
};

class Particles {
public:
    void set_density(int d) { density_ = d; }  // 0 off, 1 low, 2 medium, 3 high
    [[nodiscard]] int density() const { return density_; }
    void clear() { p_.clear(); }
    void trail(Element el, SubElement sub, float x, float y, float radius, float dt, gfx::Rgba col, float boost = 1);  // an effect alive now
    void burst(Element el, SubElement sub, float x, float y, float radius, gfx::Rgba col, float amount = 1);           // start / impact
    void step(float dt);
    void draw(SDL_Renderer* r, const gfx::View& v) const;

private:
    void spawn(Element el, SubElement sub, float x, float y, float spread, float power, gfx::Rgba col);
    [[nodiscard]] float rnd();
    std::vector<Particle> p_;
    std::uint32_t seed_ = 12345;
    int density_ = 3;
};

}  // namespace em
