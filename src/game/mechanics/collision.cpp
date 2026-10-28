// Collision layers on body borders [U-144..U-146][X-94..X-96]: terrain, trees, walls, structures, units.
#include <algorithm>
#include <cstdlib>
#include "game/match/layout.hpp"
#include "game/world.hpp"

namespace em {

std::uint8_t World::collide_mask(const Unit& u) const {
    std::uint8_t m = u.collide;
    if (u.flying()) m &= std::uint8_t(~(kCollTerrain | kCollTree | kCollWall));  // [U-51][U-77] fly over terrain
    return m;
}

bool World::protected_cell(int cx, int cy) const {  // fountains + structures can never be bent [X-95]
    for (CellPt f : {kFountainCell, mirror(kFountainCell)})
        if ((cx - f.first) * (cx - f.first) + (cy - f.second) * (cy - f.second) < 8 * 8) return true;
    return Grid::in_bounds(cx, cy) && !struct_block.empty() && struct_block[std::size_t(cy) * kGridSize + std::size_t(cx)];
}

void World::rebuild_collision_maps() {
    constexpr std::size_t N = std::size_t(kGridSize) * kGridSize;
    struct_block.assign(N, 0);
    for (const Unit& s : units) {
        if (!s.alive || !(s.is_structure() || s.kind == UnitKind::Inhibitor)) continue;
        const Shape sh = hitbox(s);
        for (int i = 0; i < sh.n; ++i) {
            const Circle& c = sh.c[i];
            const int r = c.r / kCellUnits + 1;
            for (int y = c.y / kCellUnits - r; y <= c.y / kCellUnits + r; ++y)
                for (int x = c.x / kCellUnits - r; x <= c.x / kCellUnits + r; ++x)
                    if (Grid::in_bounds(x, y) &&
                        touches(c, Circle{x * kCellUnits + kCellUnits / 2, y * kCellUnits + kCellUnits / 2, kCellUnits / 4}))
                        struct_block[std::size_t(y) * kGridSize + std::size_t(x)] = 1;
        }
    }
    // Chebyshev distance to the nearest cell a ground body cannot stand on, capped at 7.
    clearance.assign(N, 7);
    for (int y = 0; y < kGridSize; ++y)
        for (int x = 0; x < kGridSize; ++x) {
            const Cell c = grid.at(x, y);
            // [U-298] deep water is NOT blocked for paths (you swim through it; pathfind makes it cost more)
            const bool swim = c.surface() == Surface::DeepWater && c.walk() == Walk::Blocked;
            const bool blocked = (c.walk() != Walk::Walk && !swim) || c.tree() == Tree::Alive ||
                                 (c.wall() != Wall::None && !c.temp_walkable()) || c.effective_elevation() != c.elevation() ||
                                 struct_block[std::size_t(y) * kGridSize + std::size_t(x)];
            if (blocked) clearance[std::size_t(y) * kGridSize + std::size_t(x)] = 0;
        }
    // [U-357] a step of more than one level (a cliff) blocks like a wall: both sides keep the body away from it
    for (int y = 0; y + 1 < kGridSize; ++y)
        for (int x = 0; x + 1 < kGridSize; ++x) {
            const int e = grid.at(x, y).effective_elevation();
            if (std::abs(e - grid.at(x + 1, y).effective_elevation()) > 1 || std::abs(e - grid.at(x, y + 1).effective_elevation()) > 1)
                clearance[std::size_t(y) * kGridSize + std::size_t(x)] = 0;
        }
    auto relax = [&](int x, int y, int nx, int ny) {
        if (!Grid::in_bounds(nx, ny)) return;
        auto& a = clearance[std::size_t(y) * kGridSize + std::size_t(x)];
        a = std::min<std::uint8_t>(a, std::uint8_t(clearance[std::size_t(ny) * kGridSize + std::size_t(nx)] + 1));
    };
    for (int y = 0; y < kGridSize; ++y)
        for (int x = 0; x < kGridSize; ++x) {
            relax(x, y, x - 1, y); relax(x, y, x, y - 1); relax(x, y, x - 1, y - 1); relax(x, y, x + 1, y - 1);
        }
    for (int y = kGridSize - 1; y >= 0; --y)
        for (int x = kGridSize - 1; x >= 0; --x) {
            relax(x, y, x + 1, y); relax(x, y, x, y + 1); relax(x, y, x + 1, y + 1); relax(x, y, x - 1, y + 1);
        }
    clearance_dirty = false;
}

// How many sample points of the body (at a new position) sit on something it collides with.
// Every circle: centre + 8 points on its border [U-130] → part of the body cannot enter a tree / wall.
int World::body_violations(const Unit& u, std::int32_t xs, std::int32_t ys) const {
    const std::uint8_t m = collide_mask(u);
    const Shape body = place_body(*u.body, xs / kSub, ys / kSub, u.face, u.walk_phase, size_pct(u));
    const int gcx = u.ux() / kCellUnits, gcy = u.uy() / kCellUnits;
    const int ground = Grid::in_bounds(gcx, gcy) ? grid.at(gcx, gcy).effective_elevation() : 0;
    int bad = 0;
    for (int i = 0; i < body.n; ++i) {
        const Circle& c = body.c[i];
        for (int k = -1; k < 64; k += 8) {
            const std::int32_t px = k < 0 ? c.x : c.x + kDirs[k].x * (c.r - 1) / kOne;
            const std::int32_t py = k < 0 ? c.y : c.y + kDirs[k].y * (c.r - 1) / kOne;
            if (px < 0 || py < 0 || px >= kMapUnits || py >= kMapUnits) {
                ++bad;
                continue;
            }
            const Cell cell = grid.at(px / kCellUnits, py / kCellUnits);
            const bool open = cell.temp_walkable();
            if ((m & kCollTerrain) && !open &&
                ((cell.walk() == Walk::Blocked && cell.surface() != Surface::DeepWater) ||  // deep water = swim [U-188]
                 cell.walk() == Walk::FlyOnly || std::abs(cell.effective_elevation() - ground) > 1))
                ++bad;
            else if ((m & kCollTree) && cell.tree() == Tree::Alive) ++bad;
            else if ((m & kCollWall) && !open && cell.wall() != Wall::None) ++bad;
        }
    }
    // structures and other units: exact circle tests
    const std::int64_t reach = body_reach(*u.body, size_pct(u));
    for (const Unit& o : units) {
        if (&o == &u || !o.alive) continue;
        const bool structure = o.is_structure() || o.kind == UnitKind::Inhibitor;
        if (structure ? !(m & kCollStructure) : !(m & o.collide & kCollUnit)) continue;  // [U-367] both must collide
        if (!structure && (u.flying() || o.flying() || !targetable(o))) continue;
        const std::int64_t rr = reach + body_reach(*o.body, size_pct(o));
        if (dist2(xs / kSub, ys / kSub, o.ux(), o.uy()) > rr * rr) continue;
        const std::int64_t gap = em::edge_gap(body, hitbox(o));
        if (gap < 0) bad += 1'000 * std::int32_t(std::min<std::int64_t>(-gap, 999));  // overlap depth; touching is fine
    }
    return bad;
}

bool World::walkable_at(const Unit& u, std::int32_t xs, std::int32_t ys) const {
    if (xs < 0 || ys < 0 || xs / kSub >= kMapUnits || ys / kSub >= kMapUnits) return false;
    const int now_bad = body_violations(u, xs, ys);
    if (now_bad == 0) return true;
    // already stuck (wall rose under me, knocked into a tree…): a move out is allowed.
    // Units / structures count by overlap depth (×1000) → never deeper; terrain by sample points → not more.
    const int cur = body_violations(u, u.x, u.y);
    if (cur == 0) return false;
    const int now_deep = now_bad / 1'000, cur_deep = cur / 1'000;
    if (now_deep != cur_deep || cur_deep > 0) return now_deep < cur_deep || (now_deep == cur_deep && now_bad < cur);
    return now_bad <= cur;
}

void World::resolve_overlaps(Unit& u) {
    if (u.is_structure() || u.kind == UnitKind::Inhibitor || u.flying() || !u.alive) return;
    const std::int64_t reach = body_reach(*u.body, size_pct(u));
    const Shape me = hitbox(u);
    for (Unit& o : units) {
        if (&o == &u || !o.alive) continue;
        const bool structure = o.is_structure() || o.kind == UnitKind::Inhibitor;
        if (structure ? !(collide_mask(u) & kCollStructure) : !(u.collide & o.collide & kCollUnit)) continue;  // [U-367]
        if (!structure && (o.flying() || !targetable(o) || !targetable(u))) continue;
        const std::int64_t rr = reach + body_reach(*o.body, size_pct(o));
        if (dist2(u.ux(), u.uy(), o.ux(), o.uy()) > rr * rr) continue;
        const std::int64_t gap = em::edge_gap(me, hitbox(o));
        if (gap >= 0) continue;
        Dir d = dir_of(std::int64_t(u.x) - o.x, std::int64_t(u.y) - o.y);
        if (u.x == o.x && u.y == o.y) d = kDirs[u.id % 64];  // same spot (spawn): spread by id
        // structures don't move; two units each take half (at most 3 units per tick → smooth)
        const std::int64_t push = std::min<std::int64_t>(structure ? -gap + 1 : (-gap + 1) / 2 + 1, structure ? 999 : 3);
        const std::int32_t nx = u.x + std::int32_t(d.x * push * kSub / kOne), ny = u.y + std::int32_t(d.y * push * kSub / kOne);
        if (walkable_at(u, nx, ny)) {
            u.x = nx;
            u.y = ny;
        }
    }
}

void World::place_on_walkable(Unit& u) {
    if (body_violations(u, u.x, u.y) == 0) return;
    const int cx = u.ux() / kCellUnits, cy = u.uy() / kCellUnits;
    for (int r = 1; r < 40; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r || !Grid::in_bounds(cx + dx, cy + dy)) continue;
                const std::int32_t nx = ((cx + dx) * kCellUnits + kCellUnits / 2) * kSub;
                const std::int32_t ny = ((cy + dy) * kCellUnits + kCellUnits / 2) * kSub;
                if (body_violations(u, nx, ny) == 0) {
                    u.x = nx;
                    u.y = ny;
                    return;
                }
            }
}

