// Damage [X-15][U-40][U-136], CC [U-127], knock-back, autoattacks edge to edge [U-41][U-55][U-56][U-133], kills + rewards [X-11][X-12].
#include <algorithm>
#include <format>
#include "game/match/layout.hpp"
#include "game/mechanics/element_rules.hpp"
#include "game/mobs/monsters.hpp"
#include "game/mobs/turrets.hpp"
#include "game/world.hpp"

namespace em {

void World::heal(Unit& u, std::int32_t hundredths) {
    if (!u.alive) return;
    u.hp = std::min(u.stats.health.total(), u.hp + hundredths);
}

void World::apply_cc(const Stats& src_stats, std::uint32_t src_id, Unit& dst, StatusKind k, std::int32_t ms,
                     std::int32_t value, SubElement sub) {
    if (k == StatusKind::Count || ms <= 0 || !dst.alive) return;
    if (dst.is_structure()) return;
    // [U-344] a wet unit does not catch fire (the hit that dried it counts as wet too)
    if (k == StatusKind::Burn && (dst.st.has(StatusKind::Wet) || dst.dried_ms == now_ms)) return;
    if (dst.kind == UnitKind::Boss && is_cc(k) && k != StatusKind::Slow) return;
    // [U-218] slow resistance: ice slows and other slows are resisted separately (cap 80 %)
    if (k == StatusKind::Slow) {
        const std::int32_t res = sub == SubElement::Ice ? dst.stats.slow_resist_ice : dst.stats.slow_resist;
        value = value * (10'000 - std::clamp(res, 0, kMaxSlowResist)) / 10'000;
    }
    // [U-190] swimming: freezing / slowing CC lasts 50 % longer
    if (dst.st.has(StatusKind::Swimming) && (k == StatusKind::Frozen || k == StatusKind::Slow)) ms = ms * 3 / 2;
    dst.st.apply(k, now_ms, ms, value, src_id, src_stats, dst.stats);  // [U-127]
    if (dst.kind == UnitKind::Champion && is_cc(k)) {  // [U-343] the death recap lists crowd control too
        const Unit* src = find(src_id);
        TakenHit h{now_ms, src_id, dmg_tag_, 0, 0, std::uint8_t(sub), false};
        h.src_kind = std::uint8_t(src ? int(src->kind) : 255);
        h.src_extra = std::uint8_t(!src ? 0 : src->kind == UnitKind::Minion ? int(src->mtype) : src->kind == UnitKind::Champion ? src->champ : mob_name_id(*src));
        h.cc = std::uint8_t(k);
        h.cc_ms = ms;
        if (dst.taken.size() < 400) dst.taken.push_back(h);
    }
    interrupt(dst, interrupt_of(k));  // [U-337] the cast / channel decides
}

// [U-337] one place that breaks casts and channels — by their own rules (SkillDef::interrupt / channel_interrupt)
void World::interrupt(Unit& u, std::uint32_t event) {
    if (event == 0) return;
    if (u.casting) {
        const SkillDef* d = u.cast.slot < u.skill.size() ? u.skill[u.cast.slot] : nullptr;
        std::uint32_t mask = d && d->interrupt ? d->interrupt : kIntDefaultCast;
        if (d && d->sub == SubElement::Physical) mask &= ~std::uint32_t(kIntMute);  // mute stops bending only
        if (mask & event) {
            u.casting = false;
            u.windup_end_ms = 0;
        }
    }
    if (u.channel_end_ms && (u.channel_interrupt & event)) {
        u.channel_end_ms = 0;
        u.st.remove(StatusKind::Channel);
        u.order = {};
        std::erase_if(clouds, [&](const Cloud& c) { return c.kind == CloudKind::Arrival && c.owner == u.id; });  // nobody comes
    }
}

void World::knock(Unit& dst, std::int32_t from_x, std::int32_t from_y, std::int32_t dist, std::uint32_t src_id) {
    if (dist <= 0 || dst.is_structure() || dst.kind == UnitKind::Boss) return;
    interrupt(dst, kIntKnock);  // [U-337] displacement
    const Dir d = dir_of(std::int64_t(dst.ux()) - from_x, std::int64_t(dst.uy()) - from_y);
    std::int32_t moved = 0;
    for (; moved < dist; moved += 8) {
        const std::int32_t nx = dst.x + d.x * 8 / kOne * kSub, ny = dst.y + d.y * 8 / kOne * kSub;
        if (!walkable_at(dst, nx, ny)) break;
        dst.x = nx;
        dst.y = ny;
    }
    // [U-368] a displacement is crowd control too: it holds the unit (distance / knock speed); tenacity does not shorten it
    const std::int32_t ms = std::max<std::int32_t>(100, std::max(moved, 8) * 1000 / kKnockSpeed);
    dst.st.add(StatusKind::Stun, now_ms + ms, 0, src_id);
    if (dst.kind == UnitKind::Champion) {
        const Unit* src = find(src_id);
        TakenHit h{now_ms, src_id, dmg_tag_, 0, 0, 0, false};
        h.src_kind = std::uint8_t(src ? int(src->kind) : 255);
        h.src_extra = std::uint8_t(!src ? 0 : src->kind == UnitKind::Minion ? int(src->mtype) : src->kind == UnitKind::Champion ? src->champ : mob_name_id(*src));
        h.cc = kCcDisplaced;
        h.cc_ms = ms;
        if (dst.taken.size() < 400) dst.taken.push_back(h);
    }
}

void World::deal_damage(Unit* src, Unit& dst, std::int32_t points, SubElement sub, bool physical, bool is_auto,
                        const Stats* src_stats) {
    if (!dst.alive || dst.untargetable_until > now_ms || points <= 0) return;
    if (dst.is_structure() && !vulnerable(dst)) return;
    const Stats& as = src_stats ? *src_stats : (src ? src->stats : dst.stats);
    std::int64_t dmg = points;
    if (physical) {  // [X-15] same formula with defence; no physical lethality [U-40]
        std::int64_t def = dst.stats.defence.total();
        if (dst.stats.range == Range::Melee && dst.kind == UnitKind::Champion) def = def * (100 + kMeleeDefenceBonusPct) / 100;
        dmg = dmg * 10'000 / (10'000 + std::max<std::int64_t>(0, def));
    } else {
        dmg = elemental_damage(std::int32_t(dmg), as, dst.stats);
        const std::int64_t before = dmg;
        dmg = after_element_resist(std::int32_t(dmg), base_element(sub), dst.stats);  // [U-136]
        // [U-209] credit the prevented damage to the resist items, split by their resist value
        if (before > dmg && dst.kind == UnitKind::Champion) {
            const Element el = base_element(sub);
            auto resist_of = [&](const ItemDef& d) {
                return el == Element::Fire ? d.resist_fire : el == Element::Water ? d.resist_water
                     : el == Element::Air  ? d.resist_air  : el == Element::Earth ? d.resist_earth : 0;
            };
            std::int32_t total = 0;
            for (std::int16_t it : dst.items) total += it >= 0 ? resist_of(kItems[std::size_t(it)]) : 0;
            if (total > 0)
                for (std::size_t sl = 0; sl < dst.items.size(); ++sl)
                    if (dst.items[sl] >= 0)
                        dst.item_impact[sl] += (before - dmg) * resist_of(kItems[std::size_t(dst.items[sl])]) / total;
        }
    }
    apply_hit(src, dst, std::int32_t(dmg * 100), sub, physical, is_auto);
}

void World::apply_hit(Unit* src, Unit& dst, std::int32_t hund, SubElement sub, bool physical, bool is_auto) {
    if (!dst.alive || hund <= 0) return;
    // [U-286] wet + fire: the first fire hit (skill, autoattack or burn) loses 25 %, and the water is gone.
    // Lightning is not part of this: water conducts it (+25 %, effects.cpp).
    if (fire_family(sub) && sub != SubElement::Lightning && dst.st.has(StatusKind::Wet)) {
        hund = hund * (100 - elements::kWetVsFirePct) / 100;
        dry(dst);
        dst.dried_ms = now_ms;  // [U-344] this same hit cannot set it on fire
    }
    const std::int32_t before_shield = hund;
    hund = dst.st.soak(hund);
    // [U-254][U-262] what dealt it: set by the caller, else guessed from the source
    DmgTag tag = dmg_tag_;
    if (tag.from == DmgFrom::Other && src) {
        tag.from = is_auto ? DmgFrom::Auto
                 : src->kind == UnitKind::Turret ? DmgFrom::Turret
                 : src->kind == UnitKind::Minion ? DmgFrom::Minion
                 : (src->kind == UnitKind::Monster || src->kind == UnitKind::Boss) ? DmgFrom::Monster : DmgFrom::Other;
        if (tag.from == DmgFrom::Other && is_auto) tag.from = DmgFrom::Auto;
    }
    if (dst.kind == UnitKind::Champion) {
        std::erase_if(dst.taken, [&](const TakenHit& h) { return now_ms - h.ms > 30'000; });
        if (dst.taken.size() < 400)
            dst.taken.push_back({now_ms, src ? src->id : 0, tag, hund / 100, (before_shield - hund) / 100, std::uint8_t(sub), physical,
                                 std::uint8_t(src ? int(src->kind) : 255),
                                 std::uint8_t(!src ? 0 : src->kind == UnitKind::Minion ? int(src->mtype)
                                                     : src->kind == UnitKind::Champion ? src->champ : mob_name_id(*src))});
    }
    if (src) {
        dst.last_hitter = src->id;
        if (src->kind == UnitKind::Champion) {
            auto& slot = *std::ranges::min_element(dst.assist_by, {}, [](const auto& p) { return p.second; });
            bool found = false;
            for (auto& p : dst.assist_by)
                if (p.first == src->id) {
                    p.second = now_ms;
                    found = true;
                }
            if (!found) slot = {src->id, now_ms};
        }
        // [X-90] "call for help": hitting an enemy champion makes nearby minions / turrets target you
        if (src->kind == UnitKind::Champion && dst.kind == UnitKind::Champion)
            for (Unit& o : units)
                if (o.alive && o.team == dst.team && (o.kind == UnitKind::Minion || o.kind == UnitKind::Turret) &&
                    dist2(o.ux(), o.uy(), src->ux(), src->uy()) < std::int64_t(turrets::kCallForHelpRange) * turrets::kCallForHelpRange) {
                    o.aggro = src->id;
                    o.aggro_until = now_ms + turrets::kCallForHelpMs;  // [X-108][U-309]
                }
        if (dst.kind == UnitKind::Monster || dst.kind == UnitKind::Boss) dst.aggro = src->id;
        if (src->kind == UnitKind::Champion && has_passive(*src, PassiveId::ChiFlow)) src->res = std::min(src->res_max, src->res + 2'000);
    }
    // [U-337] damage: to HP / only into a shield (LoL: a shield keeps the channel going)
    interrupt(dst, hund > 0 ? std::uint32_t(kIntDamageHp) : before_shield > 0 ? std::uint32_t(kIntDamageShield) : 0u);
    dst.hp -= hund;
    dst.last_hurt_ms = now_ms;
    // [U-268] rage fills by fighting
    if (dst.stats.resource == ResourceKind::Rage) dst.res = std::min(dst.res_max, dst.res + hund * kRageTakenPct / 100);
    if (src && src->stats.resource == ResourceKind::Rage && src != &dst) src->res = std::min(src->res_max, src->res + hund * kRageDealtPct / 100);
    // ---- statistics [U-179]
    if (dst.kind == UnitKind::Champion) dst.stat_dmg_taken += hund / 100;
    if (src && src->kind == UnitKind::Champion && dst.kind == UnitKind::Champion) {
        src->stat_dmg_champs += hund / 100;
        dst.stat_dmg_from_champs += hund / 100;  // [U-296]
    }
    // ---- keystone: Elemental Surge — 3 skill hits on the same champion within 3 s [U-177]
    if (src && !is_auto && src->kind == UnitKind::Champion && dst.kind == UnitKind::Champion && dst.alive &&
        has_keystone(src->loadout, KeystoneId::ElementalSurge)) {
        if (src->surge_target == dst.id && now_ms - src->surge_ms <= 3'000) ++src->surge_count;
        else src->surge_count = 1;
        src->surge_target = dst.id;
        src->surge_ms = now_ms;
        if (src->surge_count >= 3) {
            src->surge_count = 0;
            const std::int32_t extra = 40 + src->stats.elemental_power.total() * 15 / 10'000;
            src->stat_keystone += extra;
            const DmgTag saved = dmg_tag_;
            dmg_tag_ = {DmgFrom::Keystone};
            deal_damage(src, dst, extra, sub, false, /*is_auto=*/true);  // flagged as auto → no re-trigger
            dmg_tag_ = saved;
            if (!dst.alive) return;
        }
    }
    // ---- keystone: Stoneheart — shield when falling under 35 % HP
    if (dst.kind == UnitKind::Champion && dst.hp > 0 && has_keystone(dst.loadout, KeystoneId::Stoneheart) &&
        now_ms >= dst.stoneheart_ready_ms && dst.hp * 100 < dst.stats.health.total() * 35) {
        const std::int32_t shield = (80 + 10 * dst.level) * 100;
        dst.st.add(StatusKind::Shield, now_ms + 3'000, shield, dst.id);
        dst.shield_sub = std::uint8_t(SubElement::EarthArmor);
        dst.stoneheart_ready_ms = now_ms + 60'000;
        dst.stat_keystone += shield / 100;
        dst.stat_shielding += shield / 100;
    }
    if (src) {  // [U-194] "who hit whom" for minion calls for help
        src->last_victim = dst.id;
        src->last_victim_ms = now_ms;
    }
    if (hund > 0)  // (a hit fully eaten by a shield shows no number)
        hits.push_back({src ? src->id : 0, dst.id, std::max(1, hund / 100), now_ms, std::uint8_t(is_auto), std::uint8_t(sub),
                    std::uint8_t(hit_flags_ | (physical ? kHitPhysical : 0) | (tag.from == DmgFrom::Burn ? kHitDot : 0))});
    if (dst.hp > 0) return;
    // [U-81] Spiritual Water Flask: immortal while drinking → 3 s "dead", then 5 × 5 % heals
    if (dst.flask_until > now_ms) {
        dst.flask_until = 0;
        dst.hp = 1;
        dst.st.add(StatusKind::Stun, now_ms + 3'000, 0, dst.id);
        dst.untargetable_until = now_ms + 3'000;
        dst.flask_revive = 5;
        dst.flask_next_ms = now_ms + 3'000;
        add_feed(std::format("{} cheats death (Spirit Flask)", dst.name));
        return;
    }
    on_kill(src, dst);
}

void World::update_attack(Unit& u) {
    Unit* t = find(u.order.target);
    if (!t || !t->alive || !targetable(*t) || (u.team != Team::Neutral && !visible_to(u.team, *t)) ||
        (t->is_structure() && !vulnerable(*t))) {
        u.order = {};
        u.windup_end_ms = 0;
        return;
    }
    const bool elemental = u.kind == UnitKind::Champion && champions()[std::size_t(u.champ)].element != Element::None;
    // [U-133] edge of my hitbox to the closest edge of the target
    if (u.auto_acquired && edge_gap(u, *t) > u.attack_range) {  // auto-acquired targets are not chased [X-99]
        u.order = {};
        u.auto_acquired = false;
        u.windup_end_ms = 0;
        return;
    }
    if (edge_gap(u, *t) > u.attack_range) {
        u.windup_end_ms = 0;
        if (u.kind == UnitKind::Champion && now_ms - u.repath_ms > 300) path_to(u, t->ux(), t->uy());
        if (u.kind == UnitKind::Champion && u.path_i < u.path.size())
            follow_path(u);
        else
            move_towards(u, t->x, t->y, move_speed_sub(u));
        return;
    }
    const Dir want = dir_of(std::int64_t(t->x) - u.x, std::int64_t(t->y) - u.y);
    turn_towards(u, want);
    if (!u.st.can_attack(elemental)) return;
    // [U-191] melee autoattacks can't reach flying / water-raised units
    const bool melee_auto = u.attack_range < 200 && !(u.kind == UnitKind::Champion && champions()[std::size_t(u.champ)].range == Range::Ranged);
    if (melee_auto && unit_height(*t) > ground_height(u.ux(), u.uy()) + 40) {  // [U-216] melee reach 40
        if (u.kind == UnitKind::Champion) refuse(u, "target is in the air / on water: melee can't reach");
        u.order = {};
        u.windup_end_ms = 0;
        return;
    }
    if (u.st.has(StatusKind::Blind)) {  // [U-160] blinded: no autoattacks
        u.windup_end_ms = 0;
        return;
    }
    if (elemental) {  // [U-125] eclipse mutes fire autoattacks too
        const Element el = champions()[std::size_t(u.champ)].element;
        if (sky_boost(el, sky).muted) return;
    }
    const std::int32_t period = attack_period_ms(u);
    if (u.windup_end_ms == 0) {
        if (now_ms < u.attack_ready_ms) return;
        u.windup_end_ms = now_ms + period * 30 / 100;
        u.windup_target = t->id;
        return;
    }
    if (now_ms < u.windup_end_ms) return;
    u.windup_end_ms = 0;
    u.attack_ready_ms = now_ms + period * 70 / 100;
    fire_auto(u, *t);
}

// [U-393] autoattacks on a wall cell (champions whose AutoAttackDef has hits_walls): walk in range, hit it at the
// autoattack speed with wall_pct % of the autoattack, by the element rules of walls (wall_effect_pct).
void World::update_attack_wall(Unit& u) {
    const int cx = u.order.x / kCellUnits, cy = u.order.y / kCellUnits;
    if (u.kind != UnitKind::Champion || !Grid::in_bounds(cx, cy) || grid.at(cx, cy).wall() == Wall::None) {
        u.order = {};
        u.windup_end_ms = 0;
        return;
    }
    const AutoAttackDef& a = champions()[std::size_t(u.champ)].autos;
    const std::int32_t wx = cx * kCellUnits + kCellUnits / 2, wy = cy * kCellUnits + kCellUnits / 2;
    const std::int64_t gap = isqrt(dist2(u.ux(), u.uy(), wx, wy)) - body_reach(*u.body, size_pct(u)) - kCellUnits / 2;
    if (gap > u.attack_range) {
        u.windup_end_ms = 0;
        if (now_ms - u.repath_ms > 300) path_to(u, wx, wy);
        follow_path(u);
        return;
    }
    turn_towards(u, dir_of(std::int64_t(wx) - u.ux(), std::int64_t(wy) - u.uy()));
    const bool elemental = champions()[std::size_t(u.champ)].element != Element::None;
    if (!u.st.can_attack(elemental) || u.st.has(StatusKind::Blind)) return;
    const std::int32_t period = attack_period_ms(u);
    if (u.windup_end_ms == 0) {
        if (now_ms < u.attack_ready_ms) return;
        u.windup_end_ms = now_ms + period * 30 / 100;
        return;
    }
    if (now_ms < u.windup_end_ms) return;
    u.windup_end_ms = 0;
    u.attack_ready_ms = now_ms + period * 70 / 100;
    const SubElement sub = elemental ? champions()[std::size_t(u.champ)].sub : SubElement::Physical;
    const std::int64_t dmg = std::int64_t(u.stats.attack.total()) / 100 * a.wall_pct / 100 * wall_effect_pct(grid.at(cx, cy).wall(), sub) / 100;
    damage_wall(cx, cy, std::int32_t(std::max<std::int64_t>(1, dmg)), dir_of(std::int64_t(wx) - u.ux(), std::int64_t(wy) - u.uy()));
}

// One autoattack leaves now: damage, then melee hit at once or a projectile [U-41][U-55][U-56][U-186].
void World::fire_auto(Unit& u, Unit& target) {
    Unit* t = &target;
    const bool elemental = u.kind == UnitKind::Champion && champions()[std::size_t(u.champ)].element != Element::None;
    // damage: physical attack, or elemental auto [U-56]
    std::int32_t dmg = (elemental ? elemental_auto_damage(u.stats) : u.stats.attack.total()) / 100;
    if (u.kind == UnitKind::Minion && t->kind == UnitKind::Champion) dmg = dmg * rules::kMinion.damage_to_champions_pct / 100;  // [U-156]
    if (u.kind == UnitKind::Turret) {  // [U-309] turret rules (game/mobs/turrets.hpp)
        if (t->kind == UnitKind::Minion) {
            dmg = t->stats.health.total() / 100 * turrets::kMinionHitPct[std::min<std::size_t>(std::size_t(t->mtype), 3)] / 100;
        } else if (t->kind == UnitKind::Champion) {
            u.turret_streak = u.turret_last == t->id ? u.turret_streak + 1 : 0;
            u.turret_last = t->id;
            dmg = dmg * (100 + std::min(turrets::kChampionRampMaxPct, turrets::kChampionRampPct * u.turret_streak)) / 100;
        }
    }
    if (u.chirush_until > now_ms) {  // keystone Chi Rush: first auto after a skill +30 % [U-177]
        u.chirush_until = 0;
        u.stat_keystone += dmg * 30 / 100;
        dmg = dmg * 130 / 100;
    }
    SubElement sub = SubElement::Physical;
    if (elemental) {
        sub = champions()[std::size_t(u.champ)].sub;
        dmg = dmg * sky_boost(base_element(sub), sky).power_pm() / 1000;  // [U-370]
    } else if (u.magic_until > now_ms) {  // [U-264] a non-bender in a magic stance: the auto becomes magic
        sub = SubElement(u.magic_sub);
        dmg = dmg * sky_boost(base_element(sub), sky).power_pm() / 1000;  // [U-370]
    }
    const bool ranged = u.stats.range == Range::Ranged || u.kind == UnitKind::Turret || u.kind == UnitKind::Nexus ||
                        (u.kind == UnitKind::Minion && u.mtype != MinionType::Melee && u.mtype != MinionType::Super);
    const bool projectile = ranged && (u.kind != UnitKind::Champion || champions()[std::size_t(u.champ)].auto_projectile);
    if (!projectile) {  // melee, or a ranged champion whose autos hit at once [U-186]
        deal_damage(&u, *t, dmg, sub, sub == SubElement::Physical, true);
        auto_extras(u, *t, dmg, sub, sub == SubElement::Physical);
        if (u.kind == UnitKind::Champion && has_passive(u, PassiveId::Frostbite))
            apply_cc(u.stats, u.id, *t, StatusKind::Slow, 1'000, 15);
        return;
    }
    Effect e;
    e.kind = u.kind == UnitKind::Turret || u.kind == UnitKind::Nexus ? FxKind::TowerShot : FxKind::Auto;
    e.team = u.team;
    e.owner = u.id;
    e.target = t->id;
    e.sub = sub;
    e.flow = sub != SubElement::Physical ? flow_of(sub) : Flow::Plain;
    e.x = u.x;
    e.y = u.y;
    e.radius = e.kind == FxKind::TowerShot ? 26 : 14;
    e.damage = dmg;
    e.physical = sub == SubElement::Physical;
    e.end_ms = now_ms + 3'000;
    e.src = u.stats;
    e.length = e.kind == FxKind::TowerShot ? 1'400 : 1'800;  // speed units/s
    if (fire_family(sub)) e.light = 96;
    spawn_fx(e);
}

// [U-217] the special shapes of an autoattack, after the main hit: splash around the target, a line behind it,
// or chain jumps. Champions use their AutoAttackDef; plain single hits do nothing here.
void World::auto_extras(Unit& u, Unit& primary, std::int32_t dmg, SubElement sub, bool physical) {
    if (u.kind != UnitKind::Champion) return;
    if (u.jbuff == std::uint8_t(JungleBuff::Ember) && u.jbuff_until > now_ms && primary.alive) {  // [U-366]
        apply_cc(u.stats, u.id, primary, StatusKind::Burn, 2'000, primary.stats.health.total() / 200);  // per second: 1 % over 2 s
        apply_cc(u.stats, u.id, primary, StatusKind::Slow, 1'000, kEmberSlowPct);
    }
    const AutoAttackDef& a = champions()[std::size_t(u.champ)].autos;
    switch (a.shape) {
        case AutoShape::Single: return;
        case AutoShape::Splash: {
            const Circle area{primary.ux(), primary.uy(), a.splash_radius};
            for (Unit& o : units)
                if (&o != &primary && o.alive && o.team != u.team && targetable(o) && touches(hitbox(o), area))
                    deal_damage(&u, o, dmg * a.splash_pct / 100, sub, physical, true);
            return;
        }
        case AutoShape::Line: {
            const Dir d = dir_of(std::int64_t(primary.x) - u.x, std::int64_t(primary.y) - u.y);
            const Shape line = fluid_line(primary.ux(), primary.uy(), d, a.line_length, a.line_width, Flow::Plain, 0, 0);
            for (Unit& o : units)
                if (&o != &primary && o.alive && o.team != u.team && targetable(o) && touches(line, hitbox(o)))
                    deal_damage(&u, o, dmg, sub, physical, true);
            return;
        }
        case AutoShape::Chain: {
            std::vector<std::uint32_t> done{primary.id};
            Unit* from = &primary;
            std::int32_t d = dmg;
            for (int k = 0; k < a.chain_count; ++k) {
                Unit* next = nullptr;
                std::int64_t bd = std::int64_t(a.chain_range) * a.chain_range;
                for (Unit& o : units)
                    if (o.alive && o.team != u.team && targetable(o) && std::ranges::find(done, o.id) == done.end()) {
                        const std::int64_t dd = dist2(o.ux(), o.uy(), from->ux(), from->uy());
                        if (dd <= bd) bd = dd, next = &o;
                    }
                if (!next) return;
                d = d * a.chain_pct / 100;
                deal_damage(&u, *next, d, sub, physical, true);
                done.push_back(next->id);
                from = next;
            }
            return;
        }
    }
}

// [U-217] walking AND autoattacking: fire at the closest enemy in range whenever the attack is ready.
void World::attack_on_the_move(Unit& u) {
    if (now_ms < u.attack_ready_ms || !u.st.can_attack(true) || u.st.has(StatusKind::Blind)) return;
    Unit* best = nullptr;
    std::int64_t bg = u.attack_range + 1;
    for (Unit& o : units)
        if (o.alive && o.team != u.team && o.team != Team::Neutral && targetable(o) && vulnerable(o) && visible_to(u.team, o)) {
            const std::int64_t g = edge_gap(u, o);
            if (g < bg) bg = g, best = &o;
        }
    if (!best) return;
    u.attack_ready_ms = now_ms + attack_period_ms(u);
    fire_auto(u, *best);
}

void World::announce(AnnounceKind kind, std::uint32_t a, std::uint32_t b, Team t) {
    Announce an{kind, a, b, t, now_ms};
    if (const Unit* src = a ? find(a) : nullptr) {  // [U-363]
        an.a_kind = std::uint8_t(src->kind);
        an.a_extra = std::uint8_t(src->kind == UnitKind::Minion ? int(src->mtype) : src->kind == UnitKind::Champion ? src->champ : mob_name_id(*src));
    }
    announces.push_back(an);
}

void World::on_kill(Unit* killer, Unit& dead) {
    if (!dead.alive) return;
    dead.alive = false;
    dead.hp = 0;
    dead.casting = false;
    // credit goes to the champion owning the killer (minion/turret kills give no gold)
    Unit* k = killer;
    Team kt = k ? k->team : (dead.team == Team::FireNation ? Team::EarthNation : dead.team == Team::EarthNation ? Team::FireNation : Team::Neutral);
    auto champs_near = [&](Team t, std::int32_t x, std::int32_t y, std::int64_t r) {
        std::vector<Unit*> out;
        for (Unit& o : units)
            if (o.alive && o.kind == UnitKind::Champion && o.team == t && dist2(o.ux(), o.uy(), x, y) <= r * r) out.push_back(&o);
        return out;
    };
    switch (dead.kind) {
        case UnitKind::Champion: {
            // [U-300] like LoL: a turret / minion / monster never takes the kill from a champion who hit the victim
            // in the last 10 s — the champion who hit last gets it
            if (!k || k->kind != UnitKind::Champion) {
                std::int64_t best = -1;
                for (const auto& [id, ms] : dead.assist_by)
                    if (id && now_ms - ms <= 10'000 && ms > best)
                        if (Unit* c = find(id); c && c->kind == UnitKind::Champion && c->team != dead.team) best = ms, k = c;
            }
            if (k && k->team != Team::Neutral) kt = k->team;  // [U-383] the credited champion's team (also after a monster finished it)
            if (kt == Team::Neutral) kt = dead.team == Team::FireNation ? Team::EarthNation : Team::FireNation;
            ++dead.deaths;
            dead.respawn_ms = now_ms + std::min<std::int64_t>(60'000, 8'000 + 2'000LL * dead.level);
            if (kt != Team::Neutral) ++team_kills[std::size_t(kt)];
            std::vector<Unit*> helpers;
            for (const auto& [id, ms] : dead.assist_by)
                if (id && now_ms - ms <= 10'000 && (!k || id != k->id))
                    if (Unit* h = find(id); h && h->team != dead.team) helpers.push_back(h);
            // [U-262] death recap: the fight that killed — walk back while hits come less than 10 s apart
            {
                DeathRecap rc{dead.id, k ? k->id : 0, now_ms, {}};
                std::int64_t edge = now_ms;
                for (auto h = dead.taken.rbegin(); h != dead.taken.rend(); ++h) {
                    if (edge - h->ms > 10'000 || now_ms - h->ms > 30'000) break;
                    edge = h->ms;
                    rc.hits.push_back(*h);
                }
                std::ranges::reverse(rc.hits);
                recaps.push_back(std::move(rc));
                dead.taken.clear();
            }
            // [U-263] kill announcements: first blood, multikills (next kill within 10 s), sprees, shutdowns, ace
            const bool shutdown = dead.bounty > rules::kBaseBounty;  // [U-413] a bounty above the base = a shutdown
            const std::int32_t bounty = dead.bounty;
            dead.spree = 0;
            dead.multi = 0;
            ++dead.death_streak;
            dead.bounty = rules::champion_bounty(0, dead.death_streak);
            // [U-410] XP: split between the killer, the assisters and allies nearby
            {
                std::vector<Unit*> share = champs_near(kt, dead.ux(), dead.uy(), rules::kXpRange);
                for (Unit* x : helpers)
                    if (std::ranges::find(share, x) == share.end()) share.push_back(x);
                if (k && k->kind == UnitKind::Champion && std::ranges::find(share, k) == share.end()) share.push_back(k);
                const std::int64_t each = rules::champion_kill_xp(dead.level) / std::int64_t(std::max<std::size_t>(1, share.size()));
                for (Unit* x : share) reward(*x, dead, 0, each);
            }
            if (k && k->kind == UnitKind::Champion) {
                ++k->kills;
                reward(*k, dead, bounty, 0);
                std::string names;  // [U-410] assists in the kill feed
                for (Unit* h : helpers)
                    if (h != k) names += (names.empty() ? "" : ", ") + h->name;
                add_feed(names.empty() ? std::format("{} slew {}{}", k->name, dead.name, shutdown ? std::format(" (shutdown {} g)", bounty) : "")
                                       : std::format("{} slew {} — assists: {}{}", k->name, dead.name, names,
                                                     shutdown ? std::format(" (shutdown {} g)", bounty) : ""));
                k->multi = now_ms - k->last_kill_ms <= 10'000 ? k->multi + 1 : 1;
                k->last_kill_ms = now_ms;
                ++k->spree;
                k->death_streak = 0;
                k->bounty = rules::champion_bounty(k->spree, 0);  // [U-413]
                if (!first_blood) {
                    first_blood = true;
                    announce(AnnounceKind::FirstBlood, k->id, dead.id, k->team);
                } else if (shutdown) {
                    announce(AnnounceKind::Shutdown, k->id, dead.id, k->team);
                } else {
                    announce(AnnounceKind::Kill, k->id, dead.id, k->team);
                }
                if (k->multi >= 2) announce(AnnounceKind(int(AnnounceKind::DoubleKill) + std::min(k->multi, 5) - 2), k->id, dead.id, k->team);
                else if (k->spree >= 3) announce(AnnounceKind(int(AnnounceKind::KillingSpree) + std::min(k->spree, 8) - 3), k->id, dead.id, k->team);
            } else {
                add_feed(std::format("{} died", dead.name));
                announce(AnnounceKind::Executed, killer ? killer->id : 0, dead.id, kt);
            }
            {  // ace: every champion of the dead one's team is dead
                bool all_dead = true;
                for (const Unit& o : units)
                    if (o.kind == UnitKind::Champion && o.team == dead.team && o.alive) all_dead = false;
                if (all_dead) announce(AnnounceKind::Ace, 0, 0, kt);
            }
            for (Unit* h : helpers) {  // [U-410] assisters share half of the bounty (the XP was shared above)
                ++h->assists;
                reward(*h, dead, bounty * rules::kAssistGoldPct / 100 / std::int32_t(std::max<std::size_t>(1, helpers.size())), 0);
            }
            dead.assist_by = {};
            break;
        }
        case UnitKind::Minion: {
            static constexpr std::int32_t kGold[] = {21, 14, 60, 60}, kXp[] = {60, 30, 93, 97};
            const auto t = std::size_t(dead.mtype);
            const auto near = champs_near(kt, dead.ux(), dead.uy(), rules::kXpRange);  // [U-195] XP range 1 000
            const std::int64_t share = kXp[t] * 130 / 100 / std::int64_t(std::max<std::size_t>(1, near.size())) + kXp[t] / 4;
            for (Unit* c : near) reward(*c, dead, c == k ? kGold[t] : 0, share);  // gold: last hit only
            if (k && k->kind == UnitKind::Champion) {
                ++k->cs;
                if (std::ranges::find(near, k) == near.end()) reward(*k, dead, kGold[t], 0);
            }
            dead.respawn_ms = now_ms + 600;  // corpse time
            break;
        }
        case UnitKind::Monster:
        case UnitKind::Boss: {
            const bool boss = dead.kind == UnitKind::Boss;
            const MonsterDef* md = dead.mob >= 0 ? &kMonsters[std::size_t(dead.mob)] : nullptr;  // [U-308] its own reward
            // [U-365] worth more after every respawn
            const std::int32_t gold = md ? md->gold + md->gold_growth * dead.deaths : boss ? 300 : 70;
            const std::int64_t xp = md ? md->xp + std::int64_t(md->xp_growth) * dead.deaths : boss ? 600 : 140;
            // [U-195] sharers: the killer (no range needed) + champions of the killer's team who hit it in the
            // last 3 s and are within the XP range. Killer counts as N people, every helper as 1.
            std::vector<Unit*> near;
            Unit* kc = k && k->kind == UnitKind::Champion ? k : nullptr;
            if (kc) near.push_back(kc);
            for (const auto& [id, ms] : dead.assist_by)
                if (Unit* h = find(id); h && h != kc && h->alive && h->kind == UnitKind::Champion && h->team == kt &&
                                        now_ms - ms <= rules::kJungleAssistWindowMs &&
                                        dist2(h->ux(), h->uy(), dead.ux(), dead.uy()) <= std::int64_t(rules::kXpRange) * rules::kXpRange)
                    near.push_back(h);
            const int n = int(near.size());
            for (Unit* c : near)
                reward(*c, dead, c == kc || !kc ? gold : gold / 2,
                       c == kc ? rules::jungle_killer_xp(xp, n) : kc ? rules::jungle_helper_xp(xp, n) : xp / std::max(1, n));
            if (k && k->kind == UnitKind::Champion) ++k->cs;
            dead.respawn_ms = now_ms + (md ? md->respawn_ms : boss ? 300'000 : 120'000);
            ++dead.deaths;  // [U-365] the next one grows
            if (boss) announce(AnnounceKind::BossSlain, kc ? kc->id : 0, dead.id, kt);  // [U-263] objective taken
            if (md && md->buff != JungleBuff::None && kc) {  // [U-366] the killer carries the buff
                kc->jbuff = std::uint8_t(md->buff);
                kc->jbuff_until = now_ms + md->buff_ms;
            }
            if (!kc && near.empty()) kt = Team::Neutral;  // [U-383] nobody killed it (e.g. a minion): no team reward
            if (md && kt != Team::Neutral && md->team_reward == TeamReward::LionTurtleStack) {  // [U-364] permanent team stack
                ++lion_stacks[std::size_t(kt) & 1];
                add_feed(std::format("The Lion Turtle blesses the {} (stack {})", kt == Team::FireNation ? "Fire Nation" : "Earth Nation",
                                     lion_stacks[std::size_t(kt) & 1]));
            }
            if (md && kt != Team::Neutral && md->team_reward == TeamReward::DragonGold)  // [U-364] gold for the whole team
                for (Unit& c : units)
                    if (c.kind == UnitKind::Champion && c.team == kt) reward(c, dead, md->team_value + md->gold_growth * (dead.deaths - 1), 0);
            if (boss && !near.empty() && md && md->team_reward == TeamReward::FaceSteal) {  // [X-55] the Face Stealer steals a skill of a random killer for 2 min
                Unit* victim = near[random() % near.size()];
                victim->stolen_slot = std::uint8_t(1 + random() % 4);  // Q W E R
                victim->stolen_until = now_ms + 120'000;
                add_feed(std::format("The Face Stealer took a face from {}!", victim->name));
            }
            break;
        }
        case UnitKind::Turret:
        case UnitKind::Inhibitor:
        case UnitKind::Nexus: {
            clearance_dirty = true;  // its wall is gone [X-95]
            for (Unit& o : units)
                if (o.kind == UnitKind::Champion && o.team == kt) o.gold += dead.kind == UnitKind::Inhibitor ? 50 : 150;
            add_feed(std::format("{} {} destroyed", dead.team == Team::FireNation ? "Fire" : "Earth",
                                 dead.kind == UnitKind::Turret ? "turret" : dead.kind == UnitKind::Inhibitor ? "inhibitor" : "NATION SHRINE"));
            announce(dead.kind == UnitKind::Turret ? AnnounceKind::TurretDestroyed : dead.kind == UnitKind::Inhibitor ? AnnounceKind::InhibitorDestroyed
                                                                                    : AnnounceKind::ShrineDestroyed,
                     k ? k->id : 0, dead.id, kt);  // [U-263]
            if (dead.kind == UnitKind::Inhibitor) dead.respawn_ms = now_ms + 240'000;
            if (dead.kind == UnitKind::Nexus) {  // [X-12] win
                over = true;
                winner = dead.team == Team::FireNation ? Team::EarthNation : Team::FireNation;
                add_feed(winner == Team::FireNation ? "FIRE NATION WINS" : "EARTH NATION WINS");
            }
            break;
        }
    }
}

}  // namespace em
