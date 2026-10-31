// Skill effects moving, hitting [U-132], meeting terrain [U-14..U-16][U-20][U-22][U-48].
#include <algorithm>
#include <format>
#include "game/match/layout.hpp"
#include "game/mechanics/element_rules.hpp"
#include "game/world.hpp"

namespace em {



void World::effect_hits(Effect& e) {
    Unit* owner = find(e.owner);
    const Shape sh = shape_of(e);
    for (std::size_t i = 0; i < units.size(); ++i) {
        Unit& o = units[i];
        if (!o.alive || !targetable(o) || already_hit(e, o.id)) continue;
        // [U-241][U-286] fire or waterbending touching an ally dries them (no damage to allies, as always)
        if (!e.ally && o.team == e.team && o.st.has(StatusKind::Wet) &&
            (fire_family(e.sub) || base_element(e.sub) == Element::Water) && touches(sh, hitbox(o)))
            dry(o);
        // [U-381] an ally may throw lightning INTO an ally who is redirecting: the ally sends it on (ally → ally → enemy)
        const bool ally_redirect = !e.ally && o.team == e.team && o.id != e.owner && e.sub == SubElement::Lightning &&
                                   (e.kind == FxKind::Bolt || e.kind == FxKind::Wave) && o.st.has(StatusKind::Redirect);
        if (!ally_redirect && (e.ally ? o.team != e.team : o.team == e.team)) continue;
        if (o.is_structure() && (e.kind == FxKind::Field || e.kind == FxKind::Tornado)) continue;
        if (!reaches(e, o)) continue;  // [U-191][U-216] too high for this hitbox (flying, airborne, raised ground)
        if (!touches(sh, hitbox(o))) continue;  // ONE rule: any touch = hit
        if (e.inner > 0) {  // [U-462] Ring / Arc: a body completely inside the hole is not hit
            const std::int64_t in = std::int64_t(e.inner) - body_reach(*o.body, size_pct(o));
            if (in > 0 && dist2(o.ux(), o.uy(), e.ux(), e.uy()) < in * in) continue;
        }
        if (e.def && e.def->hits && !(e.def->hits & cat_bit(o.category()))) continue;  // [U-311] not a unit this skill hits
        // [U-185] lightning redirection: the bolt changes owner and flies on towards the redirector's cursor
        if (e.sub == SubElement::Lightning && (e.kind == FxKind::Bolt || e.kind == FxKind::Wave) && o.st.has(StatusKind::Redirect)) {
            o.st.remove(StatusKind::Redirect);
            e.team = o.team;
            e.owner = o.id;
            e.src = o.stats;
            Dir d = dir_of(std::int64_t(o.redirect_x) - e.ux(), std::int64_t(o.redirect_y) - e.uy());
            if (d.x == 0 && d.y == 0) d = e.dir;  // [U-383] cursor right on the bolt: keep flying
            const std::int32_t sp = std::int32_t(isqrt(std::int64_t(e.vx) * e.vx + std::int64_t(e.vy) * e.vy));
            e.dir = d;
            e.vx = d.x * sp / kOne;
            e.vy = d.y * sp / kOne;
            e.travel_left = 1'300 * kSub;
            e.nhit = 0;
            mark_hit(e, o.id);
            add_feed(std::format("{} redirected the lightning!", o.name));
            return;
        }
        mark_hit(e, o.id);
        std::int32_t dmg = e.damage;
        if (e.def && (e.def->pct_current_hp || e.def->pct_max_hp || e.def->pct_missing_hp)) {  // [U-313] % health parts
            const std::int64_t mx = o.stats.health.total(), cur = std::max(0, o.hp);
            dmg += std::int32_t((cur * e.def->pct_current_hp + mx * e.def->pct_max_hp + (mx - cur) * e.def->pct_missing_hp) / 10'000 / 100);
        }
        // [U-222] sweet spot: a better hit in the best part of the hitbox (not a crit [U-221])
        bool sweet = false;
        const std::int32_t sweet_pct = e.sweet >= 0 ? e.sweet : e.def ? e.def->sweet_pct : 0;  // [U-456] per cast stage
        if (e.def && sweet_pct > 0) {  // [U-461] one rule for every shape (see SweetZone)
            const SweetZone z = sweet_zone_for(e.kind, Geo(e.geo), SweetZone(e.sweet_zone));
            const std::int64_t sz = sweet_size_for(z, e.sweet_size);
            const std::int64_t rx = o.ux() - e.ux(), ry = o.uy() - e.uy();
            const bool caster = from_caster(e.kind, Geo(e.geo));
            const std::int64_t along = (rx * e.dir.x + ry * e.dir.y) / kOne;
            const std::int64_t r = Geo(e.geo) == Geo::Rect ? std::abs(along) : isqrt(rx * rx + ry * ry);
            switch (z) {
                case SweetZone::Centre:
                    sweet = caster ? std::abs(along - e.length / 2) <= e.length * sz / 200 : r <= e.radius * sz / 100;
                    break;
                case SweetZone::Edge: sweet = caster ? along >= e.length * (100 - sz) / 100 : r >= e.radius * (100 - sz) / 100; break;
                case SweetZone::Tip: sweet = e.travel_left <= e.travel_total * sz / 100; break;
                case SweetZone::Auto: break;
            }
            if (sweet) dmg = dmg * (100 + sweet_pct) / 100;
        }
        hit_flags_ = sweet ? kHitSweet : 0;
        if (e.combo == Combo::Firestorm) dmg = dmg * (100 + elements::kAirEmpowersFirePct) / 100;  // [U-241]
        if (e.sub == SubElement::Lightning) {  // [U-241] +25 % wet, +25 % iron armour — they add up
            const bool iron = (o.st.has(StatusKind::Shield) && o.shield_sub == std::uint8_t(SubElement::MetalArmor)) ||
                              (o.kind == UnitKind::Champion && champions()[std::size_t(o.champ)].sub == SubElement::Metal);
            dmg = dmg * (100 + (o.st.has(StatusKind::Wet) ? elements::kLightningVsWetPct : 0) + (iron ? elements::kLightningVsIronPct : 0)) / 100;
        }
        dmg = dmg * group_damage_pct(e.feeds) / 100;  // [U-135]
        // [U-254][U-262] tag the hit with the skill that made it (its slot on the owner) or the combo
        dmg_tag_ = {e.combo != Combo::None ? DmgFrom::Combo : DmgFrom::Skill};
        if (owner && e.def)
            for (std::size_t sl = 0; sl < owner->skill.size(); ++sl)
                if (owner->skill[sl] == e.def) dmg_tag_.slot = std::uint8_t(sl);
        deal_damage(owner, o, dmg, e.sub, e.physical, false, &e.src);
        if (e.def && e.def->dispel && o.alive && o.team != e.team) dota_dispel(o, e.def->dispel >= 2, false);  // [U-476] strip buffs
        if (e.def && e.def->tether_ms > 0 && o.alive && owner && o.team != owner->team)  // [U-456] the chain holds on
            tethers.push_back({owner->id, o.id, o.ux(), o.uy(), e.def->tether_radius, e.def->pull, now_ms + e.def->tether_ms, e.def, dmg, e.sub, e.physical});
        if (owner && o.team != owner->team) {  // [U-416] passives that trigger on a skill hit
            const DmgTag keep = dmg_tag_;
            passive_proc(*owner, &o, 2);
            dmg_tag_ = keep;
        }
        hit_flags_ = 0;
        dmg_tag_ = {};
        // keystone Fuel the Fire: skill hits on champions return 10 % of the cost / 0.5 s cooldown [U-177]
        if (owner && e.def && o.kind == UnitKind::Champion && has_keystone(owner->loadout, KeystoneId::FuelTheFire))
            for (std::size_t sl = 0; sl < owner->skill.size(); ++sl)
                if (owner->skill[sl] == e.def) {
                    if (owner->res_max > 0 && e.def->cost > 0) {
                        const std::int32_t back = e.def->cost * 10;  // 10 % of cost, in hundredths
                        owner->res = std::min(owner->res_max, owner->res + back);
                        owner->stat_keystone += back / 100;
                    } else {
                        owner->ab[sl].cd_end_ms -= 500;
                        owner->stat_keystone += 1;
                    }
                }
        if (!o.alive) continue;
        apply_cc(e.src, e.owner, o, e.cc, e.cc_ms, e.cc_val, e.sub);
        // [U-241] water soaks, fire dries
        if (fire_family(e.sub)) dry(o);
        else if (e.sub == SubElement::Water || e.sub == SubElement::Mud || e.sub == SubElement::Blood)
            o.st.add(StatusKind::Wet, now_ms + (o.kind == UnitKind::Champion && champions()[std::size_t(o.champ)].element == Element::Water
                                                  ? elements::kWetWaterbenderMs : elements::kWetMs), 0, e.owner);
        if (e.sub == SubElement::Ice && o.st.has(StatusKind::Swimming))  // [U-190] ice on a swimmer = icing stun
            apply_cc(e.src, e.owner, o, StatusKind::Frozen, 600, 0);
        if (e.combo == Combo::IceShards) apply_cc(e.src, e.owner, o, StatusKind::Slow, 1'500, 40);
        if (e.combo == Combo::Firestorm || (owner && has_passive(*owner, PassiveId::InnerFlame) && e.damage > 0))
            apply_cc(e.src, e.owner, o, StatusKind::Burn, 2'000, 1'500);
        if (e.knock) knock(o, e.ux() - e.dir.x * 60 / kOne, e.uy() - e.dir.y * 60 / kOne, e.knock, e.owner);
        // [X-58] lightning chains through the connected shallow water the target stands in
        if (e.sub == SubElement::Lightning) {
            const int cx = o.ux() / kCellUnits, cy = o.uy() / kCellUnits;
            if (grid.at(cx, cy).surface() == Surface::Water) {
                std::vector<std::uint8_t> mark(std::size_t(kGridSize) * kGridSize, 0);
                std::vector<std::pair<int, int>> cells;
                flood_water(grid, cx, cy, mark, cells);
                for (Unit& w : units)
                    if (w.alive && w.team != e.team && w.id != o.id && !already_hit(e, w.id) && !w.flying() &&
                        mark[std::size_t(w.uy() / kCellUnits) * kGridSize + std::size_t(w.ux() / kCellUnits)]) {
                        mark_hit(e, w.id);
                        dmg_tag_ = {DmgFrom::Chain};
                        deal_damage(owner, w, dmg / 2, e.sub, false, false, &e.src);
                        dmg_tag_ = {};
                    }
            }
        }
        if (!e.pierce && (e.kind == FxKind::Bolt || e.kind == FxKind::Boulder)) {
            // [U-312] does this unit stop it? a kill may let it fly on (per skill and category)
            const std::uint16_t cat = cat_bit(o.category());
            const std::uint16_t stops = e.def && e.def->stop_on ? e.def->stop_on : kAllCategories;
            const bool passes = !(stops & cat) || (!o.alive && e.def && (e.def->pass_on_kill & cat));
            if (passes) continue;
            e.dead = true;
            return;
        }
    }
}

void World::step_effects() {
    fx.reserve(fx.size() + 256);  // scorch() / glow may add effects while we hold references
    const std::size_t n = fx.size();  // effects spawned this tick start next tick
    for (std::size_t i = 0; i < n; ++i) {
        Effect& e = fx[i];
        if (e.dead) continue;
        if (now_ms >= e.end_ms) {
            if (e.kind == FxKind::Burst && e.nhit == 0 && now_ms >= e.arm_ms) effect_hits(e);
            e.dead = true;
            if (e.light > 0 && e.kind != FxKind::Lantern) {  // [U-116] light stays a moment after fire skills
                Effect glow;
                glow.kind = FxKind::Steam;
                glow.team = e.team;
                glow.x = e.x;
                glow.y = e.y;
                glow.radius = 1;
                glow.light = e.light;
                glow.end_ms = now_ms + 600;
                fx_pending_.push_back(glow);  // [U-383] added after the loop (pushing into fx here moved `e`)
            }
            continue;
        }
        // homing autoattacks / tower shots
        if (e.kind == FxKind::Auto || e.kind == FxKind::TowerShot) {
            Unit* t = find(e.target);
            if (!t || !t->alive) {
                e.dead = true;
                continue;
            }
            const std::int64_t dx = t->x - e.x, dy = t->y - e.y;
            const std::int64_t d = isqrt(dx * dx + dy * dy);
            const std::int64_t sp = std::int64_t(e.length) * kSub / cfg.tick_hz;
            if (d <= sp || touches(shape_of(e), hitbox(*t))) {
                Unit* owner = find(e.owner);
                if (owner && dota_evaded(*owner, *t)) {  // [U-476] evasion
                    e.dead = true;
                    continue;
                }
                hit_flags_ = e.crit ? kHitCrit : 0;  // [U-463]
                deal_damage(owner, *t, e.damage, e.sub, e.physical, true, &e.src);
                hit_flags_ = 0;
                if (owner && t->alive) auto_extras(*owner, *t, e.damage, e.sub, e.physical);  // [U-217]
                if (owner && owner->kind == UnitKind::Champion) {
                    if (has_passive(*owner, PassiveId::Frostbite)) apply_cc(e.src, e.owner, *t, StatusKind::Slow, 1'000, 15);
                    if (has_passive(*owner, PassiveId::Static)) {  // [X-58] autos chain on water
                        e.sub = SubElement::Lightning;
                        e.damage = e.damage / 2;
                        e.cc = StatusKind::Count;
                        mark_hit(e, t->id);
                        const int cx = t->ux() / kCellUnits, cy = t->uy() / kCellUnits;
                        if (grid.at(cx, cy).surface() == Surface::Water) {
                            std::vector<std::uint8_t> mark(std::size_t(kGridSize) * kGridSize, 0);
                            std::vector<std::pair<int, int>> cells;
                            flood_water(grid, cx, cy, mark, cells);
                            for (Unit& w : units)
                                if (w.alive && w.team != e.team && w.id != t->id && !w.flying() &&
                                    mark[std::size_t(w.uy() / kCellUnits) * kGridSize + std::size_t(w.ux() / kCellUnits)])
                                    deal_damage(owner, w, e.damage, SubElement::Lightning, false, false, &e.src);
                        }
                    }
                }
                e.dead = true;
                continue;
            }
            e.x += std::int32_t(dx * sp / d);
            e.y += std::int32_t(dy * sp / d);
            continue;
        }
        if (e.kind == FxKind::Steam || e.kind == FxKind::Cone) continue;  // visual / already resolved
        if (e.kind == FxKind::Heal) continue;
        if (e.kind == FxKind::Burst) {
            if (now_ms >= e.arm_ms && e.nhit == 0 && e.next_tick_ms == 0) {
                e.next_tick_ms = now_ms;
                effect_hits(e);
                if (e.sub == SubElement::Steam) steam_cloud(e.ux(), e.uy(), e.radius, 3'000);  // [X-57]
                if (e.sub == SubElement::Ice) freeze_water(e.ux(), e.uy(), e.radius, 6'000);     // [U-188]
                scorch(e);                                                                       // [U-227][U-244]
                if (e.sub == SubElement::Lava) lava_pool(e.ux(), e.uy(), e.radius * 2 / 3, 6'000);
            }
            continue;
        }
        if (e.kind == FxKind::Field) {
            if (now_ms >= e.next_tick_ms) {
                e.next_tick_ms = now_ms + e.tick_ms;
                e.nhit = 0;  // every tick hits again
                effect_hits(e);
            }
            continue;
        }
        // moving things: Bolt (incl. dash), Wave, Tornado, Lantern, launched Boulder
        const bool resting_boulder = e.kind == FxKind::Boulder && e.vx == 0 && e.vy == 0;
        if (e.kind == FxKind::Lantern && e.travel_left <= 0) continue;  // hangs in the air
        if (resting_boulder) continue;
        if (e.kind == FxKind::Tornado && now_ms >= e.next_tick_ms) {
            e.next_tick_ms = now_ms + 500;
            e.nhit = 0;
        }
        const std::int32_t step = std::int32_t(isqrt(std::int64_t(e.vx) * e.vx + std::int64_t(e.vy) * e.vy));
        // [U-402] steered skills turn towards their caster's cursor while the cast key is held
        if (e.def && e.def->control == 1 && step > 0 && e.travel_left > 0)
            if (const Unit* so = find(e.owner); so && so->steer_until > now_ms && so->steer_slot < so->skill.size() && so->skill[so->steer_slot] == e.def) {
                const Dir want = dir_of(std::int64_t(so->steer_x) - e.ux(), std::int64_t(so->steer_y) - e.uy());
                const std::int64_t dot = (std::int64_t(e.dir.x) * want.x + std::int64_t(e.dir.y) * want.y) / kOne;
                if (dot < kOne - e.def->steer_turn / 8) {
                    const std::int64_t cross = std::int64_t(e.dir.x) * want.y - std::int64_t(e.dir.y) * want.x;
                    const std::int64_t s = cross >= 0 ? e.def->steer_turn : -e.def->steer_turn, c = kOne - s * s / (2 * kOne);
                    e.dir = dir_of((e.dir.x * c - e.dir.y * s) / kOne, (e.dir.x * s + e.dir.y * c) / kOne);
                } else {
                    e.dir = want;
                }
                e.vx = e.dir.x * step / kOne;
                e.vy = e.dir.y * step / kOne;
            }
        // [U-388] metal attracts lightning: the bolt bends towards the nearest metal wall cell within kMetalPull
        if (e.sub == SubElement::Lightning && (e.kind == FxKind::Bolt || e.kind == FxKind::Wave) && step > 0 && e.travel_left > 0) {
            const int r = kMetalPull / kCellUnits, ccx = e.ux() / kCellUnits, ccy = e.uy() / kCellUnits;
            std::int64_t best = -1;
            int bx = 0, by = 0;
            for (int y = ccy - r; y <= ccy + r; ++y)
                for (int x = ccx - r; x <= ccx + r; ++x) {
                    if (!Grid::in_bounds(x, y) || grid.at(x, y).wall() != Wall::Metal) continue;
                    const std::int64_t d2 = dist2(x * kCellUnits + 16, y * kCellUnits + 16, e.ux(), e.uy());
                    if (d2 <= std::int64_t(kMetalPull) * kMetalPull && (best < 0 || d2 < best)) best = d2, bx = x, by = y;
                }
            if (best >= 0) {
                const Dir want = dir_of(std::int64_t(bx * kCellUnits + 16) - e.ux(), std::int64_t(by * kCellUnits + 16) - e.uy());
                const std::int64_t cross = std::int64_t(e.dir.x) * want.y - std::int64_t(e.dir.y) * want.x;
                const std::int64_t dot = (std::int64_t(e.dir.x) * want.x + std::int64_t(e.dir.y) * want.y) / kOne;
                Dir nd = want;
                if (dot < kOne - kMetalTurnPerTick / 8) {  // turn a little each tick (a curve, not a snap)
                    const std::int64_t s = cross >= 0 ? kMetalTurnPerTick : -kMetalTurnPerTick, c = kOne - s * s / (2 * kOne);
                    nd = dir_of((e.dir.x * c - e.dir.y * s) / kOne, (e.dir.x * s + e.dir.y * c) / kOne);
                }
                e.dir = nd;
                e.vx = nd.x * step / kOne;
                e.vy = nd.y * step / kOne;
            }
        }
        if (e.travel_left > 0) {
            e.x += e.vx;
            e.y += e.vy;
            e.travel_left -= step;
        } else if (e.kind != FxKind::Tornado) {
            e.dead = true;
            // [U-359] a thrown rock (earth / lava bolt) stays where it lands
            if (e.kind == FxKind::Bolt && e.def && e.def->kind == SkillKind::Bolt && (e.sub == SubElement::Earth || e.sub == SubElement::Lava))
                drop_rock(e.ux(), e.uy(), e.sub == SubElement::Lava);
        }
        // dash: drag the owner
        Unit* owner = find(e.owner);
        if (e.def && e.def->kind == SkillKind::Dash) {
            if (owner && e.def->pass_units) owner->ghost_until = now_ms + 100;  // [U-471]
            if (!owner || !owner->alive) {
                e.dead = true;
                continue;
            }
            if (!walkable_at(*owner, e.x, e.y)) {
                // [U-471] through_walls: the dash goes on over the wall, the dasher lands on the first free spot behind it
                if (!e.def->through_walls || e.travel_left <= 0) e.dead = true;
                if (e.dead) continue;
            } else {
                owner->x = e.x;
                owner->y = e.y;
            }
            owner->moving = true;
            owner->walk_phase += 24;
            if (e.travel_left <= 0) e.dead = true;
        }
        // terrain [U-14][U-15][U-16][U-20][U-22][U-48]
        const int cx = e.ux() / kCellUnits, cy = e.uy() / kCellUnits;
        if (!Grid::in_bounds(cx, cy)) {
            e.dead = true;
            continue;
        }
        // [U-184] skills marked through_walls ignore walls, objects and trees
        // [U-346] dashes stop at walls too — only air dashes (over the air) and skills marked through_walls pass
        const bool air_dash = e.def && e.def->kind == SkillKind::Dash && base_element(e.sub) == Element::Air;
        if (e.kind != FxKind::Lantern && !(e.def && (air_dash || e.def->through_walls))) {
            const Cell c = grid.at(cx, cy);
            const AttackInfo ai{.sub = e.sub, .elevated = owner && owner->flying()};
            // [U-227] trees: burn (fire / lava) or break (skills marked destroys_trees)
            const bool destroys = e.def && e.def->destroys_trees >= 0 ? e.def->destroys_trees > 0 : fire_family(e.sub) || e.sub == SubElement::Lava;
            if (destroys && (c.tree() == Tree::Alive || c.bush() == Bush::Alive)) {
                if (fire_family(e.sub) || e.sub == SubElement::Lava) burn_cell(cx, cy);
                else scorch(e);
            }
            // [U-241] water on lava: it cools at once and turns into a steam cloud
            if (base_element(e.sub) == Element::Water && e.sub != SubElement::Ice && c.surface() == Surface::Magma) {
                cool_lava(e.ux(), e.uy(), std::max(64, e.radius));
                add_cloud(CloudKind::Steam, e.ux(), e.uy(), std::max(64, e.radius), elements::kSteamMs);
            }
            // [U-225] air that bends the environment blows steam / smoke / sand / snow clouds away
            if (e.def && e.def->env_bend && base_element(e.sub) == Element::Air)
                std::erase_if(clouds, [&](const Cloud& cl) { return touches(shape_of(e), Circle{cl.x, cl.y, cl.radius}); });
            // [U-293] ice (freezing) passing through steam: the steam around it falls as water (or ice, by the bender's choice)
            if (e.sub == SubElement::Ice && e.kind != FxKind::Steam)
                condense_steam(e.ux(), e.uy(), std::max(48, e.radius), owner, owner && owner->source_ice);
            if (c.wall() != Wall::None && !c.temp_walkable()) {
                const WallHit h = wall_hit(c.wall() == Wall::Ice ? WallKind::Ice : WallKind::Stone, ai);
                // [U-288] a skill that does not pass over / through damages the wall cell (HP; fire melts ice fast)
                if (h == WallHit::Melts || h == WallHit::Stopped) hit_wall(e, owner, cx, cy);
                if (c.wall() == Wall::Metal && e.sub == SubElement::Lightning) {  // [U-388] the strike hits around the metal
                    e.x = (cx * kCellUnits + 16) * kSub;
                    e.y = (cy * kCellUnits + 16) * kSub;
                    e.radius = kCellUnits / 2 + kMetalSplash;
                    effect_hits(e);
                    e.dead = true;
                    continue;
                }
                if (h == WallHit::Melts) {  // [U-15][U-16] fire / water on ice: the skill is spent on the ice
                    e.dead = true;
                    continue;
                }
                if (h == WallHit::Stopped) {
                    // [U-20][X-35] earth stops water only partly: 50 % goes through
                    if (base_element(e.sub) == Element::Water && c.wall() == Wall::Stone && e.combo != Combo::Mud) {
                        e.combo = Combo::Mud;
                        e.damage = e.damage * kPartialBlockPassPct / 100;
                    } else if (base_element(e.sub) != Element::Water || c.wall() != Wall::Stone) {
                        e.dead = true;
                        continue;
                    }
                }
            }
            if (c.tree() == Tree::Alive && !fire_family(e.sub) && e.kind != FxKind::Tornado && !destroys) {  // trees stop skills
                e.dead = true;
                continue;
            }
        }
        if (e.kind == FxKind::Lantern) continue;
        effect_hits(e);
        if (e.dead) continue;

        effect_interactions(e, i);  // skill vs skill: see mechanics/interactions.cpp
    }
}

}  // namespace em
