#pragma once
// [U-309] Turret rules in one place (Claude idea numbers, LoL-like). Change here to balance.
#include <array>
#include <cstdint>

namespace em::turrets {

// damage to minions = this % of the minion's MAX health per shot (index = MinionType: melee, ranged, siege, super)
inline constexpr std::array<std::int32_t, 4> kMinionHitPct = {45, 70, 14, 7};
// damage to champions grows with every shot in a row on the same champion
inline constexpr std::int32_t kChampionRampPct = 40;      // +40 % per shot …
inline constexpr std::int32_t kChampionRampMaxPct = 120;  // … up to +120 %
// focus: a champion that damages an allied champion near the turret becomes its target at once (call for help)
inline constexpr std::int32_t kCallForHelpRange = 900;
inline constexpr std::int64_t kCallForHelpMs = 2'500;
// later: turrets may cast skills (not used now) [U-309]
inline constexpr bool kTurretsCastSkills = false;

}  // namespace em::turrets
