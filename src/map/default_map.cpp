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
// [U-544] the jungle as a plan: every camp stands in its own clearing with two ways out, paths join the camps to each other and to the lanes.
// Numbers of the camps: maps.cpp (camps, aggro_camps, buff_camps).
constexpr P kClearings[] = {{110, 300}, {150, 410}, {230, 430}, {80, 360}, {190, 360}, {130, 250}, {270, 400}, {60, 250}, {150, 300}, {174, 442}};
constexpr double kClearingRadius = 15;
constexpr Seg kForestPaths[] = {
    // top jungle
    {{28, 250}, {60, 250}},   {{60, 250}, {130, 250}},  {{130, 250}, {150, 300}}, {{150, 300}, {110, 300}}, {{110, 300}, {80, 360}},
    {{80, 360}, {28, 360}},   {{150, 300}, {205, 306}}, {{80, 360}, {115, 396}},  {{130, 250}, {100, 262}},
    // bottom jungle
    {{150, 483}, {174, 442}}, {{174, 442}, {150, 410}}, {{150, 410}, {115, 396}}, {{150, 410}, {190, 360}}, {{190, 360}, {205, 306}},
    {{190, 360}, {270, 400}}, {{270, 400}, {230, 430}}, {{230, 430}, {230, 483}}, {{270, 400}, {252, 420}}, {{174, 442}, {230, 430}},
};
// rock outcrops at the corners of the paths: places to hide behind, not a maze
struct Rock { P at; double core; };
constexpr Rock kRocks[] = {{{95, 225}, 3.0}, {{168, 262}, 2.4}, {{55, 310}, 2.6}, {{130, 345}, 3.2}, {{178, 322}, 2.0}, {{118, 430}, 2.6}, {{205, 395}, 3.0}, {{262, 450}, 2.4}, {{190, 462}, 2.2}, {{305, 430}, 2.8}};
constexpr double kPathHalfWidth = 2.6, kRoadHalfWidth = 3;

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

// [U-544] smooth noise: forests, coasts and rivers with natural edges (the same on every machine)
double smooth_noise(double x, double y, double scale, std::uint32_t salt) {
    const double fx = x / scale, fy = y / scale;
    const int ix = int(std::floor(fx)), iy = int(std::floor(fy));
    auto r = [&](int a, int b) { return double(hash(a, b, salt) % 1000) / 999.0; };
    auto ease = [](double t) { return t * t * (3 - 2 * t); };
    const double tx = ease(fx - ix), ty = ease(fy - iy);
    return (r(ix, iy) * (1 - tx) + r(ix + 1, iy) * tx) * (1 - ty) + (r(ix, iy + 1) * (1 - tx) + r(ix + 1, iy + 1) * tx) * ty;
}
double forest_noise(P p) { return 0.6 * smooth_noise(p.x, p.y, 22, 21) + 0.28 * smooth_noise(p.x, p.y, 10, 22) + 0.12 * smooth_noise(p.x, p.y, 5, 23); }
P wobbled(P p, double amount) {  // a point pushed aside a little: straight lines drawn with it curve
    return {p.x + (smooth_noise(p.x, p.y, 30, 31) - 0.5) * amount, p.y + (smooth_noise(p.x, p.y, 30, 32) - 0.5) * amount};
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

    const double sea_d = std::abs(p.x - p.y) / std::sqrt(2.0) + (smooth_noise(p.x, p.y, 14, 34) - 0.5) * 7;  // [U-544] a coast, not a ruler
    const double lane_d = dist_to_lanes(p);
    const double base_d = dist(p, kFireBase) + (dist(p, kFireBase) > kBaseStone + 4 ? (smooth_noise(p.x, p.y, 12, 35) - 0.5) * 12 : 0);  // [U-544] the core stays round
    const double pole_d = dist(p, kNorthPole) + (smooth_noise(p.x, p.y, 12, 36) - 0.5) * 8;
    const double mid_d = std::abs(p.x + p.y - M) / std::sqrt(2.0);

    // 1. Jungle [U-544]: forests with natural edges, thick along the lanes, a clearing around every camp, paths with bushes beside them, rocks at the corners.
    const P curved = wobbled(p, 9);
    double path_d = 1e9;
    for (Seg sg : kForestPaths) path_d = std::min(path_d, dist_to_segment(curved, sg));
    if (lane_d > kLaneHalfWidth + 3 && base_d > kBaseSand + 4 && pole_d > kPoleRadius + 4) {
        const double f = forest_noise(p), edge = lane_d - kLaneHalfWidth;
        double clear_d = 1e9;
        for (P camp : kClearings) clear_d = std::min(clear_d, dist(p, camp) - kClearingRadius);
        const double want = edge < 9 ? 0.40 : 0.47;  // (the forest closes in along the lanes)
        const bool open = clear_d <= 0 || path_d <= kPathHalfWidth + 1;
        if (!open && f > want && hash(cx, cy, 2) % 100 < 88) c.set_tree(Tree::Alive);
        else if (!open && f > want - 0.05 && hash(cx / 3, cy / 3, 3) % 100 < 45) c.set_bush(Bush::Alive);
        else if (path_d > kPathHalfWidth + 1 && path_d < kPathHalfWidth + 3.5 && hash(cx / 4, cy / 4, 4) % 100 < 28) c.set_bush(Bush::Alive);
        for (const Rock& rk : kRocks) {
            const double d = dist(p, rk.at) + (smooth_noise(p.x, p.y, 3, 33) - 0.5) * 1.6;
            if (d < rk.core) {
                c.set_surface(Surface::Stone), c.set_tree(Tree::None), c.set_bush(Bush::None);
                c.set_walk(Walk::Blocked);
                c.set_wall(Wall::Stone);  // [U-362] a real rock: skills hit it (and can break it [U-360])
                c.set_elevation(2);
                c.set_vision_fog(SenseBlock::Full);
                c.set_skill_fog(SenseBlock::Full);
            } else if (d < rk.core + 2.2) {
                c.set_surface(Surface::Stone), c.set_tree(Tree::None), c.set_bush(Bush::None);
                c.set_elevation(1);
            }
        }
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
    for (P pond : kPonds) river |= dist(p, pond) + (smooth_noise(p.x, p.y, 5, 37) - 0.5) * 4 < kPondRadius;
    for (Seg s : kRivers) river |= dist_to_segment(wobbled(p, 11), s) < kRiverHalfWidth;
    // [U-527][U-544] mud on the banks of the ponds
    bool bank = false;
    for (P pond : kPonds) bank |= dist(p, pond) < kPondRadius + 4.5 && smooth_noise(p.x, p.y, 4, 38) > 0.42;
    if (bank && !river && c.wall() == Wall::None && c.surface() == Surface::Land) {
        c.set_surface(Surface::Mud);
        c.set_tree(Tree::None);
        c.set_bush(Bush::None);
    }
    if (river && c.surface() != Surface::DeepWater) {
        c.set_surface(Surface::Water);
        c.set_tree(Tree::None);
        c.set_bush(Bush::None);
    }
    // 4b. Forest paths through the jungle [U-269] (not over water)
    const bool on_path = path_d < kPathHalfWidth;
    if (on_path && c.wall() == Wall::None && c.surface() != Surface::Water && c.surface() != Surface::DeepWater && sea_d >= kSeaHalfWidth + kBeach) {
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
