// [U-473] LEAGUE OF LEGENDS MECHANICS — one place for the LoL-style rules of the unified engine:
//   lifesteal (autoattacks), spellvamp (skills), omnivamp (everything), champion vamp (only damage to champions),
//   grievous wounds on hit, critical strikes, on-hit damage, fear.
// Champions / items / skills only hold the NUMBERS (Stats fields, StatKey names, ChampionDef::crits, StatusKind::Fear);
// the rules live here. Dota-style rules stay in their own places (denies, …).
#include <algorithm>

#include "game/content.hpp"
#include "game/world.hpp"

namespace em {

// After damage landed on dst (hund = hundredths of HP lost): the source heals by its vamp, the target gets grievous wounds.
void World::lol_after_damage(Unit* src, Unit& dst, std::int32_t hund, bool is_auto) {
    if (!src || src == &dst || src->kind != UnitKind::Champion || !classic_rules(cfg.ruleset) || hund <= 0) return;
    const Stats& s = src->stats;
    const std::int64_t vamp = s.omnivamp + (is_auto ? s.lifesteal : s.spellvamp) + (dst.kind == UnitKind::Champion ? s.champion_vamp : 0);
    if (vamp > 0) heal(*src, std::int32_t(std::int64_t(hund) * vamp / 10'000));
    if (s.grievous_on_hit > 0 && !dst.is_structure())
        dst.st.add(StatusKind::Grievous, now_ms + 3'000, std::int16_t(s.grievous_on_hit / 100), src->id);
}

// Critical strike of an autoattack: champions that can crit (ChampionDef::crits), crit_chance 100 = 1 % → × crit_damage.
bool World::lol_crit(const Unit& u, std::int32_t& dmg) {
    if (u.kind != UnitKind::Champion || !champions()[std::size_t(u.champ)].crits || u.stats.crit_chance <= 0) return false;
    if (std::int32_t(random() % 10'000) >= u.stats.crit_chance) return false;
    dmg = std::int32_t(std::int64_t(dmg) * u.stats.crit_damage / 10'000);
    return true;
}

// On-hit: every autoattack that lands adds flat damage + % of the target's max HP (same damage type as the auto).
void World::lol_on_hit(Unit& u, Unit& target, bool physical) {
    if (!target.alive || (u.stats.on_hit_dmg <= 0 && u.stats.on_hit_max_hp <= 0)) return;
    const std::int64_t extra = u.stats.on_hit_dmg / 100 + std::int64_t(target.stats.health.total()) / 100 * u.stats.on_hit_max_hp / 10'000;
    if (extra > 0) deal_damage(&u, target, std::int32_t(extra), physical ? SubElement::Physical : champions()[std::size_t(u.champ)].sub, physical, true);  // (lifesteal counts, like LoL)
}

// Fear: the unit cannot act and runs straight away from the one who scared it (if it can move at all).
// Returns true when the unit is feared (its normal step is skipped).
bool World::lol_fear_step(Unit& u) {
    if (!u.st.has(StatusKind::Fear) || u.is_structure()) return false;
    u.windup_end_ms = std::min<std::int64_t>(u.windup_end_ms, 0);
    u.casting = false;
    const Unit* src = find(u.st.source_of(StatusKind::Fear));
    const bool held = u.st.has(StatusKind::Stun) || u.st.has(StatusKind::Airborne) || u.st.has(StatusKind::Root) || u.st.has(StatusKind::Frozen);
    if (src && !held) {
        const Dir away = dir_of(std::int64_t(u.ux()) - src->ux(), std::int64_t(u.uy()) - src->uy());
        move_towards(u, u.x + away.x * 200 / kOne * kSub, u.y + away.y * 200 / kOne * kSub, move_speed_sub(u));
    }
    return true;
}

}  // namespace em
