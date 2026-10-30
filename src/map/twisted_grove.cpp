// [U-421] Twisted Grove: a 3 v 3 map with two lanes (top / bottom) around a dark jungle and a centre pit
// (Claude idea in the spirit of LoL's old two-lane map). Same grid, same cell format as the original map; outside
// the playable band a chasm (not walkable, no vision block). Point-symmetric: Earth = the mirror of Fire.
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "map/default_map.hpp"

namespace em {

namespace {
constexpr int N = kGridSize, M = N - 1;
struct P {
    double x, y;
};
double seg_dist(P p, P a, P b) {
    const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
    const double t = l2 > 0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / l2, 0.0, 1.0) : 0.0;
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}
// the two lanes (the same waypoints as the MapDef in game/match/maps.cpp)
constexpr P kTop[] = {{72, 236}, {100, 168}, {170, 150}, {256, 146}, {342, 150}, {412, 168}, {440, 236}};
constexpr P kBot[] = {{72, 275}, {99, 343}, {169, 361}, {255, 365}, {341, 361}, {411, 343}, {439, 275}};
double lane_dist(P p) {
    double d = 1e9;
    for (std::size_t i = 1; i < std::size(kTop); ++i) d = std::min({d, seg_dist(p, kTop[i - 1], kTop[i]), seg_dist(p, kBot[i - 1], kBot[i])});
    return d;
}
constexpr std::uint32_t hash(int x, int y, std::uint32_t salt) {
    std::uint64_t z = (std::uint64_t(std::uint32_t(x)) << 32 | std::uint32_t(y)) + salt * 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return std::uint32_t(z ^ (z >> 31));
}
constexpr double kLaneHalf = 11, kRoadHalf = 3, kBaseR = 36;
// smooth value noise (0 … 1) on a 14-cell lattice → organic groves instead of single trees
double noise(double x, double y) {
    const double gx = x / 14, gy = y / 14;
    const int ix = int(std::floor(gx)), iy = int(std::floor(gy));
    const double fx = gx - ix, fy = gy - iy;
    auto v = [](int a, int b) { return double(hash(a, b, 9) % 1000) / 999.0; };
    auto sm = [](double t) { return t * t * (3 - 2 * t); };
    const double a0 = v(ix, iy) + (v(ix + 1, iy) - v(ix, iy)) * sm(fx);
    const double a1 = v(ix, iy + 1) + (v(ix + 1, iy + 1) - v(ix, iy + 1)) * sm(fx);
    return a0 + (a1 - a0) * sm(fy);
}

Cell make_cell(int cx, int cy) {
    const P p{cx + 0.5, cy + 0.5};
    Cell c;
    const double lane_d = lane_dist(p);
    const double base_d = std::hypot(p.x - 52, p.y - 256);
    const double pit_d = std::hypot(p.x - 256, p.y - 256);
    // the jungle: the band between the two lanes
    const bool jungle = p.x > 92 && p.x < 420 && p.y > 160 && p.y < 352 && lane_d > kLaneHalf;
    const bool lane = lane_d <= kLaneHalf;
    const bool base = base_d <= kBaseR;
    if (!(jungle || lane || base)) {  // the chasm: no walking, nothing grows
        c.set_surface(Surface::Stone);
        c.set_walk(Walk::Blocked);
        c.set_elevation(-2);
        return c;
    }
    c.set_surface(Surface::Land);
    if (lane) {
        c.set_surface(lane_d < kRoadHalf ? Surface::StoneRoad : Surface::Dryland);
        if (lane_d > kLaneHalf - 2 && hash(cx / 3, cy / 3, 5) % 100 < 22) c.set_bush(Bush::Alive);  // lane brush
    }
    if (jungle && !lane) {
        // dark groves with paths between them; the centre pit and its ring stay open
        // paths: the centre cross + a ring around the pit + the jungle edge along the lanes stay open
        const bool path = std::abs(p.y - 256) < 3 || std::abs(p.x - 256) < 3 || std::abs(pit_d - 40) < 3 || lane_d < kLaneHalf + 4;
        const double n = noise(p.x, p.y);
        if (pit_d < 22 || path) c.set_surface(pit_d < 14 ? Surface::Stone : Surface::ForestPath);
        else if (n > 0.52) c.set_tree(Tree::Alive);                                   // groves
        else if (n > 0.46 && hash(cx / 2, cy / 2, 3) % 100 < 45) c.set_bush(Bush::Alive);  // brush around them
    }
    if (base) {
        c.set_tree(Tree::None);
        c.set_bush(Bush::None);
        c.set_surface(base_d < 20 ? Surface::Stone : Surface::Dryland);
    }
    if (c.tree() == Tree::Alive) {
        c.set_vision_fog(SenseBlock::Full);
        c.set_skill_fog(SenseBlock::Full);
    }
    if (c.bush() == Bush::Alive) c.set_vision_fog(SenseBlock::Brush);
    return c;
}
}  // namespace

Grid make_twisted_grove() {
    Grid g;
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            // Fire half = west (x < 256); the east half is the point mirror
            const bool west = x < 256;
            g.at(x, y) = make_cell(west ? x : M - x, west ? y : M - y);
        }
    return g;
}

}  // namespace em
