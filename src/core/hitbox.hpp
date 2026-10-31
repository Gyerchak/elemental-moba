#pragma once
// [U-129..U-133][U-140] Hitboxes. Everything is circles in game units, integer math only [X-04].
//  - Units: a BODY made of a few circles (shoulders, belly, butt, tail…) [U-130][X-69],
//    scaled by the model size stat [U-129], moving with the walk animation.
//  - Skills: a FLUID shape = union of wobbling blobs [U-131][X-70].
//  - One hit rule: any touch of two circles = hit ("1 pixel" = 1 game unit) [U-132][X-71].
//  - Autoattack range: edge to closest edge [U-133].
#include <array>
#include <cstdint>

#include "core/light.hpp"  // isqrt

namespace em {

// Q14 unit vectors: 16384 = 1.0.
inline constexpr std::int32_t kOne = 16'384;
struct Dir {
    std::int32_t x = kOne, y = 0;
};

// 64 directions around the circle (cos, sin) in Q14 — generated once, no float at runtime.
inline constexpr Dir kDirs[64] = {
    {16384, 0}, {16305, 1606}, {16069, 3196}, {15679, 4756}, {15137, 6270}, {14449, 7723}, {13623, 9102},
    {12665, 10394}, {11585, 11585}, {10394, 12665}, {9102, 13623}, {7723, 14449}, {6270, 15137}, {4756, 15679},
    {3196, 16069}, {1606, 16305}, {0, 16384}, {-1606, 16305}, {-3196, 16069}, {-4756, 15679}, {-6270, 15137},
    {-7723, 14449}, {-9102, 13623}, {-10394, 12665}, {-11585, 11585}, {-12665, 10394}, {-13623, 9102},
    {-14449, 7723}, {-15137, 6270}, {-15679, 4756}, {-16069, 3196}, {-16305, 1606}, {-16384, 0}, {-16305, -1606},
    {-16069, -3196}, {-15679, -4756}, {-15137, -6270}, {-14449, -7723}, {-13623, -9102}, {-12665, -10394},
    {-11585, -11585}, {-10394, -12665}, {-9102, -13623}, {-7723, -14449}, {-6270, -15137}, {-4756, -15679},
    {-3196, -16069}, {-1606, -16305}, {0, -16384}, {1606, -16305}, {3196, -16069}, {4756, -15679}, {6270, -15137},
    {7723, -14449}, {9102, -13623}, {10394, -12665}, {11585, -11585}, {12665, -10394}, {13623, -9102},
    {14449, -7723}, {15137, -6270}, {15679, -4756}, {16069, -3196}, {16305, -1606},
};

// Normalise any vector to Q14. Zero vector → (1, 0).
[[nodiscard]] constexpr Dir dir_of(std::int64_t dx, std::int64_t dy) {
    const std::int64_t len = isqrt(dx * dx + dy * dy);
    if (len == 0) return {};
    return {std::int32_t(dx * kOne / len), std::int32_t(dy * kOne / len)};
}
// Rotate local (fwd, side) by facing → world offset.
[[nodiscard]] constexpr std::int32_t rot_x(Dir f, std::int32_t fwd, std::int32_t side) {
    return std::int32_t((std::int64_t(fwd) * f.x - std::int64_t(side) * f.y) / kOne);
}
[[nodiscard]] constexpr std::int32_t rot_y(Dir f, std::int32_t fwd, std::int32_t side) {
    return std::int32_t((std::int64_t(fwd) * f.y + std::int64_t(side) * f.x) / kOne);
}

struct Circle {
    std::int32_t x = 0, y = 0, r = 0;
};

[[nodiscard]] constexpr std::int64_t dist2(std::int64_t ax, std::int64_t ay, std::int64_t bx, std::int64_t by) {
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
}
// [U-132] Touching counts (distance == r1 + r2 is a hit).
[[nodiscard]] constexpr bool touches(const Circle& a, const Circle& b) {
    const std::int64_t rr = std::int64_t(a.r) + b.r;
    return dist2(a.x, a.y, b.x, b.y) <= rr * rr;
}
// Gap between two circle edges; negative = overlap.
[[nodiscard]] constexpr std::int64_t edge_gap(const Circle& a, const Circle& b) {
    return isqrt(dist2(a.x, a.y, b.x, b.y)) - a.r - b.r;
}

// Up to 12 circles; used for both bodies and fluid skill shapes.
inline constexpr int kMaxCircles = 12;
struct Shape {
    std::array<Circle, kMaxCircles> c{};
    std::uint8_t n = 0;
    constexpr void add(Circle k) {
        if (n < kMaxCircles) c[n++] = k;
    }
    [[nodiscard]] constexpr bool empty() const { return n == 0; }
};

[[nodiscard]] constexpr bool touches(const Shape& a, const Shape& b) {
    for (int i = 0; i < a.n; ++i)
        for (int j = 0; j < b.n; ++j)
            if (touches(a.c[i], b.c[j])) return true;
    return false;
}
[[nodiscard]] constexpr bool touches(const Shape& a, const Circle& b) {
    for (int i = 0; i < a.n; ++i)
        if (touches(a.c[i], b)) return true;
    return false;
}
// [U-133] Edge to closest edge.
[[nodiscard]] constexpr std::int64_t edge_gap(const Shape& a, const Shape& b) {
    std::int64_t best = INT64_MAX;
    for (int i = 0; i < a.n; ++i)
        for (int j = 0; j < b.n; ++j) {
            const std::int64_t g = edge_gap(a.c[i], b.c[j]);
            if (g < best) best = g;
        }
    return best;
}
// Point inside the union?
[[nodiscard]] constexpr bool contains(const Shape& s, std::int32_t x, std::int32_t y) {
    return touches(s, Circle{x, y, 0});
}

// ---------------------------------------------------------------- bodies [U-130][X-69]
// One part of the core body in local space: fwd = towards facing, side = to the right.
// swing = how far it moves forward/back over one walk cycle (shoulders, hips, tail).
struct BodyPart {
    std::int16_t fwd = 0, side = 0, r = 0, swing = 0;
};
struct BodyDef {
    std::array<BodyPart, 6> parts{};
    std::uint8_t n = 0;
};

// Placeholder bodies (sizes in game units at 100 % model size). Heads and limbs are NOT core body.
// Humanoid: two shoulders, belly, butt. Slim = air nomad. Broad = earth warden. Beast = tail.
inline constexpr BodyDef kHumanoidBody{{{{0, -22, 16, 6}, {0, 22, 16, 6}, {6, 0, 21, 0}, {-12, 0, 18, -3}}}, 4};
inline constexpr BodyDef kSlimBody{{{{0, -18, 13, 6}, {0, 18, 13, 6}, {4, 0, 17, 0}, {-9, 0, 15, -3}}}, 4};
inline constexpr BodyDef kBroadBody{{{{0, -30, 21, 4}, {0, 30, 21, 4}, {7, 0, 27, 0}, {-15, 0, 24, -3}}}, 4};
inline constexpr BodyDef kMinionBody{{{{0, -14, 12, 4}, {0, 14, 12, 4}, {3, 0, 15, 0}}}, 3};
inline constexpr BodyDef kBeastBody{{{{33, 0, 24, 3}, {0, 0, 33, 0}, {-33, 0, 27, -3}, {-64, 0, 12, 9}, {-82, 0, 9, 15}}},
                                    5};  // spirit animal: chest, belly, butt, tail
inline constexpr BodyDef kTowerBody{{{{0, 0, 75, 0}}}, 1};
inline constexpr BodyDef kNexusBody{{{{0, 0, 140, 0}}}, 1};

// Walk cycle: phase 0..1023 → triangle −1024..+1024.
[[nodiscard]] constexpr std::int32_t walk_wave(std::int32_t phase) {
    const std::int32_t p = phase & 1023;
    return p < 512 ? -1024 + p * 4 : 3072 - p * 4;
}

// [U-129] model size: 100 = normal. The body grows / shrinks around its centre.
[[nodiscard]] constexpr Shape place_body(const BodyDef& b, std::int32_t x, std::int32_t y, Dir facing,
                                         std::int32_t walk_phase, std::int32_t size_pct) {
    Shape s;
    const std::int32_t w = walk_wave(walk_phase);
    for (int i = 0; i < b.n; ++i) {
        const BodyPart& p = b.parts[i];
        // left side swings opposite to the right side (shoulders / hips)
        const std::int32_t sw = p.side < 0 ? -p.swing : p.swing;
        const std::int32_t fwd = (p.fwd + sw * w / 1024) * size_pct / 100;
        const std::int32_t side = p.side * size_pct / 100;
        s.add({x + rot_x(facing, fwd, side), y + rot_y(facing, fwd, side), std::max(1, p.r * size_pct / 100)});
    }
    return s;
}

// Radius of a circle around the whole body (broad phase).
[[nodiscard]] constexpr std::int32_t body_reach(const BodyDef& b, std::int32_t size_pct) {
    std::int32_t m = 0;
    for (int i = 0; i < b.n; ++i) {
        const auto& p = b.parts[i];
        const std::int32_t d = std::int32_t(isqrt(std::int64_t(std::abs(p.fwd) + std::abs(p.swing)) * (std::abs(p.fwd) + std::abs(p.swing)) +
                                                  std::int64_t(p.side) * p.side)) + p.r;
        m = std::max(m, d);
    }
    return m * size_pct / 100;
}

// ---------------------------------------------------------------- fluid skills [U-131][X-70]
// Water and fire flow, earth is almost rigid. Wobble = % of radius, changes smoothly over ticks.
enum class Flow : std::uint8_t { Water, Fire, Earth, Air, Plain };
[[nodiscard]] constexpr int wobble_pct(Flow f) {
    switch (f) {
        case Flow::Water: return 14;
        case Flow::Fire: return 20;
        case Flow::Air: return 10;
        case Flow::Earth: return 3;
        case Flow::Plain: return 0;
    }
    return 0;
}

[[nodiscard]] constexpr std::uint32_t mix_hash(std::uint32_t a, std::uint32_t b) {
    std::uint64_t z = (std::uint64_t(a) << 32 | b) * 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 29)) * 0xBF58476D1CE4E5B9ull;
    return std::uint32_t(z ^ (z >> 32));
}
// Smooth noise −1000..+1000: linear between random keys every 16 ticks.
[[nodiscard]] constexpr std::int32_t smooth_noise(std::uint32_t seed, std::uint32_t tick) {
    const std::uint32_t k = tick / 16, t = tick % 16;
    const std::int32_t a = std::int32_t(mix_hash(seed, k) % 2001) - 1000;
    const std::int32_t b = std::int32_t(mix_hash(seed, k + 1) % 2001) - 1000;
    return a + (b - a) * std::int32_t(t) / 16;
}
[[nodiscard]] constexpr std::int32_t wobble(std::int32_t r, Flow f, std::uint32_t seed, std::uint32_t tick) {
    return std::max(1, r + r * wobble_pct(f) * smooth_noise(seed, tick) / 100'000);
}

// Round blob: a core + 5 satellites that drift around the edge (splash / fireball / dust).
[[nodiscard]] constexpr Shape fluid_blob(std::int32_t x, std::int32_t y, std::int32_t radius, Flow f,
                                         std::uint32_t seed, std::uint32_t tick) {
    Shape s;
    if (f == Flow::Plain) {
        s.add({x, y, radius});
        return s;
    }
    s.add({x, y, wobble(radius * 78 / 100, f, seed, tick)});
    for (std::uint32_t i = 0; i < 5; ++i) {
        const std::int32_t drift = smooth_noise(seed + 7 * i + 1, tick) * wobble_pct(f) / 1000;  // ±20 dir steps
        const Dir d = kDirs[(i * 13 + 64 + std::uint32_t(drift)) & 63];
        const std::int32_t off = radius * 55 / 100;
        s.add({x + d.x * off / kOne, y + d.y * off / kOne, wobble(radius * 45 / 100, f, seed + 31 * i, tick)});
    }
    return s;
}

// Stream: chain of blobs from (x,y) along dir, `length` long, `width` wide (whip, wave, jet, wall).
[[nodiscard]] constexpr Shape fluid_line(std::int32_t x, std::int32_t y, Dir dir, std::int32_t length,
                                         std::int32_t width, Flow f, std::uint32_t seed, std::uint32_t tick) {
    Shape s;
    const std::int32_t r = std::max(1, width / 2);
    const std::int32_t steps = std::clamp(length / std::max(1, r) + 1, 1, kMaxCircles);
    for (std::int32_t i = 0; i < steps; ++i) {
        const std::int32_t along = steps == 1 ? 0 : length * i / (steps - 1);
        const std::int32_t sway = width * wobble_pct(f) * smooth_noise(seed + 101 * std::uint32_t(i), tick) / 200'000;
        s.add({x + rot_x(dir, along, sway), y + rot_y(dir, along, sway), wobble(r, f, seed + std::uint32_t(i), tick)});
    }
    return s;
}

// Cone / fan: widens from 20 % of `end_width` at the caster to `end_width` at `length`.
[[nodiscard]] constexpr Shape fluid_cone(std::int32_t x, std::int32_t y, Dir dir, std::int32_t length,
                                         std::int32_t end_width, Flow f, std::uint32_t seed, std::uint32_t tick) {
    Shape s;
    constexpr int kRows = 4;
    for (int row = 1; row <= kRows; ++row) {
        const std::int32_t along = length * row / kRows;
        const std::int32_t w = end_width * (20 + 80 * row / kRows) / 100;
        const int per = row <= 2 ? 1 : row == 3 ? 3 : 3;
        const std::int32_t r = std::max<std::int32_t>(1, per == 1 ? w / 2 : w / 4);
        for (int k = 0; k < per; ++k) {
            const std::int32_t side = per == 1 ? 0 : (k - 1) * w / 3;
            const std::uint32_t sd = seed + std::uint32_t(row * 17 + k);
            s.add({x + rot_x(dir, along - r / 2, side), y + rot_y(dir, along - r / 2, side), wobble(r, f, sd, tick)});
        }
    }
    return s;
}

// [U-462] Ring: 12 circles around the band between `inner` and `outer` (the hole is cut out by the hit test).
[[nodiscard]] constexpr Shape ring_shape(std::int32_t x, std::int32_t y, std::int32_t outer, std::int32_t inner) {
    Shape s;
    inner = std::clamp(inner, 0, std::max(0, outer - 8));
    const std::int32_t mid = (outer + inner) / 2, half = std::max(4, (outer - inner) / 2);
    const std::int32_t gap = mid * 27 / 100;  // ≈ π·mid / 12 → the circles overlap along the ring
    const std::int32_t r = std::max(half, gap);
    for (int i = 0; i < kMaxCircles; ++i) {
        const Dir d = kDirs[std::size_t(i * 64 / kMaxCircles) & 63];
        s.add({x + std::int32_t(std::int64_t(d.x) * mid / kOne), y + std::int32_t(std::int64_t(d.y) * mid / kOne), r});
    }
    return s;
}

// ---------------------------------------------------------------- bushes [U-140][X-78]
// Hidden only if EVERY sample point of every body circle is on a brush cell.
// Samples: centre + 12 points on the edge of each circle.
template <class IsBrushAt>
[[nodiscard]] constexpr bool fully_covered(const Shape& body, IsBrushAt&& is_brush) {
    for (int i = 0; i < body.n; ++i) {
        const Circle& c = body.c[i];
        if (!is_brush(c.x, c.y)) return false;
        for (int k = 0; k < 64; k += 64 / 12 + 1) {  // 0,6,12…60 → 11 points + centre
            const Dir d = kDirs[k];
            if (!is_brush(c.x + d.x * c.r / kOne, c.y + d.y * c.r / kOne)) return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------- checks
static_assert(touches(Circle{0, 0, 10}, Circle{20, 0, 10}));    // exactly touching = hit [U-132]
static_assert(!touches(Circle{0, 0, 10}, Circle{21, 0, 10}));
static_assert(edge_gap(Circle{0, 0, 10}, Circle{100, 0, 20}) == 70);  // edge to edge [U-133]
static_assert(dir_of(3, 4).x == 9'830 && dir_of(3, 4).y == 13'107);
static_assert(walk_wave(0) == -1024 && walk_wave(256) == 0 && walk_wave(512) == 1024 && walk_wave(768) == 0);
// [U-129] Double model size = double body.
static_assert(place_body(kHumanoidBody, 0, 0, {}, 256, 200).c[2].r == 42);
static_assert(place_body(kHumanoidBody, 0, 0, {}, 256, 100).c[1].y == 22);
// Facing down (0, 1): the right shoulder (side +22) ends up at x = −22.
static_assert(place_body(kHumanoidBody, 0, 0, {0, kOne}, 256, 100).c[1].x == -22);
// [U-133] Two bodies 100 units apart, both facing +x: belly (6, r21) to the other butt (88, r18) → 82 − 39 = 43.
static_assert(edge_gap(place_body(kHumanoidBody, 0, 0, {}, 256, 100), place_body(kHumanoidBody, 100, 0, {}, 256, 100)) == 43);
// [U-131] Fluid shapes stay inside 125 % of their nominal size and still hit their centre.
static_assert([] {
    for (std::uint32_t t = 0; t < 200; t += 7) {
        const Shape s = fluid_blob(0, 0, 100, Flow::Fire, 42, t);
        if (!contains(s, 0, 0)) return false;
        for (int i = 0; i < s.n; ++i)
            if (isqrt(dist2(s.c[i].x, s.c[i].y, 0, 0)) + s.c[i].r > 125) return false;
    }
    return true;
}());
// [U-140] A body with one point outside the brush is NOT hidden.
static_assert(fully_covered(place_body(kHumanoidBody, 0, 0, {}, 256, 100), [](int x, int) { return x < 40; }));
static_assert(!fully_covered(place_body(kHumanoidBody, 0, 0, {}, 256, 100), [](int x, int) { return x < 15; }));

}  // namespace em
