// Structures [X-07][U-144]: spawning + turret targeting.
#include <algorithm>
#include "game/match/layout.hpp"
#include "game/mobs/rules.hpp"
#include "game/world.hpp"

namespace em {

namespace {
std::int32_t cell_mid(int c) { return (c * kCellUnits + kCellUnits / 2) * kSub; }

Unit structure(UnitKind k, Team t, CellPt c, int lane, int tier) {
    Unit u;
    u.kind = k;
    u.team = t;
    u.lane = std::uint8_t(lane);
    u.tier = std::int8_t(tier);
    u.x = u.home_x = cell_mid(c.first);
    u.y = u.home_y = cell_mid(c.second);
    u.body = k == UnitKind::Nexus ? &kNexusBody : &kTowerBody;
    const bool turret = k == UnitKind::Turret;
    u.stats.health.base = k == UnitKind::Nexus ? 550'000 : k == UnitKind::Inhibitor ? 300'000 : tier == 4 ? 270'000 : 350'000;
    u.stats.attack.base = turret ? 16'000 + 2'000 * tier : k == UnitKind::Nexus ? 20'000 : 0;
    u.stats.defence.base = 6'000;
    u.stats.elemental_defence.base = 6'000;
    u.stats.range = Range::Ranged;
    u.hp = u.stats.health.total();
    u.as100 = 83;
    u.attack_range = turret || k == UnitKind::Nexus ? 680 : 0;
    u.sight = rules::kSightTurret;
    u.name = turret ? "Turret" : k == UnitKind::Inhibitor ? "Inhibitor" : "Nation Shrine";
    return u;
}
}  // namespace

// [X-07][U-144] Turrets, inhibitors, nexus turrets and the Nation Shrine for both teams.
void World::spawn_structures() {
    for (int side = 0; side < 2; ++side) {
        const Team t = side == 0 ? Team::FireNation : Team::EarthNation;
        auto P = [&](CellPt c) { return side == 0 ? c : mirror(c); };
        for (int lane = 0; lane < 3; ++lane) {
            // Earth's lane L is the mirror of Fire's lane 2−L
            const LaneStructs& s = kLaneStructs[std::size_t(side == 0 ? lane : 2 - lane)];
            spawn_unit(structure(UnitKind::Turret, t, P(s.t1), lane, 1));
            spawn_unit(structure(UnitKind::Turret, t, P(s.t2), lane, 2));
            spawn_unit(structure(UnitKind::Turret, t, P(s.t3), lane, 3));
            spawn_unit(structure(UnitKind::Inhibitor, t, P(s.inhib), lane, 0));
        }
        for (CellPt c : kNexusTurrets) spawn_unit(structure(UnitKind::Turret, t, P(c), 1, 4));
        spawn_unit(structure(UnitKind::Nexus, t, P(kNexusCell), 1, 5));
    }
}

void World::step_turret(Unit& u) {
    if (u.kind == UnitKind::Inhibitor) return;
    Unit* cur = find(u.order.target);
    const bool ok = u.order.kind == OrderKind::Attack && cur && cur->alive && targetable(*cur) && edge_gap(u, *cur) <= u.attack_range &&
                    visible_to(u.team, *cur);
    Unit* ag = find(u.aggro);
    if (ag && ag->alive && targetable(*ag) && edge_gap(u, *ag) <= u.attack_range && visible_to(u.team, *ag)) {
        u.order = {OrderKind::Attack, ag->id};
    } else if (!ok) {
        u.aggro = 0;
        std::uint32_t best = 0;
        std::int64_t bd = INT64_MAX;
        for (const Unit& o : units) {
            if (!o.alive || o.team == u.team || o.team == Team::Neutral || !targetable(o) || o.is_structure() || o.kind == UnitKind::Inhibitor)
                continue;
            if (edge_gap(u, o) > u.attack_range || !visible_to(u.team, o)) continue;
            const std::int64_t score = (o.kind == UnitKind::Minion ? 0 : 1) * 10'000'000 + dist2(o.ux(), o.uy(), u.ux(), u.uy());
            if (score < bd) {
                bd = score;
                best = o.id;
            }
        }
        u.order = best ? Order{OrderKind::Attack, best} : Order{};
    }
    if (u.order.kind == OrderKind::Attack) update_attack(u);
}

}  // namespace em
