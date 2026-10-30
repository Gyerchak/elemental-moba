// Jungle monsters [U-82][U-147][X-55][X-97]: camps and behaviour.
#include <algorithm>
#include "game/match/layout.hpp"
#include "game/match/maps.hpp"
#include "game/mobs/monsters.hpp"
#include "game/mobs/rules.hpp"
#include "game/world.hpp"

namespace em {

namespace {
std::int32_t cell_mid(int c) { return (c * kCellUnits + kCellUnits / 2) * kSub; }
}  // namespace

// [U-308] a monster from the table (game/mobs/monsters.hpp) at a camp
Unit World::make_monster(std::string_view name, std::int32_t x_sub, std::int32_t y_sub) {
    const int mi = monster_index(name);
    const MonsterDef& d = kMonsters[std::size_t(std::max(0, mi))];
    Unit m;
    m.kind = d.boss ? UnitKind::Boss : UnitKind::Monster;
    m.team = Team::Neutral;
    m.mob = std::int8_t(mi);
    m.name = std::string(d.name);
    m.body = d.body;
    m.aggressive = d.aggressive;
    if (d.spirit) m.collide &= std::uint8_t(~kCollUnit);  // [U-367] spirits pass through units
    m.x = m.home_x = x_sub;
    m.y = m.home_y = y_sub;
    m.stats.health.base = d.hp;
    m.stats.attack.base = d.attack;
    m.stats.defence.base = d.defence;
    m.stats.elemental_defence.base = d.elemental_defence;
    m.stats.move_speed.base = d.move_speed;
    m.stats.turn_rate.base = d.boss ? 300 : 400;
    m.stats.range = d.range;
    m.stats.model_size = d.model_size;
    m.hp = m.stats.health.total();
    m.as100 = d.attack_speed;
    m.attack_range = d.attack_range;
    m.sight = d.sight;
    return m;
}

// [U-365] every respawn: + hp_growth_pct / attack_growth_pct of the base (deaths = how often it was killed)
void World::apply_monster_growth(Unit& m) {
    if (m.mob < 0) return;
    const MonsterDef& d = kMonsters[std::size_t(m.mob)];
    m.stats.health.base = d.hp + std::int32_t(std::int64_t(d.hp) * d.hp_growth_pct * m.deaths / 100);
    m.stats.attack.base = d.attack + std::int32_t(std::int64_t(d.attack) * d.attack_growth_pct * m.deaths / 100);
}

// [U-82][U-147] Jungle: passive spirits / animals, aggressive camps [X-97], the Face Stealer [X-55].
void World::spawn_jungle() {
    for (int side = 0; side < 2; ++side) {
        auto P = [&](CellPt c) { return side == 0 ? c : mirror(c); };
        for (std::size_t i = 0; i < cur_map().camps.size(); ++i) {  // [U-82] jungle spirits and animals
            const CellPt c = P(cur_map().camps[i]);
            spawn_unit(make_monster(i % 2 ? "Spirit Wolf" : "Wandering Spirit", cell_mid(c.first), cell_mid(c.second)));
        }
        // [X-97] aggressive jungle: cursed spirit (ranged curse), angry beast (body-blocks [X-96]), bandit pair
        for (std::size_t i = 0; i < cur_map().aggro_camps.size(); ++i) {
            const CellPt c = P(cur_map().aggro_camps[i]);
            if (i == 2) {
                spawn_unit(make_monster("Bandit", cell_mid(c.first), cell_mid(c.second)));
                spawn_unit(make_monster("Bandit Archer", cell_mid(c.first) + 50 * kSub, cell_mid(c.second)));
            } else {
                spawn_unit(make_monster(i == 0 ? "Cursed Spirit" : "Angry Beast", cell_mid(c.first), cell_mid(c.second)));
            }
        }
    }
    for (int side = 0; side < 2; ++side)  // [U-366] buff camps
        for (std::size_t i = 0; i < cur_map().buff_camps.size(); ++i) {
            const CellPt c = side == 0 ? cur_map().buff_camps[i] : mirror(cur_map().buff_camps[i]);
            spawn_unit(make_monster(i == 0 ? "Platypus Bear" : "Knowledge Seeker", cell_mid(c.first), cell_mid(c.second)));
        }
    for (CellPt c : cur_map().dark_spirits)  // [U-401] dark spirits at both portals
        for (CellPt p : {c, mirror(c)}) spawn_unit(make_monster("Dark Spirit", cell_mid(p.first), cell_mid(p.second)));
    // [X-55][U-364][U-404][U-420] the map's epic monsters (some appear later: first_spawn_ms)
    for (const EpicSpot& e : cur_map().epics) spawn_unit(make_monster(e.monster, cell_mid(e.cell.first), cell_mid(e.cell.second)));
    for (Unit& u : units)
        if (u.mob >= 0 && kMonsters[std::size_t(u.mob)].first_spawn_ms > 0 && u.alive && u.respawn_ms == 0) {
            u.alive = false;
            u.respawn_ms = kMonsters[std::size_t(u.mob)].first_spawn_ms;
        }
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
    const MonsterDef* md = u.mob >= 0 ? &kMonsters[std::size_t(u.mob)] : nullptr;  // [U-308] its own numbers
    const std::int64_t leash = md ? md->patience : u.kind == UnitKind::Boss ? rules::kBossLeash : rules::kMonsterLeash;
    const std::int64_t give_up = md ? md->give_up_ms : rules::kMonsterGiveUpMs;
    const bool target_ok = t && t->alive && targetable(*t) &&
                           dist2(t->ux(), t->uy(), u.home_x / kSub, u.home_y / kSub) < (leash + 300) * (leash + 300);
    const bool fighting = target_ok && (from_home < leash * leash || now_ms - u.last_hurt_ms < give_up);
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
    if (now_ms - u.last_hurt_ms < give_up && from_home < leash * leash) return;  // just hit by someone unseen: hold
    // reset: walk home, heal 5 % / s
    u.aggro = 0;
    u.order = {};
    if (from_home > 40LL * 40) {
        move_towards(u, u.home_x, u.home_y, move_speed_sub(u) * 3 / 2);
    }
    // [U-308] the fight is over: heal (walking home AND standing at the camp) until full, like LoL
    const std::int32_t heal_pct = md ? md->reset_heal_pct : rules::kMonsterResetHealPctPerS;
    u.hp = std::min<std::int32_t>(u.stats.health.total(), u.hp + std::max<std::int32_t>(1, u.stats.health.total() * heal_pct / 100 / std::int32_t(cfg.tick_hz)));
}

}  // namespace em
