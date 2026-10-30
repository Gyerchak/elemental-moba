// Terrain bending at runtime: walls [U-18][U-24][U-44], elevation [U-45], burning [U-14],
// melting [U-15][U-16], lava → stone [X-56], steam fog [X-57]. Every change → cell_changes (network).
#include <cmath>
#include "core/wall_rules.hpp"
#include <algorithm>

#include "game/match/layout.hpp"
#include "game/mechanics/element_rules.hpp"
#include "game/world.hpp"

namespace em {

void prepare_match_grid(Grid& g) {
    auto clear_disc = [&](CellPt c, int r, Surface s, bool force_surface) {
        for (int y = c.second - r; y <= c.second + r; ++y)
            for (int x = c.first - r; x <= c.first + r; ++x) {
                if (!Grid::in_bounds(x, y) || (x - c.first) * (x - c.first) + (y - c.second) * (y - c.second) > r * r) continue;
                Cell& cell = g.at(x, y);
                cell.set_tree(Tree::None);
                cell.set_bush(Bush::None);
                cell.set_vision_fog(SenseBlock::None);
                cell.set_skill_fog(SenseBlock::None);
                if (force_surface || cell.surface() == Surface::DeepWater || cell.surface() == Surface::Stone) cell.set_surface(s);
                cell.set_walk(Walk::Walk);
                cell.set_wall(Wall::None);  // [U-359] map rocks are walls now
                cell.set_elevation(0);  // [U-346] no rock hill inside a camp
            }
    };
    for (CellPt c : kCamps) {
        clear_disc(c, 7, Surface::Land, false);
        clear_disc(mirror(c), 7, Surface::Land, false);
    }
    for (CellPt c : kAggroCamps) {  // [X-97]
        clear_disc(c, 8, Surface::Land, false);
        clear_disc(mirror(c), 8, Surface::Land, false);
    }
    clear_disc(kSpiritIsland, 30, Surface::Land, true);  // [X-86]
    for (CellPt c : kBuffCamps) {  // [U-366]
        clear_disc(c, 7, Surface::Land, false);
        clear_disc(mirror(c), 7, Surface::Land, false);
    }
    // [U-364] epic islands in the sea + a shallow ford across the sea to each (wade, no swimming)
    for (CellPt c : {kLionTurtleCell, kDragonsCell}) {
        for (int t = -48; t <= 48; t += 2) clear_disc({c.first + t, c.second - t}, 3, Surface::Water, true);
        clear_disc(c, 11, Surface::Land, true);
    }
    // [U-349] water fountains in both bases: a pond next to every inhibitor (towards the base) and one by the shrine,
    // so waterbenders have a source at home
    auto pond = [&](CellPt c, int r) {
        clear_disc(c, r, Surface::Water, true);
        clear_disc(mirror(c), r, Surface::Water, true);
    };
    for (const LaneStructs& l : kLaneStructs) {
        const int dx = kBaseCell.first - l.inhib.first, dy = kBaseCell.second - l.inhib.second;
        const double len = std::max(1.0, std::hypot(double(dx), double(dy)));
        pond({l.inhib.first + int(dx / len * 7), l.inhib.second + int(dy / len * 7)}, 3);
    }
    pond({kNexusCell.first - 8, kNexusCell.second + 9}, 3);
    for (int k = 0; k < 2; ++k) {  // portals: keep the pole centre open [X-54]
        const CellPt p = k ? mirror(kPortalCell) : kPortalCell;
        clear_disc(p, 6, Surface::Glacier, true);
    }
}

namespace {
void remember(std::vector<CellTimer>& timers, int x, int y, std::uint32_t raw, std::int64_t at, CellTimerKind k) {
    for (CellTimer& t : timers)
        if (t.x == x && t.y == y) {  // keep the ORIGINAL cell, extend the time
            t.at_ms = std::max(t.at_ms, at);
            if (k == CellTimerKind::LavaToStone) t.kind = k;
            return;
        }
    timers.push_back({std::int16_t(x), std::int16_t(y), raw, at, k});
}
}  // namespace

void World::raise_wall(std::int32_t x, std::int32_t y, Dir along, std::int32_t length, Wall kind, std::int32_t life_ms,
                       Team by_team, bool earthbending, const WallSpec& ws) {
    // [U-380] n segments of 100 units side by side (at least one), centred on x / y
    const int n = std::max(1, (length + kWallSegLen / 2) / kWallSegLen);
    for (int i = 0; i < n; ++i) {
        const std::int32_t off = (2 * i - (n - 1)) * kWallSegLen / 2;
        raise_segment(x + rot_x(along, off, 0), y + rot_y(along, off, 0), along, kind, life_ms, by_team, earthbending, ws);
    }
}

void World::raise_ring(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms, Team by_team, Wall kind,
                       const WallSpec& ws) {
    // [U-380] a ring of segments, each along the tangent; as many as fit on the circle (at least 4)
    const int n = std::max(4, int((2 * 314 * std::int64_t(radius) / 100 + kWallSegLen / 2) / kWallSegLen));
    for (int i = 0; i < n; ++i) {
        const Dir r = kDirs[std::size_t(i * 64 / n) % 64];
        raise_segment(x + r.x * radius / kOne, y + r.y * radius / kOne, Dir{-r.y, r.x}, kind, life_ms, by_team, true, ws);
    }
}

void World::raise_segment(std::int32_t x, std::int32_t y, Dir along, Wall kind, std::int32_t life_ms, Team by_team, bool earthbending,
                          const WallSpec& ws) {
    const Dir side{-along.y, along.x};
    std::vector<std::uint32_t> cells;
    const int half_t = std::max(8, ws.thick / 2);
    for (std::int32_t s = -kWallSegLen / 2; s <= kWallSegLen / 2; s += 8)
        for (std::int32_t t = -half_t; t <= half_t; t += 8) {
            const int cx = (x + rot_x(along, s, 0) + side.x * t / kOne) / kCellUnits;
            const int cy = (y + rot_y(along, s, 0) + side.y * t / kOne) / kCellUnits;
            if (!Grid::in_bounds(cx, cy) || protected_cell(cx, cy)) continue;
            const std::uint32_t key = std::uint32_t(cy) * kGridSize + std::uint32_t(cx);
            if (std::ranges::find(cells, key) != cells.end()) continue;
            const Cell c = grid.at(cx, cy);
            if (c.surface() == Surface::DeepWater && kind != Wall::Ice) continue;
            if (c.wall() != Wall::None) continue;  // never on top of another wall
            cells.push_back(key);
        }
    if (cells.empty()) return;
    const std::uint32_t id = next_seg_++;
    WallSeg& seg = segs[id];
    seg = {ws.hp, ws.hp, cells, now_ms + life_ms};
    // [U-380] who stands on it: a unit whose whole body fits on the segment is LIFTED on top (the cells it stands on
    // become walkable at the raised level); everybody else is pushed out of it as before
    bool lifted = false;
    for (const Unit& o : units) {
        if (!o.alive || o.flying() || o.is_structure() || o.kind == UnitKind::Inhibitor) continue;
        const Shape body = hitbox(o);
        bool touches_seg = false, fits = true;
        for (int i = 0; i < body.n; ++i)
            for (int k = -1; k < 64; k += 8) {
                const Circle& c = body.c[i];
                const std::int32_t px = k < 0 ? c.x : c.x + kDirs[k].x * (c.r - 1) / kOne;
                const std::int32_t py = k < 0 ? c.y : c.y + kDirs[k].y * (c.r - 1) / kOne;
                const std::uint32_t key = std::uint32_t(py / kCellUnits) * kGridSize + std::uint32_t(px / kCellUnits);
                const bool in = std::ranges::find(cells, key) != cells.end();
                touches_seg |= in;
                fits &= in;
            }
        if (touches_seg && fits) lifted = true;
    }
    for (const std::uint32_t key : cells) {
        const int cx = int(key % kGridSize), cy = int(key / kGridSize);
        Cell c = grid.at(cx, cy);
        remember(cell_timers, cx, cy, c.raw(), now_ms + life_ms, CellTimerKind::Restore);
        c.set_wall(kind);
        c.set_temp_elevation(c.elevation() + ws.level);  // [U-380] 1 or 2 levels high
        c.set_temp_walkable(lifted);                     // somebody stands on it: its top is ground for them
        if (kind == Wall::Stone || kind == Wall::Metal) c.set_vision_fog(SenseBlock::Full);
        c.set_skill_fog(SenseBlock::Full);
        set_cell(cx, cy, c);
        walls[key] = {ws.hp, ws.hp, id};
        if (earthbending) {
            earth_events.push_back({cx, cy});
            earth_event_team.push_back(by_team);
        }
    }
}

void World::raise_pillar(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms, Team by_team) {
    const int r = radius / kCellUnits;
    const int ccx = x / kCellUnits, ccy = y / kCellUnits;
    for (int cy = ccy - r; cy <= ccy + r; ++cy)
        for (int cx = ccx - r; cx <= ccx + r; ++cx) {
            if (!Grid::in_bounds(cx, cy) || (cx - ccx) * (cx - ccx) + (cy - ccy) * (cy - ccy) > r * r || protected_cell(cx, cy)) continue;
            Cell c = grid.at(cx, cy);
            remember(cell_timers, cx, cy, c.raw(), now_ms + life_ms, CellTimerKind::Restore);
            c.set_temp_elevation(c.elevation() + 2);  // [U-45][U-93] 2 levels: cannot walk up
            set_cell(cx, cy, c);
            earth_events.push_back({cx, cy});
            earth_event_team.push_back(by_team);
        }
}

void World::lava_pool(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms) {
    const int r = std::max(1, radius / kCellUnits);
    const int ccx = x / kCellUnits, ccy = y / kCellUnits;
    for (int cy = ccy - r; cy <= ccy + r; ++cy)
        for (int cx = ccx - r; cx <= ccx + r; ++cx) {
            if (!Grid::in_bounds(cx, cy) || (cx - ccx) * (cx - ccx) + (cy - ccy) * (cy - ccy) > r * r || protected_cell(cx, cy)) continue;
            Cell c = grid.at(cx, cy);
            if (c.surface() == Surface::DeepWater) continue;
            if (c.surface() == Surface::Water) {  // lava on water → steam [X-57]
                steam_cloud(cx * kCellUnits + 16, cy * kCellUnits + 16, 32, 3'000);
                continue;
            }
            if (c.tree() == Tree::Alive || c.bush() == Bush::Alive) {  // [U-227] lava burns trees too
                burn_cell(cx, cy);
                c = grid.at(cx, cy);
            }
            remember(cell_timers, cx, cy, c.raw(), now_ms + life_ms, CellTimerKind::LavaToStone);
            c.set_surface(Surface::Magma);
            set_cell(cx, cy, c);
        }
}

void World::steam_cloud(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms) {
    add_cloud(CloudKind::Steam, x, y, radius, life_ms);
}

// [U-241] clouds are their own objects (not cell edits): they block vision like walls for both teams
void World::add_cloud(CloudKind k, std::int32_t x, std::int32_t y, std::int32_t radius, std::int64_t ms) {
    clouds.push_back({x, y, std::max(24, radius), now_ms + ms, k});
}

// [U-225][U-241] lava under water / Heat Draw: the lava stops at once and cools into plain stone ground
void World::cool_lava(std::int32_t x, std::int32_t y, std::int32_t radius) {
    const int r = std::max(1, radius / kCellUnits), ccx = x / kCellUnits, ccy = y / kCellUnits;
    for (CellTimer& t : cell_timers)
        if (t.kind == CellTimerKind::LavaToStone && (t.x - ccx) * (t.x - ccx) + (t.y - ccy) * (t.y - ccy) <= r * r) {
            Cell c(t.raw);
            if (c.surface() != Surface::Water && c.surface() != Surface::DeepWater) c.set_surface(Surface::Stone);
            t.raw = c.raw();
            t.kind = CellTimerKind::Restore;
            t.at_ms = now_ms;  // restore now → stone ground, no wall
        }
}


// [U-188] water (deep or shallow) → walkable glacier ice for a while, then back to water
void World::freeze_water(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms) {
    const int r = std::max(1, radius / kCellUnits);
    const int ccx = x / kCellUnits, ccy = y / kCellUnits;
    for (int cy = ccy - r; cy <= ccy + r; ++cy)
        for (int cx = ccx - r; cx <= ccx + r; ++cx) {
            if (!Grid::in_bounds(cx, cy) || (cx - ccx) * (cx - ccx) + (cy - ccy) * (cy - ccy) > r * r) continue;
            Cell c = grid.at(cx, cy);
            if (c.surface() != Surface::DeepWater && c.surface() != Surface::Water) continue;
            remember(cell_timers, cx, cy, c.raw(), now_ms + life_ms, CellTimerKind::Restore);
            c.set_surface(Surface::Glacier);
            c.set_walk(Walk::Walk);
            c.set_temp_walkable(true);
            set_cell(cx, cy, c);
        }
}

void World::burn_cell(int cx, int cy) {  // [U-14] burns now, regrows later (bushes fast)
    Cell c = grid.at(cx, cy);
    const std::uint32_t orig = c.raw();
    std::int64_t regrow = 0;
    if (c.tree() == Tree::Alive) {
        c.set_tree(Tree::Burned);
        c.set_vision_fog(SenseBlock::None);
        c.set_skill_fog(SenseBlock::None);
        regrow = elements::kTreeRegrowMs;  // [U-227] 1 game day
        add_cloud(CloudKind::Smoke, cx * kCellUnits + 16, cy * kCellUnits + 16, 40, elements::kSmokeMs);  // [U-241]
        glows.push_back({{cx * kCellUnits + 16, cy * kCellUnits + 16, 140, 95}, now_ms + elements::kTreeFireMs});  // [U-256] it burns a while
    }
    if (c.bush() == Bush::Alive) {
        c.set_bush(Bush::Burned);
        c.set_vision_fog(SenseBlock::None);
        regrow = std::max<std::int64_t>(regrow, elements::kBushRegrowMs);
    }
    if (!regrow) return;
    remember(cell_timers, cx, cy, orig, now_ms + regrow, CellTimerKind::Regrow);
    set_cell(cx, cy, c);
    // fire spreads to a neighbouring tree sometimes
    if (random() % 100 < 12) {
        const int nx = cx + int(random() % 3) - 1, ny = cy + int(random() % 3) - 1;
        if (Grid::in_bounds(nx, ny) && grid.at(nx, ny).tree() == Tree::Alive) burn_cell(nx, ny);
    }
}

// [U-227][U-244] trees under the skill burn (fire) or break (other destroyers); fire on the ground keeps burning
void World::scorch(const Effect& e) {
    if (!e.def) return;
    const bool fire = fire_family(e.sub) || e.sub == SubElement::Lava;
    const bool trees = e.def->destroys_trees < 0 ? fire : e.def->destroys_trees > 0;
    const bool ground = e.def->burns_ground < 0
                            ? fire_family(e.sub) && (e.kind == FxKind::Burst || e.kind == FxKind::Field || e.kind == FxKind::Cone)
                            : e.def->burns_ground > 0;
    const Shape sh = shape_of(e);
    if (trees)
        for (int i = 0; i < sh.n; ++i) {
            const Circle& c = sh.c[i];
            const int r = c.r / kCellUnits;
            for (int y = c.y / kCellUnits - r; y <= c.y / kCellUnits + r; ++y)
                for (int x = c.x / kCellUnits - r; x <= c.x / kCellUnits + r; ++x) {
                    if (!Grid::in_bounds(x, y) || !touches(c, Circle{x * kCellUnits + 16, y * kCellUnits + 16, 0})) continue;
                    const Cell cell = grid.at(x, y);
                    if (cell.tree() != Tree::Alive && cell.bush() != Bush::Alive) continue;
                    if (fire) {
                        burn_cell(x, y);
                    } else {  // broken, not burned: same regrow time, no smoke
                        Cell broken = cell;
                        broken.set_tree(cell.tree() == Tree::Alive ? Tree::Destroyed : cell.tree());
                        broken.set_bush(cell.bush() == Bush::Alive ? Bush::Burned : cell.bush());
                        broken.set_vision_fog(SenseBlock::None);
                        broken.set_skill_fog(SenseBlock::None);
                        remember(cell_timers, x, y, cell.raw(), now_ms + elements::kTreeRegrowMs, CellTimerKind::Regrow);
                        set_cell(x, y, broken);
                    }
                }
        }
    if (ground && e.kind != FxKind::Field) {  // a small burning patch where it landed
        Effect g;
        g.kind = FxKind::Field;
        g.team = e.team;
        g.owner = e.owner;
        g.sub = SubElement::Fire;
        g.flow = Flow::Fire;
        g.x = e.x;
        g.y = e.y;
        g.radius = std::max(40, e.kind == FxKind::Cone ? e.width / 3 : e.radius * 2 / 3);
        g.damage = elements::kBurningGroundDps / 200;  // per 0.5 s tick, points
        g.tick_ms = 500;
        g.end_ms = now_ms + elements::kBurningGroundMs;
        g.next_tick_ms = now_ms + 500;
        g.light = 128;
        g.src = e.src;
        if (e.kind == FxKind::Cone) {  // the far half of the cone
            g.x = e.x + e.dir.x * (e.length * 2 / 3) / kOne * kSub;
            g.y = e.y + e.dir.y * (e.length * 2 / 3) / kOne * kSub;
        }
        fx.push_back(g);  // spawned after this tick's loop (vector may grow: callers must not hold references)
        fx.back().id = next_id++;
        fx.back().born_ms = now_ms;
    }
}

// [U-356] lava lying on the ground melts the walls around it; ice cools it (its rest time is halved once)
void World::lava_melts_walls() {
    if (now_ms < lava_melt_next_) return;
    lava_melt_next_ = now_ms + elements::kLavaMeltMs;
    constexpr int kDx[] = {1, -1, 0, 0, 0}, kDy[] = {0, 0, 1, -1, 0};
    for (std::size_t i = 0; i < cell_timers.size(); ++i) {
        if (cell_timers[i].kind != CellTimerKind::LavaToStone) continue;
        const int lx = cell_timers[i].x, ly = cell_timers[i].y;
        for (int k = 0; k < 5; ++k) {
            const int cx = lx + kDx[k], cy = ly + kDy[k];
            if (!Grid::in_bounds(cx, cy)) continue;
            const Wall w = grid.at(cx, cy).wall();
            if (w == Wall::None) continue;
            damage_wall(cx, cy, w == Wall::Ice ? elements::kLavaMeltIce : w == Wall::Metal ? elements::kLavaMeltMetal : elements::kLavaMeltStone);
            CellTimer& t = cell_timers[i];  // damage_wall may not grow the list, but keep the reference fresh
            if (w == Wall::Ice && !t.iced) {
                t.iced = true;
                if (t.at_ms > now_ms) t.at_ms = now_ms + (t.at_ms - now_ms) / 2;
            }
        }
    }
}

void World::step_terrain() {
    std::erase_if(clouds, [&](const Cloud& c) { return c.until_ms <= now_ms; });
    lava_melts_walls();
    if (tick % cfg.tick_hz == 0) std::erase_if(segs, [&](const auto& kv) { return kv.second.until_ms + 1'000 < now_ms; });  // [U-380]
    for (std::size_t i = 0; i < cell_timers.size();) {
        CellTimer& t = cell_timers[i];
        if (t.at_ms > now_ms) {
            ++i;
            continue;
        }
        if (t.kind == CellTimerKind::LavaToStone &&  // [U-356] most lava cools into plain ground
            (std::uint32_t(t.x) * 73'856'093u ^ std::uint32_t(t.y) * 19'349'663u ^ std::uint32_t(now_ms / 1000)) % 100 >= std::uint32_t(elements::kLavaRockPct)) {
            t.kind = CellTimerKind::Restore;
            continue;  // restored by the next pass of this loop
        }
        if (t.kind == CellTimerKind::LavaToStone) {  // [X-56] cooled lava = a small stone rock for 4 s
            Cell c = grid.at(t.x, t.y);
            c.set_surface(Cell(t.raw).surface());
            c.set_wall(Wall::Stone);
            c.set_vision_fog(SenseBlock::Full);
            set_cell(t.x, t.y, c);
            add_wall_cell(t.x, t.y, Wall::Stone);
            t.kind = CellTimerKind::Restore;
            t.at_ms = now_ms + kStoneWallLifeMs;
            ++i;
            continue;
        }
        cell_public_ = true;  // [U-288] regrowth, walls falling, lava cooling: everybody's map shows it, seen or not
        if (const Cell was = grid.at(t.x, t.y); was.wall() != Wall::None && Cell(t.raw).wall() == Wall::None &&
            std::ranges::none_of(wall_breaks, [&](const WallBreak& b) { return b.x == t.x && b.y == t.y && b.ms == now_ms; }))
            wall_breaks.push_back({t.x, t.y, 0, 0, std::uint8_t(was.wall()), now_ms});  // [U-341] crumbles by time
        set_cell(t.x, t.y, Cell(t.raw));
        cell_public_ = false;
        t = cell_timers.back();
        cell_timers.pop_back();
    }
}

}  // namespace em

namespace em {

// ---------------------------------------------------------------- [U-288] walls with HP
void World::add_wall_cell(int cx, int cy, Wall kind) {
    const std::int32_t hp = wall_max_hp(kind);
    if (hp > 0) walls[std::uint32_t(cy) * kGridSize + std::uint32_t(cx)] = {hp, hp};
}

// [U-359] one cell of rock where a thrown rock landed (only on free, walkable ground); a magma rock glows a moment
void World::drop_rock(std::int32_t x, std::int32_t y, bool magma) {
    const int cx = x / kCellUnits, cy = y / kCellUnits;
    if (!Grid::in_bounds(cx, cy) || protected_cell(cx, cy)) return;
    Cell c = grid.at(cx, cy);
    if (c.wall() != Wall::None || c.walk() != Walk::Walk || c.tree() == Tree::Alive || c.surface() == Surface::Water ||
        c.surface() == Surface::DeepWater || c.surface() == Surface::Magma)
        return;
    const Circle cell{cx * kCellUnits + kCellUnits / 2, cy * kCellUnits + kCellUnits / 2, kCellUnits / 2};
    for (const Unit& o : units)
        if (o.alive && !o.flying() && touches(hitbox(o), cell)) return;  // never on somebody
    remember(cell_timers, cx, cy, c.raw(), now_ms + elements::kThrownRockMs, CellTimerKind::Restore);
    c.set_wall(Wall::Stone);
    c.set_skill_fog(SenseBlock::Full);
    set_cell(cx, cy, c);
    add_wall_cell(cx, cy, Wall::Stone);
    if (magma) glows.push_back({{x, y, 96, 80}, now_ms + 3'000});
}

bool World::rock_source_at(std::int32_t x, std::int32_t y) const {
    const int cx = x / kCellUnits, cy = y / kCellUnits;
    return x >= 0 && y >= 0 && Grid::in_bounds(cx, cy) && grid.at(cx, cy).wall() == Wall::Stone && !protected_cell(cx, cy) &&
           cx >= 3 && cy >= 3 && cx < kGridSize - 3 && cy < kGridSize - 3;
}

bool World::natural_wall(int cx, int cy) const {
    return std::ranges::none_of(cell_timers, [&](const CellTimer& t) { return t.x == cx && t.y == cy; });
}

void World::damage_wall(int cx, int cy, std::int32_t amount, Dir from) {
    if (!Grid::in_bounds(cx, cy) || amount <= 0) return;
    const Cell c = grid.at(cx, cy);
    if (c.wall() == Wall::None || protected_cell(cx, cy)) return;
    const std::uint32_t key = std::uint32_t(cy) * kGridSize + std::uint32_t(cx);
    auto it = walls.find(key);
    if (it != walls.end() && it->second.seg) {  // [U-380] a segment: shared HP, it falls as a whole
        const auto sg = segs.find(it->second.seg);
        if (sg != segs.end()) {
            WallSeg& seg = sg->second;
            seg.hp -= amount;
            for (const std::uint32_t k : seg.cells)
                if (auto w = walls.find(k); w != walls.end() && w->second.seg == sg->first) w->second.hp = std::max(0, seg.hp);
            if (seg.hp > 0) return;
            for (const std::uint32_t k : seg.cells) {
                const int x = int(k % kGridSize), y = int(k / kGridSize);
                for (CellTimer& t : cell_timers)
                    if (t.x == x && t.y == y) t.at_ms = now_ms;
                if (auto w = walls.find(k); w != walls.end() && w->second.seg == sg->first) walls.erase(w);
                wall_breaks.push_back({std::int16_t(x), std::int16_t(y), std::int16_t(std::int64_t(from.x) * 1000 / kOne),
                                       std::int16_t(std::int64_t(from.y) * 1000 / kOne), std::uint8_t(grid.at(x, y).wall()), now_ms});
            }
            segs.erase(sg);
            return;
        }
    }
    const bool natural = natural_wall(cx, cy);
    if (natural && (cx < 3 || cy < 3 || cx >= kGridSize - 3 || cy >= kGridSize - 3)) return;  // the map border never breaks
    if (it == walls.end()) {
        const std::int32_t hp = wall_max_hp(c.wall()) * (natural ? elements::kNaturalWallHpPct : 100) / 100;
        it = walls.emplace(key, WallCell{hp, hp}).first;
    }
    it->second.hp -= amount;
    if (it->second.hp > 0) return;
    if (natural) {  // [U-360] a map rock breaks into walkable rubble and grows back later
        remember(cell_timers, cx, cy, c.raw(), now_ms + elements::kNaturalRockRegrowMs, CellTimerKind::Restore);
        Cell r = c;
        r.set_wall(Wall::None);
        r.set_walk(Walk::Walk);
        r.set_vision_fog(SenseBlock::None);
        r.set_skill_fog(SenseBlock::None);
        r.set_elevation(std::min<int>(r.elevation(), 1));
        cell_public_ = true;
        set_cell(cx, cy, r);
        cell_public_ = false;
    }
    for (CellTimer& t : cell_timers)  // falls now: the timer restores the original cell next tick (public)
        if (t.x == cx && t.y == cy) t.at_ms = now_ms;
    walls.erase(it);
    // [U-341] broken by a hit: the pieces fly the way the hit went
    wall_breaks.push_back({std::int16_t(cx), std::int16_t(cy), std::int16_t(std::int64_t(from.x) * 1000 / kOne),
                           std::int16_t(std::int64_t(from.y) * 1000 / kOne), std::uint8_t(c.wall()), now_ms});
}

void World::hit_wall(Effect& e, const Unit* owner, int cx, int cy) {
    const std::uint32_t key = std::uint32_t(cy) * kGridSize + std::uint32_t(cx);
    if (e.last_wall_cell == key) return;
    e.last_wall_cell = key;
    const Wall w = grid.at(cx, cy).wall();
    std::int64_t pct = wall_effect_pct(w, e.sub);
    if (owner) {  // [U-288] items: against all walls + against this kind
        const Stats& s = owner->stats;
        const std::int32_t kind_bonus = w == Wall::Stone ? s.wall_damage_stone : w == Wall::Ice ? s.wall_damage_ice : s.wall_damage_metal;
        pct = pct * (10'000 + s.wall_damage + kind_bonus) / 10'000;
    }
    damage_wall(cx, cy, std::int32_t(std::max<std::int64_t>(1, e.damage) * pct / 100), e.dir);
}

int World::draw_from_wall(const Unit& u, SubElement sub) {
    const int ccx = u.ux() / kCellUnits, ccy = u.uy() / kCellUnits;
    int best = -1, bx = 0, by = 0;
    for (int y = ccy - kWallSourceCells; y <= ccy + kWallSourceCells; ++y)
        for (int x = ccx - kWallSourceCells; x <= ccx + kWallSourceCells; ++x) {
            if (!Grid::in_bounds(x, y) || !wall_feeds(grid.at(x, y).wall(), sub)) continue;
            const int d = (x - ccx) * (x - ccx) + (y - ccy) * (y - ccy);
            if (best < 0 || d < best) best = d, bx = x, by = y;
        }
    if (best < 0) return 0;
    damage_wall(bx, by, kWallSourceCost);  // own or enemy wall: it gives its element away
    return kWallSourceBonusPct;
}

}  // namespace em

namespace em {

// ---------------------------------------------------------------- [U-293] steam → water / ice on the ground
void World::condense_steam(std::int32_t x, std::int32_t y, std::int32_t radius, const Unit* by, bool as_ice) {
    bool any = false;
    for (Cloud& c : clouds) {
        if (c.kind != CloudKind::Steam) continue;
        if (dist2(c.x, c.y, x, y) > std::int64_t(c.radius + radius) * (c.radius + radius)) continue;
        c.radius -= radius;  // only the steam around the cold condenses; the rest of the cloud stays (smaller)
        if (c.radius < 32) c.until_ms = now_ms;
        any = true;
    }
    if (!any) return;
    // those standing in the condensing steam get wet (the normal time) — not on ice
    if (!as_ice)
        for (Unit& o : units)
            if (o.alive && !o.flying() && !o.is_structure() && dist2(o.ux(), o.uy(), x, y) <= std::int64_t(radius) * radius) {
                const bool wb = o.kind == UnitKind::Champion && champions()[std::size_t(o.champ)].element == Element::Water;
                o.st.add(StatusKind::Wet, now_ms + (wb ? elements::kWetWaterbenderMs : elements::kWetMs), 0, by ? by->id : 0);
            }
    const CloudKind k = as_ice ? CloudKind::IcePatch : CloudKind::Puddle;
    for (Cloud& p : clouds)  // join a patch already there (a projectile makes one every tick)
        if (p.kind == k && dist2(p.x, p.y, x, y) <= std::int64_t(radius) * radius / 4) {
            p.until_ms = now_ms + elements::kPuddleMs;
            return;
        }
    clouds.push_back({x, y, radius + 16, now_ms + elements::kPuddleMs, k});
}

int World::big_water_near(const Unit& u, std::size_t* patch) const {
    const int r = elements::kIceWallSourceCells, ccx = u.ux() / kCellUnits, ccy = u.uy() / kCellUnits;
    for (int y = ccy - r; y <= ccy + r; ++y)
        for (int x = ccx - r; x <= ccx + r; ++x) {
            if (!Grid::in_bounds(x, y)) continue;
            const Cell c = grid.at(x, y);
            const Surface s = c.surface();
            if (s == Surface::Water || s == Surface::DeepWater || s == Surface::Glacier || c.wall() == Wall::Ice) return 1;
        }
    const std::int64_t reach = std::int64_t(r) * kCellUnits;
    for (std::size_t i = 0; i < clouds.size(); ++i) {
        const Cloud& p = clouds[i];
        if ((p.kind == CloudKind::Puddle || p.kind == CloudKind::IcePatch) &&
            dist2(p.x, p.y, u.ux(), u.uy()) <= (reach + p.radius) * (reach + p.radius)) {
            if (patch) *patch = i;
            return 2;
        }
    }
    return 0;
}

void World::use_water_source(const Unit& u) {
    std::size_t i = 0;
    if (big_water_near(u, &i) == 2) clouds[i].until_ms = now_ms;
}

// [U-339][U-340] what kind of water is at this point (game units)
World::WaterSource World::water_source_at(std::int32_t x, std::int32_t y, std::size_t* patch) const {
    if (x < 0 || y < 0) return WaterSource::None;
    const int cx = x / kCellUnits, cy = y / kCellUnits;
    if (Grid::in_bounds(cx, cy)) {
        const Cell c = grid.at(cx, cy);
        if (c.wall() == Wall::Ice) return WaterSource::IceWall;
        const Surface s = c.surface();
        if (s == Surface::Water || s == Surface::DeepWater || s == Surface::Glacier) return WaterSource::Lasting;
    }
    for (std::size_t i = 0; i < clouds.size(); ++i) {
        const Cloud& p = clouds[i];
        if ((p.kind == CloudKind::Puddle || p.kind == CloudKind::IcePatch) && dist2(p.x, p.y, x, y) <= std::int64_t(p.radius) * p.radius) {
            if (patch) *patch = i;
            return WaterSource::Patch;
        }
    }
    return WaterSource::None;
}

bool World::water_sourced(const Unit& u, const SkillDef& d, std::int32_t sx, std::int32_t sy) const {
    if (sx >= 0 && water_source_at(sx, sy) != WaterSource::None) {
        const std::int64_t reach = std::max<std::int32_t>(d.range, 300) + elements::kSourceReachCells * kCellUnits;
        if (dist2(sx, sy, u.ux(), u.uy()) <= reach * reach) return true;
    }
    return big_water_near(u) != 0;  // standing next to water: bent from there
}

void World::drain_water(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t ms) {
    const int r = std::max(1, radius / kCellUnits), ccx = x / kCellUnits, ccy = y / kCellUnits;
    for (int cy = ccy - r; cy <= ccy + r; ++cy)
        for (int cx = ccx - r; cx <= ccx + r; ++cx) {
            if (!Grid::in_bounds(cx, cy) || (cx - ccx) * (cx - ccx) + (cy - ccy) * (cy - ccy) > r * r || protected_cell(cx, cy)) continue;
            Cell c = grid.at(cx, cy);
            if (c.temp_walkable()) continue;  // frozen / bent already
            if (c.surface() == Surface::Water) c.set_surface(Surface::Land);
            else if (c.surface() == Surface::DeepWater) c.set_surface(Surface::Water), c.set_walk(Walk::Walk);
            else continue;
            remember(cell_timers, cx, cy, grid.at(cx, cy).raw(), now_ms + ms, CellTimerKind::Restore);
            set_cell(cx, cy, c);
        }
}

void World::take_from_source(const Unit& u, std::int32_t sx, std::int32_t sy) {
    (void)u;
    std::size_t i = 0;
    switch (water_source_at(sx, sy, &i)) {
        case WaterSource::Patch: clouds[i].until_ms = now_ms; break;  // used up
        case WaterSource::IceWall: damage_wall(sx / kCellUnits, sy / kCellUnits, elements::kIceWallMeltPerUse); break;  // melts a bit
        default: break;  // rivers / seas / glaciers never run out
    }
}

int World::draw_from_water_source(const Unit& u, SubElement& sub) {
    if (u.kind != UnitKind::Champion || champions()[std::size_t(u.champ)].element != Element::Water) return 0;
    if (sub != SubElement::Water && sub != SubElement::Ice) return 0;
    const std::int64_t reach = std::int64_t(elements::kWaterSourceCells) * kCellUnits;
    for (Cloud& p : clouds) {
        if (p.kind != CloudKind::Puddle && p.kind != CloudKind::IcePatch) continue;
        if (dist2(p.x, p.y, u.ux(), u.uy()) > (reach + p.radius) * (reach + p.radius)) continue;
        p.until_ms = now_ms;  // used up
        sub = u.source_ice ? SubElement::Ice : SubElement::Water;  // the bender's chosen form
        return elements::kWaterSourceBonusPct;
    }
    return 0;
}

}  // namespace em
