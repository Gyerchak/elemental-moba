// Movement: orders, walking with turn rate [U-74][X-88], paths [X-09], channels [X-54][X-87], respawn.
#include <algorithm>
#include <cstdlib>
#include "game/match/layout.hpp"
#include "game/match/maps.hpp"
#include "game/mechanics/pathfind.hpp"
#include "game/mechanics/element_rules.hpp"
#include "game/world.hpp"

namespace em {

namespace {
// Amount for this tick of a per-second rate, exact over time (no rounding drift).
std::int64_t per_tick(std::int64_t per_s, std::uint64_t tick, std::uint32_t hz) {
    return per_s * std::int64_t(tick) / hz - per_s * std::int64_t(tick - 1) / hz;
}
}  // namespace

std::int32_t World::move_speed_sub(const Unit& u) const {
    // [U-219] move speed = (base + flat bonus) × (100 % + bonus %) × statuses (slows / hastes / swimming)
    std::int64_t ms100 = u.stats.move_speed.total();
    if (u.st.has(StatusKind::Polymorph)) ms100 = std::max<std::int64_t>(10'000, ms100 - 6'000);  // [U-484] LoL polymorph: −60 base speed
    ms100 = ms100 * (10'000 + u.stats.move_speed_pct) / 10'000;
    ms100 = ms100 * u.st.move_speed_pct() / 100;
    if (u.st.has(StatusKind::Hex)) ms100 = std::min<std::int64_t>(ms100, 14'000);  // [U-476] a critter crawls
    if (u.casting && u.cast.slot < std::uint8_t(Slot::Count)) {
        const SkillDef* d = u.skill[u.cast.slot];
        if (d && d->move == CastMove::Slowed) ms100 /= 2;  // [U-53]
    }
    return std::int32_t(ms100 * kSub / 100 / cfg.tick_hz);
}

// [U-74] Turn at most `turn_rate` per tick; Q14 small-angle rotation (cos ≈ 1 − a²/2, sin ≈ a − a³/6).
void World::turn_towards(Unit& u, Dir want) {
    const std::int64_t a = 102'944LL * u.stats.turn_rate.total() / (100LL * cfg.tick_hz);  // Q14 radians per tick
    const std::int64_t c = kOne - a * a / (2 * kOne);
    const std::int64_t sn = a - a * a / kOne * a / (6 * kOne);
    const std::int64_t dot = (std::int64_t(u.face.x) * want.x + std::int64_t(u.face.y) * want.y) / kOne;
    if (dot >= c) {
        u.face = want;
        return;
    }
    const std::int64_t cross = std::int64_t(u.face.x) * want.y - std::int64_t(u.face.y) * want.x;
    const std::int64_t s = cross >= 0 ? sn : -sn;
    const std::int64_t nx = (u.face.x * c - u.face.y * s) / kOne;
    const std::int64_t ny = (u.face.x * s + u.face.y * c) / kOne;
    u.face = dir_of(nx, ny);
}

void World::move_towards(Unit& u, std::int32_t tx, std::int32_t ty, std::int32_t speed) {
    const std::int64_t dx = std::int64_t(tx) - u.x, dy = std::int64_t(ty) - u.y;
    const std::int64_t d = isqrt(dx * dx + dy * dy);
    if (d == 0 || speed <= 0) return;
    const Dir want = dir_of(dx, dy);
    turn_towards(u, want);
    // [U-74] must face roughly the way (within 60°) before walking
    if ((std::int64_t(u.face.x) * want.x + std::int64_t(u.face.y) * want.y) / kOne < kOne / 2) return;
    const std::int64_t step = std::min<std::int64_t>(speed, d);
    const std::int32_t nx = u.x + std::int32_t(dx * step / d), ny = u.y + std::int32_t(dy * step / d);
    bool moved = false;
    if (walkable_at(u, nx, ny)) {
        u.x = nx;
        u.y = ny;
        moved = true;
    } else if (walkable_at(u, nx, u.y) && nx != u.x) {
        u.x = nx;
        moved = true;
    } else if (walkable_at(u, u.x, ny) && ny != u.y) {
        u.y = ny;
        moved = true;
    } else {
        // slide along the obstacle: try ±45°, then ±90° [X-94]
        for (const auto [c, sn] : {std::pair{11'585, 11'585}, {11'585, -11'585}, {0, kOne}, {0, -kOne}}) {
            const std::int64_t rx = (want.x * std::int64_t(c) - want.y * std::int64_t(sn)) / kOne;
            const std::int64_t ry = (want.x * std::int64_t(sn) + want.y * std::int64_t(c)) / kOne;
            const std::int32_t sx = u.x + std::int32_t(rx * step / kOne), sy = u.y + std::int32_t(ry * step / kOne);
            if (walkable_at(u, sx, sy)) {
                u.x = sx;
                u.y = sy;
                moved = true;
                break;
            }
        }
    }
    if (moved) {
        u.moving = true;
        u.last_move_ms = now_ms;
        u.walk_phase += std::int32_t(step * 1024 / (90 * kSub));  // one step cycle per 90 units
        u.stuck_ms = 0;
    } else {
        u.stuck_ms += 1'000 / cfg.tick_hz;
    }
}

void World::path_to(Unit& u, std::int32_t tx, std::int32_t ty) {
    u.path_pending = false;
    u.path.clear();
    u.path_i = 0;
    const int sx = u.ux() / kCellUnits, sy = u.uy() / kCellUnits;
    int gx = std::clamp(tx / kCellUnits, 0, kGridSize - 1), gy = std::clamp(ty / kCellUnits, 0, kGridSize - 1);
    if (clearance_dirty || clearance.empty()) rebuild_collision_maps();
    // [X-94] the body must fit: k = cells the body reaches past its own cell
    const int k = std::min(3, int((body_reach(*u.body, size_pct(u)) + kCellUnits / 2) / kCellUnits));
    // [U-287] a goal the body cannot stand on (trees, a wall, a structure) → the nearest cell where it fits.
    // Without this the search had to explore the whole reachable map before giving up (30–40 ms spikes).
    auto fits = [&](int x, int y) { return Grid::in_bounds(x, y) && clearance[std::size_t(y) * kGridSize + std::size_t(x)] > k; };
    if (!u.flying() && !fits(gx, gy) && grid.at(gx, gy).surface() != Surface::DeepWater)
        for (int r = 1; r <= 8; ++r) {
            int best = -1, bx = gx, by = gy;
            for (int y = gy - r; y <= gy + r; ++y)
                for (int x = gx - r; x <= gx + r; ++x) {
                    if (std::max(std::abs(x - gx), std::abs(y - gy)) != r || !fits(x, y)) continue;
                    const int d = (x - sx) * (x - sx) + (y - sy) * (y - sy);  // of the ring: the side towards me
                    if (best < 0 || d < best) best = d, bx = x, by = y;
                }
            if (best >= 0) {
                gx = bx, gy = by;
                break;
            }
        }
    if (u.flying() || (sx == gx && sy == gy) || line_walkable(grid, sx, sy, gx, gy, clearance.data(), k)) {
        u.path.push_back({std::int16_t(gx), std::int16_t(gy)});
    } else if (path_budget_ <= 2'000) {
        // [U-287] too many searches this tick: walk straight for now, the real path comes next tick
        u.path.push_back({std::int16_t(gx), std::int16_t(gy)});
        u.repath_ms = now_ms - 10'000;  // (minions ask again by their timer …)
        u.path_pending = true;          // (… everybody else in follow_path, next tick)
        return;
    } else {
        // [U-287] two tries at most: the whole body, then the thinnest (was: every size from k down to 0).
        // Each search may explore only what is left of this tick's budget; a search that runs out returns the
        // best part of the way (towards the goal) and the unit asks again later.
        int used = 0;
        const int body_r = int(body_reach(*u.body, size_pct(u)));  // [U-394] thin passes the body fits through
        bool ok = find_path(grid, sx, sy, gx, gy, u.path, std::min(60'000, path_budget_), clearance.data(), k, &used, body_r);
        path_budget_ -= used;
        // [U-357] the second try is only one size thinner (was: the thinnest, which led bodies into gaps they
        // could not pass → units scraping along walls). The player still reaches the exact spot: the last step
        // walks to the clicked point itself.
        if (!ok && k > 0 && path_budget_ > 2'000) {
            ok = find_path(grid, sx, sy, gx, gy, u.path, std::min(30'000, path_budget_), clearance.data(), k - 1, &used);
            path_budget_ -= used;
        }
        // [U-188] the goal is in the sea: walk to the shore, then swim straight to it
        if (!ok && grid.at(gx, gy).surface() == Surface::DeepWater) u.path.push_back({std::int16_t(gx), std::int16_t(gy)});
        if (!u.path.empty()) u.path_i = 1;
    }
    u.repath_ms = now_ms;
}

void World::follow_path(Unit& u) {
    if (u.path_pending && path_budget_ > 0 && !u.path.empty()) {  // [U-287] the search that did not fit last tick
        u.path_pending = false;
        const auto [px, py] = u.path.back();
        path_to(u, px * kCellUnits + kCellUnits / 2, py * kCellUnits + kCellUnits / 2);
    }
    if (u.path_i >= u.path.size()) {
        if (u.order.kind == OrderKind::Move) u.order = {};
        return;
    }
    // [U-357] look ahead: skip waypoints while the straight line to a later one keeps the whole body clear
    // (clearance ≥ the body's reach) → smooth paths, no zig-zag along walls; at most 6 checks per tick.
    if (!u.flying() && u.path.size() > u.path_i + 1) {
        const int k = std::min(3, int((body_reach(*u.body, size_pct(u)) + kCellUnits / 2) / kCellUnits));
        const int ux_ = u.ux() / kCellUnits, uy_ = u.uy() / kCellUnits;
        for (int n = 0; n < 6 && u.path_i + 1 < u.path.size(); ++n) {
            const auto [nx, ny] = u.path[u.path_i + 1];
            if (!line_walkable(grid, ux_, uy_, nx, ny, clearance.data(), k)) break;
            ++u.path_i;
        }
    }
    const auto [cx, cy] = u.path[u.path_i];
    const bool last = u.path_i + 1 == u.path.size();
    std::int32_t tx = (cx * kCellUnits + kCellUnits / 2) * kSub, ty = (cy * kCellUnits + kCellUnits / 2) * kSub;
    if (last && (u.order.kind == OrderKind::Move || u.order.kind == OrderKind::AttackMove)) {
        tx = std::clamp(u.order.x, 0, kMapUnits - 1) * kSub;
        ty = std::clamp(u.order.y, 0, kMapUnits - 1) * kSub;
    }
    move_towards(u, tx, ty, move_speed_sub(u));
    if (dist2(u.x / kSub, u.y / kSub, tx / kSub, ty / kSub) <= 8 * 8) ++u.path_i;
    if (u.stuck_ms > 250 && now_ms - u.repath_ms > 400) {  // wall appeared → new path [X-09]
        const std::int32_t gx = last ? tx / kSub : (u.path.back().first * kCellUnits + 16);
        const std::int32_t gy = last ? ty / kSub : (u.path.back().second * kCellUnits + 16);
        path_to(u, gx, gy);
    }
}

void World::step_units() {
    const std::int32_t tick_ms = std::int32_t(1'000 / cfg.tick_hz);
    rock_bodies_.clear();  // [U-386]
    for (const Effect& f : fx)
        if (!f.dead && f.kind == FxKind::Boulder) rock_bodies_.push_back({f.ux(), f.uy(), f.radius});
    for (std::size_t i = 0; i < units.size(); ++i) {
        Unit& u = units[i];
        if (!u.alive) {
            if (u.kind == UnitKind::Champion && now_ms >= u.respawn_ms) respawn(u);
            continue;
        }
        u.moving = false;
        u.st.expire(now_ms);
        if (u.tmp_size_until && u.tmp_size_until <= now_ms) {
            u.tmp_size_until = 0;
            recompute_stats(u);
        }
        if (u.transform_until && u.transform_until <= now_ms) {  // [U-456] the form ends
            u.transform_until = 0;
            recompute_stats(u);
        }
        if (u.lotus_until && u.lotus_until <= now_ms) {
            u.lotus_until = 0;
            recompute_stats(u);
        }
        regen(u);
        if (u.kind == UnitKind::Champion)  // [U-433] charges come back one by one
            for (std::size_t sl = 0; sl < u.skill.size(); ++sl) {
                AbilityRt& a = u.ab[sl];
                const SkillDef* d = u.skill[sl];
                if (!d || d->charges == 0 || a.charges_used == 0 || now_ms < a.charge_next_ms) continue;
                --a.charges_used;
                a.charge_next_ms = a.charges_used > 0 ? now_ms + d->charge_ms : 0;
                if (a.cd_end_ms > now_ms + d->charge_gap_ms) a.cd_end_ms = now_ms + d->charge_gap_ms;  // a charge is ready again
            }
        resolve_overlaps(u);  // [X-94]
        // [U-188][U-189] water: riding needs water under you; otherwise deep water = swimming
        if (u.st.has(StatusKind::WaterRide) && !on_any_water(u)) u.st.remove(StatusKind::WaterRide);
        if (!u.is_structure() && u.kind != UnitKind::Inhibitor && !elevated(u) && on_deep_water(u))
        {
            u.st.add(StatusKind::Swimming, now_ms + 150, 0, u.id);
            const bool waterbender = u.kind == UnitKind::Champion && champions()[std::size_t(u.champ)].element == Element::Water;
            u.st.add(StatusKind::Wet, now_ms + (waterbender ? 1'000 : 5'000), 0, u.id);  // [U-241] element_rules.hpp
        }
        else
            u.st.remove(StatusKind::Swimming);
        // [U-292] standing in shallow water or on a puddle (water on the ground, the body not under water):
        // wet only while there (it keeps being refreshed; a longer wet from a hit is kept)
        if (!u.is_structure() && u.kind != UnitKind::Inhibitor && !u.flying() && !u.st.has(StatusKind::Swimming) && on_ground_water(u))
            u.st.add(StatusKind::Wet, now_ms + elements::kWetWhileOnWaterMs, 0, u.id);
        // [U-293] an ice patch is slippery: slower while on it
        if (!u.is_structure() && u.kind != UnitKind::Inhibitor && !u.flying())
            for (const Cloud& p : clouds)
                if (p.kind == CloudKind::IcePatch && dist2(u.ux(), u.uy(), p.x, p.y) <= std::int64_t(p.radius) * p.radius) {
                    u.st.add(StatusKind::Slow, now_ms + elements::kWetWhileOnWaterMs, elements::kIcePatchSlowPct, 0);
                    break;
                }
        // [U-36] burn: damage per second from every burn source. [U-254] gathered and dealt every 0.5 s through
        // apply_hit → shields absorb it, it shows as a number, counts in the statistics and the death recap.
        for (int k = 0; k < u.st.size(); ++k) {
            const Status& s = u.st[k];
            if (s.kind == StatusKind::Burn) {
                u.burn_acc += per_tick(s.value, tick, cfg.tick_hz);
                u.burn_src = s.source;
            }
        }
        if (u.burn_acc > 0 && (now_ms - u.burn_flush_ms >= 500 || !u.st.has(StatusKind::Burn))) {
            const std::int32_t d = std::int32_t(std::min<std::int64_t>(u.burn_acc, 2'000'000'000));
            u.burn_acc = 0;
            u.burn_flush_ms = now_ms;
            const DmgTag saved = dmg_tag_;
            dmg_tag_ = {DmgFrom::Burn};
            apply_hit(find(u.burn_src), u, d, SubElement::Fire, false, false);
            dmg_tag_ = saved;
            if (!u.alive) continue;
        }
        if (u.hp <= 0) {
            Unit* k = find(u.last_hitter);
            on_kill(k, u);
            continue;
        }
        // [U-78] bloodbound: dragged towards the bloodbender
        if (u.st.has(StatusKind::Bloodbound)) {
            for (int k = 0; k < u.st.size(); ++k)
                if (u.st[k].kind == StatusKind::Bloodbound)
                    if (const Unit* b = find(u.st[k].source); b && b->alive) {
                        const std::int64_t dx = b->x - u.x, dy = b->y - u.y;
                        const std::int64_t d = isqrt(dx * dx + dy * dy);
                        if (d > 200 * kSub) {
                            const std::int32_t sp = 120 * kSub / std::int32_t(cfg.tick_hz);
                            const std::int32_t nx = u.x + std::int32_t(dx * sp / d), ny = u.y + std::int32_t(dy * sp / d);
                            if (walkable_at(u, nx, ny)) {
                                u.x = nx;
                                u.y = ny;
                            }
                        }
                    }
        }
        if (!u.st.has(StatusKind::Fear))  // [U-472] a feared unit only runs (below)
        switch (u.kind) {
            case UnitKind::Minion: step_minion(u); break;
            case UnitKind::Turret:
            case UnitKind::Nexus: step_turret(u); break;
            case UnitKind::Monster:
            case UnitKind::Boss: step_monster(u); break;
            default: break;
        }
        if (u.is_structure() || u.kind == UnitKind::Inhibitor) continue;
        if (u.riding) continue;  // [U-407] the companion moves the rider

        // channels
        if (u.channel_end_ms) {
            if (u.channel_end_ms <= now_ms) {
                u.channel_end_ms = 0;
                if (u.order.kind == OrderKind::Recall) {
                    const auto [fx_, fy_] = fountain(u.team);
                    u.x = fx_ * kSub;
                    u.y = fy_ * kSub;
                } else if (u.order.kind == OrderKind::Portal) {  // [X-54]
                    const CellPt n = cur_map().portal, s = mirror(cur_map().portal);
                    const bool at_north = dist2(u.ux(), u.uy(), n.first * kCellUnits, n.second * kCellUnits) <
                                          dist2(u.ux(), u.uy(), s.first * kCellUnits, s.second * kCellUnits);
                    const CellPt to = at_north ? s : n;
                    u.x = (to.first * kCellUnits + 16) * kSub;
                    u.y = (to.second * kCellUnits + 16) * kSub;
                    place_on_walkable(u);
                }
                u.order = {};
            }
            continue;
        }
        // dashing: the Dash effect moves the unit
        bool dashing = false;
        for (const Effect& e : fx) dashing |= !e.dead && e.kind == FxKind::Bolt && e.def && e.def->kind == SkillKind::Dash && e.owner == u.id;
        if (dashing) continue;

        if (u.casting) {
            const SkillDef* d = u.skill[u.cast.slot];
            const Dir want = dir_of(std::int64_t(u.cast.x) - u.ux(), std::int64_t(u.cast.y) - u.uy());
            const bool turns = !d || d->turns_caster;  // [U-184] some skills cast without turning
            if (turns && (u.cast.x != u.ux() || u.cast.y != u.uy())) turn_towards(u, want);
            const bool facing = !turns || (std::int64_t(u.face.x) * want.x + std::int64_t(u.face.y) * want.y) / kOne >= kOne / 2;
            if (!facing) u.cast_end_ms += tick_ms;  // [U-74] turn first, then the cast time runs
            if (!u.st.can_cast() || (d && skill_block_reason(u, *d))) u.casting = false;  // [U-474]
            else if (now_ms >= u.cast_end_ms) finish_cast(u);
            if (d && d->move == CastMove::Rooted && u.casting) continue;  // [U-53]
        }
        if (lol_fear_step(u)) continue;  // [U-472] feared: runs away (lol.cpp)
        if (!u.st.can_move()) {
            u.windup_end_ms = std::min<std::int64_t>(u.windup_end_ms, 0);
            continue;
        }
        if (u.kind != UnitKind::Champion) {
            if (u.order.kind == OrderKind::Attack) update_attack(u);
            continue;
        }
        switch (u.order.kind) {
            case OrderKind::Move:
                follow_path(u);
                if (champions()[std::size_t(u.champ)].autos.move_while_attacking) attack_on_the_move(u);  // [U-217]
                break;
            case OrderKind::Attack: update_attack(u); break;
            case OrderKind::AttackWall: update_attack_wall(u); break;  // [U-393]
            case OrderKind::AttackMove: {
                // nearest visible enemy within 600 → attack it. slot bit 1 = champions only [X-98];
                // bit 2 = nearest to the CLICKED point instead of to me [U-303] (an option in Controls)
                std::uint32_t best = 0;
                std::int64_t bd = 600LL * 600;
                const bool champs_only = (u.order.slot & 1) != 0, to_cursor = (u.order.slot & 2) != 0;
                for (const Unit& o : units)
                    if (o.alive && o.team != u.team && o.team != Team::Neutral && targetable(o) && vulnerable(o) &&
                        (!champs_only || o.kind == UnitKind::Champion) && visible_to(u.team, o)) {
                        const std::int64_t d = to_cursor ? dist2(o.ux(), o.uy(), u.order.x, u.order.y) : dist2(o.ux(), o.uy(), u.ux(), u.uy());
                        if (d < bd) {
                            bd = d;
                            best = o.id;
                        }
                    }
                if (best) {
                    u.order = {OrderKind::Attack, best};
                    update_attack(u);
                } else {
                    follow_path(u);
                }
                break;
            }
            case OrderKind::Cast: {  // targeted skill: walk into range, then cast
                Unit* t = find(u.order.target);
                const SkillDef* od = u.order.slot < u.skill.size() ? u.skill[u.order.slot] : nullptr;
                if (!t || !t->alive || !od || !can_target(u, *od, *t)) {  // [U-431] seen, or sensed
                    u.order = {};
                    break;
                }
                const ResolvedAbility r = resolved(u, Slot(u.order.slot));
                if (edge_gap(u, *t) <= r.range) {
                    start_cast(u, {OrderKind::Cast, t->id, t->ux(), t->uy(), u.order.slot});
                    u.order = {};
                } else {
                    move_towards(u, t->x, t->y, move_speed_sub(u));
                }
                break;
            }
            case OrderKind::None: {
                // [X-99] auto-acquire: an idle champion hits the closest enemy already in attack range
                if (!u.auto_attack || u.casting) break;
                std::uint32_t best = 0;
                std::int64_t bd = INT64_MAX;
                for (const Unit& o : units)
                    if (o.alive && o.team != u.team && o.team != Team::Neutral && targetable(o) && vulnerable(o) &&
                        visible_to(u.team, o)) {
                        const std::int64_t g = edge_gap(u, o);
                        if (g <= u.attack_range && g < bd) {
                            bd = g;
                            best = o.id;
                        }
                    }
                if (best) {
                    u.order = {OrderKind::Attack, best};
                    u.auto_acquired = true;
                    update_attack(u);
                }
                break;
            }
            default: break;
        }
    }
}

void World::respawn(Unit& u) {
    const auto [fx_, fy_] = fountain(u.team);
    u.alive = true;
    u.x = fx_ * kSub;
    u.y = fy_ * kSub;
    u.st.clear();
    u.order = {};
    u.path.clear();
    u.casting = false;
    // [U-383] nothing of the last life carries over
    u.riding = false, u.steer_until = 0, u.draw_pts.clear(), u.channel_end_ms = 0, u.windup_end_ms = 0;
    u.untargetable_until = 0, u.flask_revive = 0, u.emote = 255;
    recompute_stats(u);
    u.hp = u.stats.health.total();
    u.res = u.stats.resource == ResourceKind::Rage ? 0 : u.res_max;  // [U-268] rage starts empty
    u.x += std::int32_t(kDirs[u.id % 64].x * 40 / kOne * kSub);  // spread around the fountain
    u.y += std::int32_t(kDirs[u.id % 64].y * 40 / kOne * kSub);
    place_on_walkable(u);
}

}  // namespace em
