// [X-09] A* on the 512 × 512 grid.
#include "game/mechanics/pathfind.hpp"

#include <algorithm>
#include <cstdlib>
#include <queue>

namespace em {
namespace {

constexpr int N = kGridSize;
constexpr int kStraight = 10, kDiag = 14;
constexpr int kSwimCostPct = 250;  // [U-298] a deep-water cell costs 2.5 × (swimming is half speed + prefer land)

// [U-298] like Grid::can_step, but deep water counts as passable (swimming); returns the cost % of that step
int step_cost_pct(const Grid& g, int x0, int y0, int x1, int y1) {
    if (g.can_step(x0, y0, x1, y1)) return g.at(x1, y1).surface() == Surface::DeepWater ? kSwimCostPct : 100;
    if (!Grid::in_bounds(x1, y1)) return 0;
    const Cell to = g.at(x1, y1);
    if (to.surface() == Surface::DeepWater && to.wall() == Wall::None && to.tree() != Tree::Alive &&
        std::abs(to.effective_elevation() - g.at(x0, y0).effective_elevation()) <= 1)
        return kSwimCostPct;
    return 0;  // blocked
}

struct Scratch {
    std::vector<std::int32_t> g;
    std::vector<std::int32_t> parent;
    std::vector<std::uint32_t> stamp;
    std::uint32_t cur = 0;
    Scratch() : g(std::size_t(N) * N), parent(std::size_t(N) * N), stamp(std::size_t(N) * N, 0) {}
};

int octile(int x0, int y0, int x1, int y1) {
    const int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    return kStraight * std::max(dx, dy) + (kDiag - kStraight) * std::min(dx, dy);
}

// [U-394] does a circle of radius r (units) centred at px / py touch any blocked cell (clearance 0)?
bool circle_clear(const std::uint8_t* clr, int px, int py, int r) {
    if (px - r < 0 || py - r < 0) return false;  // [U-383] off the map edge
    for (int y = (py - r) / kCellUnits; y <= (py + r) / kCellUnits; ++y)
        for (int x = (px - r) / kCellUnits; x <= (px + r) / kCellUnits; ++x) {
            if (!Grid::in_bounds(x, y)) return false;
            if (clr[std::size_t(y) * N + std::size_t(x)] != 0) continue;
            const int qx = std::clamp(px, x * kCellUnits, x * kCellUnits + kCellUnits - 1);
            const int qy = std::clamp(py, y * kCellUnits, y * kCellUnits + kCellUnits - 1);
            if ((qx - px) * (qx - px) + (qy - py) * (qy - py) < r * r) return false;
        }
    return true;
}
bool body_fits_near(const std::uint8_t* clr, int cx, int cy, int r) {
    const int mx = cx * kCellUnits + kCellUnits / 2, my = cy * kCellUnits + kCellUnits / 2, h = kCellUnits / 2;
    for (const auto [ox, oy] : {std::pair{0, 0}, {h, 0}, {-h, 0}, {0, h}, {0, -h}})
        if (circle_clear(clr, mx + ox, my + oy, r)) return true;
    return false;
}

}  // namespace

bool line_walkable(const Grid& g, int x0, int y0, int x1, int y1, const std::uint8_t* clr, int k) {
    auto fits = [&](int x, int y) { return !clr || clr[std::size_t(y) * N + std::size_t(x)] > k; };
    int x = x0, y = y0;
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (x != x1 || y != y1) {
        const int e2 = 2 * err;
        int nx = x, ny = y;
        if (e2 >= dy) { err += dy; nx += sx; }
        if (e2 <= dx) { err += dx; ny += sy; }
        if (nx != x && ny != y)  // diagonal step: both corners must be open
            if (!g.can_step(x, y, nx, y) || !g.can_step(x, y, x, ny)) return false;
        if (!g.can_step(x, y, nx, ny) || !fits(nx, ny)) return false;
        x = nx;
        y = ny;
    }
    return true;
}

bool find_path(const Grid& grid, int sx, int sy, int tx, int ty, PathCells& out, int max_nodes, const std::uint8_t* clr,
               int k, int* expanded_out, int body_r) {
    thread_local Scratch s;
    out.clear();
    if (!Grid::in_bounds(sx, sy) || !Grid::in_bounds(tx, ty)) return false;
    if (++s.cur == 0) {
        std::ranges::fill(s.stamp, 0u);
        s.cur = 1;
    }
    auto idx = [](int x, int y) { return std::size_t(y) * N + std::size_t(x); };
    using Node = std::pair<std::int32_t, std::int32_t>;  // f, index
    std::priority_queue<Node, std::vector<Node>, std::greater<>> open;
    const std::size_t start = idx(sx, sy);
    s.stamp[start] = s.cur;
    s.g[start] = 0;
    s.parent[start] = -1;
    open.push({octile(sx, sy, tx, ty), std::int32_t(start)});
    std::size_t best = start;
    int best_h = octile(sx, sy, tx, ty);
    int expanded = 0;
    bool found = false;
    static constexpr int kDx[] = {1, -1, 0, 0, 1, 1, -1, -1};
    static constexpr int kDy[] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!open.empty() && expanded < max_nodes) {
        const auto [f, ci] = open.top();
        open.pop();
        const int cx = ci % N, cy = ci / N;
        if (f - octile(cx, cy, tx, ty) > s.g[std::size_t(ci)]) continue;  // stale
        ++expanded;
        const int h = octile(cx, cy, tx, ty);
        if (h < best_h) {
            best_h = h;
            best = std::size_t(ci);
        }
        if (cx == tx && cy == ty) {
            found = true;
            best = std::size_t(ci);
            break;
        }
        // (the direction index is `d`, NOT `k`: `k` is the body size — a shadowed `k` here once made every
        //  path ignore the body edges [U-252])
        for (int d = 0; d < 8; ++d) {
            const int nx = cx + kDx[d], ny = cy + kDy[d];
            const int cost_pct = step_cost_pct(grid, cx, cy, nx, ny);
            if (cost_pct == 0) continue;
            // body does not fit [X-94] — except the goal and the first 3 cells around the start (way out of a tight spot)
            int narrow_pct = 100;
            if (clr && clr[idx(nx, ny)] <= k && !(nx == tx && ny == ty) &&
                (clr[idx(nx, ny)] == 0 || std::max(std::abs(nx - sx), std::abs(ny - sy)) > 3)) {
                // [U-394] one short of the clearance: a thin pass the body still fits through (exact circle test)
                if (body_r > 0 && clr[idx(nx, ny)] == k && body_fits_near(clr, nx, ny, body_r)) narrow_pct = 150;
                else continue;
            }
            if (d >= 4 && (!step_cost_pct(grid, cx, cy, nx, cy) || !step_cost_pct(grid, cx, cy, cx, ny))) continue;
            const std::size_t ni = idx(nx, ny);
            const std::int32_t ng = s.g[std::size_t(ci)] + (d >= 4 ? kDiag : kStraight) * cost_pct / 100 * narrow_pct / 100;
            if (s.stamp[ni] == s.cur && s.g[ni] <= ng) continue;
            s.stamp[ni] = s.cur;
            s.g[ni] = ng;
            s.parent[ni] = ci;
            open.push({ng + octile(nx, ny, tx, ty), std::int32_t(ni)});
        }
    }
    if (expanded_out) *expanded_out = expanded;
    // walk back
    PathCells rev;
    for (std::int32_t i = std::int32_t(best); i != -1; i = s.parent[std::size_t(i)])
        rev.push_back({std::int16_t(i % N), std::int16_t(i / N)});
    std::ranges::reverse(rev);
    // string pulling: keep only corners
    if (!rev.empty()) {
        std::size_t a = 0;
        out.push_back(rev[0]);
        while (a + 1 < rev.size()) {
            std::size_t b = a + 1;
            while (b + 1 < rev.size() && line_walkable(grid, rev[a].first, rev[a].second, rev[b + 1].first, rev[b + 1].second, clr, k))
                ++b;
            out.push_back(rev[b]);
            a = b;
        }
    }
    return found;
}

}  // namespace em