// [U-299] minions of the same team that overlap push each other apart a little every tick (30 % of the overlap,
// only onto walkable ground) → they spread into a group instead of walking into one another. Not a hard block:
// a minion never gets stuck behind another one.
void World::separate_minions() {
    std::vector<Unit*> ms;
    for (Unit& u : units)
        if (u.alive && u.kind == UnitKind::Minion) ms.push_back(&u);
    for (std::size_t i = 0; i < ms.size(); ++i)
        for (std::size_t j = i + 1; j < ms.size(); ++j) {
            Unit& a = *ms[i];
            Unit& b = *ms[j];
            if (a.team != b.team) continue;
            const std::int64_t ra = body_reach(*a.body, size_pct(a)), rb = body_reach(*b.body, size_pct(b));
            const std::int64_t dx = b.x - a.x, dy = b.y - a.y, want = (ra + rb) * kSub;
            if (std::abs(dx) >= want || std::abs(dy) >= want) continue;
            const std::int64_t d2 = dx * dx + dy * dy;
            if (d2 >= want * want) continue;
            const std::int64_t d = std::max<std::int64_t>(1, isqrt(d2));
            const std::int64_t push = (want - d) * 3 / 10 / 2;  // each one moves half of 30 % of the overlap
            const std::int32_t px = d2 == 0 ? std::int32_t(push) : std::int32_t(dx * push / d), py = d2 == 0 ? 0 : std::int32_t(dy * push / d);
            if (walkable_at(a, a.x - px, a.y - py)) a.x -= px, a.y -= py;
            if (walkable_at(b, b.x + px, b.y + py)) b.x += px, b.y += py;
        }
}

}  // namespace em
