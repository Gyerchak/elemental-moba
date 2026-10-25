// [U-75][X-10] Ray-cast fog of war. See vision.hpp.
#include "game/mechanics/vision.hpp"

#include <algorithm>
#include <cstdlib>

namespace em {

void cloud_mask(std::span<const Cloud> clouds, std::vector<std::uint8_t>& out) {
    out.assign(std::size_t(kGridSize) * kGridSize, 0);
    for (const Cloud& c : clouds) {
        if (c.kind == CloudKind::Puddle || c.kind == CloudKind::IcePatch || c.kind == CloudKind::Arrival) continue;  // [U-292][U-293] on the ground: you see over it
        const int r = c.radius / kCellUnits, cx = c.x / kCellUnits, cy = c.y / kCellUnits;
        for (int y = cy - r; y <= cy + r; ++y)
            for (int x = cx - r; x <= cx + r; ++x)
                if (Grid::in_bounds(x, y) && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) out[std::size_t(y) * kGridSize + std::size_t(x)] = 1;
    }
}

void VisionMap::compute(const Grid& g, std::span<const Viewer> viewers, const LightMap& light, int ambient_pct,
                        const std::vector<std::uint8_t>* clouds) {
    std::ranges::fill(vis_, std::uint8_t{0});
    for (const Viewer& v : viewers) cast(g, v, light, ambient_pct, clouds);
}

void VisionMap::cast(const Grid& g, const Viewer& v, const LightMap& light, int ambient_pct, const std::vector<std::uint8_t>* clouds) {
    const int cx = v.x / kCellUnits, cy = v.y / kCellUnits;
    if (!Grid::in_bounds(cx, cy)) return;
    const int R = v.sight / kCellUnits + 1;
    const int my_elev = g.at(cx, cy).effective_elevation();
    const bool in_brush = g.at(cx, cy).vision_fog() == SenseBlock::Brush;
    vis_[std::size_t(cy) * kGridSize + std::size_t(cx)] = 1;
    const std::int64_t sight2 = std::int64_t(v.sight) * v.sight;

    auto ray = [&](int tx, int ty) {
        // [U-150] rays outside the eyes' cone only reach the body-sense radius
        std::int64_t ray2 = sight2;
        if (v.fov_cos > -16'384) {
            const std::int64_t dx = tx - cx, dy = ty - cy;
            const std::int64_t len = isqrt(dx * dx + dy * dy);
            if (len > 0 && (dx * v.fx + dy * v.fy) / len < v.fov_cos) ray2 = std::int64_t(v.near) * v.near;
        }
        // Bresenham from (cx,cy) to (tx,ty)
        int x = cx, y = cy;
        const int dx = std::abs(tx - cx), sx = cx < tx ? 1 : -1;
        const int dy = -std::abs(ty - cy), sy = cy < ty ? 1 : -1;
        int err = dx + dy;
        bool inside = in_brush;
        while (x != tx || y != ty) {
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x += sx; }
            if (e2 <= dx) { err += dx; y += sy; }
            if (!Grid::in_bounds(x, y)) return;
            const std::int64_t ddx = std::int64_t(x) * kCellUnits + kCellUnits / 2 - v.x;
            const std::int64_t ddy = std::int64_t(y) * kCellUnits + kCellUnits / 2 - v.y;
            const std::int64_t d2 = ddx * ddx + ddy * ddy;
            if (d2 > ray2) return;
            const Cell c = g.at(x, y);
            // [U-117] dist ≤ sight × max(ambient, light) — squared to stay in integers
            const std::int64_t pct = std::max(ambient_pct, light.at(x, y));
            const bool lit = d2 * 10'000 <= sight2 * pct * pct || d2 <= std::int64_t(v.near) * v.near;
            const SenseBlock f = c.vision_fog();
            const bool brush = f == SenseBlock::Brush;
            if (!brush) inside = false;
            if (lit && (!brush || inside)) vis_[std::size_t(y) * kGridSize + std::size_t(x)] = 1;
            if (f == SenseBlock::Full || c.wall() == Wall::Stone || c.wall() == Wall::Metal || c.effective_elevation() > my_elev) return;
            if (clouds && (*clouds)[std::size_t(y) * kGridSize + std::size_t(x)]) return;  // [U-241] clouds block like walls
        }
    };
    for (int i = -R; i <= R; ++i) {
        ray(cx + i, cy - R);
        ray(cx + i, cy + R);
        ray(cx - R, cy + i);
        ray(cx + R, cy + i);
    }
}

}  // namespace em
