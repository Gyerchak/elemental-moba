#pragma once
// [U-25][U-115..U-118] Light vision — a second kind of vision next to normal sight.
// At night everyone's own sight is short (new moon ≈ 0 %). Light is a team-neutral
// layer over the grid: a lit cell can be seen by EVERY team from up to their full
// (day) sight range. Light "opens a hole" in the night vision fog [?-19].
// Terrain line of sight is still checked by the vision system [X-10].
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "core/abilities.hpp"
#include "core/grid.hpp"
#include "core/world_clock.hpp"

namespace em {

struct LightSource {
    std::int32_t x = 0, y = 0;        // game units
    std::int32_t radius = 0;          // game units
    std::uint8_t strength_pct = 100;  // light in the centre
};

[[nodiscard]] constexpr std::int64_t isqrt(std::int64_t v) {
    if (v <= 0) return 0;
    std::int64_t x = v, y = (v + 1) / 2;  // Newton, stops when it stops shrinking
    while (y < x) {
        x = y;
        y = (x + v / x) / 2;
    }
    return x;
}

// [X-60] Full strength in the inner half, then linear down to 0 at the edge.
[[nodiscard]] constexpr int light_falloff_pct(std::int64_t dist, std::int64_t radius, int strength) {
    if (radius <= 0 || dist >= radius) return 0;
    const std::int64_t inner = radius / 2;
    if (dist <= inner) return strength;
    return int(strength * (radius - dist) / (radius - inner));
}

// [U-117] The rule of night vision: a unit sees a target at `dist` if
//   dist ≤ sight × max(ambient %, light % at the target) / 100.
// Ambient = vision_range_pct(sky) (day 100 %, new moon 5 %) [U-108][U-115].
[[nodiscard]] constexpr bool in_sight(std::int64_t dist, std::int64_t sight, int ambient_pct, int light_pct) {
    return dist * 100 <= sight * std::max(ambient_pct, light_pct);
}

class LightMap {
public:
    LightMap() : pct_(std::size_t(kGridSize) * kGridSize, 0) {}

    void clear() { std::ranges::fill(pct_, std::uint8_t{0}); }

    [[nodiscard]] int at(int x, int y) const { return Grid::in_bounds(x, y) ? pct_[index(x, y)] : 0; }

    // [X-60] Brightest source wins (no sum) → result does not depend on order.
    // Cells with full vision fog stop light; partial fog / brush let 50 % through.
    void add(const LightSource& s, const Grid& g) {
        const int cx = Grid::to_cell(s.x), cy = Grid::to_cell(s.y);
        const int r = s.radius / kCellUnits + 1;
        for (int y = cy - r; y <= cy + r; ++y) {
            for (int x = cx - r; x <= cx + r; ++x) {
                if (!Grid::in_bounds(x, y)) continue;
                const std::int64_t dx = std::int64_t(x) * kCellUnits + kCellUnits / 2 - s.x;
                const std::int64_t dy = std::int64_t(y) * kCellUnits + kCellUnits / 2 - s.y;
                int v = light_falloff_pct(isqrt(dx * dx + dy * dy), s.radius, s.strength_pct);
                if (v == 0) continue;
                v = v * pass_pct(g, cx, cy, x, y) / 100;
                std::uint8_t& p = pct_[index(x, y)];
                p = std::uint8_t(std::max<int>(p, v));
            }
        }
    }

private:
    static constexpr std::size_t index(int x, int y) { return std::size_t(y) * kGridSize + std::size_t(x); }

    // Walk the cells between source and target (both ends excluded).
    static int pass_pct(const Grid& g, int x0, int y0, int x1, int y1) {
        // the light's own cell: nothing in between. (Without this the walk below stepped AWAY from the
        // target and could hit any tree on that side → random dark spots under lights [U-256].)
        if (x0 == x1 && y0 == y1) return 100;
        int pass = 100;
        const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        int x = x0, y = y0;
        while (true) {
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x += sx; }
            if (e2 <= dx) { err += dx; y += sy; }
            if ((x == x1 && y == y1) || !Grid::in_bounds(x, y)) return pass;
            switch (g.at(x, y).vision_fog()) {
                case SenseBlock::Full: return 0;
                case SenseBlock::Partial:
                case SenseBlock::Brush: pass /= 2; break;
                case SenseBlock::None: break;
            }
        }
    }

    std::vector<std::uint8_t> pct_;  // 0..100 per cell, 256 KiB. Rebuild at 16 Hz, not every tick [X-60].
};

// [U-118][U-79] Placeholder firebender special skill "Sky Lantern" (F/G):
// throws a slow flame that hangs in the air and lights a big area for 20 s [X-62].
inline constexpr AbilityDef kSkyLantern{
    .slot = Slot::F,
    .sub = SubElement::Fire,
    .delivery = Delivery::Skillshot,
    .cast_move = CastMove::Free,
    .range = {.base = 900, .per_rank = 50},
    .radius = {.base = 64},
    .speed = {.base = 700},
    .cast_time_ms = {.base = 200},
    .cooldown_ms = {.base = 60'000},
    .cost = {.base = 30},
    .light_radius = {.base = 640, .per_rank = 64},  // 20 cells, +2 per rank [U-112]
    .light_ms = {.base = 20'000},
};

// Checks.
static_assert(isqrt(0) == 0 && isqrt(99) == 9 && isqrt(100) == 10 && isqrt(1'000'000) == 1'000);
static_assert(light_falloff_pct(0, 192, 100) == 100 && light_falloff_pct(96, 192, 100) == 100 &&
              light_falloff_pct(144, 192, 100) == 50 && light_falloff_pct(192, 192, 100) == 0);
// New moon: 1200 sight sees only 60 units in the dark, but a fully lit target at 1000.
static_assert(in_sight(60, 1'200, 5, 0) && !in_sight(61, 1'200, 5, 0));
static_assert(in_sight(1'000, 1'200, 5, 100) && !in_sight(1'201, 1'200, 5, 100));
// Day: light changes nothing.
static_assert(in_sight(1'200, 1'200, 100, 0));
static_assert(resolve(kSkyLantern, {.stats = Stats{}, .rank = 3}).light_radius == 768);

}  // namespace em
