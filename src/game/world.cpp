// [X-05] World: setup, commands, the fixed tick, shared queries.
#include <chrono>
#include "core/surface_rules.hpp"
#include "game/world.hpp"
#include "game/mobs/monsters.hpp"
#include "game/mechanics/element_rules.hpp"

#include <algorithm>
#include <format>

#include "game/match/layout.hpp"
#include "game/match/maps.hpp"

namespace em {

std::int32_t isqrt32(std::int64_t v) { return std::int32_t(isqrt(v)); }

World::World(MatchConfig c) : cfg(c), grid((set_map(c.map), cur_map().make())), rng(c.seed ? c.seed : 1) {  // [U-420] the chosen map
    prepare_match_grid(grid);
    sky = WorldClock::at_ms(cfg.start_ms);
    next_wave_ms = std::min(cfg.first_wave_ms, cur_map().minions.first_ms);  // [U-420] the map's first wave (a test may start earlier)
    setup_match();
    rebuild_collision_maps();
}

std::uint32_t World::random() {  // xorshift32, deterministic [X-04]
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

Unit& World::spawn_unit(Unit u) {
    u.id = next_id++;
    index[u.id] = units.size();
    units.push_back(std::move(u));
    return units.back();
}

Effect& World::spawn_fx(Effect e) {
    e.id = next_id++;
    e.seed = e.id * 2'654'435'761u;
    e.born_ms = now_ms;
    e.travel_total = e.travel_left;
    if (const Unit* o = find(e.owner)) e.base_height = ground_height(o->ux(), o->uy());  // [U-216]
    fx.push_back(e);
    return fx.back();
}

Unit* World::find(std::uint32_t id) {
    const auto it = index.find(id);
    return it == index.end() ? nullptr : &units[it->second];
}
const Unit* World::find(std::uint32_t id) const {
    const auto it = index.find(id);
    return it == index.end() ? nullptr : &units[it->second];
}

void World::set_cell(int cx, int cy, Cell c) {
    if (!Grid::in_bounds(cx, cy) || grid.at(cx, cy) == c) return;
    grid.at(cx, cy) = c;
    clearance_dirty = true;
    cell_changes.push_back({std::int16_t(cx), std::int16_t(cy), c.raw(), std::uint8_t(cell_public_)});
    if (c.wall() == Wall::None) walls.erase(std::uint32_t(cy) * kGridSize + std::uint32_t(cx));  // [U-288]
}

void World::dry(Unit& u) { u.st.remove(StatusKind::Wet); }  // [U-241]

void World::refuse(Unit& u, std::string why) {
    u.fail = std::move(why);
    u.fail_ms = now_ms;
}

void World::add_feed(std::string text) {
    feed.push_back({now_ms, std::move(text)});
}

UnitCategory Unit::category() const {  // [U-311]
    switch (kind) {
        case UnitKind::Champion: return UnitCategory::Champion;
        case UnitKind::Minion: return mtype == MinionType::Siege ? UnitCategory::SiegeMinion : mtype == MinionType::Super ? UnitCategory::SuperMinion
                                                                                                                       : UnitCategory::Minion;
        case UnitKind::Monster: return UnitCategory::Monster;
        case UnitKind::Boss: return UnitCategory::EpicMonster;
        case UnitKind::Turret: return UnitCategory::Turret;
        case UnitKind::Inhibitor: return UnitCategory::Inhibitor;
        case UnitKind::Nexus: return UnitCategory::Shrine;
    }
    return UnitCategory::Monster;
}

std::uint8_t World::mob_name_id(const Unit& u) {
    for (std::uint8_t k = 1; k < std::size(kMobNameList); ++k)
        if (u.name == kMobNameList[k]) return k;
    return 0;
}

std::int32_t World::size_pct(const Unit& u) const { return model_size_pct(u.stats); }

Shape World::hitbox(const Unit& u) const {
    return place_body(*u.body, u.ux(), u.uy(), u.face, u.walk_phase, size_pct(u));
}

Shape fx_shape(FxKind kind, std::int32_t ux, std::int32_t uy, std::int32_t radius, std::int32_t width,
               std::int32_t length, Dir dir, Flow flow, std::uint32_t seed, std::uint32_t t) {
    switch (kind) {
        case FxKind::Bolt:
        case FxKind::Auto:
        case FxKind::TowerShot:
        case FxKind::Boulder:
        case FxKind::Lantern:
        case FxKind::Burst:
        case FxKind::Field:
        case FxKind::Tornado:
        case FxKind::Heal:
        case FxKind::Steam:
            return fluid_blob(ux, uy, radius, flow, seed, t);
        case FxKind::Wave: {  // wide front, across the direction of travel
            const Dir side{-dir.y, dir.x};
            const std::int32_t half = width / 2;
            return fluid_line(ux - side.x * half / kOne, uy - side.y * half / kOne, side, width, std::max(40, width / 4),
                              flow, seed, t);
        }
        case FxKind::Cone:
            return fluid_cone(ux, uy, dir, length, width, flow, seed, t);
    }
    return {};
}

Shape World::shape_of(const Effect& e) const {
    return fx_shape(e.kind, e.ux(), e.uy(), e.radius, e.width, e.length, e.dir, e.flow, e.seed, std::uint32_t(tick));
}

namespace {
constexpr const BodyDef* kBodies[] = {&kHumanoidBody, &kSlimBody, &kBroadBody, &kMinionBody,
                                      &kBeastBody,    &kTowerBody, &kNexusBody};
}
const BodyDef& body_by_index(int i) { return *kBodies[std::size_t(std::clamp(i, 0, int(std::size(kBodies)) - 1))]; }
int body_index(const BodyDef* b) {
    for (int i = 0; i < int(std::size(kBodies)); ++i)
        if (kBodies[i] == b) return i;
    return 0;
}

bool World::targetable(const Unit& u) const { return u.alive && u.untargetable_until <= now_ms; }

bool World::vulnerable(const Unit& u) const {
    if (!u.is_structure()) return true;
    auto dead_struct = [&](UnitKind k, int lane, int tier) {
        for (const Unit& o : units)
            if (o.team == u.team && o.kind == k && (lane < 0 || o.lane == lane) && (tier < 0 || o.tier == tier) &&
                o.alive)
                return false;
        return true;
    };
    if (u.kind == UnitKind::Turret) {
        if (u.tier == 1) return true;
        if (u.tier <= 3) return dead_struct(UnitKind::Turret, u.lane, u.tier - 1);
        // nexus turrets: any inhibitor down
        for (const Unit& o : units)
            if (o.team == u.team && o.kind == UnitKind::Inhibitor && !o.alive) return true;
        return false;
    }
    if (u.kind == UnitKind::Inhibitor) return dead_struct(UnitKind::Turret, u.lane, 3);
    if (u.kind == UnitKind::Nexus) return dead_struct(UnitKind::Turret, -1, 4);
    return true;
}

std::int64_t World::edge_gap(const Unit& a, const Unit& b) const { return em::edge_gap(hitbox(a), hitbox(b)); }

bool World::visible_to(Team t, const Unit& u) const {
    if (u.team == t || u.is_structure()) return true;
    if (!u.alive || t == Team::Neutral) return false;
    const VisionMap& v = vision[std::size_t(t)];
    // [U-140] one sample point of the hitbox on a seen cell = seen
    const Shape s = hitbox(u);
    for (int i = 0; i < s.n; ++i) {
        const Circle& c = s.c[i];
        if (v.at_units(c.x, c.y)) return true;
        for (int k = 0; k < 64; k += 8)
            if (v.at_units(c.x + kDirs[k].x * c.r / kOne, c.y + kDirs[k].y * c.r / kOne)) return true;
    }
    return false;
}

bool World::visible_to(Team t, const Effect& e) const {
    if (e.team == t) return true;
    if (t == Team::Neutral) return false;
    const Shape s = shape_of(e);
    for (int i = 0; i < s.n; ++i)
        if (vision[std::size_t(t)].at_units(s.c[i].x, s.c[i].y)) return true;  // [U-141] normal vision
    return false;
}

ResolvedAbility World::resolved(const Unit& u, Slot s) const {
    const SkillDef* d = u.skill[std::size_t(s)];
    if (!d) return {};
    SkyBoost b = sky_boost(base_element(skill_sub(u, *d)), sky);  // [U-123][U-264]
    if (u.avatar_until > now_ms) {  // [U-279] Avatar State
        b.power_pct += 50;
        b.size_pct += 25, b.width_pct += 25, b.radius_pct += 25;
    }
    if (u.team != Team::Neutral) b.power_pct += lion_stacks[std::size_t(u.team) & 1] * kLionTurtlePctPerStack;  // [U-364]
    if (u.jbuff == std::uint8_t(JungleBuff::Wisdom) && u.jbuff_until > now_ms) b.cd_pct = b.cd_pct * kWisdomCdPct / 100;  // [U-366]
    const int extra = has_passive(u, PassiveId::MistForm) ? 10 : 0;
    const std::uint32_t rank = std::max<std::uint32_t>(1, u.ab[std::size_t(s)].rank);
    return resolve(to_ability(*d, s), ScaleCtx{.stats = u.stats, .rank = rank, .level = std::uint32_t(u.level),
                                               .power_pct = b.power_pct, .size_pct = b.size_pct + extra,
                                               .width_pct = b.width_pct + extra, .radius_pct = b.radius_pct + extra,
                                               .speed_pct = b.speed_pct + extra, .dur_pct = b.dur_pct, .dot_pct = b.dot_pct,
                                               .power_pm = b.moon_pm, .cd_pct = b.cd_pct});
}

// ---------------------------------------------------------------- water + elevation [U-188..U-191]
bool World::on_deep_water(const Unit& u) const {
    const int cx = u.ux() / kCellUnits, cy = u.uy() / kCellUnits;
    return Grid::in_bounds(cx, cy) && grid.at(cx, cy).surface() == Surface::DeepWater && !grid.at(cx, cy).temp_walkable();
}
bool World::on_ground_water(const Unit& u) const {
    const int cx = u.ux() / kCellUnits, cy = u.uy() / kCellUnits;
    if (Grid::in_bounds(cx, cy)) {
        const Cell c = grid.at(cx, cy);
        if (c.surface() == Surface::Water && c.wall() == Wall::None) return true;  // a river / pond (ice walls on it: frozen)
    }
    for (const Cloud& p : clouds)
        if (p.kind == CloudKind::Puddle && dist2(u.ux(), u.uy(), p.x, p.y) <= std::int64_t(p.radius) * p.radius) return true;
    return false;
}
bool World::on_any_water(const Unit& u) const {
    const int cx = u.ux() / kCellUnits, cy = u.uy() / kCellUnits;
    if (!Grid::in_bounds(cx, cy)) return false;
    const Surface s = grid.at(cx, cy).surface();
    return s == Surface::DeepWater || s == Surface::Water;
}
bool World::elevated(const Unit& u) const { return u.flying() || u.st.has(StatusKind::WaterRide); }
// Melee and ground skills can't reach flying / water-raised units; projectiles, waves, blasts can [U-191].
bool World::can_hit_elevated(const Effect& e) const {
    if (e.kind == FxKind::Auto) return true;  // ranged autos fly
    if (!e.def) return true;
    if (e.def->melee || e.def->ground) return false;
    switch (e.def->kind) {
        case SkillKind::Dash: case SkillKind::Pillar: return false;  // melee rush / ground raise
        case SkillKind::Field: return base_element(e.sub) != Element::Earth;  // quakes, spikes, lava = ground
        default: return true;
    }
}

// ---------------------------------------------------------------- heights [U-216]
inline constexpr std::int32_t kLevelHeight = 40, kAirborneHeight = 60, kWaterRideHeight = 80, kFlyingHeight = 120;
std::int32_t World::ground_height(std::int32_t ux, std::int32_t uy) const {
    const int cx = ux / kCellUnits, cy = uy / kCellUnits;
    return Grid::in_bounds(cx, cy) ? grid.at(cx, cy).effective_elevation() * kLevelHeight : 0;
}
std::int32_t World::unit_height(const Unit& u) const {
    std::int32_t h = ground_height(u.ux(), u.uy());
    if (u.flying()) h += u.fly_height;  // [U-224] Sky Ascension can fly higher
    else if (u.st.has(StatusKind::WaterRide)) h += kWaterRideHeight;
    else if (u.st.has(StatusKind::Airborne)) h += kAirborneHeight;
    return h;
}
std::int32_t World::effect_reach(const Effect& e) const {
    if (e.def && e.def->reach_height >= 0) return e.base_height + e.def->reach_height;
    if (e.def && e.def->kind == SkillKind::Pillar) return e.base_height + 200;  // it lifts the ground it hits
    return e.base_height + (can_hit_elevated(e) ? 160 : 40);
}
bool World::reaches(const Effect& e, const Unit& t) const { return unit_height(t) <= effect_reach(e); }

PassiveId World::special_passive(const Unit& u) const {
    for (const SkillDef* d : {u.skill[std::size_t(Slot::F)], u.skill[std::size_t(Slot::G)]})
        if (d && d->kind == SkillKind::Passive) return d->passive;
    return PassiveId::None;
}
bool World::has_passive(const Unit& u, PassiveId p) const {
    // passive skills (P, passive specials) always; [U-291] an active skill's own passive once the skill is learned
    for (std::size_t s = 0; s < u.skill.size(); ++s) {
        const SkillDef* d = u.skill[s];
        if (!d || d->passive != p || p == PassiveId::None) continue;
        if (d->kind == SkillKind::Passive || u.ab[s].rank > 0) return true;
    }
    return false;
}

std::pair<std::int32_t, std::int32_t> World::fountain(Team t) const {
    const auto [cx, cy] = t == Team::FireNation ? cur_map().fountain : mirror(cur_map().fountain);
    return {cx * kCellUnits + kCellUnits / 2, cy * kCellUnits + kCellUnits / 2};
}

// [X-100] the shop stands at the fountain and has a range (buy only inside it)
bool World::in_base(const Unit& u) const {
    if (u.team == Team::Neutral) return false;
    const auto [fx_, fy_] = fountain(u.team);
    return dist2(u.ux(), u.uy(), fx_, fy_) <= std::int64_t(kShopRange) * kShopRange;
}

std::int32_t World::attack_period_ms(const Unit& u) const {
    std::int64_t bonus = u.stats.attack_speed.bonus;
    if (u.kind == UnitKind::Champion && champions()[std::size_t(u.champ)].element != Element::None)
        bonus += u.stats.elemental_haste * kEAutoHastePct / 100;  // [U-55]
    const std::int64_t per_s100 = std::max<std::int64_t>(20, u.as100 * (10'000 + bonus) / 10'000);
    return std::int32_t(std::max<std::int64_t>(200, 100'000 / std::min<std::int64_t>(per_s100, 500)));
}

bool World::can_cast_now(const Unit& u, Slot s, std::string* why, std::int32_t sx, std::int32_t sy) const {
    auto no = [&](const char* w) {
        if (why) *why = w;
        return false;
    };
    const SkillDef* d = u.skill[std::size_t(s)];
    if (!u.alive) return no("dead");
    if (u.riding) return no("riding home");  // [U-407]
    if (!d || d->kind == SkillKind::Passive || d->kind == SkillKind::None) return no("no active");
    const AbilityRt& a = u.ab[std::size_t(s)];
    if (a.rank == 0) return no("not learned");
    if (!u.st.can_cast()) return no("can't cast (CC)");
    if (u.stolen_until > now_ms && u.stolen_slot == std::uint8_t(s)) return no("stolen by the Face Stealer");
    const Element el = base_element(d->sub);
    if (el != Element::None && sky_boost(el, sky).muted) return no("muted by the eclipse");  // [U-125]
    if (d->full_moon_only && !bloodbending_allowed(sky)) return no("only at full moon");    // [U-78]
    if (d->targeted && u.st.has(StatusKind::Blind)) return no("blinded: no targeted skills");  // [U-160]
    if (u.flying()) {  // [U-249] lifted into the air: only skills allowed for this kind of lift
        const bool melee_like = d->melee || d->kind == SkillKind::Dash;
        const std::uint8_t allowed = d->air_use != 0xff ? d->air_use : melee_like ? 0 : (kAirSelf | kAirAlly | kAirEnemy);
        if (!(allowed & (u.lifted_by ? u.lifted_by : kAirSelf))) return no("can't use this in the air");
    }
    if (!u.flying()) {  // [U-269] earthbending needs earth under the feet (not wood, glacier, deep water…)
        const int cx = u.ux() / kCellUnits, cy = u.uy() / kCellUnits;
        if (Grid::in_bounds(cx, cy) && !can_bend_on(d->sub, grid.at(cx, cy).surface())) return no("no earth to bend here");
    }
    // [U-294][U-339] water as numbers: a skill bent from a source costs no carried water
    const bool water_free = water_skill(*d) && water_sourced(u, *d, sx, sy);
    if (water_skill(*d) && u.water_max > 0 && !water_free && u.water < elements::kAvatarWaterCost)
        return no("not enough water: bend it from a source (river, sea, pole ice, a puddle, an ice wall)");
    if (u.st.has(StatusKind::Swimming) && d->kind != SkillKind::WaterSpout && d->kind != SkillKind::IcePath)
        return no("swimming: no spells (only water mobility skills)");  // [U-188]
    const bool recast = d->casts > 1 && a.mc.casts_used > 0 && a.mc.casts_used < d->casts && now_ms <= a.mc.window_end_ms;
    if (!recast && a.cd_end_ms > now_ms && !u.no_cooldowns) return no("cooldown");
    const std::int64_t cost = water_free && u.stats.resource == ResourceKind::Water ? 0 : resolved(u, s).cost * 100;
    if (u.stats.resource == ResourceKind::Water && cost > u.res_max)
        return no("costs more water than you carry: bend it from a source (river, sea, pole ice, a puddle, an ice wall)");
    const bool free_fuel = u.stats.resource == ResourceKind::Fuel && unlimited_fuel(sky);  // [U-138]
    // [U-268] calm flame: what the fuel lacks is paid with life (HP ×2, never the last 1 HP); rage never blocks
    const std::int64_t life = u.stats.resource == ResourceKind::Fuel ? std::max<std::int64_t>(0, u.hp - 100) / kLifePerFuel : 0;
    if (cost > 0 && !free_fuel && u.stats.resource != ResourceKind::Rage && u.res + life < cost) return no("not enough resource");
    return true;
}

void World::recompute_stats(Unit& u) {
    if (u.kind != UnitKind::Champion) return;
    const ChampionDef& d = champions()[std::size_t(u.champ)];
    const std::int64_t gl = growth_levels(std::uint32_t(u.level), cfg.soft_cap);  // [X-45]
    const std::int32_t old_max = u.stats.health.total();
    Stats s;
    s.health.base = std::int32_t(d.hp + d.hp_lvl * gl / 100);
    s.attack.base = std::int32_t(d.attack + d.attack_lvl * gl / 100);
    s.elemental_power.base = std::int32_t(d.ep_lvl * gl / 100);
    s.defence.base = std::int32_t(d.def + d.def_lvl * gl / 100);
    s.elemental_defence.base = std::int32_t(d.edef + d.edef_lvl * gl / 100);
    s.attack_speed.base = d.as;
    s.attack_speed.bonus = std::int32_t(d.as_lvl * gl / 100);
    s.move_speed.base = d.ms;
    s.turn_rate.base = 400;  // [U-74][X-88] 4 turns / s: faster than Dota, not instant like LoL
    s.range = d.range;
    s.resource = d.resource;
    u.collide = kCollDefault;
    // [U-367] every champion body-blocks; the Mist Dancer's mist form walks through units (Claude idea)
    for (std::int16_t it : u.items)
        if (it >= 0) {
            add_item(s, kItems[std::size_t(it)]);
            if (kItems[std::size_t(it)].ghost) u.collide &= std::uint8_t(~kCollUnit);  // [U-367]
        }
    if (has_passive(u, PassiveId::MistForm)) u.collide &= std::uint8_t(~kCollUnit);
    if (has_passive(u, PassiveId::Rooted)) s.tenacity += 2'000;
    if (has_passive(u, PassiveId::MetalSkin)) s.defence.bonus += 1'500;
    if (has_passive(u, PassiveId::LightFeet)) s.move_speed.bonus += 1'500;
    apply_loadout(s, u.loadout);                                   // [U-177] runes + specialisation
    if (u.lotus_until > now_ms) s.cast_speed += kLotusCastSpeed;  // [X-46][U-114]
    if (u.tmp_size_until > now_ms) s.model_size += u.tmp_size;     // [U-129]
    u.attack_range = d.attack_range * sky_boost(d.element, sky).auto_range_pct / 100;  // [U-370] water autos at full moon
    if (d.stance && u.stance_b) {  // [U-72] knives stance: ranged, weaker hits
        s.range = Range::Ranged;
        s.attack.base = s.attack.base * 80 / 100;
        u.attack_range = 420 * sky_boost(d.element, sky).auto_range_pct / 100;
    }
    u.as100 = d.as;
    u.stats = s;
    u.res_max = d.res_max;
    u.res_regen = d.res_regen;
    u.water_max = d.avatar && d.resource != ResourceKind::Water ? elements::kAvatarWaterMax : 0;  // [U-339]
    u.water = std::min(u.water, u.water_max);
    const std::int32_t new_max = s.health.total();
    if (old_max > 0) u.hp = std::max(1, u.hp + (new_max - old_max));
    u.hp = std::min(u.hp, new_max);
}

std::uint32_t World::add_champion(Team team, int champ, int q_pick, int f_pick, int g_pick, bool bot, int player,
                                  std::string name, Loadout loadout) {
    const auto roster = champions();
    champ = std::clamp(champ, 0, int(roster.size()) - 1);
    const ChampionDef& d = roster[std::size_t(champ)];
    Unit u;
    u.kind = UnitKind::Champion;
    u.team = team;
    u.champ = std::int8_t(champ);
    u.name = name.empty() ? std::string(d.name) : std::move(name);
    u.body = d.body;
    u.q_pick = std::int8_t(q_allowed(champ, q_pick) ? q_pick : default_q(champ));  // [U-178] first allowed if unset
    // [U-71] specials must fit the element, else take the first allowed ones
    auto pick = [&](int want, int other) {
        if (special_allowed(want, d) && want != other) return want;
        for (int i = 0; i < int(specials().size()); ++i)
            if (special_allowed(i, d) && i != other) return i;
        return -1;
    };
    u.f_pick = std::int8_t(pick(f_pick, -1));
    u.g_pick = std::int8_t(pick(g_pick, u.f_pick));
    u.skill[std::size_t(Slot::P)] = &d.p;
    u.skill[std::size_t(Slot::Q)] = u.q_pick >= 0 ? &q_options()[std::size_t(u.q_pick)] : nullptr;
    u.skill[std::size_t(Slot::W)] = &d.w;
    u.skill[std::size_t(Slot::E)] = &d.e;
    u.skill[std::size_t(Slot::R)] = &d.r;
    u.skill[std::size_t(Slot::T)] = &d.t;
    u.skill[std::size_t(Slot::Y)] = &d.y;  // [U-289]
    if (u.f_pick >= 0) u.skill[std::size_t(Slot::F)] = &specials()[std::size_t(u.f_pick)];
    if (u.g_pick >= 0) u.skill[std::size_t(Slot::G)] = &specials()[std::size_t(u.g_pick)];
    // [U-71] special skills are ready from level 1
    u.ab[std::size_t(Slot::F)].rank = 1;
    u.ab[std::size_t(Slot::G)].rank = 1;
    u.bot = bot;
    u.player = player;
    u.loadout = validated(loadout);  // [U-177] bad indices → nothing [X-24]
    u.sight = rules::kSightChampion;
    u.gold = 500;
    const auto [fx_, fy_] = fountain(team);
    int nth = 0;
    for (const Unit& o : units) nth += o.kind == UnitKind::Champion && o.team == team;
    u.x = u.home_x = (fx_ + kDirs[std::size_t(nth * 13) % 64].x * 70 / kOne) * kSub;  // spawn ring around the fountain
    u.y = u.home_y = (fy_ + kDirs[std::size_t(nth * 13) % 64].y * 70 / kOne) * kSub;
    u.face = dir_of(team == Team::FireNation ? 1 : -1, team == Team::FireNation ? -1 : 1);
    recompute_stats(u);
    u.hp = u.stats.health.total();
    u.res = u.stats.resource == ResourceKind::Rage ? 0 : u.res_max;  // [U-268] rage starts empty
    // count champions per team → bot lane: 0 top, 1 mid, 2 bot, 2 bot (duo), 0 top …
    int n = 0;
    for (const Unit& o : units) n += o.kind == UnitKind::Champion && o.team == team;
    static constexpr std::uint8_t kLaneOf[] = {1, 0, 2, 2, 0, 1, 0, 2};
    u.botm.lane = std::uint8_t(kLaneOf[std::size_t(n) % std::size(kLaneOf)] % lane_count());  // [U-420] the map's lanes
    return spawn_unit(std::move(u)).id;
}

void World::command(std::uint32_t id, const Cmd& c) {
    Unit* up = find(id);
    if (!up) return;
    Unit& u = *up;
    if (u.kind != UnitKind::Champion) return;
    // [U-407] on the companion's back: only talking / pinging / emotes / settings; nothing cancels the ride
    if (u.riding && c.kind != CmdKind::Ping && c.kind != CmdKind::Emote && c.kind != CmdKind::SetCompanion &&
        c.kind != CmdKind::SetAutoAttack && c.kind != CmdKind::SourceForm)
        return;
    // [X-24] the server validates everything; bad input is ignored
    switch (c.kind) {
        case CmdKind::Buy: buy(u, c.item); return;
        case CmdKind::Sell:
            if (c.slot < kItemSlots && u.items[c.slot] >= 0 && (in_base(u) || !u.alive)) {
                u.gold += kItems[std::size_t(u.items[c.slot])].cost * 70 / 100;
                u.items[c.slot] = -1;
                u.item_impact[c.slot] = 0;
                recompute_stats(u);
            }
            return;
        case CmdKind::LevelUp: level_up(u, Slot(std::min<int>(c.slot, int(Slot::Count) - 1))); return;
        case CmdKind::CheatGold:
            if (cfg.practice) u.gold += 1'000;
            return;
        case CmdKind::CheatLevel:
            if (cfg.practice) give_xp(u, total_xp_to(std::uint32_t(u.level + 1)) - u.xp);
            return;
        case CmdKind::CheatNoCooldowns:
            if (cfg.practice) u.no_cooldowns = !u.no_cooldowns;
            return;
        case CmdKind::SetAutoAttack:  // [U-158]
            u.auto_attack = c.slot != 0;
            return;
        case CmdKind::Ping: {  // [U-246] max 6 pings per 5 s per player (no spam)
            int recent = 0;
            for (const PingEvent& p : pings) recent += p.from == u.id;
            if (recent >= 6 || c.slot >= std::uint8_t(PingKind::Count)) return;
            pings.push_back({u.team, PingKind(c.slot), c.x, c.y, u.id, now_ms});
            return;
        }
        default: break;
    }
    if (!u.alive) return refuse(u, "you are dead");
    if (u.channel_interrupt & kIntMove) u.channel_end_ms = 0;  // [U-337] a move order breaks recall / portal (by their rules)
    u.auto_acquired = false;
    u.st.remove(StatusKind::Channel);
    switch (c.kind) {
        case CmdKind::Move:
            u.order = {OrderKind::Move, 0, c.x, c.y};
            path_to(u, c.x, c.y);
            break;
        case CmdKind::Attack: {
            const Unit* t = find(c.target);
            // [U-416] Dota-like: an own minion under 50 % HP may be attacked (a deny: no gold, less XP for the enemy)
            const bool deny = t && t->team == u.team && t->kind == UnitKind::Minion && denies_allowed(cfg.ruleset) &&
                              t->hp * 2 < t->stats.health.total();
            if (t && (t->team != u.team || deny) && visible_to(u.team, *t)) u.order = {OrderKind::Attack, c.target};
            else refuse(u, "can't attack that (not visible / ally)");
            break;
        }
        case CmdKind::AttackWall: {  // [U-393]
            const int cx = c.x / kCellUnits, cy = c.y / kCellUnits;
            if (u.kind != UnitKind::Champion || !champions()[std::size_t(u.champ)].autos.hits_walls) refuse(u, "your autoattacks can't hit walls");
            else if (!Grid::in_bounds(cx, cy) || grid.at(cx, cy).wall() == Wall::None) refuse(u, "no wall there");
            else u.order = {OrderKind::AttackWall, 0, c.x, c.y};
            break;
        }
        case CmdKind::AttackMove:  // slot 1 = closest champion only [X-98]
            u.order = {OrderKind::AttackMove, 0, c.x, c.y, std::uint8_t(c.slot & 3)};  // [U-303] bit 2 = to the cursor
            path_to(u, c.x, c.y);
            break;
        case CmdKind::Cast: {
            const Slot s = Slot(std::min<int>(c.slot, int(Slot::Count) - 1));
            std::string why;
            if (!can_cast_now(u, s, &why, c.sx, c.sy)) {
                u.draw_pts.clear();  // [U-383] a refused drawn wall never comes back with the next cast
                const SkillDef* d0 = u.skill[std::size_t(s)];
                refuse(u, std::format("{}: {}", d0 ? std::string(d0->name) : std::string("skill"), why));
                break;
            }
            const SkillDef* d = u.skill[std::size_t(s)];
            if (d->targeted) {  // walk into range first
                const Unit* t = find(c.target);
                if (!t || t->team == u.team || !visible_to(u.team, *t)) {
                    refuse(u, std::format("{}: click on a visible enemy", d->name));
                    break;
                }
                u.order = {OrderKind::Cast, c.target, t->ux(), t->uy(), std::uint8_t(s)};
                break;
            }
            if (u.casting) {
                refuse(u, "already casting");
                break;
            }
            start_cast(u, {OrderKind::Cast, c.target, c.x, c.y, std::uint8_t(s), c.sx, c.sy});
            break;
        }
        case CmdKind::Stop:  // [U-265] S stops moving / attacking; the cast or channel goes on
            u.order = {};
            u.path.clear();
            break;
        case CmdKind::SourceForm:  // [U-293] waterbenders only
            if (u.kind == UnitKind::Champion && champions()[std::size_t(u.champ)].element == Element::Water) u.source_ice = c.slot != 0;
            break;
        case CmdKind::CancelCast:  // [U-265] Z cancels the cast windup or a channel (recall, portal)
            u.casting = false;
            u.windup_end_ms = 0;
            u.channel_end_ms = 0;
            u.order = {};
            break;
        case CmdKind::Emote:  // [U-246] the animation / sound plays on every client that sees me
            if (c.slot < std::uint8_t(EmoteId::Count) + 4) {  // [U-414] 6…9 = the player's own emotes
                u.emote = c.slot;
                u.emote_until = now_ms + 3'000;
                u.order = {};
                u.path.clear();
            }
            break;
        case CmdKind::Stance:
            if (champions()[std::size_t(u.champ)].stance) {
                u.stance_b = !u.stance_b;
                recompute_stats(u);
            } else {
                refuse(u, "this champion has no stance");
            }
            break;
        case CmdKind::UseItem: use_item(u, c.slot, c.x, c.y); break;
        case CmdKind::Portal: {  // [X-54][U-350] channel 8 s (= recall) at a pole portal → the other one
            if (cur_map().portal.first < 0) {  // [U-420]
                refuse(u, "this map has no spirit portals");
                break;
            }
            refuse(u, "portal: stand in the beam of light at a pole (glacier corner)");
            for (const auto& p : {cur_map().portal, mirror(cur_map().portal)}) {
                if (dist2(u.ux(), u.uy(), p.first * kCellUnits, p.second * kCellUnits) <= 260LL * 260) {
                    u.order = {OrderKind::Portal};
                    u.path.clear();
                    u.channel_end_ms = now_ms + kRecallMs;
                    u.channel_interrupt = kIntPortal;
                    u.st.add(StatusKind::Channel, u.channel_end_ms, 0, u.id);
                    u.fail_ms = -1;
                    // the far portal shows that someone is coming (everybody sees it)
                    const auto& far = p == cur_map().portal ? mirror(cur_map().portal) : cur_map().portal;
                    std::erase_if(clouds, [&](const Cloud& c) { return c.kind == CloudKind::Arrival && c.owner == u.id; });
                    clouds.push_back({far.first * kCellUnits, far.second * kCellUnits, 90, u.channel_end_ms, CloudKind::Arrival, u.id});
                }
            }
            break;
        }
        case CmdKind::Steer:  // [U-402]
            u.steer_slot = c.slot, u.steer_x = c.x, u.steer_y = c.y, u.steer_until = now_ms + 200;
            break;
        case CmdKind::DrawPoint:  // [U-402]
            if (c.item == 1 || c.slot != u.draw_slot) u.draw_pts.clear();
            u.draw_slot = c.slot;
            if (u.draw_pts.size() < 40) u.draw_pts.push_back({std::clamp(c.x, 0, kMapUnits - 1), std::clamp(c.y, 0, kMapUnits - 1)});
            break;
        case CmdKind::SetCompanion:  // [U-407]
            u.companion = CompanionKind(std::min<int>(c.slot, int(CompanionKind::Count) - 1));
            break;
        case CmdKind::Recall:  // [X-87] 6 s channel home — [U-407] by default the companion comes instead
            if (!cfg.classic_recall) {
                call_companion(u);
                break;
            }
            u.order = {OrderKind::Recall};
            u.path.clear();
            u.channel_end_ms = now_ms + kRecallMs;
            u.channel_interrupt = kIntRecall;  // [U-337]
            u.st.add(StatusKind::Channel, u.channel_end_ms, 0, u.id);
            break;
        default: break;
    }
}

void World::step() {
    if (over) return;
    // [U-287] optional timing of every part (profile = true)
    using clk = std::chrono::steady_clock;
    auto t = clk::now();
    auto lap = [&](int part) {
        if (!profile) return;
        const auto n = clk::now();
        prof_ms[std::size_t(part)] = std::chrono::duration<double, std::milli>(n - t).count();
        t = n;
    };
    ++tick;
    now_ms = std::int64_t(tick * 1'000 / cfg.tick_hz);
    path_budget_ = kPathCellsPerTick;  // [U-287]
    const bool was_full = sky.full_moon;
    sky = WorldClock::at_ms(cfg.start_ms + now_ms);
    if (sky.full_moon != was_full)  // [U-370] the full moon starts / ends: autoattack ranges change
        for (Unit& u : units)
            if (u.kind == UnitKind::Champion) recompute_stats(u);
    earth_events.clear();
    earth_event_team.clear();

    step_match();
    lap(0);
    for (Unit& u : units)
        if (u.bot && u.kind == UnitKind::Champion) step_bot(u);
    lap(1);
    if (clearance_dirty && tick % 8 == 0) rebuild_collision_maps();  // [X-94]
    lap(2);
    step_units();
    step_companions();  // [U-407]
    separate_minions();  // [U-299]
    lap(3);
    step_effects();
    lap(4);
    step_terrain();
    lap(5);
    const std::uint32_t every16 = std::max<std::uint32_t>(1, cfg.tick_hz / 16);
    if (tick % every16 == 0) step_lights();
    lap(6);
    // [U-287] vision: team 0 a quarter step after the lights, team 1 three quarters → never two big jobs in one tick
    if (tick % every16 == every16 / 4) step_vision(0);
    if (tick % every16 == every16 * 3 / 4) step_vision(1);
    lap(7);
    step_senses();
    lap(8);
    step_falcons();  // [U-235]
    lap(9);

    // drop dead effects and dead non-champions (after a short corpse time)
    for (const Effect& e : fx)  // [U-256] fire skills with light_ms keep glowing after they end
        if (e.dead && e.light > 0 && e.def && e.def->light_ms > 0 && e.kind != FxKind::Lantern)
            glows.push_back({{e.ux(), e.uy(), e.light, 80}, now_ms + e.def->light_ms});
    std::erase_if(fx, [](const Effect& e) { return e.dead; });
    for (const Effect& p : fx_pending_) spawn_fx(p);  // [U-383] with real ids / seeds
    fx_pending_.clear();
    const std::size_t before = units.size();
    std::erase_if(units, [&](const Unit& u) {
        return !u.alive && (u.kind == UnitKind::Minion) && now_ms > u.respawn_ms;
    });
    if (units.size() != before) {
        index.clear();
        for (std::size_t i = 0; i < units.size(); ++i) index[units[i].id] = i;
    }
    std::erase_if(waves, [&](const WaveOut& w) { return now_ms - w.ms > 2'000; });
    std::erase_if(hits, [&](const HitEvent& h) { return now_ms - h.ms > 1'000; });
    std::erase_if(wall_breaks, [&](const WallBreak& b) { return now_ms - b.ms > 1'000; });
    std::erase_if(pings, [&](const PingEvent& p) { return now_ms - p.ms > 5'000; });
    for (Unit& u : units)  // [U-246] moving or casting ends an emote
        if (u.emote != 255 && (now_ms > u.emote_until || u.moving || u.casting)) u.emote = 255;
    std::erase_if(rewards, [&](const RewardEvent& r) { return now_ms - r.ms > 1'000; });
    lap(10);
}

}  // namespace em
