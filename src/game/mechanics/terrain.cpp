// Terrain bending at runtime: walls [U-18][U-24][U-44], elevation [U-45], burning [U-14],
// melting [U-15][U-16], lava → stone [X-56], steam fog [X-57]. Every change → cell_changes (network).
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
                if (force_surface || cell.surface() == Surface::DeepWater) cell.set_surface(s);
                cell.set_walk(Walk::Walk);
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
    for (int k = 0; k < 2; ++k) {  // portals: keep the pole centre open [X-54]
        const CellPt p = k ? mirror(kNorthPoleCell) : kNorthPoleCell;
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
                       Team by_team, bool earthbending) {
    // [X-38] stone 4 s, ice 2.5 s (the skill's own duration wins). 2 cells thick → no diagonal leaks.
    const Dir side{-along.y, along.x};
    for (std::int32_t s = -length / 2; s <= length / 2; s += 12)
        for (std::int32_t t = -16; t <= 16; t += 16) {
            const int cx = (x + rot_x(along, s, 0) + side.x * t / kOne) / kCellUnits;
            const int cy = (y + rot_y(along, s, 0) + side.y * t / kOne) / kCellUnits;
            if (!Grid::in_bounds(cx, cy) || protected_cell(cx, cy)) continue;
            Cell c = grid.at(cx, cy);
            if (c.surface() == Surface::DeepWater && kind != Wall::Ice) continue;
            remember(cell_timers, cx, cy, c.raw(), now_ms + life_ms, CellTimerKind::Restore);
            c.set_wall(kind);
            c.set_temp_walkable(false);
            if (kind == Wall::Stone || kind == Wall::Metal) c.set_vision_fog(SenseBlock::Full);
            c.set_skill_fog(SenseBlock::Full);
            set_cell(cx, cy, c);
            add_wall_cell(cx, cy, kind);  // [U-288]
            if (earthbending) {
                earth_events.push_back({cx, cy});
                earth_event_team.push_back(by_team);
            }
        }
}

void World::raise_ring(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms, Team by_team, Wall kind) {
    for (int k = 0; k < 64; ++k) {
        const int cx = (x + kDirs[k].x * radius / kOne) / kCellUnits, cy = (y + kDirs[k].y * radius / kOne) / kCellUnits;
        if (!Grid::in_bounds(cx, cy) || protected_cell(cx, cy)) continue;
        Cell c = grid.at(cx, cy);
        remember(cell_timers, cx, cy, c.raw(), now_ms + life_ms, CellTimerKind::Restore);
        c.set_wall(kind);
        c.set_skill_fog(SenseBlock::Full);
        if (kind == Wall::Stone || kind == Wall::Metal) c.set_vision_fog(SenseBlock::Full);
        set_cell(cx, cy, c);
        add_wall_cell(cx, cy, kind);  // [U-288]
        earth_events.push_back({cx, cy});
        earth_event_team.push_back(by_team);
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

void World::step_terrain() {
    std::erase_if(clouds, [&](const Cloud& c) { return c.until_ms <= now_ms; });
    for (std::size_t i = 0; i < cell_timers.size();) {
        CellTimer& t = cell_timers[i];
        if (t.at_ms > now_ms) {
            ++i;
            continue;
        }
        if (t.kind == CellTimerKind::LavaToStone) {  // [X-56] cooled lava = stone wall for 4 s
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

void World::damage_wall(int cx, int cy, std::int32_t amount) {
    if (!Grid::in_bounds(cx, cy) || amount <= 0) return;
    const Cell c = grid.at(cx, cy);
    if (c.wall() == Wall::None) return;
    auto [it, fresh] = walls.try_emplace(std::uint32_t(cy) * kGridSize + std::uint32_t(cx), WallCell{wall_max_hp(c.wall()), wall_max_hp(c.wall())});
    it->second.hp -= amount;
    if (it->second.hp > 0) return;
    for (CellTimer& t : cell_timers)  // falls now: the timer restores the original cell next tick (public)
        if (t.x == cx && t.y == cy) t.at_ms = now_ms;
    walls.erase(it);
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
    damage_wall(cx, cy, std::int32_t(std::max<std::int64_t>(1, e.damage) * pct / 100));
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
