// Casting [U-42][U-53][U-106] and what every SkillKind does in the world [U-109][U-131].
#include "core/wall_rules.hpp"
#include <algorithm>
#include <format>
#include "game/match/layout.hpp"
#include "game/mechanics/element_rules.hpp"
#include "game/world.hpp"

namespace em {

void World::start_cast(Unit& u, const Order& o) {
    const Slot s = Slot(o.slot);
    if (!can_cast_now(u, s, nullptr, o.sx, o.sy)) return;
    u.casting = true;
    u.cast = o;
    u.windup_end_ms = 0;
    u.cast_end_ms = now_ms + resolved(u, s).cast_time_ms;
    if (u.cast_end_ms <= now_ms) finish_cast(u);
}

void World::finish_cast(Unit& u) {
    u.casting = false;
    const Slot s = Slot(u.cast.slot);
    if (!can_cast_now(u, s, nullptr, u.cast.sx, u.cast.sy)) return;
    const SkillDef& d = *u.skill[std::size_t(s)];
    AbilityRt& a = u.ab[std::size_t(s)];
    ResolvedAbility r = resolved(u, s);
    // [U-339] water as numbers: from a source = no carried water; the Avatar pays from its small pool
    const bool water_free = water_skill(d) && water_sourced(u, d, u.cast.sx, u.cast.sy);
    if (water_free && u.stats.resource == ResourceKind::Water) r.cost = 0;
    if (water_skill(d) && u.water_max > 0 && !water_free) u.water = std::max(0, u.water - elements::kAvatarWaterCost);
    // pay [U-60..U-64]; comet = free fuel [U-138]
    const bool free_fuel = u.stats.resource == ResourceKind::Fuel && unlimited_fuel(sky);
    const std::int32_t cost = std::int32_t(r.cost * 100);
    u.rage_boost = false;
    if (u.stats.resource == ResourceKind::Rage) {  // [U-268] full rage cost paid → this skill +20 % damage
        if (cost > 0 && u.res >= cost) {
            u.res -= cost;
            u.rage_boost = true;
        }
    } else if (!free_fuel) {
        const std::int32_t from_fuel = std::min(u.res, cost);
        u.res -= from_fuel;
        if (u.stats.resource == ResourceKind::Fuel && cost > from_fuel)  // [U-268] life pays the rest
            u.hp = std::max(1, u.hp - (cost - from_fuel) * kLifePerFuel);
    }
    // cooldown + multicast [U-106]
    if (d.casts > 1) {
        const MulticastDef md{.max_casts = std::uint8_t(d.casts), .recast_window_ms = std::uint16_t(d.recast_ms)};
        if (a.mc.casts_used > 0 && now_ms > a.mc.window_end_ms) a.mc = {};
        a.mc.try_cast(md, now_ms);
        a.cd_end_ms = a.mc.casts_used >= d.casts ? now_ms + r.cooldown_ms : now_ms + d.recast_ms + r.cooldown_ms;
        if (a.mc.casts_used >= d.casts) a.mc = {};
    } else if (d.charges > 0) {  // [U-433] one charge used; empty → wait for the next charge
        if (a.charges_used == 0) a.charge_next_ms = now_ms + d.charge_ms;
        a.charges_used = std::uint8_t(std::min<int>(a.charges_used + 1, d.charges));
        a.cd_end_ms = a.charges_used < d.charges ? now_ms + d.charge_gap_ms : a.charge_next_ms;
    } else {
        a.cd_end_ms = now_ms + r.cooldown_ms;
    }
    // [U-279] Elemental Cycle: this skill gets the bonus if the cycle was complete; then count its element
    const bool cycle_bonus = u.cycle_ready && d.kind != SkillKind::AvatarState;
    if (cycle_bonus) u.cycle_ready = false;
    u.cycle_boost = cycle_bonus;
    cast_skill(u, s, u.cast.x, u.cast.y, u.cast.target);
    if (water_free && u.cast.sx >= 0) take_from_source(u, u.cast.sx, u.cast.sy);  // [U-339]
    {  // [U-434] shaping the ground holds the bender in place a moment
        const bool shapes = d.kind == SkillKind::Pillar || d.kind == SkillKind::Wall || d.kind == SkillKind::Boulder || d.kind == SkillKind::StonePush;
        const int root = d.self_root_ms >= 0 ? d.self_root_ms : shapes ? 300 : 0;
        if (root > 0) u.st.add(StatusKind::Root, now_ms + root, 0, u.id);
    }
    u.cycle_boost = false;
    if (has_passive(u, PassiveId::ElementalCycle) && !cycle_bonus) {
        const Element el = base_element(d.sub);
        if (el != Element::None) u.cycle_mask |= std::uint8_t(1u << int(el));
        if (u.cycle_mask == 0b1111) {
            u.cycle_mask = 0;
            u.cycle_ready = true;
        }
    }
    // [U-241] a waterbender casting waterbending pulls the water out of themselves at once
    if (base_element(d.sub) == Element::Water && u.kind == UnitKind::Champion && champions()[std::size_t(u.champ)].element == Element::Water)
        dry(u);
    // keystones that trigger on any skill [U-177]
    if (has_keystone(u.loadout, KeystoneId::Tailwind)) {
        u.st.add(StatusKind::Haste, now_ms + 1'500, 20, u.id);
        ++u.stat_keystone;
    }
    if (has_keystone(u.loadout, KeystoneId::ChiRush)) u.chirush_until = now_ms + 3'000;
}

void World::cast_skill(Unit& u, Slot slot, std::int32_t tx, std::int32_t ty, std::uint32_t target) {
    const SkillDef& d = *u.skill[std::size_t(slot)];
    ResolvedAbility r = resolved(u, slot);
    // [U-340] source first: a water skill bent from a chosen source starts there, +25 % range
    const bool from_source = water_skill(d) && u.cast.sx >= 0 && u.cast.slot == std::uint8_t(slot) &&
                             water_source_at(u.cast.sx, u.cast.sy) != WaterSource::None && water_sourced(u, d, u.cast.sx, u.cast.sy);
    // [U-378] rock bending (lift a rock, then throw it): an earth bolt / wave picks up a chosen rock and throws IT
    const bool from_rock = !from_source && d.sub == SubElement::Earth && source_first(d) && u.cast.sx >= 0 &&
                           u.cast.slot == std::uint8_t(slot) && rock_source_at(u.cast.sx, u.cast.sy) &&
                           dist2(u.cast.sx, u.cast.sy, u.ux(), u.uy()) <=
                               std::int64_t(d.range + elements::kSourceReachCells * kCellUnits) * (d.range + elements::kSourceReachCells * kCellUnits);
    const bool starts_at_source = (from_source && (d.kind == SkillKind::Bolt || d.kind == SkillKind::Wave || d.kind == SkillKind::Cone)) || from_rock;
    if (from_source || from_rock) r.range = r.range * elements::kSourceRangePct / 100;
    std::int32_t ox = starts_at_source ? u.cast.sx : u.ux(), oy = starts_at_source ? u.cast.sy : u.uy();
    if (from_rock) {  // the rock leaves its place (the whole segment / rock breaks) and flies from just beyond it
        const Dir rd = dir_of(std::int64_t(tx) - ox, std::int64_t(ty) - oy);
        damage_wall(ox / kCellUnits, oy / kCellUnits, 1'000'000, rd);
        ox += rd.x * 48 / kOne, oy += rd.y * 48 / kOne;
    }
    // [U-264] sky events (planetary alignment) stretch durations; damage-over-time has its own %
    const bool dot_skill = d.kind == SkillKind::Field && d.dmg > 0;
    const std::int32_t dur_ms = std::int32_t(std::int64_t(d.dur_ms) * (dot_skill ? r.dot_pct : r.dur_pct) / 100);
    // [U-184] aim: at the cursor (normal) or where the champion faces
    if (d.aim == Aim::Facing) {
        const std::int64_t len = std::max<std::int64_t>(1, isqrt(dist2(tx, ty, ox, oy)));
        tx = ox + std::int32_t(u.face.x * len / kOne);
        ty = oy + std::int32_t(u.face.y * len / kOne);
    }
    const Dir dir = (tx == ox && ty == oy) ? u.face : dir_of(std::int64_t(tx) - ox, std::int64_t(ty) - oy);
    if (d.turns_caster) u.face = dir;
    const std::int64_t dist = isqrt(dist2(tx, ty, ox, oy));
    // point casts are clamped to range
    std::int32_t px = tx, py = ty;
    if (r.range > 0 && dist > r.range) {
        px = ox + std::int32_t(std::int64_t(dir.x) * r.range / kOne);
        py = oy + std::int32_t(std::int64_t(dir.y) * r.range / kOne);
    }
    if (r.range == 0) {
        px = u.ux();
        py = u.uy();
    }
    Effect e;
    e.team = u.team;
    e.owner = u.id;
    e.def = &d;
    e.sub = skill_sub(u, d);  // [U-264]
    e.flow = flow_of(e.sub);
    e.x = starts_at_source ? ox * kSub : u.x;
    e.y = starts_at_source ? oy * kSub : u.y;
    e.dir = dir;
    e.damage = std::int32_t(r.damage * (u.rage_boost ? 100 + kRageBoostPct : 100) / 100 * (u.cycle_boost ? 140 : 100) / 100);  // [U-268][U-279]
    if (e.damage > 0) e.damage = e.damage * (100 + draw_from_wall(u, e.sub)) / 100;  // [U-288] a wall nearby as the source
    if (from_rock) e.damage = e.damage * (100 + elements::kRockSourceBonusPct) / 100;  // [U-378]
    if (from_source && champions()[std::size_t(u.champ)].element == Element::Water) {  // [U-340] the chosen source, form chosen
        e.sub = u.source_ice ? SubElement::Ice : SubElement::Water;
        e.flow = flow_of(e.sub);
        e.damage = e.damage * (100 + elements::kWaterSourceBonusPct) / 100;
    } else if (const int wb = draw_from_water_source(u, e.sub); wb > 0) {  // [U-293] a puddle / ice patch as the source (form chosen)
        e.flow = flow_of(e.sub);
        e.damage = e.damage * (100 + wb) / 100;
    }
    e.physical = e.sub == SubElement::Physical;
    e.cc = d.cc;
    e.cc_ms = d.cc == StatusKind::Burn ? std::int32_t(std::int64_t(d.cc_ms) * r.dot_pct / 100) : d.cc_ms;  // [U-264]
    e.cc_val = d.cc_val;
    e.knock = d.knock;
    e.pierce = d.pierce;
    e.group = d.group;
    e.ally = d.ally;
    e.src = u.stats;
    e.tick_ms = d.tick_ms;
    e.light = std::int32_t(r.light_radius);
    if (e.light > 0) e.light_until = 0;  // set when the effect ends
    const std::int32_t speed_sub = std::int32_t(std::max<std::int64_t>(1, r.speed) * kSub / cfg.tick_hz);
    switch (d.kind) {
        case SkillKind::Bolt:
        case SkillKind::Wave: {
            e.kind = d.kind == SkillKind::Bolt ? FxKind::Bolt : FxKind::Wave;
            e.radius = std::int32_t(std::max<std::int64_t>(12, r.width / 2));
            e.width = std::int32_t(r.width);
            e.vx = dir.x * speed_sub / kOne;
            e.vy = dir.y * speed_sub / kOne;
            e.travel_left = std::int32_t(r.range * kSub);
            e.end_ms = now_ms + 10'000;
            spawn_fx(e);
            break;
        }
        case SkillKind::Dash: {  // the effect drags its owner [U-51] fire jets / charges
            e.kind = FxKind::Bolt;
            e.radius = std::int32_t(std::max<std::int64_t>(20, r.width / 2));
            e.vx = dir.x * speed_sub / kOne;
            e.vy = dir.y * speed_sub / kOne;
            e.travel_left = std::int32_t(std::min<std::int64_t>(r.range, dist == 0 ? r.range : std::max<std::int64_t>(dist, 120)) * kSub);
            e.pierce = true;
            e.end_ms = now_ms + 3'000;
            if (d.sub == SubElement::Fire) u.st.add(StatusKind::Flying, now_ms + 600, 0, u.id);  // jets lift you
            spawn_fx(e);
            break;
        }
        case SkillKind::Burst: {
            e.kind = FxKind::Burst;
            e.x = px * kSub;
            e.y = py * kSub;
            e.radius = std::int32_t(r.radius);
            e.arm_ms = now_ms + d.delay_ms;
            e.end_ms = e.arm_ms + 300;
            spawn_fx(e);
            break;
        }
        case SkillKind::Field:
        case SkillKind::Heal: {
            e.kind = d.kind == SkillKind::Heal ? FxKind::Heal : FxKind::Field;
            e.x = px * kSub;
            e.y = py * kSub;
            e.radius = std::int32_t(r.radius);
            std::int32_t dur = d.kind == SkillKind::Heal ? 450 : dur_ms;
            if (has_passive(u, PassiveId::Molten) && d.sub == SubElement::Lava) dur += 1'000;
            e.end_ms = now_ms + dur;
            e.next_tick_ms = now_ms;
            Effect& f = spawn_fx(e);
            if (d.sub == SubElement::Lava) lava_pool(px, py, f.radius, dur);  // [X-56]
            if (d.kind == SkillKind::Field) scorch(f);                        // [U-227][U-244]
            if (d.cc == StatusKind::Blind)  // smoke / ash / mist / sand: a cloud that blocks vision [U-241]
                add_cloud(d.sub == SubElement::Steam ? CloudKind::Steam : base_element(d.sub) == Element::Earth ? CloudKind::Sand : CloudKind::Smoke,
                          px, py, f.radius, dur);
            if (d.kind == SkillKind::Heal) {
                std::int32_t amount = std::int32_t(r.damage * 100);
                if (has_passive(u, PassiveId::Tide)) amount = amount * 130 / 100;
                const Shape sh = shape_of(f);
                for (Unit& o : units)
                    if (o.alive && o.team == u.team && o.kind == UnitKind::Champion && touches(sh, hitbox(o))) {
                        // [U-313] + % of the ally's current / max / missing health
                        const std::int64_t mx = o.stats.health.total(), cur = std::max(0, o.hp);
                        const std::int32_t heal_here = amount + std::int32_t((cur * d.pct_current_hp + mx * d.pct_max_hp +
                                                                              (mx - cur) * d.pct_missing_hp) / 10'000);
                        u.stat_healing += std::max(0, std::min(heal_here, o.stats.health.total() - o.hp)) / 100;
                        heal(o, heal_here);
                    }
            }
            break;
        }
        case SkillKind::Cone: {
            e.kind = FxKind::Cone;
            e.length = std::int32_t(r.range);
            e.width = std::int32_t(r.width);
            e.end_ms = now_ms + 250;
            Effect& f = spawn_fx(e);
            effect_hits(f);
            scorch(f);  // [U-227][U-244]
            break;
        }
        case SkillKind::Wall: {
            if (d.sub == SubElement::Ice && !from_source) use_water_source(u);  // [U-294] a puddle / ice patch used as the source is gone
            // [U-380] the wall standard: segments of 100, 33 / 66 / 99 thick, 1 / 2 levels, HP per segment (+ per rank)
            const int rank = std::max<int>(1, u.ab[std::size_t(slot)].rank);
            const bool metal = wall_kind_of(d.sub) == Wall::Metal;  // [U-388] small, strong
            const WallSpec ws{.thick = (d.wall_thick ? std::clamp<int>(d.wall_thick, 1, 3) : metal || d.control == 2 ? 1 : 2) * kWallThickStep,  // drawn: thin [U-436]
                              .level = d.wall_level ? std::clamp<int>(d.wall_level, 1, 2) : 2,
                              .hp = (d.wall_hp ? d.wall_hp : d.sub == SubElement::Ice ? kIceWallSegHp : metal ? kMetalSegHp : kWallSegHp) +
                                    d.wall_hp_rank * (rank - 1)};
            const std::int32_t wall_len = d.wall_segs ? d.wall_segs * kWallSegLen : metal ? kWallSegLen : std::int32_t(r.width);
            if (d.control == 2 && u.draw_pts.size() >= 2 && u.draw_slot == std::uint8_t(slot)) {  // [U-402] the drawn path
                const std::int32_t life = wall_kind_of(d.sub) == Wall::Metal ? dur_ms * kMetalWallLifePct / 100 : dur_ms;
                std::int64_t left = r.width, carry = kWallSegLen / 2;  // a segment centre every 100 units of the path
                for (std::size_t i = 1; i < u.draw_pts.size() && left > 0; ++i) {
                    const auto [ax, ay] = u.draw_pts[i - 1];
                    const auto [bx, by] = u.draw_pts[i];
                    const std::int64_t len = isqrt(dist2(ax, ay, bx, by));
                    if (len == 0) continue;
                    const Dir along = dir_of(std::int64_t(bx) - ax, std::int64_t(by) - ay);
                    std::int64_t at = carry;
                    for (; at <= len && left > 0; at += kWallSegLen) {
                        const std::int32_t cx = ax + std::int32_t((bx - ax) * at / len), cy = ay + std::int32_t((by - ay) * at / len);
                        if (dist2(cx, cy, u.ux(), u.uy()) <= (r.range + 50) * (r.range + 50))  // only within reach
                            raise_segment(cx, cy, along, wall_kind_of(d.sub), life, u.team, base_element(d.sub) == Element::Earth, ws);
                        left -= kWallSegLen;
                    }
                    carry = at - len;  // how far into the next piece of the path the next centre lies
                }
                u.draw_pts.clear();
            } else if (d.radius > 0) {  // ring [Iron Prison]
                raise_ring(px, py, std::int32_t(r.radius), wall_kind_of(d.sub) == Wall::Metal ? dur_ms * kMetalWallLifePct / 100 : dur_ms, u.team,
                           wall_kind_of(d.sub), ws);
                e.kind = FxKind::Burst;
                e.x = px * kSub;
                e.y = py * kSub;
                e.radius = std::int32_t(r.radius) - 20;
                e.arm_ms = now_ms;
                e.end_ms = now_ms + 200;
                spawn_fx(e);
            } else {
                const Dir along{-dir.y, dir.x};
                raise_wall(px, py, along, wall_len, wall_kind_of(d.sub),
                           wall_kind_of(d.sub) == Wall::Metal ? dur_ms * kMetalWallLifePct / 100 : dur_ms, u.team,
                           base_element(d.sub) == Element::Earth, ws);
            }
            break;
        }
        case SkillKind::Pillar: {  // [U-45]
            raise_pillar(px, py, std::int32_t(r.radius), dur_ms, u.team);
            e.kind = FxKind::Burst;
            e.x = px * kSub;
            e.y = py * kSub;
            e.radius = std::int32_t(r.radius);
            e.arm_ms = now_ms + 150;
            e.end_ms = now_ms + 450;
            spawn_fx(e);
            break;
        }
        case SkillKind::Tornado: {
            e.kind = FxKind::Tornado;
            e.radius = std::int32_t(r.radius);
            e.vx = dir.x * speed_sub / kOne;
            e.vy = dir.y * speed_sub / kOne;
            e.travel_left = std::int32_t(r.range * kSub);
            e.end_ms = now_ms + dur_ms;
            e.next_tick_ms = now_ms;
            e.pierce = true;
            spawn_fx(e);
            break;
        }
        case SkillKind::Shield: {
            u.st.add(StatusKind::Shield, now_ms + dur_ms, std::int32_t(r.damage * 100), u.id);
            u.stat_shielding += r.damage;
            u.shield_sub = std::uint8_t(d.sub);  // shown on the HP bar [U-155]
            if (d.size_bonus) {  // [U-129] armour makes the model (and hitbox) bigger
                u.tmp_size = d.size_bonus;
                u.tmp_size_until = now_ms + dur_ms;
                recompute_stats(u);
            }
            break;
        }
        case SkillKind::Haste: u.st.add(StatusKind::Haste, now_ms + dur_ms, d.cc_val, u.id); break;
        case SkillKind::Blink: {
            if (!d.teleport) {  // [U-229] no dash teleports for now: a very fast dash instead
                e.kind = FxKind::Bolt;
                e.radius = 20;
                const std::int32_t fast = 4'000 * kSub / std::int32_t(cfg.tick_hz);
                e.vx = dir.x * fast / kOne;
                e.vy = dir.y * fast / kOne;
                e.travel_left = std::int32_t(std::min<std::int64_t>(dist, r.range) * kSub);
                e.pierce = true;
                e.damage = 0;
                e.end_ms = now_ms + 1'000;
                static constexpr SkillDef kFastDash{.name = "fast dash", .kind = SkillKind::Dash};
                e.def = &kFastDash;
                spawn_fx(e);
                break;
            }
            const std::int32_t step = std::int32_t(std::min<std::int64_t>(dist, r.range));  // real teleport
            for (std::int32_t s = step; s >= 0; s -= 16) {
                const std::int32_t nx = (u.ux() + dir.x * s / kOne) * kSub, ny = (u.uy() + dir.y * s / kOne) * kSub;
                if (nx >= 0 && ny >= 0 && nx / kSub < kMapUnits && ny / kSub < kMapUnits && body_violations(u, nx, ny) == 0) {  // whole body must fit [X-94]
                    u.x = nx;
                    u.y = ny;
                    break;
                }
            }
            u.order = {};
            break;
        }
        case SkillKind::Lantern: {  // [U-118][X-62]
            e.kind = FxKind::Lantern;
            e.radius = std::int32_t(r.radius);
            e.vx = dir.x * speed_sub / kOne;
            e.vy = dir.y * speed_sub / kOne;
            e.travel_left = std::int32_t(std::min<std::int64_t>(dist, r.range) * kSub);
            e.end_ms = now_ms + r.light_ms;
            e.damage = 0;
            spawn_fx(e);
            break;
        }
        case SkillKind::WindSense: u.wind_sense_until = now_ms + kWindSenseMs; break;  // [U-141]
        case SkillKind::Flight:  // [U-51][U-77][U-224]
            u.st.add(StatusKind::Flying, now_ms + dur_ms, 0, u.id);
            u.lifted_by = kAirSelf;
            // Sky Ascension: the farther you aim, the higher you fly (40 … reach_height); Updraft: 120
            u.fly_height = d.reach_height > 0 ? std::int32_t(40 + (d.reach_height - 40) * std::min<std::int64_t>(dist, std::max(1, d.range)) /
                                                                  std::max(1, d.range))
                                              : 120;
            u.st.add(StatusKind::Haste, now_ms + dur_ms, d.cc_val, u.id);
            break;
        case SkillKind::MagicStance:  // [U-264]
            u.magic_until = now_ms + dur_ms;
            u.magic_sub = std::uint8_t(d.sub);
            break;
        case SkillKind::AvatarState:  // [U-279] fly + power + size + speed
            u.avatar_until = now_ms + dur_ms;
            u.st.add(StatusKind::Flying, now_ms + dur_ms, 0, u.id);
            u.lifted_by = kAirSelf;
            u.fly_height = 120;
            u.st.add(StatusKind::Haste, now_ms + dur_ms, d.cc_val, u.id);
            add_feed(std::format("{} enters the Avatar State!", u.name));
            break;
        case SkillKind::SelfFreeze:  // [U-143] frozen self: untouchable, no heat
            u.st.add(StatusKind::Frozen, now_ms + dur_ms, 0, u.id);
            u.untargetable_until = now_ms + dur_ms;
            u.order = {};
            break;
        case SkillKind::Bloodbend: {  // [U-78] targeted, full moon only
            Unit* t = find(target);
            if (t && t->alive && t->team != u.team) apply_cc(u.stats, u.id, *t, StatusKind::Bloodbound, d.cc_ms, 0);
            break;
        }
        case SkillKind::Boulder: {  // [U-27] cast 1 = raise, cast 2 = push
            AbilityRt& a = u.ab[std::size_t(slot)];
            Effect* rock = nullptr;
            for (Effect& f : fx)
                if (f.id == a.boulder && !f.dead) rock = &f;
            if (!rock || d.casts <= 1) {  // [U-406] a one-cast lift only raises (others push it)
                if (rock) rock->dead = true;  // [U-383] one lifted stone per caster and skill (no stacking)
                if (d.lift_beside) {
                    px = u.ux() + std::int32_t(std::int64_t(dir.x) * (body_reach(*u.body, size_pct(u)) + r.width / 2 + 8) / kOne);
                    py = u.uy() + std::int32_t(std::int64_t(dir.y) * (body_reach(*u.body, size_pct(u)) + r.width / 2 + 8) / kOne);
                }
                e.kind = FxKind::Boulder;
                e.x = px * kSub;
                e.y = py * kSub;
                e.radius = std::int32_t(r.width / 2);
                e.end_ms = now_ms + d.recast_ms;
                e.travel_left = std::int32_t(r.range * kSub);
                a.boulder = spawn_fx(e).id;
                earth_events.push_back({px / kCellUnits, py / kCellUnits});
                earth_event_team.push_back(u.team);
            } else {
                const Effect rest = *rock;  // (copy: spawn_fx below may move the vector)
                const Dir pd = dir_of(std::int64_t(tx) - rock->ux(), std::int64_t(ty) - rock->uy());
                rock->dir = pd;
                rock->vx = pd.x * speed_sub / kOne;
                rock->vy = pd.y * speed_sub / kOne;
                rock->end_ms = now_ms + 5'000;
                if (d.casts > 2 && a.mc.casts_used > 0 && a.mc.casts_used < d.casts) {
                    a.boulder = spawn_fx(rest).id;  // [U-383] more shots left: the next stone is ready where this one was
                } else {
                    a.boulder = 0;
                    a.mc = {};
                }
            }
            break;
        }
        case SkillKind::WaterSpout:  // [U-189] ride the water (only over water, ends on land)
            if (!on_any_water(u)) {
                refuse(u, "Water Spout needs water under you");
                u.ab[std::size_t(slot)].cd_end_ms = now_ms + 1'000;
                break;
            }
            u.st.remove(StatusKind::Swimming);
            u.st.add(StatusKind::WaterRide, now_ms + dur_ms, 0, u.id);
            u.st.add(StatusKind::Haste, now_ms + dur_ms, d.cc_val, u.id);
            break;
        case SkillKind::StonePush: {  // [U-406] push a lifted stone: the chosen one, else the nearest within range
            Effect* rock = nullptr;
            std::int64_t best = -1;
            const bool chosen = u.cast.sx >= 0 && u.cast.slot == std::uint8_t(slot);
            for (Effect& f : fx) {
                if (f.dead || f.kind != FxKind::Boulder || f.vx != 0 || f.vy != 0) continue;
                const std::int64_t to_me = dist2(f.ux(), f.uy(), u.ux(), u.uy());
                if (to_me > std::int64_t(d.range) * d.range) continue;
                const std::int64_t d2 = chosen ? dist2(f.ux(), f.uy(), u.cast.sx, u.cast.sy) : to_me;
                if (chosen && d2 > 120LL * 120) continue;
                if (best < 0 || d2 < best) best = d2, rock = &f;
            }
            if (!rock) {
                refuse(u, std::format("{}: no lifted stone in reach", d.name));
                u.ab[std::size_t(slot)].cd_end_ms = now_ms + 1'000;
                break;
            }
            // the stone flies with THIS skill (its damage, its cc, its owner); the old resting one is gone
            const Dir pd = dir_of(std::int64_t(tx) - rock->ux(), std::int64_t(ty) - rock->uy());
            e.kind = FxKind::Boulder;
            e.x = rock->x, e.y = rock->y;
            e.radius = rock->radius;
            e.dir = pd;
            e.vx = pd.x * speed_sub / kOne;
            e.vy = pd.y * speed_sub / kOne;
            e.travel_left = std::int32_t(r.range * kSub);
            e.end_ms = now_ms + 5'000;
            rock->dead = true;
            spawn_fx(e);  // (rock may dangle after this push_back — not used any more)
            break;
        }
        case SkillKind::MoveWater: {  // [U-398] the water is MOVED: taken from the source, laid down at the target
            const std::int32_t rad = std::int32_t(std::max<std::int64_t>(48, r.radius));
            const std::int32_t life = dur_ms > 0 ? dur_ms : std::int32_t(elements::kPuddleMs);
            std::size_t pi = 0;
            switch (from_source ? water_source_at(u.cast.sx, u.cast.sy, &pi) : WaterSource::None) {
                case WaterSource::Patch: clouds[pi].until_ms = now_ms; break;                               // the puddle moves
                case WaterSource::Lasting: drain_water(u.cast.sx, u.cast.sy, rad, life); break;              // the river gives it
                case WaterSource::IceWall: damage_wall(u.cast.sx / kCellUnits, u.cast.sy / kCellUnits, rad); break;  // melted off
                default: break;  // from carried water (paid as the skill's cost)
            }
            add_cloud(u.source_ice ? CloudKind::IcePatch : CloudKind::Puddle, px, py, rad, life);
            if (d.dmg > 0 || d.cc != StatusKind::Count || d.knock > 0) {  // the wave of water lands: a hit there
                e.kind = FxKind::Burst;
                e.x = px * kSub;
                e.y = py * kSub;
                e.radius = rad;
                e.arm_ms = now_ms + d.delay_ms;
                e.end_ms = e.arm_ms + 300;
                spawn_fx(e);
            }
            break;
        }
        case SkillKind::IcePath:
            if (fire_family(d.sub)) {  // [U-225] Heat Draw: pull the heat out of lava → stone ground, fuel back
                cool_lava(u.ux(), u.uy(), std::int32_t(r.radius));
                if (u.res_max > 0) u.res = std::min(u.res_max, u.res + elements::kHeatDrawFuel);
                // [U-292] … and out of steam: the cloud falls as water on the ground. Everybody inside it at that
                // moment becomes wet (the normal time); nothing else happens to them.
                for (Cloud& c : clouds)  // [U-293] … and ice patches melt into water
                    if (c.kind == CloudKind::IcePatch && dist2(c.x, c.y, u.ux(), u.uy()) <= std::int64_t(c.radius + r.radius) * (c.radius + r.radius))
                        c.kind = CloudKind::Puddle;
                for (Cloud& c : clouds) {
                    if (c.kind != CloudKind::Steam) continue;
                    if (dist2(c.x, c.y, u.ux(), u.uy()) > std::int64_t(c.radius + r.radius) * (c.radius + r.radius)) continue;
                    for (Unit& o : units)
                        if (o.alive && !o.flying() && !o.is_structure() && dist2(o.ux(), o.uy(), c.x, c.y) <= std::int64_t(c.radius) * c.radius) {
                            const bool wb = o.kind == UnitKind::Champion && champions()[std::size_t(o.champ)].element == Element::Water;
                            o.st.add(StatusKind::Wet, now_ms + (wb ? elements::kWetWaterbenderMs : elements::kWetMs), 0, u.id);
                        }
                    c.kind = CloudKind::Puddle;
                    c.until_ms = now_ms + elements::kPuddleMs;
                }
            } else {  // [U-188][U-189] freeze the water around me
                freeze_water(u.ux(), u.uy(), std::int32_t(r.radius), dur_ms);
            }
            break;
        case SkillKind::Redirect:  // [U-185] open the window; incoming lightning goes where the cursor was
            u.st.add(StatusKind::Redirect, now_ms + dur_ms, 0, u.id);
            u.redirect_x = tx;
            u.redirect_y = ty;
            break;
        case SkillKind::Passive:
        case SkillKind::None: break;
    }
}

}  // namespace em
