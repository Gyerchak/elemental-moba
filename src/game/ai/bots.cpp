// [X-21] Bots for practice mode [U-100] and empty slots. Simple rules, same Cmds as players,
// and they only see what their team sees (fair fog of war).
#include <algorithm>

#include "game/match/layout.hpp"
#include "game/world.hpp"
#include "game/match/maps.hpp"

namespace em {

namespace {
// [U-261] bots build towards finished items like players: buy the whole item when the gold is there, else the
// next missing part (and its parts). Claude idea lists.
constexpr std::string_view kMageBuild[] = {"Mercury Treads", "Staff of the Moon Spirit", "Comet Shard Crown", "Fire Lord's Mantle",
                                           "Lens of the Four Nations", "Spiritual Water Flask"};
constexpr std::string_view kFighterBuild[] = {"Mercury Treads", "Dragon Bone Halberd", "Earth King's Bulwark", "Mountain Warden Plate",
                                              "Tidal Bracers"};

// the next item to buy on the way to `target` (−1 = wait for gold)
int next_buy(const Bag& bag, int gold, int target) {
    if (gold >= plan_buy(bag, target).price) return target;
    std::array<int, 3> need{-1, -1, -1};  // −1 = nothing missing in that part slot (NOT 0: 0 is an item id)
    for (std::size_t k = 0; k < 3; ++k) {
        const std::string_view p = kItems[std::size_t(target)].from[k];
        if (p.empty()) continue;
        const int pi = item_index(p);
        int needed = 0, have = 0;
        for (std::size_t j = 0; j <= k; ++j) needed += kItems[std::size_t(target)].from[j] == p;
        for (std::int16_t it : bag) have += it == pi;
        need[k] = needed > have ? pi : -1;
    }
    for (int pi : need)
        if (pi >= 0)
            if (const int b = next_buy(bag, gold, pi); b >= 0) return b;
    return -1;
}

bool offensive(SkillKind k) {
    switch (k) {
        case SkillKind::Bolt: case SkillKind::Wave: case SkillKind::Burst: case SkillKind::Field:
        case SkillKind::Cone: case SkillKind::Tornado: case SkillKind::Pillar: case SkillKind::Bloodbend:
        case SkillKind::Boulder: case SkillKind::Dash:
            return true;
        default: return false;
    }
}
}  // namespace

void World::step_bot(Unit& u) {
    if (now_ms < u.botm.next_think_ms) return;
    u.botm.next_think_ms = now_ms + 120 + std::int64_t(u.id % 7) * 10;
    const ChampionDef& def = champions()[std::size_t(u.champ)];
    auto send = [&](Cmd c) { command(u.id, c); };

    // shopping (also while dead)
    if (in_base(u) || !u.alive) {
        // [U-136] one element-defensive item against the strongest enemy element
        int count[5] = {};
        for (const Unit& o : units)
            if (o.kind == UnitKind::Champion && o.team != u.team) ++count[std::size_t(champions()[std::size_t(o.champ)].element)];
        const int top = int(std::max_element(count, count + 4) - count);
        static constexpr std::int16_t kVs[] = {14, 16, 13, 15};  // vs water, earth, fire, air
        const auto build = def.mage ? std::span<const std::string_view>(kMageBuild) : std::span<const std::string_view>(kFighterBuild);
        std::int16_t want = -1;
        int done = 0;
        for (std::string_view t : build) {
            const int ti = item_index(t);
            if (ti < 0) continue;
            if (std::ranges::find(u.items, std::int16_t(ti)) != u.items.end()) {
                ++done;
                if (done == 2 && count[top] >= 2 && std::ranges::find(u.items, kVs[top]) == u.items.end() && u.gold >= kItems[std::size_t(kVs[top])].cost) {
                    want = kVs[top];
                    break;
                }
                continue;
            }
            want = std::int16_t(next_buy(u.items, u.gold, ti));
            break;
        }
        if (want >= 0) send({.kind = CmdKind::Buy, .item = want});
    }
    if (!u.alive || u.channel_end_ms) return;
    if (u.skill_points > 0)
        for (Slot s : {Slot::Y, Slot::Q, Slot::W, Slot::E, Slot::R, Slot::T}) send({.kind = CmdKind::LevelUp, .slot = std::uint8_t(s)});

    const std::int32_t max_hp = u.stats.health.total();
    const int hp_pct = int(std::int64_t(u.hp) * 100 / std::max(1, max_hp));
    const auto [fx_, fy_] = fountain(u.team);
    const bool at_fountain = dist2(u.ux(), u.uy(), fx_, fy_) < 600LL * 600;
    if (at_fountain && hp_pct < 95) {  // heal up
        if (u.order.kind != OrderKind::None) send({.kind = CmdKind::Stop});
        u.botm.mode = 0;
        return;
    }

    // what can I see
    Unit* enemy = nullptr;  // closest enemy champion
    Unit* minion = nullptr; // weakest enemy minion near
    std::int64_t ed = 1'300LL * 1'300, md = 750LL * 750;
    int enemies_near = 0, allies_near = 0;
    for (Unit& o : units) {
        if (!o.alive || o.kind != UnitKind::Champion) continue;
        const std::int64_t d = dist2(o.ux(), o.uy(), u.ux(), u.uy());
        if (o.team == u.team) {
            allies_near += d < 1'200LL * 1'200 && o.id != u.id;
            continue;
        }
        if (!targetable(o) || !visible_to(u.team, o)) continue;
        enemies_near += d < 1'200LL * 1'200;
        if (d < ed) {
            ed = d;
            enemy = &o;
        }
    }
    for (Unit& o : units)
        if (o.alive && o.kind == UnitKind::Minion && o.team != u.team && visible_to(u.team, o)) {
            const std::int64_t d = dist2(o.ux(), o.uy(), u.ux(), u.uy());
            if (d < md && (!minion || o.hp < minion->hp)) {
                md = d;
                minion = &o;
            }
        }
    // enemy turret danger
    Unit* turret = nullptr;
    for (Unit& o : units)
        if (o.alive && (o.kind == UnitKind::Turret || o.kind == UnitKind::Nexus) && o.team != u.team &&
            dist2(o.ux(), o.uy(), u.ux(), u.uy()) < 950LL * 950)
            turret = &o;
    int allied_minions_at_turret = 0;
    if (turret)
        for (const Unit& o : units)
            allied_minions_at_turret += o.alive && o.kind == UnitKind::Minion && o.team == u.team &&
                                        dist2(o.ux(), o.uy(), turret->ux(), turret->uy()) < 800LL * 800;

    auto cast_at = [&](Slot s, const Unit& t) -> bool {
        if (!can_cast_now(u, s)) return false;
        const SkillDef* d = u.skill[std::size_t(s)];
        const ResolvedAbility r = resolved(u, s);
        const std::int64_t gap = edge_gap(u, t);
        std::int64_t reach = r.range + r.radius;
        if (d->kind == SkillKind::Cone) reach = r.range;
        if (d->kind == SkillKind::Burst && r.range == 0) reach = r.radius;
        if (gap > reach) return false;
        // lead the target a little
        std::int32_t ax = t.ux(), ay = t.uy();
        if (t.moving) {
            const std::int64_t lead_ms = d->kind == SkillKind::Burst ? d->delay_ms : r.speed > 0 ? gap * 1'000 / r.speed : 150;
            const std::int64_t ms = t.stats.move_speed.total() / 100 * std::min<std::int64_t>(lead_ms, 900) / 1'000;
            ax += std::int32_t(t.face.x * ms / kOne);
            ay += std::int32_t(t.face.y * ms / kOne);
        }
        send({.kind = CmdKind::Cast, .slot = std::uint8_t(s), .target = t.id, .x = ax, .y = ay});
        return true;
    };

    // retreat
    const bool low = hp_pct < 30;
    if (low || u.botm.mode == 1) {
        u.botm.mode = hp_pct > 80 ? 0 : 1;
        if (u.botm.mode == 1) {
            if (enemy) {
                // defensive kit: shields / freeze / heal / blink / wall between us [U-24]
                for (Slot s : {Slot::W, Slot::E, Slot::R, Slot::Y, Slot::F, Slot::G}) {
                    const SkillDef* d = u.skill[std::size_t(s)];
                    if (!d || !can_cast_now(u, s)) continue;
                    if (d->kind == SkillKind::Shield || d->kind == SkillKind::Heal || d->kind == SkillKind::Haste ||
                        d->kind == SkillKind::Flight || (d->kind == SkillKind::SelfFreeze && hp_pct < 18)) {
                        send({.kind = CmdKind::Cast, .slot = std::uint8_t(s), .x = u.ux(), .y = u.uy()});
                        break;
                    }
                    if (d->kind == SkillKind::Blink) {
                        send({.kind = CmdKind::Cast, .slot = std::uint8_t(s), .x = fx_, .y = fy_});
                        break;
                    }
                    if (d->kind == SkillKind::Wall && d->radius == 0) {
                        send({.kind = CmdKind::Cast, .slot = std::uint8_t(s), .x = (u.ux() + enemy->ux()) / 2,
                              .y = (u.uy() + enemy->uy()) / 2});
                        break;
                    }
                }
                if (now_ms >= u.botm.next_path_ms) {
                    u.botm.next_path_ms = now_ms + 800;
                    send({.kind = CmdKind::Move, .x = fx_, .y = fy_});
                }
            } else if (u.order.kind != OrderKind::Recall &&
                       std::ranges::none_of(companions, [&](const Companion& c) { return c.owner == u.id; })) {  // [U-383]
                send({.kind = CmdKind::Recall});
            }
            return;
        }
    }

    // fight
    const bool turret_unsafe = turret && allied_minions_at_turret < 2;
    if (enemy) {
        const int their = int(std::int64_t(enemy->hp) * 100 / std::max(1, enemy->stats.health.total()));
        const bool enemy_under_turret = turret && dist2(enemy->ux(), enemy->uy(), turret->ux(), turret->uy()) < 760LL * 760;
        const bool go = (hp_pct + 10 * (allies_near - enemies_near + 1) >= their || their < 30) &&
                        !(enemy_under_turret && turret_unsafe && their > 20);
        if (go) {
            u.botm.mode = 2;
            for (Slot s : {Slot::Y, Slot::R, Slot::T, Slot::E, Slot::W, Slot::Q, Slot::F, Slot::G}) {  // ultimate (Y) first
                const SkillDef* d = u.skill[std::size_t(s)];
                if (!d || !offensive(d->kind)) continue;
                if (d->kind == SkillKind::Dash && hp_pct < 50) continue;
                if (cast_at(s, *enemy)) return;
            }
            if (hp_pct < 55)
                for (Slot s : {Slot::W, Slot::E, Slot::Y, Slot::F, Slot::G})
                    if (const SkillDef* d = u.skill[std::size_t(s)]; d && (d->kind == SkillKind::Shield || d->kind == SkillKind::Heal) && can_cast_now(u, s))
                        send({.kind = CmdKind::Cast, .slot = std::uint8_t(s), .x = u.ux(), .y = u.uy()});
            // light up the night fight [U-118]
            if (vision_range_pct(sky) < 50)
                for (Slot s : {Slot::F, Slot::G})
                    if (const SkillDef* d = u.skill[std::size_t(s)]; d && (d->kind == SkillKind::Lantern || d->kind == SkillKind::WindSense) && can_cast_now(u, s))
                        send({.kind = CmdKind::Cast, .slot = std::uint8_t(s), .x = enemy->ux(), .y = enemy->uy()});
            if (u.order.kind != OrderKind::Attack || u.order.target != enemy->id)
                send({.kind = CmdKind::Attack, .target = enemy->id});
            return;
        }
        // too strong: back off a bit towards home
        if (ed < 700LL * 700 && now_ms >= u.botm.next_path_ms) {
            u.botm.next_path_ms = now_ms + 600;
            const Dir away = dir_of(std::int64_t(u.ux()) - enemy->ux(), std::int64_t(u.uy()) - enemy->uy());
            send({.kind = CmdKind::Move, .x = u.ux() + away.x * 500 / kOne, .y = u.uy() + away.y * 500 / kOne});
            return;
        }
    }
    u.botm.mode = 0;
    // stay out of turret range without minions
    if (turret && turret_unsafe && dist2(turret->ux(), turret->uy(), u.ux(), u.uy()) < 820LL * 820) {
        if (now_ms >= u.botm.next_path_ms) {
            u.botm.next_path_ms = now_ms + 500;
            const Dir away = dir_of(std::int64_t(u.ux()) - turret->ux(), std::int64_t(u.uy()) - turret->uy());
            send({.kind = CmdKind::Move, .x = u.ux() + away.x * 400 / kOne, .y = u.uy() + away.y * 400 / kOne});
        }
        return;
    }
    // farm
    if (minion) {
        if (u.order.kind != OrderKind::Attack || u.order.target != minion->id) send({.kind = CmdKind::Attack, .target = minion->id});
        // clear groups with zone skills when there is resource to spare
        int group = 0;
        for (const Unit& o : units) group += o.alive && o.kind == UnitKind::Minion && o.team != u.team && dist2(o.ux(), o.uy(), minion->ux(), minion->uy()) < 250LL * 250;
        if (group >= 3 && (u.res_max == 0 || u.res > u.res_max / 2))
            for (Slot s : {Slot::Q, Slot::W})
                if (const SkillDef* d = u.skill[std::size_t(s)]; d && (d->kind == SkillKind::Field || d->kind == SkillKind::Cone || d->kind == SkillKind::Burst || d->kind == SkillKind::Wave))
                    if (cast_at(s, *minion)) break;
        return;
    }
    // push: hit a vulnerable structure when minions are there
    if (turret && !turret_unsafe && vulnerable(*turret)) {
        if (u.order.kind != OrderKind::Attack) send({.kind = CmdKind::Attack, .target = turret->id});
        return;
    }
    for (Unit& o : units)
        if (o.alive && (o.kind == UnitKind::Inhibitor || o.kind == UnitKind::Nexus) && o.team != u.team && vulnerable(o) &&
            dist2(o.ux(), o.uy(), u.ux(), u.uy()) < 900LL * 900) {
            send({.kind = CmdKind::Attack, .target = o.id});
            return;
        }
    // walk to the front of my lane: the allied minion furthest along, else the next lane point
    if (now_ms < u.botm.next_path_ms && u.order.kind == OrderKind::Move) return;
    u.botm.next_path_ms = now_ms + 1'500;
    const auto& lane = lane_path(u.botm.lane);
    const bool fire = u.team == Team::FireNation;
    const Unit* front = nullptr;
    int front_wp = -1;
    for (const Unit& o : units)
        if (o.alive && o.kind == UnitKind::Minion && o.team == u.team && o.lane == u.botm.lane) {
            const int prog = fire ? o.lane_wp : int(lane.size()) - 1 - o.lane_wp;
            if (prog > front_wp) {
                front_wp = prog;
                front = &o;
            }
        }
    std::int32_t tx, ty;
    if (front) {
        tx = front->ux() - (fire ? 150 : -150) / 2;
        ty = front->uy() + (fire ? 150 : -150) / 2;
    } else {
        // wait at my outer turret
        for (const Unit& o : units)
            if (o.alive && o.kind == UnitKind::Turret && o.team == u.team && o.lane == u.botm.lane &&
                (o.tier == 1 || front_wp < 0)) {
                tx = o.ux() + (fire ? 120 : -120);
                ty = o.uy() - (fire ? 120 : -120);
                send({.kind = CmdKind::AttackMove, .x = tx, .y = ty});
                return;
            }
        const CellPt c = fire ? lane[4] : lane[4];
        tx = c.first * kCellUnits;
        ty = c.second * kCellUnits;
    }
    send({.kind = CmdKind::AttackMove, .x = tx, .y = ty});
}

}  // namespace em
