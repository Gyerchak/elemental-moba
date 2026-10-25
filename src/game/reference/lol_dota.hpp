#pragma once
// ======================================================================================
//  [U-230] REFERENCE MECHANICS from League of Legends (LoL_) and Dota 2 (Dota_), after their wikis.
//  NOT USED by the game in active mode. They are here so mechanics are easy to compare, mix or switch to.
//  Plain constexpr formulas + constants; numbers are the wiki values at the time of writing (may drift).
// ======================================================================================
#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace em::reference {

// ================================================================ LoL_
// Armour / magic resist: damage multiplier.
[[nodiscard]] constexpr double LoL_DamageMultiplier(double resist) {
    return resist >= 0 ? 100.0 / (100.0 + resist) : 2.0 - 100.0 / (100.0 - resist);
}
// Order: flat reduction → % reduction → % penetration → flat penetration (lethality) [U-248 copies this].
[[nodiscard]] constexpr double LoL_EffectiveResist(double r, double flat_red, double pct_red, double pct_pen, double flat_pen) {
    r -= flat_red;
    if (r > 0) r *= 1 - pct_red;
    if (r > 0) r *= 1 - pct_pen;
    if (r > 0) r = std::max(0.0, r - flat_pen);
    return r;
}
// Attack speed = base × (1 + bonus% + growth × (level−1) × (0.7025 + 0.0175 × (level−1))), cap 2.5.
[[nodiscard]] constexpr double LoL_AttackSpeed(double base, double ratio, double bonus, double growth, int level) {
    const double g = growth * (level - 1) * (0.7025 + 0.0175 * (level - 1));
    return std::min(2.5, base + ratio * (bonus + g));
}
// Ability haste → cooldown: cd × 100 / (100 + haste)  (this game uses the same for elemental haste).
[[nodiscard]] constexpr double LoL_Cooldown(double cd, double haste) { return cd * 100.0 / (100.0 + haste); }
// Tenacity stacks multiplicatively: 1 − Π(1 − t_i).
[[nodiscard]] constexpr double LoL_TenacityStack(double a, double b) { return 1 - (1 - a) * (1 - b); }
// Move speed soft caps: above 415 → 80 %, above 490 → 50 %; below 220 → raised.
[[nodiscard]] constexpr double LoL_MoveSpeed(double raw) {
    if (raw > 490) return 415 + (490 - 415) * 0.8 + (raw - 490) * 0.5;
    if (raw > 415) return 415 + (raw - 415) * 0.8;
    if (raw < 220) return raw * 0.5 + 110;
    return raw;
}
// Slows: only the strongest counts. Lifesteal / omnivamp: heal = damage × %.
[[nodiscard]] constexpr double LoL_Lifesteal(double dmg, double pct) { return dmg * pct; }
// Critical strikes — NOT in this game [U-221]; kept only for reference.
inline constexpr double LoL_CritDamage = 1.75;
// XP share between champions near a dying minion: 1 → 100 %, 2 → 65.16 % each, 3 → 43.44 %, 4 → 32.58 %, 5 → 26.06 %.
inline constexpr std::array<double, 5> LoL_ExpShare{1.0, 0.6516, 0.4344, 0.3258, 0.2606};
inline constexpr double LoL_ExpRange = 1'400;
// Minion gold: melee 21, caster 14, siege 60–90 (grows), super 15.
inline constexpr int LoL_MinionGoldMelee = 21, LoL_MinionGoldCaster = 14, LoL_MinionGoldSiege = 60;
// Turret plating: 5 plates × 125 gold, fall at 14:00.
inline constexpr int LoL_TurretPlates = 5, LoL_PlateGold = 125, LoL_PlatingEndsMin = 14;
// Minion aggro (2016): see rules::MinionPriority — the game uses this one actively [U-194].
// Brushes: a unit inside is hidden unless an enemy is in the same brush or it is revealed (true sight).
// Wards: stealth ward 90–120 s, control ward reveals + disables.
inline constexpr int LoL_StealthWardSeconds = 90, LoL_ControlWardRange = 900;

// ================================================================ Dota_
// Armour: multiplier = 1 − 0.06·a / (1 + 0.06·|a|).
[[nodiscard]] constexpr double Dota_ArmorMultiplier(double a) {
    const double abs_a = a < 0 ? -a : a;
    return 1 - (0.06 * a) / (1 + 0.06 * abs_a);
}
// Magic resistance and status resistance stack multiplicatively.
[[nodiscard]] constexpr double Dota_ResistStack(double a, double b) { return 1 - (1 - a) * (1 - b); }
// Status resistance shortens disables (like tenacity) — but in Dota it also applies to most debuffs.
[[nodiscard]] constexpr double Dota_DisableDuration(double base, double status_resist) { return base * (1 - status_resist); }
// Attack time = base attack time / (1 + attack speed / 100), attack speed capped 20 … 700.
[[nodiscard]] constexpr double Dota_AttackTime(double bat, double attack_speed) {
    return bat / (std::clamp(attack_speed, 20.0, 700.0) / 100.0);
}
// Turning: time = 0.03 × angle (rad) / turn rate.
[[nodiscard]] constexpr double Dota_TurnTime(double angle_rad, double turn_rate) { return 0.03 * angle_rad / turn_rate; }
// Day / night vision: day 1800, night 800 (heroes); a full day = 10 minutes.
inline constexpr int Dota_DayVision = 1'800, Dota_NightVision = 800, Dota_DayNightCycleMinutes = 10;
// Cleave: autoattacks also hit a cone behind the target at a % (an AoE autoattack like U-217's Line / Splash).
inline constexpr double Dota_CleavePct = 0.4;
// Buyback cost ≈ 200 + net worth / 13; denying own creeps gives the enemy less XP.
[[nodiscard]] constexpr int Dota_BuybackCost(int net_worth) { return 200 + net_worth / 13; }
inline constexpr double Dota_DeniedExpPct = 0.3;
// Roshan: Aegis of the Immortal (revive once), drops after the 1st kill: cheese, after the 3rd: refresher shard.
inline constexpr std::string_view Dota_RoshanDrops = "Aegis, Cheese (2nd+), Refresher Shard (3rd+), Aghanim's Blessing";

// ---------------------------------------------------------------- checks
static_assert(LoL_DamageMultiplier(100) == 0.5);
static_assert(LoL_Cooldown(10, 100) == 5);
static_assert(Dota_ArmorMultiplier(0) == 1);
static_assert(Dota_AttackTime(1.7, 100) == 1.7);

}  // namespace em::reference
