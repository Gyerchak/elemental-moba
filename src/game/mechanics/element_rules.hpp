#pragma once
// ======================================================================================
//  ELEMENT RULES — every number of the element interactions in one place [U-225][U-227][U-241][U-244].
//  Game time: 1 game hour = 5 real seconds, 1 game day = 2 real minutes [U-30].
// ======================================================================================
#include <cstdint>

namespace em::elements {

// ---- wet [U-241]
inline constexpr std::int64_t kWetMs = 5'000;              // 1 game hour for everyone …
inline constexpr std::int64_t kWetWaterbenderMs = 1'000;   // … waterbenders shake it off in 1 s
inline constexpr std::int32_t kLightningVsWetPct = 25;     // +25 % lightning damage to wet units
inline constexpr std::int32_t kWetVsFirePct = 25;          // [U-286] the FIRST fire hit on a wet unit: −25 %, then it is dry
inline constexpr std::int32_t kLightningVsIronPct = 25;    // +25 % to iron-armoured units (adds to wet)

// ---- fire [U-241][U-244]
inline constexpr std::int32_t kAirEmpowersFirePct = 25;    // fire through an air zone: +25 % size and damage
inline constexpr std::int64_t kBurningGroundMs = 3'000;    // fire hitting the ground keeps burning
inline constexpr std::int64_t kTreeFireMs = 6'000;         // [U-256] a burning tree lights its area this long
inline constexpr std::int32_t kBurningGroundDps = 1'200;   // hundredths of HP per second
inline constexpr std::int32_t kFireCastLight = 160;        // light while casting a fire skill
inline constexpr std::int32_t kFireInHandLight = 300;      // passive "Fire in Hand"
inline constexpr std::int32_t kLanternLight = 190;         // lantern of non-firebenders
inline constexpr std::int32_t kFirebenderGlow = 64;        // every firebender at night [X-65]

// ---- trees, smoke, steam [U-227][U-241]
inline constexpr std::int64_t kTreeRegrowMs = 120'000;     // 1 game day
inline constexpr std::int64_t kBushRegrowMs = 20'000;      // bushes regrow fast [U-14]
inline constexpr std::int64_t kSmokeMs = 6'000;            // smoke over a burning tree
inline constexpr std::int64_t kSteamMs = 3'000;            // steam from water on fire / lava
inline constexpr std::int32_t kHeatDrawFuel = 2'000;       // Heat Draw gives this much fuel (hundredths)
inline constexpr std::int64_t kPuddleMs = 8'000;           // [U-292] steam cooled by Heat Draw lies as water this long
inline constexpr std::int64_t kWetWhileOnWaterMs = 150;    // [U-292] on shallow water / a puddle: wet only while standing there
inline constexpr std::int32_t kIcePatchSlowPct = 20;       // [U-293] an ice patch is slippery: −20 % move speed while on it
inline constexpr std::int32_t kWaterSourceBonusPct = 15;   // [U-293] a waterbender bending from a puddle / ice patch
inline constexpr std::int32_t kWaterSourceCells = 4;
inline constexpr int kIceWallSourceCells = 6;              // [U-294] ice walls need a big water source this close
// [U-339] water as numbers: the Avatar (resource is not water) carries a small water pool; each water skill takes this much
inline constexpr std::int32_t kAvatarWaterMax = 15'000, kAvatarWaterCost = 6'000, kAvatarWaterRegen = 250;  // hundredths
inline constexpr std::int32_t kSourceRangePct = 125;     // [U-339] bending from a chosen source: +25 % range
inline constexpr std::int32_t kSourceReachCells = 4;     // the source may be this far beyond the skill's range
inline constexpr std::int32_t kIceWallMeltPerUse = 30;   // [U-339] an ice wall used as a source loses this much HP (of 90)
// [U-356] lava on the ground: only some cells cool into a small rock; the rest become plain ground again.
// While it lies there it melts the walls next to it (HP per kLavaMeltMs); ice cools it: the lava's rest time is halved once.
inline constexpr int kLavaRockPct = 12;
inline constexpr std::int64_t kLavaMeltMs = 250;
inline constexpr std::int32_t kLavaMeltStone = 4, kLavaMeltIce = 20, kLavaMeltMetal = 3;
// [U-360] natural rocks: 2 × a bent wall's HP; broken ones grow back after this long
inline constexpr int kNaturalWallHpPct = 200;
inline constexpr std::int64_t kThrownRockMs = 20'000;
inline constexpr std::int32_t kRockSourceBonusPct = 20;  // [U-378] a real rock thrown: +20 % damage (and +25 % range)  // [U-359] the small rock a thrown rock leaves
inline constexpr std::int64_t kNaturalRockRegrowMs = 180'000;

// ---- fire meets fire [U-241]: the stronger caster (elemental power) wins the clash
// returns how much of the stronger fire is left, in % (equal power → 0 = both gone)
[[nodiscard]] constexpr std::int32_t clash_left_pct(std::int64_t strong_ep, std::int64_t weak_ep) {
    if (strong_ep <= 0) return 0;
    return std::int32_t(100 - weak_ep * 100 / strong_ep);
}
static_assert(clash_left_pct(100, 100) == 0 && clash_left_pct(200, 100) == 50 && clash_left_pct(100, 0) == 100);

}  // namespace em::elements
