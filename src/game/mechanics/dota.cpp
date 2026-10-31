// [U-476] DOTA 2 MECHANICS — one place for the Dota-style rules of the unified engine:
//   attributes (Strength / Agility / Intelligence, a primary attribute), mana, HP / mana regeneration, magic
//   resistance %, evasion + true strike, damage block, bash, mana break, hex, break, debuff immunity, dispels.
// Champions / items / skills only hold the NUMBERS (Stats fields, StatKey names, ChampionDef::attr, SkillDef::dispel).
// Units: attribute points in hundredths (1 point = 100); % stats 100 = 1 %.
#include <algorithm>
#include <format>
#include <vector>

#include "game/content.hpp"
#include "game/world.hpp"

namespace em {

// Attributes → stats (Dota 7.3x numbers): Strength +22 HP, +0.1 HP regen; Agility +1/6 armour (defence), +1 attack
// speed; Intelligence +12 mana, +0.05 mana regen, +0.1 % magic resistance; the primary attribute +1 attack per point
// (Universal: +0.7 per point of all three).
void World::dota_attributes(Stats& s, Unit& u, const ChampionDef& d) {
    const std::int32_t lv = std::max(0, u.level - 1);
    s.strength += d.str + d.str_lvl * lv, s.agility += d.agi + d.agi_lvl * lv, s.intelligence += d.intel + d.int_lvl * lv;
    if (s.strength + s.agility + s.intelligence == 0) return;
    s.health.bonus += s.strength * 22;          // 22 HP (2 200 hundredths) per point
    s.hp_regen += s.strength / 10;              // 0.1 / s per point
    s.defence.bonus += s.agility * 17 / 100;    // ≈ 1/6 armour per point
    s.attack_speed.bonus += s.agility;          // 1 attack speed (1 %) per point
    s.magic_resist += s.intelligence / 10;      // 0.1 % per point
    const std::int32_t primary = d.attr == 1 ? s.strength : d.attr == 2 ? s.agility : d.attr == 3 ? s.intelligence
                               : d.attr == 4 ? (s.strength + s.agility + s.intelligence) * 70 / 100 : 0;
    s.attack.bonus += primary;                  // 1 attack per point
}

// Magic resistance: non-physical damage × (1 − resistance), after the elemental defence; max 90 %.
std::int64_t World::dota_magic_resist(std::int64_t dmg, const Unit& dst) const {
    return dmg * (10'000 - std::clamp(dst.stats.magic_resist, -10'000, 9'000)) / 10'000;
}

// Evasion: an autoattack misses by chance, unless the attacker has true strike.
bool World::dota_evaded(const Unit& attacker, const Unit& target) {
    if (target.stats.evasion <= 0 || attacker.stats.true_strike > 0) return false;
    return std::int32_t(random() % 10'000) < std::min(target.stats.evasion, 9'000);
}

// Damage block: physical autoattacks taken lose a flat amount (with block_chance; 0 = always).
std::int64_t World::dota_block(const Unit& dst, std::int64_t dmg) {
    if (dst.stats.damage_block <= 0) return dmg;
    if (dst.stats.block_chance > 0 && std::int32_t(random() % 10'000) >= dst.stats.block_chance) return dmg;
    return std::max<std::int64_t>(0, dmg - dst.stats.damage_block / 100);
}

// On-hit (Dota): bash (chance → stun + damage), mana break (burns the target's mana, half of it as damage).
void World::dota_on_hit(Unit& u, Unit& target) {
    if (!target.alive) return;
    if (u.stats.bash_chance > 0 && std::int32_t(random() % 10'000) < u.stats.bash_chance) {
        if (u.stats.bash_dmg > 0) deal_damage(&u, target, u.stats.bash_dmg / 100, SubElement::Physical, true, false);
        if (target.alive && u.stats.bash_ms > 0) apply_cc(u.stats, u.id, target, StatusKind::Stun, u.stats.bash_ms, 0);
    }
    if (u.stats.mana_break > 0 && target.alive && target.stats.resource == ResourceKind::Mana && target.res > 0) {
        const std::int32_t burned = std::min(target.res, u.stats.mana_break);
        target.res -= burned;
        deal_damage(&u, target, burned / 200, SubElement::Physical, true, false);  // 50 % of the burned mana
    }
}

// Dispel: basic = slows, silences, chi block, disarm, roots, blinds, burns, grievous wounds; strong = also stuns,
// knock-ups, freezes, fear, subdue, hex. `debuffs` = clean a friend; else strip an enemy's buffs (haste, shields).
void World::dota_dispel(Unit& u, bool strong, bool debuffs) {
    if (!debuffs) {
        for (StatusKind k : {StatusKind::Haste, StatusKind::Shield, StatusKind::DebuffImmune}) u.st.remove(k);
        return;
    }
    for (StatusKind k : {StatusKind::Slow, StatusKind::Silence, StatusKind::Mute, StatusKind::Disarm, StatusKind::Root, StatusKind::Blind,
                         StatusKind::Burn, StatusKind::Grievous, StatusKind::Break})
        u.st.remove(k);
    if (strong)
        for (StatusKind k : {StatusKind::Stun, StatusKind::Airborne, StatusKind::Frozen, StatusKind::Fear, StatusKind::Subdue, StatusKind::Hex})
            u.st.remove(k);
}

}  // namespace em

// ================================================================ [U-478] switchable features (all OFF by default;
// a map / the lobby owner switches them on: MatchConfig::features)
namespace em {

std::int32_t World::buyback_cost(const Unit& u) const { return 200 + 40 * u.level; }

// Buyback: dead → pay and come back at once at the fountain; 5 min cooldown.
void World::dota_buyback(Unit& u) {
    if (!feature(kFeatBuyback)) return refuse(u, "buyback is off in this match");
    if (u.alive) return refuse(u, "you are alive");
    if (now_ms < u.buyback_ready_ms) return refuse(u, std::format("buyback ready in {} s", (u.buyback_ready_ms - now_ms + 999) / 1000));
    const std::int32_t cost = buyback_cost(u);
    if (u.gold < cost) return refuse(u, std::format("buyback costs {} gold", cost));
    u.gold -= cost;
    u.buyback_ready_ms = now_ms + 300'000;
    u.respawn_ms = now_ms;  // (the respawn happens this tick)
}

// Glyph: the team's structures cannot be damaged for 5 s; 5 min cooldown (shared by the team).
void World::dota_glyph(Unit& u) {
    if (!feature(kFeatGlyph)) return refuse(u, "the glyph is off in this match");
    const std::size_t t = std::size_t(u.team) & 1;
    if (now_ms < glyph_ready[t]) return refuse(u, std::format("glyph ready in {} s", (glyph_ready[t] - now_ms + 999) / 1000));
    glyph_until[t] = now_ms + 5'000;
    glyph_ready[t] = now_ms + 300'000;
}

// Talents: one of two bonuses at levels 10 / 15 / 20 / 25 (kDefaultTalents or the champion's own).
void World::dota_talent(Unit& u, int tier, int side) {
    if (!feature(kFeatTalents) || tier < 0 || tier > 3 || side < 0 || side > 1) return;
    if (u.level < 10 + 5 * tier) return refuse(u, std::format("talent at level {}", 10 + 5 * tier));
    if (u.talents[std::size_t(tier)]) return;
    u.talents[std::size_t(tier)] = std::uint8_t(side + 1);
    recompute_stats(u);
}

// Courier: bought away from the shop → it arrives after the courier's trip (it waits while the bag is full).
void World::dota_courier_step(Unit& u) {
    for (std::size_t i = 0; i < u.courier.size();) {
        Unit::Delivery& dv = u.courier[i];
        if (dv.at_ms > now_ms || !u.alive) {
            ++i;
            continue;
        }
        const ItemDef& d = kItems[std::size_t(dv.item)];
        int slot = -1;
        if (d.group == ItemGroup::Boots) slot = u.items[kBootsSlot] < 0 ? kBootsSlot : -1;
        else if (d.group == ItemGroup::Special) slot = u.items[kSpecialSlot] < 0 ? kSpecialSlot : -1;
        else
            for (std::size_t s = 1; s <= 8 && slot < 0; ++s)
                if (u.items[s] < 0) slot = int(s);
        if (slot < 0) {  // full: try again in a second
            dv.at_ms = now_ms + 1'000;
            ++i;
            continue;
        }
        u.items[std::size_t(slot)] = dv.item;
        u.courier.erase(u.courier.begin() + std::ptrdiff_t(i));
        recompute_stats(u);
    }
}

// Neutral items: a jungle monster may drop one for its killer (tier by match time; a better tier replaces).
void World::dota_neutral_drop(Unit& killer, const Unit&) {
    if (!feature(kFeatNeutralItems) || killer.kind != UnitKind::Champion || random() % 100 >= 25) return;
    const int tier = int(std::min<std::int64_t>(4, 1 + now_ms / 600'000));
    std::vector<int> pick;
    for (std::size_t i = 0; i < kNeutralItems.size(); ++i)
        if (kNeutralItems[i].tier == tier) pick.push_back(int(i));
    if (pick.empty()) return;
    if (killer.neutral >= 0 && kNeutralItems[std::size_t(killer.neutral)].tier >= tier) return;
    killer.neutral = std::int8_t(pick[random() % pick.size()]);
    recompute_stats(killer);
}

// True sight: turrets (700) and units with true sight (wards / lanterns) of team t see invisible enemies near them.
bool World::true_sighted(Team t, const Unit& u) const {
    for (const Unit& o : units) {
        if (!o.alive || o.team != t) continue;
        const std::int64_t r = o.kind == UnitKind::Turret ? 700 : o.stats.true_sight;
        if (r > 0 && dist2(o.ux(), o.uy(), u.ux(), u.uy()) <= r * r) return true;
    }
    return false;
}

}  // namespace em
