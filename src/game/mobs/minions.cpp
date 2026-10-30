// Minions [X-08][U-12][U-13]: waves and lane behaviour.
#include <algorithm>
#include <format>
#include "game/match/layout.hpp"
#include "game/mobs/rules.hpp"
#include "game/world.hpp"

namespace em {

namespace {
std::int32_t cell_mid(int c) { return (c * kCellUnits + kCellUnits / 2) * kSub; }
}  // namespace

void World::spawn_wave() {
    ++wave_no;
    if (wave_no == 1) announce(AnnounceKind::MinionsSpawned, 0, 0, Team::Neutral);  // [U-263]
    for (int side = 0; side < 2; ++side) {
        const Team t = side == 0 ? Team::FireNation : Team::EarthNation;
        for (int lane = 0; lane < 3; ++lane) {
            // super minions when the enemy inhibitor of this lane is down
            bool super = false;
            for (const Unit& o : units)
                if (o.kind == UnitKind::Inhibitor && o.team != t && o.lane == lane && !o.alive) super = true;
            std::vector<MinionType> types{MinionType::Melee, MinionType::Melee, MinionType::Melee};
            if (wave_no % 3 == 0) types.push_back(MinionType::Siege);
            if (super) types.push_back(MinionType::Super);
            for (int k = 0; k < 3; ++k) types.push_back(MinionType::Ranged);
            const auto& path = lane_path(lane);
            const CellPt start = side == 0 ? path.front() : path.back();
            for (std::size_t k = 0; k < types.size(); ++k) {
                Unit m;
                m.kind = UnitKind::Minion;
                m.team = t;
                m.mtype = types[k];
                m.lane = std::uint8_t(lane);
                m.body = &kMinionBody;
                m.name = "Minion";
                m.x = cell_mid(start.first) + std::int32_t(k % 3) * 20 * kSub;
                m.y = cell_mid(start.second) + std::int32_t(k / 3) * 20 * kSub;
                const std::int32_t grow = std::int32_t(std::min<std::int64_t>(now_ms / 60'000, 60));  // +2 %/min [X-08]
                static constexpr std::int32_t kHp[] = {47'700, 29'600, 90'000, 150'000};
                static constexpr std::int32_t kAtk[] = {1'200, 2'300, 4'000, 9'000};
                static constexpr std::int32_t kRange[] = {40, 470, 300, 50};
                m.stats.health.base = kHp[std::size_t(m.mtype)] * (100 + 2 * grow) / 100;
                m.stats.attack.base = kAtk[std::size_t(m.mtype)] * (100 + grow) / 100;
                m.stats.move_speed.base = 32'500;
                m.stats.turn_rate.base = 600;
                m.stats.range = kRange[std::size_t(m.mtype)] > 100 ? Range::Ranged : Range::Melee;
                m.hp = m.stats.health.total();
                m.attack_range = kRange[std::size_t(m.mtype)];
                m.as100 = m.mtype == MinionType::Ranged ? 67 : 125;
                m.sight = rules::kSightMinion;
                m.lane_wp = side == 0 ? 1 : int(path.size()) - 2;
                m.face = dir_of(side == 0 ? 1 : -1, side == 0 ? -1 : 1);
                spawn_unit(std::move(m));
            }
        }
    }
}

// LoL-2016-style target choice [U-194]: see MinionPriority in mobs/rules.hpp.
rules::MinionPriority World::minion_priority(const Unit& me, const Unit& enemy) const {
    using P = rules::MinionPriority;
    const auto& R = rules::kMinion;
    if (enemy.is_structure() || enemy.kind == UnitKind::Inhibitor) {
        // a turret that is hitting one of my allied minions near me
        if (enemy.kind == UnitKind::Turret && now_ms - enemy.last_victim_ms < R.call_for_help_ms)
            if (const Unit* v = find(enemy.last_victim); v && v->team == me.team && v->kind == UnitKind::Minion &&
                                                         dist2(v->ux(), v->uy(), me.ux(), me.uy()) < std::int64_t(R.call_for_help_range) * R.call_for_help_range)
                return P::TurretHittingAllyMinion;
        return P::Structure;
    }
    if (now_ms - enemy.last_victim_ms < R.call_for_help_ms)
        if (const Unit* v = find(enemy.last_victim); v && v->alive && v->team == me.team &&
                                                     dist2(v->ux(), v->uy(), me.ux(), me.uy()) < std::int64_t(R.call_for_help_range) * R.call_for_help_range) {
            const bool victim_champ = v->kind == UnitKind::Champion;
            if (enemy.kind == UnitKind::Champion) return victim_champ ? P::ChampionHittingAllyChampion : P::ChampionHittingAllyMinion;
            if (enemy.kind == UnitKind::Minion) return victim_champ ? P::MinionHittingAllyChampion : P::MinionHittingAllyMinion;
        }
    return enemy.kind == UnitKind::Minion ? P::ClosestMinion : enemy.kind == UnitKind::Champion ? P::ClosestChampion : P::None;
}

void World::step_minion(Unit& u) {
    const auto& R = rules::kMinion;
    const auto& lane = lane_path(u.lane);
    const bool fire = u.team == Team::FireNation;

    // 1. choose a target 4× per second; switch only for a strictly more important one [U-194]
    if (now_ms >= u.next_scan_ms) {
        u.next_scan_ms = now_ms + R.retarget_ms;
        Unit* cur = u.order.kind == OrderKind::Attack ? find(u.order.target) : nullptr;
        if (cur && (!cur->alive || !targetable(*cur) || !visible_to(u.team, *cur) ||
                    dist2(cur->ux(), cur->uy(), u.ux(), u.uy()) > std::int64_t(R.drop_range) * R.drop_range))
            cur = nullptr;
        auto best_pr = cur ? minion_priority(u, *cur) : rules::MinionPriority::None;
        std::int64_t best_d = cur ? dist2(cur->ux(), cur->uy(), u.ux(), u.uy()) : INT64_MAX;
        std::uint32_t best = cur ? cur->id : 0;
        for (const Unit& o : units) {
            if (!o.alive || o.team == u.team || o.team == Team::Neutral || !targetable(o) || !vulnerable(o)) continue;
            const std::int64_t d = dist2(o.ux(), o.uy(), u.ux(), u.uy());
            if (d > std::int64_t(R.acquire_range) * R.acquire_range || !visible_to(u.team, o)) continue;
            const auto pr = minion_priority(u, o);
            if (pr < best_pr || (pr == best_pr && !cur && d < best_d)) {
                best_pr = pr;
                best_d = d;
                best = o.id;
            }
        }
        u.order = best ? Order{OrderKind::Attack, best} : Order{};
        if (best) u.path.clear();  // after the fight: a fresh path back to the lane
    }
    if (u.order.kind == OrderKind::Attack) {
        update_attack(u);
        return;
    }

    // 2. walk the lane on the shortest path around turrets / walls [U-196]
    if (u.lane_wp < 0 || u.lane_wp >= int(lane.size())) return;
    // never walk back to a lane point behind me: if the next one is closer, take it
    const int step = fire ? 1 : -1;
    for (int k = u.lane_wp + step; k >= 0 && k < int(lane.size()); k += step) {
        const CellPt a = lane[std::size_t(u.lane_wp)], b = lane[std::size_t(k)];
        if (dist2(u.ux(), u.uy(), cell_mid(b.first) / kSub, cell_mid(b.second) / kSub) <
            dist2(u.ux(), u.uy(), cell_mid(a.first) / kSub, cell_mid(a.second) / kSub))
            u.lane_wp = k;
        else
            break;
    }
    const CellPt wp = lane[std::size_t(u.lane_wp)];
    const std::int32_t tx = cell_mid(wp.first), ty = cell_mid(wp.second);
    if (dist2(u.ux(), u.uy(), tx / kSub, ty / kSub) < std::int64_t(R.waypoint_reached) * R.waypoint_reached) {
        u.lane_wp += step;
        u.path.clear();
        return;
    }
    if (u.path_i >= u.path.size() || now_ms - u.repath_ms > R.repath_ms) path_to(u, tx / kSub, ty / kSub);
    follow_path(u);

    // 3. [U-12][U-13] blocked by bending: earth minions clear the way at 1.5 s, fire minions blow it up at 2.5 s
    // [U-383] only a WALL ahead counts (with unit collision a minion often waits behind other units)
    const bool wall_ahead = [&] {
        for (int d = 1; d <= 2; ++d) {
            const int ax = (u.ux() + u.face.x * d * kCellUnits / kOne) / kCellUnits, ay = (u.uy() + u.face.y * d * kCellUnits / kOne) / kCellUnits;
            if (Grid::in_bounds(ax, ay) && (grid.at(ax, ay).wall() != Wall::None || grid.at(ax, ay).has_temp_elevation())) return true;
        }
        return false;
    }();
    if (u.stuck_ms > 0 && wall_ahead) u.blocked_ms += 1'000 / cfg.tick_hz;
    else u.blocked_ms = std::max<std::int64_t>(0, u.blocked_ms - 1'000 / cfg.tick_hz);
    if (u.blocked_ms >= (fire ? 2'500 : 1'500)) {
        u.blocked_ms = 0;
        const int cx = u.ux() / kCellUnits, cy = u.uy() / kCellUnits;
        bool cleared = false;
        for (CellTimer& t : cell_timers)
            if (std::abs(t.x - cx) <= 4 && std::abs(t.y - cy) <= 4 && (grid.at(t.x, t.y).wall() != Wall::None ||
                                                                          grid.at(t.x, t.y).has_temp_elevation())) {
                t.at_ms = now_ms;
                cleared = true;
            }
        if (!cleared) {  // not a bent wall (a structure, other units): just take a new path
            path_to(u, tx / kSub, ty / kSub);
            return;
        }
        if (fire)  // explosives hurt enemies close by
            for (Unit& o : units)
                if (o.alive && o.team != u.team && o.team != Team::Neutral && !o.is_structure() &&
                    dist2(o.ux(), o.uy(), u.ux(), u.uy()) < 160LL * 160)
                    deal_damage(&u, o, 60, SubElement::Combustion, false, false);
        add_feed(fire ? "Fire minions blew up a wall" : "Earth minions cleared a wall");
    }
}

}  // namespace em
