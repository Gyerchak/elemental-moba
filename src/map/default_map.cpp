// [U-83..U-89] Default map. Point-symmetric: (x, y) ↔ (511 - x, 511 - y).
// Every cell is computed from its CANONICAL coordinate (the Fire-side twin),
// so the two halves are always exactly identical [U-83].
#include "map/default_map.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace em {
namespace {

constexpr int N = kGridSize;
constexpr int M = N - 1;  // 511

struct P {
    double x, y;
};

// --- layout constants (cells, Fire-side coordinates) ----------------------
constexpr double kSeaHalfWidth = 40;     // [U-85] sea band along x = y
constexpr double kBeach = 5;             // [X-42] sand beach around the sea
constexpr double kLaneHalfWidth = 11;    // [X-07]
constexpr double kLaneEdge = 28;         // top/bot lane distance from map edge
constexpr P kNorthPole{kLaneEdge, kLaneEdge};  // [U-84] top lane corner
constexpr double kPoleRadius = 66;
constexpr P kFireBase{40, M - 40};       // [U-06] Team A base
constexpr double kBaseStone = 38, kBaseDry = 66, kBaseSand = 92;  // [U-87]
constexpr P kPonds[] = {{100, 262}, {252, 420}, {165, 425}};      // [U-89] water sources
constexpr double kPondRadius = 8;
struct Seg {
    P a, b;
};
// [U-87][U-88] shallow rivers: jungle springs → base, jungle springs → sea [?-14]
constexpr Seg kRivers[] = {
    {{100, 262}, {62, 398}},    // top jungle → base
    {{100, 262}, {172, 192}},   // top jungle → sea
    {{252, 420}, {118, 452}},   // bot jungle → base
    {{252, 420}, {334, 346}},   // bot jungle → sea
};
constexpr double kRiverHalfWidth = 3.5;
// [U-269] forest paths: every jungle pond → the nearest outer lane and the mid lane (Claude idea layout)
constexpr Seg kForestPaths[] = {
    {{100, 262}, {28, 262}},  {{100, 262}, {175, 337}},
    {{252, 420}, {252, 484}}, {{252, 420}, {172, 340}},
    {{165, 425}, {165, 484}}, {{165, 425}, {126, 386}},
};
constexpr double kPathHalfWidth = 1.6, kRoadHalfWidth = 3;

double dist(P a, P b) { return std::hypot(a.x - b.x, a.y - b.y); }

double dist_to_segment(P p, Seg s) {
    const double dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
    const double t = std::clamp(((p.x - s.a.x) * dx + (p.y - s.a.y) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
    return dist(p, {s.a.x + t * dx, s.a.y + t * dy});
}

// Lanes on the Fire side of the canonical half [X-07].
double dist_to_lanes(P p) {
    const Seg top_v{{kLaneEdge, double(M) - 60}, {kLaneEdge, kLaneEdge}};  // up the left edge
    const Seg top_h{{kLaneEdge, kLaneEdge}, {double(M) - 60, kLaneEdge}};  // along the top edge
    const Seg bot_h{{60, double(M) - kLaneEdge}, {double(M) - kLaneEdge, double(M) - kLaneEdge}};
    const Seg bot_v{{double(M) - kLaneEdge, double(M) - kLaneEdge}, {double(M) - kLaneEdge, 60}};
    const Seg mid{{kFireBase.x, kFireBase.y}, {double(M) - kFireBase.x, double(M) - kFireBase.y}};
    return std::min({dist_to_segment(p, top_v), dist_to_segment(p, top_h), dist_to_segment(p, bot_h),
                     dist_to_segment(p, bot_v), dist_to_segment(p, mid)});
}

// Deterministic hash (splitmix-style) → same map on every machine.
constexpr std::uint32_t hash(int x, int y, std::uint32_t salt) {
    std::uint64_t z = (std::uint64_t(std::uint32_t(x)) << 32 | std::uint32_t(y)) + salt * 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return std::uint32_t(z ^ (z >> 31));
}

std::pair<int, int> canonical(int x, int y) {
    const int mx = M - x, my = M - y;
    if (y > x || (y == x && x <= mx)) return {x, y};
    return {mx, my};
}

Cell make_cell(int cx, int cy) {
    const P p{cx + 0.5, cy + 0.5};
    Cell c;  // land, elevation 0, walkable [U-83: no elevation anywhere]
    c.set_surface(Surface::Land);

    const double sea_d = std::abs(p.x - p.y) / std::sqrt(2.0);
    const double lane_d = dist_to_lanes(p);
    const double base_d = dist(p, kFireBase);
    const double pole_d = dist(p, kNorthPole);
    const double mid_d = std::abs(p.x + p.y - M) / std::sqrt(2.0);

    // 1. Jungle [U-89]: groves of trees + bushes, kept off the lanes.
    if (lane_d > kLaneHalfWidth + 3) {
        const bool grove = hash(cx / 9, cy / 9, 1) % 100 < 45;
        if (grove && hash(cx, cy, 2) % 100 < 75) c.set_tree(Tree::Alive);
        else if (hash(cx / 4, cy / 4, 3) % 100 < 9) c.set_bush(Bush::Alive);
    }
    // 2. Sea [U-85] deep water [?-08] with beaches [X-42].
    if (sea_d < kSeaHalfWidth + kBeach) {
        c.set_tree(Tree::None);
        c.set_bush(Bush::None);
        c.set_surface(sea_d < kSeaHalfWidth ? Surface::DeepWater : Surface::Sand);
    }
    // 3. Mid land bridge [U-86] (tropical land, a bit wider than the lane).
    if (mid_d < kLaneHalfWidth + 6 && sea_d < kSeaHalfWidth + kBeach) c.set_surface(Surface::Land);
    // 4. Ponds and shallow rivers [U-88][U-89].
    bool river = false;
    for (P pond : kPonds) river |= dist(p, pond) < kPondRadius;
    for (Seg s : kRivers) river |= dist_to_segment(p, s) < kRiverHalfWidth;
    if (river && c.surface() != Surface::DeepWater) {
        c.set_surface(Surface::Water);
        c.set_tree(Tree::None);
        c.set_bush(Bush::None);
    }
    // 4b. Forest paths through the jungle [U-269] (not over water)
    bool on_path = false;
    for (Seg s : kForestPaths) on_path |= dist_to_segment(p, s) < kPathHalfWidth;
    if (on_path && c.surface() != Surface::Water && c.surface() != Surface::DeepWater && sea_d >= kSeaHalfWidth + kBeach) {
        c.set_surface(Surface::ForestPath);
        c.set_tree(Tree::None);
        c.set_bush(Bush::None);
    }
    // 5. Lanes: clear land [X-07]; a stone road in the middle [U-269]; wooden bridges over rivers [?-12]
    if (lane_d < kLaneHalfWidth) {
        c.set_tree(Tree::None);
        c.set_bush(Bush::None);
        if (c.surface() == Surface::DeepWater || c.surface() == Surface::Sand || c.surface() == Surface::ForestPath) c.set_surface(Surface::Land);
        if (c.surface() == Surface::Water) c.set_surface(Surface::Wood);
        else if (lane_d < kRoadHalfWidth && c.surface() == Surface::Land) c.set_surface(Surface::StoneRoad);
    }
    // 6. Base: stone core, dryland ring, desert edge [U-87].
    if (base_d < kBaseSand) {
        c.set_tree(Tree::None);
        c.set_bush(Bush::None);
        if (c.surface() != Surface::Water || base_d < kBaseStone)
            c.set_surface(base_d < kBaseStone ? Surface::Stone
                          : base_d < kBaseDry ? Surface::Dryland
                                              : Surface::Sand);
    }
    // 7. Pole: glacier over sea, lane and jungle [U-84][?-07].
    if (pole_d < kPoleRadius) {
        c.set_tree(Tree::None);
        c.set_bush(Bush::None);
        c.set_surface(Surface::Glacier);
    }

    // Derived fields.
    if (c.surface() == Surface::DeepWater) c.set_walk(Walk::Blocked);  // [?-08]
    if (c.tree() == Tree::Alive) {
        c.set_vision_fog(SenseBlock::Full);   // [X-10]
        c.set_skill_fog(SenseBlock::Full);
    }
    if (c.bush() == Bush::Alive) c.set_vision_fog(SenseBlock::Brush);
    return c;
}

}  // namespace

Grid make_default_map() {
    Grid g;
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            const auto [cx, cy] = canonical(x, y);
            g.at(x, y) = make_cell(cx, cy);
        }
    return g;
}

}  // namespace em
