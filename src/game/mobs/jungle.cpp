// Jungle monsters [U-82][U-147][X-55][X-97]: camps and behaviour.
#include <algorithm>
#include "game/match/layout.hpp"
#include "game/mobs/rules.hpp"
#include "game/world.hpp"

namespace em {

namespace {
std::int32_t cell_mid(int c) { return (c * kCellUnits + kCellUnits / 2) * kSub; }
}  // namespace

// [U-82][U-147] Jungle: passive spirits / animals, aggressive camps [X-97], the Face Stealer [X-55].
void World::spawn_jungle() {
    for (int side = 0; side < 2; ++side) {
        auto P = [&](CellPt c) { return side == 0 ? c : mirror(c); };
        // [U-82] jungle spirits and animals
        for (std::size_t i = 0; i < kCamps.size(); ++i) {
            Unit m;
            m.kind = UnitKind::Monster;
            m.team = Team::Neutral;
            m.body = i % 2 ? &kBeastBody : &kHumanoidBody;
            m.name = i % 2 ? "Spirit Wolf" : "Wandering Spirit";
            const CellPt c = P(kCamps[i]);
            m.x = m.home_x = cell_mid(c.first);
            m.y = m.home_y = cell_mid(c.second);
            m.stats.health.base = 140'000;
            m.stats.attack.base = 3'500;
            m.stats.defence.base = 2'000;
            m.stats.elemental_defence.base = 2'000;
            m.stats.move_speed.base = 30'000;
            m.stats.turn_rate.base = 400;
            m.hp = m.stats.health.total();
            m.as100 = 70;
            m.attack_range = 40;
            m.sight = rules::kSightMonster;
            spawn_unit(std::move(m));
        }
        // [X-97] aggressive jungle: cursed spirit (ranged curse), angry beast (body-blocks [X-96]), bandit pair
        for (std::size_t i = 0; i < kAggroCamps.size(); ++i) {
            const CellPt c = P(kAggroCamps[i]);
            const int count = i == 2 ? 2 : 1;
            for (int k = 0; k < count; ++k) {
                Unit m;
                m.kind = UnitKind::Monster;
                m.team = Team::Neutral;
                m.aggressive = true;
                m.x = m.home_x = cell_mid(c.first) + k * 50 * kSub;
                m.y = m.home_y = cell_mid(c.second);
                m.stats.move_speed.base = 31'000;
                m.stats.turn_rate.base = 400;
                m.stats.defence.base = 2'500;
                m.stats.elemental_defence.base = 2'500;
                m.sight = rules::kSightAggressiveMonster;
                if (i == 0) {
                    m.name = "Cursed Spirit";
                    m.body = &kSlimBody;
                    m.stats.health.base = 120'000;
                    m.stats.attack.base = 5'000;
                    m.stats.range = Range::Ranged;
                    m.attack_range = 380;
                    m.as100 = 60;
                } else if (i == 1) {
                    m.name = "Angry Beast";
                    m.body = &kBeastBody;
                    m.stats.health.base = 220'000;
                    m.stats.attack.base = 6'500;
                    m.attack_range = 40;
                    m.as100 = 65;
                    m.collide |= kCollUnit;
                } else {
                    m.name = "Bandit";
                    m.body = &kHumanoidBody;
                    m.stats.health.base = 90'000;
                    m.stats.attack.base = 4'000;
                    m.stats.range = k ? Range::Ranged : Range::Melee;
                    m.attack_range = k ? 420 : 40;
                    m.as100 = 75;
                }
                m.hp = m.stats.health.total();
                spawn_unit(std::move(m));
            }
        }
    }
    // [X-55] Face Stealer on the spirit island [X-86]
    Unit b;
    b.kind = UnitKind::Boss;
    b.team = Team::Neutral;
    b.body = &kBeastBody;
    b.name = "Face Stealer";
    b.x = b.home_x = cell_mid(kSpiritIsland.first + 14);
    b.y = b.home_y = cell_mid(kSpiritIsland.second + 14);
    b.stats.health.base = 600'000;
    b.stats.attack.base = 12'000;
    b.stats.defence.base = 6'000;
    b.stats.elemental_defence.base = 6'000;
    b.stats.move_speed.base = 32'000;
    b.stats.turn_rate.base = 300;
    b.stats.model_size = 6'000;  // [U-129] big spirit = big hitbox
    b.hp = b.stats.health.total();
    b.as100 = 60;
    b.attack_range = 60;
    b.sight = rules::kSightBoss;
    b.collide |= kCollUnit;  // [X-96] the big spirit body-blocks
    spawn_unit(std::move(b));
}

void World::step_monster(Unit& u) {
    // [X-97] aggressive monsters pick the closest non-neutral unit near their camp
    if (u.aggressive && !u.aggro && now_ms >= u.next_scan_ms) {
        u.next_scan_ms = now_ms + 250;
        std::int64_t bd = std::int64_t(rules::kAggressiveSightRange) * rules::kAggressiveSightRange;
        for (const Unit& o : units)
            if (o.alive && o.team != Team::Neutral && targetable(o) && !o.is_structure() && o.kind != UnitKind::Inhibitor &&
                dist2(o.ux(), o.uy(), u.ux(), u.uy()) < bd &&
                dist2(o.ux(), o.uy(), u.home_x / kSub, u.home_y / kSub) < 650LL * 650) {
                bd = dist2(o.ux(), o.uy(), u.ux(), u.uy());
                u.aggro = o.id;
            }
    }
    Unit* t = find(u.aggro);
    const std::int64_t from_home = dist2(u.x / kSub, u.y / kSub, u.home_x / kSub, u.home_y / kSub);
    // [U-154][X-107] leash like LoL: give up only when the target is gone / far from the camp AND
    // nobody has hit me for 2.5 s; then walk home healing 5 % per second.
    const std::int64_t leash = u.kind == UnitKind::Boss ? rules::kBossLeash : rules::kMonsterLeash;
    const bool target_ok = t && t->alive && targetable(*t) &&
                           dist2(t->ux(), t->uy(), u.home_x / kSub, u.home_y / kSub) < (leash + 300) * (leash + 300);
    const bool fighting = target_ok && (from_home < leash * leash || now_ms - u.last_hurt_ms < rules::kMonsterGiveUpMs);
    if (fighting) {
        u.order = {OrderKind::Attack, t->id};
        update_attack(u);
        // the Face Stealer slams the ground every 6 s (spirit shockwave)
        if (u.kind == UnitKind::Boss && tick % (cfg.tick_hz * 6) == 0) {
            const Shape me = place_body(*u.body, u.ux(), u.uy(), u.face, 0, size_pct(u) + 150);
            for (Unit& o : units)
                if (o.alive && o.team != Team::Neutral && touches(me, hitbox(o))) {
                    deal_damage(&u, o, 120, SubElement::Physical, true, false);
                    apply_cc(u.stats, u.id, o, StatusKind::Slow, 1'500, 40);
                }
        }
        return;
    }
    if (now_ms - u.last_hurt_ms < rules::kMonsterGiveUpMs && from_home < leash * leash) return;  // just hit by someone unseen: hold
    // reset: walk home, heal 5 % / s
    u.aggro = 0;
    u.order = {};
    if (from_home > 40LL * 40) {
        move_towards(u, u.home_x, u.home_y, move_speed_sub(u) * 3 / 2);
        u.hp = std::min<std::int32_t>(u.stats.health.total(), u.hp + std::max<std::int32_t>(1, u.stats.health.total() * rules::kMonsterResetHealPctPerS / 100 / std::int32_t(cfg.tick_hz)));
    }
}

}  // namespace em
