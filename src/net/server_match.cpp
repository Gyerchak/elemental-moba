// [X-01][X-10][U-179][U-211] Match side of the server: start, snapshots (only numbers and results —
// the client draws everything), terrain diffs, final statistics and the way back to the lobby.
#include "core/modifiers.hpp"
#include "net/server.hpp"

#include <enet/enet.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <format>
#include <thread>

namespace em::net {

namespace {
// Bots get a sensible loadout for their class [U-177].
Loadout bot_loadout(int champ) {
    switch (champions()[std::size_t(champ)].role) {
        case Role::Tank: return {.keystone = 1, .tree = 1, .nodes = {0, 0, 0}, .runes = {1, 2, 0}};
        case Role::Fighter: return {.keystone = 4, .tree = 0, .nodes = {1, 0, 0}, .runes = {1, 1, 0}};
        case Role::Support: return {.keystone = 2, .tree = 3, .nodes = {0, 1, 1}, .runes = {0, 0, 1}};
        case Role::Assassin: return {.keystone = 0, .tree = 0, .nodes = {1, 0, 0}, .runes = {1, 1, 0}};
        case Role::Marksman: return {.keystone = 3, .tree = 0, .nodes = {1, 0, 0}, .runes = {2, 1, 0}};
        case Role::Mage: return {.keystone = 0, .tree = 2, .nodes = {0, 1, 0}, .runes = {0, 0, 1}};
    }
    return {};
}
}  // namespace

namespace {
std::uint32_t status_bits(const StatusSet& s) {
    std::uint32_t b = 0;
    for (int i = 0; i < s.size(); ++i)
        if (int(s[i].kind) < 32) b |= 1u << int(s[i].kind);
    return b;
}
}  // namespace

void Server::start_match() {
    MatchConfig mc;
    mc.tick_hz = cfg_.tick_hz;
    mc.practice = cfg_.practice;
    mc.soft_cap = cfg_.soft_cap;
    mc.start_ms = cfg_.start_ms;
    mc.first_wave_ms = cfg_.first_wave_ms;
    mc.seed = std::uint32_t(std::chrono::steady_clock::now().time_since_epoch().count() | 1);
    world_ = std::make_unique<World>(mc);
    // [U-276] skill-based matchmaking: humans are split into two teams of equal strength (snake draft by MMR)
    if (cfg_.ranked && cfg_.matchmaking && !cfg_.practice) {
        std::vector<std::pair<int, Player*>> hs;
        for (Player& p : players_)
            if (p.connected && p.joined) {
                const Rating* rt = ratings_.get(p.slot.name, p.secret);
                hs.push_back({rt ? rt->mmr : 1200, &p});
            }
        if (hs.size() >= 2) {
            std::ranges::sort(hs, std::greater{}, &std::pair<int, Player*>::first);
            for (std::size_t i = 0; i < hs.size(); ++i) hs[i].second->slot.team = std::int8_t((i % 4 == 0 || i % 4 == 3) ? 0 : 1);
        }
    }
    // [U-09] 5v5 default: fill each team up to 5 with bots [X-21]
    int count[2] = {};
    std::vector<bool> taken(champions().size(), false);  // [U-08] a character only once per game
    for (std::size_t c = 0; c < taken.size(); ++c) taken[c] = champion_banned(int(c));  // [U-181] bans
    for (Player& p : players_) {
        if (!p.connected) continue;
        const int team = std::clamp<int>(p.slot.team, 0, 1);
        int champ = p.slot.champ;
        if (champ < 0 || taken[std::size_t(champ)])
            for (int c = 0; c < int(taken.size()); ++c)
                if (!taken[std::size_t(c)]) {
                    champ = c;
                    break;
                }
        taken[std::size_t(champ)] = true;
        p.unit = world_->add_champion(Team(team), champ, p.slot.q, p.slot.f, p.slot.g, cfg_.players_are_bots, p.slot.id,
                                      p.slot.name, p.slot.loadout);
        ++count[team];
    }
    const int per_team = std::max({5, count[0], count[1]});
    for (int team = 0; team < 2; ++team)
        for (; count[team] < per_team; ++count[team]) {
            int champ = 0;
            for (int c = int(taken.size()) - 1; c >= 0; --c)
                if (!taken[std::size_t(c)]) {
                    champ = c;
                    break;
                }
            if (std::ranges::all_of(taken, [](bool b) { return b; })) champ = int(world_->random() % taken.size());
            taken[std::size_t(champ)] = true;
            world_->add_champion(Team(team), champ, -1, -1, -1, true, -1,
                                 std::format("Bot {}", champions()[std::size_t(champ)].name), bot_loadout(champ));
        }
    in_match = true;
    feed_sent_ = 0;
    over_ms_ = -1;
    for (Player& p : players_)
        if (p.connected && p.joined) send_match_start(p);
    send_skins();
    send_lobby();
}

void Server::send_match_start(Player& p) {
    Writer w;
    w.put(Msg::MatchStart);
    w.put(p.unit);
    const Unit* me = world_->find(p.unit);
    w.put<std::uint8_t>(me ? std::uint8_t(me->team) : 0);
    w.put<std::uint32_t>(cfg_.tick_hz);
    w.put<std::int64_t>(cfg_.start_ms);
    w.put<std::uint8_t>(cfg_.practice);
    // champion roster of this match: id, team, champ, name, specials
    std::vector<const Unit*> champs;
    for (const Unit& u : world_->units)
        if (u.kind == UnitKind::Champion) champs.push_back(&u);
    w.put<std::uint8_t>(std::uint8_t(champs.size()));
    for (const Unit* u : champs) {
        w.put(u->id);
        w.put<std::uint8_t>(std::uint8_t(u->team));
        w.put(u->champ);
        w.put(u->q_pick);
        w.put(u->f_pick);
        w.put(u->g_pick);
        w.str(u->name);
    }
    send(p.peer, w, true);
    send_all_cells(p);
}

void Server::send_all_cells(Player& p) {  // reconnect / late start: every cell that differs from the start map
    Grid fresh = make_default_map();
    prepare_match_grid(fresh);
    Writer w;
    w.put(Msg::Cells);
    std::vector<CellChange> diff;
    for (int y = 0; y < kGridSize; ++y)
        for (int x = 0; x < kGridSize; ++x)
            if (fresh.at(x, y) != world_->grid.at(x, y)) diff.push_back({std::int16_t(x), std::int16_t(y), world_->grid.at(x, y).raw()});
    w.put<std::uint32_t>(std::uint32_t(diff.size()));
    for (const CellChange& c : diff) w.put(c);
    send(p.peer, w, true);
}

void Server::send_snapshot(Player& p) {
    const World& W = *world_;
    const Unit* me = W.find(p.unit);
    if (!me) return;
    const Team team = me->team;
    Writer w;
    w.put(Msg::Snapshot);
    w.put<std::uint64_t>(W.tick);
    w.put<std::int64_t>(W.now_ms);
    w.put<std::int32_t>(W.team_kills[0]);
    w.put<std::int32_t>(W.team_kills[1]);
    // units the team can see [X-10][U-140]
    std::vector<NetUnit> us;
    for (const Unit& u : W.units) {
        if (!u.alive && u.kind != UnitKind::Champion && !u.is_structure() && u.kind != UnitKind::Inhibitor) continue;
        if (!u.alive && u.team != team && u.kind == UnitKind::Champion) continue;
        if (u.alive && !W.visible_to(team, u)) continue;
        NetUnit n{};
        n.id = u.id;
        n.kind = std::uint8_t(u.kind);
        n.team = std::uint8_t(u.team);
        n.champ = u.champ;
        n.mtype = std::uint8_t(u.mtype);
        n.body = std::uint8_t(body_index(u.body));
        n.alive = u.alive;
        n.x = u.ux();
        n.y = u.uy();
        n.fx = std::int16_t(std::clamp(u.face.x, -32'767, 32'767));
        n.fy = std::int16_t(std::clamp(u.face.y, -32'767, 32'767));
        n.walk = std::uint16_t(u.walk_phase & 0xffff);
        n.size_pct = std::uint16_t(W.size_pct(u));
        n.sight = std::uint16_t(u.st.has(StatusKind::Blind) ? u.sight * (100 - u.st.value(StatusKind::Blind)) / 100 : u.sight);
        n.hp = u.hp;
        n.hp_max = u.kind == UnitKind::Champion ? u.stats.health.total() : u.stats.health.total();
        n.res = u.res;  // resource bars are visible for everyone (like mana in LoL)
        n.res_max = u.res_max;
        n.level = std::uint16_t(u.level);
        n.status = status_bits(u.st);
        n.casting = u.casting ? std::uint8_t(u.cast.slot + 1) : 0;
        for (std::uint8_t k = 1; k < std::size(kMobNames); ++k)
            if (u.name == kMobNames[k]) n.name_id = k;
        n.shield = 0;
        for (int k = 0; k < u.st.size(); ++k)
            if (u.st[k].kind == StatusKind::Shield) n.shield += u.st[k].value / 100;
        n.shield_sub = u.shield_sub;
        n.atk = std::int16_t(std::min(32'000, u.stats.attack.total() / 100));
        n.def = std::int16_t(std::min(32'000, u.stats.defence.total() / 100));
        n.edef = std::int16_t(std::min(32'000, u.stats.elemental_defence.total() / 100));
        n.range = std::int16_t(std::min(32'000, u.attack_range));
        n.cc_kind = 255;  // [U-193] the CC that lasts longest right now
        std::int64_t cc_left = 0;
        for (int k = 0; k < u.st.size(); ++k)
            if (is_cc(u.st[k].kind) && u.st[k].until_ms - W.now_ms > cc_left) {
                cc_left = u.st[k].until_ms - W.now_ms;
                n.cc_kind = std::uint8_t(u.st[k].kind);
            }
        n.cc_left_cs = std::uint16_t(std::min<std::int64_t>(65'000, cc_left / 10));
        n.emote = u.emote;
        us.push_back(n);
    }
    w.put<std::uint16_t>(std::uint16_t(us.size()));
    for (const NetUnit& n : us) w.put(n);
    // effects in normal vision [U-141]
    std::vector<NetFx> fs;
    for (const Effect& e : W.fx) {
        if (e.dead || !W.visible_to(team, e)) continue;
        if (e.kind == FxKind::Steam && e.radius <= 1) continue;
        NetFx n{};
        n.id = e.id;
        n.seed = e.seed;
        n.kind = std::uint8_t(e.kind);
        n.team = std::uint8_t(e.team);
        n.sub = std::uint8_t(e.sub);
        n.flow = std::uint8_t(e.flow);
        n.x = e.ux();
        n.y = e.uy();
        n.radius = std::int16_t(e.radius);
        n.width = std::int16_t(e.width);
        n.length = std::int16_t(e.length);
        n.dx = std::int16_t(std::clamp(e.dir.x, -32'767, 32'767));
        n.dy = std::int16_t(std::clamp(e.dir.y, -32'767, 32'767));
        n.feeds = std::uint8_t(e.feeds);
        n.combo = std::uint8_t(e.combo);
        n.armed = e.kind != FxKind::Burst || W.now_ms >= e.arm_ms;
        n.ally = e.ally;
        fs.push_back(n);
    }
    w.put<std::uint16_t>(std::uint16_t(fs.size()));
    for (const NetFx& n : fs) w.put(n);
    // lights: team-neutral layer, every team gets all [U-116][U-117]
    // [U-280] lights change at 16 Hz → sent only when they were rebuilt since this player's last snapshot (0xffff = same)
    if (p.lights_sent_tick == W.lights_tick) {
        w.put<std::uint16_t>(0xffff);
    } else {
        p.lights_sent_tick = W.lights_tick;
        const std::size_t nl = std::min<std::size_t>(W.lights.size(), 4'000);
        w.put<std::uint16_t>(std::uint16_t(nl));
        for (std::size_t i = 0; i < nl; ++i) w.put(NetLight{W.lights[i].x, W.lights[i].y, std::int16_t(W.lights[i].radius), W.lights[i].strength, 0});
    }
    // new sense waves for my team (Wind only for its airbender) [U-141..U-143]
    std::vector<NetWave> ws;
    for (const WaveOut& v : W.waves)
        if (v.ms > p.waves_sent_ms && v.team == team && (v.only_for == 0 || v.only_for == p.unit))
            ws.push_back({std::uint8_t(v.w.kind), 0, std::int16_t(std::clamp(v.w.dx, -32'767, 32'767)),
                          std::int16_t(std::clamp(v.w.dy, -32'767, 32'767)), v.w.x, v.w.y, v.ms});
    p.waves_sent_ms = W.now_ms;
    w.put<std::uint16_t>(std::uint16_t(ws.size()));
    for (const NetWave& n : ws) w.put(n);
    // damage I can see (or did / took) → floating numbers [X-103]
    std::vector<NetHit> hs;
    for (const HitEvent& h : W.hits) {
        if (h.ms <= p.hits_sent_ms) continue;
        const Unit* d = W.find(h.dst);
        if (h.src != p.unit && h.dst != p.unit && (!d || !W.visible_to(team, *d))) continue;
        hs.push_back({h.src, h.dst, h.amount, h.is_auto, h.sub, std::int16_t(h.flags), h.ms});
    }
    p.hits_sent_ms = W.now_ms;
    w.put<std::uint16_t>(std::uint16_t(std::min<std::size_t>(hs.size(), 400)));
    for (std::size_t i = 0; i < std::min<std::size_t>(hs.size(), 400); ++i) w.put(hs[i]);
    // [U-203] paths of my allied champions (only cells, max 8 points)
    std::vector<NetPath> ps;
    for (const Unit& u : W.units)
        if (u.alive && u.kind == UnitKind::Champion && u.team == team && u.path_i < u.path.size()) {
            NetPath np{};
            np.unit = u.id;
            for (std::size_t k = u.path_i; k < u.path.size() && np.n < 8; ++k, ++np.n) {
                np.pts[np.n][0] = u.path[k].first;
                np.pts[np.n][1] = u.path[k].second;
            }
            ps.push_back(np);
        }
    w.put<std::uint8_t>(std::uint8_t(ps.size()));
    for (const NetPath& np : ps) w.put(np);
    // [U-235] my team's falcons + ghosts of the reports being replayed now
    std::vector<NetFalcon> fl;
    std::vector<NetGhost> gh;
    for (const FalconRun& f : W.falcons) {
        if (f.team != team) continue;
        fl.push_back({f.x, f.y, f.state, {}});
        if (f.state != 3 || f.frames.empty()) continue;
        const std::int64_t t = W.now_ms - f.replay_from;
        const GhostFrame* best = &f.frames.front();
        for (const GhostFrame& fr : f.frames)
            if (fr.t <= t) best = &fr;
        for (const GhostUnit& g : best->units)
            if (gh.size() < 120) gh.push_back({g.id, g.champ, g.team, g.kind, {}, g.x, g.y});
    }
    w.put<std::uint8_t>(std::uint8_t(std::min<std::size_t>(fl.size(), 40)));
    for (std::size_t i = 0; i < std::min<std::size_t>(fl.size(), 40); ++i) w.put(fl[i]);
    w.put<std::uint8_t>(std::uint8_t(gh.size()));
    for (const NetGhost& g : gh) w.put(g);
    // [U-241] clouds (everyone sees them)
    w.put<std::uint16_t>(std::uint16_t(std::min<std::size_t>(W.clouds.size(), 400)));
    for (std::size_t i = 0; i < std::min<std::size_t>(W.clouds.size(), 400); ++i)
        w.put(NetCloud{W.clouds[i].x, W.clouds[i].y, std::int16_t(W.clouds[i].radius), std::uint8_t(W.clouds[i].kind), 0});
    // [U-288] damaged wall pieces the team can see (cracks on the client)
    {
        std::vector<NetWallHp> wh;
        const VisionMap& vm = W.vision[std::size_t(p.slot.team & 1)];
        for (const auto& [key, wc] : W.walls) {
            if (wc.hp >= wc.max || wh.size() >= 300) continue;
            const int x = int(key % kGridSize), y = int(key / kGridSize);
            if (!vm.cell(x, y)) continue;
            wh.push_back({std::int16_t(x), std::int16_t(y), std::uint8_t(std::clamp(wc.hp * 100 / std::max(1, wc.max), 1, 99)), 0});
        }
        w.put<std::uint16_t>(std::uint16_t(wh.size()));
        for (const NetWallHp& n : wh) w.put(n);
        std::vector<NetWallBreak> wb;  // [U-341] pieces that fell in the last second, where the team sees
        for (const WallBreak& b : W.wall_breaks)
            if (vm.cell(b.x, b.y) && wb.size() < 200) wb.push_back({b.x, b.y, b.dx, b.dy, b.kind, {}, b.ms});
        w.put<std::uint16_t>(std::uint16_t(wb.size()));
        for (const NetWallBreak& n : wb) w.put(n);
    }
    // [U-246] pings of my team (the last 5 s; the client shows each once)
    std::vector<NetPing> pg;
    for (const PingEvent& pe : W.pings)
        if (pe.team == team) pg.push_back({std::uint8_t(pe.kind), {}, pe.from, pe.x, pe.y, pe.ms});
    w.put<std::uint8_t>(std::uint8_t(std::min<std::size_t>(pg.size(), 60)));
    for (std::size_t i = 0; i < std::min<std::size_t>(pg.size(), 60); ++i) w.put(pg[i]);
    // scoreboard (everyone, public info)
    std::vector<NetScore> sc;
    for (const Unit& u : W.units)
        if (u.kind == UnitKind::Champion) {
            NetScore s{};
            s.id = u.id;
            s.kills = std::uint16_t(u.kills);
            s.deaths = std::uint16_t(u.deaths);
            s.assists = std::uint16_t(u.assists);
            s.cs = std::uint16_t(u.cs);
            s.level = std::uint16_t(u.level);
            s.alive = u.alive;
            s.respawn_ms = u.alive ? 0 : std::int32_t(u.respawn_ms - W.now_ms);
            for (int i = 0; i < kItemSlots; ++i) s.items[i] = u.items[std::size_t(i)];
            for (int i = 0; i < kSkillSlots; ++i) {  // [U-289] nine skill slots
                s.rank[i] = u.ab[std::size_t(i)].rank;
                s.cd_left[i] = -1;
                if (u.skill[std::size_t(i)]) {
                    const ResolvedAbility ra = W.resolved(u, Slot(i));
                    s.cd_total[i] = std::int32_t(ra.cooldown_ms);
                    s.dmg[i] = std::int32_t(ra.damage);
                    if (u.team == team) s.cd_left[i] = std::int32_t(std::max<std::int64_t>(0, u.ab[std::size_t(i)].cd_end_ms - W.now_ms));
                }
            }
            s.attack = std::int16_t(u.stats.attack.total() / 100);
            s.ep = std::int16_t(u.stats.elemental_power.total() / 100);
            s.def = std::int16_t(u.stats.defence.total() / 100);
            s.edef = std::int16_t(u.stats.elemental_defence.total() / 100);
            s.ms = std::int16_t(u.stats.move_speed.total() / 100);
            s.as100x = std::int16_t(100'000 / std::max(1, W.attack_period_ms(u)));
            s.range = std::int16_t(u.attack_range);
            s.size = std::int16_t(W.size_pct(u));
            sc.push_back(s);
        }
    w.put<std::uint8_t>(std::uint8_t(sc.size()));
    for (const NetScore& s : sc) w.put(s);
    {  // [U-365] camp timers
        std::vector<NetCamp> cp;
        for (const Unit& u : W.units)
            if ((u.kind == UnitKind::Monster || u.kind == UnitKind::Boss) && cp.size() < 250)
                cp.push_back({std::int16_t(u.home_x / kSub / kCellUnits), std::int16_t(u.home_y / kSub / kCellUnits), World::mob_name_id(u),
                              std::uint8_t(u.kind == UnitKind::Boss),
                              std::uint16_t(u.alive ? 0 : std::clamp<std::int64_t>((u.respawn_ms - W.now_ms + 999) / 1000, 1, 65'000))});
        w.put<std::uint8_t>(std::uint8_t(cp.size()));
        for (const NetCamp& c : cp) w.put(c);
    }
    // me
    NetMe m{};
    m.unit = me->id;
    m.gold = me->gold;
    m.skill_points = me->skill_points;
    m.xp = me->xp;
    m.xp_this = me->xp - total_xp_to(std::uint32_t(me->level));
    m.xp_next = xp_to_next(std::uint32_t(me->level));
    for (int i = 0; i < kSkillSlots; ++i) {
        const AbilityRt& a = me->ab[std::size_t(i)];
        m.rank[i] = a.rank;
        m.cd_left_ms[i] = std::int32_t(std::max<std::int64_t>(0, a.cd_end_ms - W.now_ms));
        m.cd_total_ms[i] = me->skill[std::size_t(i)] ? std::int32_t(W.resolved(*me, Slot(i)).cooldown_ms) : 0;
        m.recast[i] = a.mc.casts_used;
        if (me->skill[std::size_t(i)]) {
            const ResolvedAbility ra = W.resolved(*me, Slot(i));
            m.s_range[i] = std::int32_t(ra.range);
            m.s_width[i] = std::int32_t(ra.width);
            m.s_radius[i] = std::int32_t(ra.radius);
            m.s_cost[i] = std::int32_t(ra.cost);
            m.s_dmg[i] = std::int32_t(ra.damage);
            m.s_cast_ms[i] = std::int32_t(ra.cast_time_ms);
            m.s_speed[i] = std::int32_t(ra.speed);
            m.s_length[i] = std::int32_t(ra.length);
        }
    }
    m.res = me->res;
    m.res_max = me->res_max;
    m.auto_attack = me->auto_attack;
    m.loadout = me->loadout;
    m.keystone_impact = std::int32_t(std::min<std::int64_t>(me->stat_keystone, 2'000'000'000));
    m.ep_raw = me->stats.elemental_power.total();
    m.attack_raw = me->stats.attack.total();
    m.bonus_attack_raw = me->stats.attack.bonus;
    m.cast_speed = me->stats.cast_speed;
    if (me->champ >= 0) {
        const SkyBoost sb = sky_boost(champions()[std::size_t(me->champ)].element, W.sky);
        m.power_pct = sb.power_pm();  // [U-370] sent in per-mille (the moon adds 0.5 % steps)
        m.jbuff = me->jbuff_until > W.now_ms ? me->jbuff : 0;  // [U-366]
        m.jbuff_ms = std::int32_t(std::max<std::int64_t>(0, me->jbuff_until - W.now_ms));
        m.water = me->water, m.water_max = me->water_max;  // [U-339]
        m.lion_stacks[0] = std::int16_t(W.lion_stacks[0]), m.lion_stacks[1] = std::int16_t(W.lion_stacks[1]);
        m.size_pct = sb.size_pct;
    }
    for (int k = 0; k < me->st.size() && m.nstatus < 20; ++k) {
        const Status& st = me->st[k];
        m.status[m.nstatus++] = {std::uint8_t(st.kind), 0, std::int16_t(std::clamp(st.value / (st.kind == StatusKind::Shield || st.kind == StatusKind::Burn ? 100 : 1), -32'000, 32'000)),
                                 std::int32_t(std::max<std::int64_t>(0, st.until_ms - W.now_ms))};
    }
    // [U-255] keystone / item / ability buffs that are not statuses
    auto extra = [&](ModKind k, std::int64_t until, std::int32_t value = 0) {
        if (until > W.now_ms && m.nstatus < 20)
            m.status[m.nstatus++] = {std::uint8_t(kModWireBase + std::uint8_t(k)), 0, std::int16_t(std::clamp(value, -32'000, 32'000)),
                                     std::int32_t(std::min<std::int64_t>(until - W.now_ms, 2'000'000'000))};
    };
    if (me->surge_count > 0) extra(ModKind::SurgeStacks, me->surge_ms + 3'000, me->surge_count);
    extra(ModKind::StoneheartCd, me->stoneheart_ready_ms);
    extra(ModKind::ChiRush, me->chirush_until);
    extra(ModKind::Lotus, me->lotus_until);
    extra(ModKind::Flask, me->flask_until);
    extra(ModKind::Untargetable, me->untargetable_until);
    extra(ModKind::WindSense, me->wind_sense_until);
    extra(ModKind::Bigger, me->tmp_size_until, me->tmp_size / 100);
    extra(ModKind::SkillStolen, me->stolen_until, me->stolen_slot);
    extra(ModKind::AvatarState, me->avatar_until);
    extra(ModKind::MagicStance, me->magic_until, me->magic_sub);
    if (me->source_ice) extra(ModKind::SourceIce, W.now_ms + 1'000'000);
    if (me->cycle_ready || me->cycle_mask)  // no timer: shown while it counts
        extra(ModKind::Cycle, W.now_ms + 1'000'000, me->cycle_ready ? 4 : std::popcount(unsigned(me->cycle_mask)));
    for (int i = 0; i < kItemSlots; ++i) {
        m.items[i] = me->items[std::size_t(i)];
        m.item_impact[i] = std::int32_t(std::min<std::int64_t>(me->item_impact[std::size_t(i)], 2'000'000'000));
        m.item_cd_ms[i] = std::int32_t(std::max<std::int64_t>(0, me->item_cd_ms[std::size_t(i)] - W.now_ms));
    }
    m.cast_left_ms = me->casting ? std::int32_t(std::max<std::int64_t>(0, me->cast_end_ms - W.now_ms)) : 0;
    m.cast_total_ms = me->casting ? std::int32_t(W.resolved(*me, Slot(me->cast.slot)).cast_time_ms) : 0;
    m.channel_left_ms = me->channel_end_ms ? std::int32_t(me->channel_end_ms - W.now_ms) : 0;
    m.respawn_ms = me->alive ? 0 : std::int32_t(me->respawn_ms - W.now_ms);
    m.wind_sense_ms = std::int32_t(std::max<std::int64_t>(0, me->wind_sense_until - W.now_ms));
    m.stolen_ms = std::int32_t(std::max<std::int64_t>(0, me->stolen_until - W.now_ms));
    m.stolen_slot = me->stolen_slot;
    m.stance_b = me->stance_b;
    m.no_cd = me->no_cooldowns;
    m.can_shop = W.in_base(*me) || !me->alive;
    m.attack_range = me->attack_range;
    m.move_speed = me->stats.move_speed.total() / 100;
    m.attack = me->stats.attack.total() / 100;
    m.ep = me->stats.elemental_power.total() / 100;
    m.def = me->stats.defence.total() / 100;
    m.edef = me->stats.elemental_defence.total() / 100;
    m.as100x = 100'000 / std::max(1, W.attack_period_ms(*me));
    m.haste = me->stats.elemental_haste / 100;
    m.tenacity = me->stats.tenacity / 100;
    m.model_size = W.size_pct(*me);
    w.put(m);
    // my gold / XP rewards [U-159]
    std::vector<NetReward> rw;
    for (const RewardEvent& r : W.rewards)
        if (r.to == me->id && r.ms > p.rewards_sent_ms) rw.push_back({r.x, r.y, r.gold, r.xp, r.ms});
    p.rewards_sent_ms = W.now_ms;
    w.put<std::uint16_t>(std::uint16_t(rw.size()));
    for (const NetReward& r : rw) w.put(r);
    send(p.peer, w, false);
    if (me->fail_ms > p.fail_sent_ms) {  // why my last order did nothing [X-103]
        p.fail_sent_ms = me->fail_ms;
        Writer n;
        n.put(Msg::Notice);
        n.str(me->fail);
        send(p.peer, n, true);
    }
}

// ---------------------------------------------------------------- skins [U-200]
// Every uploaded skin goes to every player who allows other players' skins (Minecraft-like).
void Server::send_skins() {
    for (const Player& from : players_) {
        if (!from.connected || !from.joined || from.skin.empty() || !from.unit) continue;
        Writer w;
        w.put(Msg::SkinData);
        w.put(from.unit);
        w.put<std::uint32_t>(std::uint32_t(from.skin.size()));
        for (std::uint8_t b : from.skin) w.put(b);
        for (Player& to : players_)
            if (to.connected && to.joined && (to.allow_skins || &to == &from)) send(to.peer, w, true);
    }
}

// ---------------------------------------------------------------- end of the match [U-179]
void Server::finish_match() {
    if (over_ms_ < 0) {
        over_ms_ = world_->now_ms;
        Writer w;
        w.put(Msg::GameOver);
        w.put<std::uint8_t>(std::uint8_t(world_->winner));
        broadcast(w, true);
        // [U-276] ranking: every human player's MMR + LP (bots count as 1200; practice / unranked: nothing)
        std::vector<Ratings::Result> rated;
        const bool ranked = cfg_.ranked && !cfg_.practice && (world_->winner == Team::FireNation || world_->winner == Team::EarthNation);
        if (ranked) {
            for (const Unit& u : world_->units) {
                if (u.kind != UnitKind::Champion) continue;
                Ratings::Result res{u.name, 0, int(u.team), true, u.id};
                for (const Player& p : players_)
                    if (p.unit == u.id && p.joined && !cfg_.players_are_bots) res = {p.slot.name, p.secret, int(u.team), false, u.id};
                rated.push_back(res);
            }
            ratings_.apply(rated, int(world_->winner));
        }
        Writer f;
        f.put(Msg::FinalStats);
        std::vector<FinalRow> rows;
        for (const Unit& u : world_->units)
            if (u.kind == UnitKind::Champion) {
                FinalRow r{};
                r.unit = u.id;
                r.team = std::uint8_t(u.team);
                r.champ = u.champ;
                r.keystone = u.loadout.keystone;
                r.level = std::uint16_t(u.level);
                r.kills = std::uint16_t(u.kills);
                r.deaths = std::uint16_t(u.deaths);
                r.assists = std::uint16_t(u.assists);
                r.cs = std::uint16_t(u.cs);
                r.gold = std::int32_t(u.stat_gold);
                r.dmg_champs = std::int32_t(u.stat_dmg_champs);
                r.dmg_taken = std::int32_t(u.stat_dmg_taken);
                r.healing = std::int32_t(u.stat_healing);
                r.shielding = std::int32_t(u.stat_shielding);
                r.keystone_impact = std::int32_t(u.stat_keystone);
                for (int i = 0; i < kItemSlots; ++i) r.items[i] = u.items[std::size_t(i)];
                r.lp_after = -1;
                for (const auto& rr : rated)
                    if (rr.unit == u.id && !rr.bot && rr.lp_delta != 0) {
                        r.lp_delta = std::int16_t(rr.lp_delta);
                        if (const Rating* rt = ratings_.get(rr.name, rr.secret)) r.lp_after = rt->lp;
                    }
                rows.push_back(r);
            }
        f.put<std::uint8_t>(std::uint8_t(rows.size()));
        for (const FinalRow& r : rows) f.put(r);
        broadcast(f, true);
        return;
    }
    // 5 seconds for everyone to see the result, then everyone is back in the lobby
    if (++over_ticks_ < std::int64_t(cfg_.tick_hz) * 5) return;
    over_ticks_ = 0;
    world_.reset();
    in_match = false;
    for (Player& p : players_) {
        p.unit = 0;
        p.slot.ready = false;
        p.slot.ban = -1;
        if (!p.connected) p = Player{};
    }
    phase_ = cfg_.bans ? DraftPhase::Banning : DraftPhase::Picking;
    send_lobby();
}

}  // namespace em::net
