#pragma once
// ======================================================================================
//  MOB RULES — every number that decides how minions and jungle monsters behave and pay.
//  Change a number here; nothing else needs to change. [U-194][U-195]
// ======================================================================================
#include <cstdint>

namespace em::rules {

// ---------------------------------------------------------------- experience [U-195]
inline constexpr std::int32_t kXpRange = 1'000;             // minions AND monsters: share XP within this range
inline constexpr std::int64_t kJungleAssistWindowMs = 3'000; // hit the monster in the last 3 s = you share its XP

// Jungle XP split [U-195]: the killer counts as N people (N = everyone sharing), every helper as 1.
//   2 people → killer 2/3, helper 1/3;   3 people → killer 3/5, helpers 1/5 each; …
// The killer needs no range; helpers must be within kXpRange.
[[nodiscard]] constexpr std::int64_t jungle_killer_xp(std::int64_t xp, int sharers) {
    return sharers <= 1 ? xp : xp * sharers / (2 * sharers - 1);
}
[[nodiscard]] constexpr std::int64_t jungle_helper_xp(std::int64_t xp, int sharers) {
    return sharers <= 1 ? 0 : xp / (2 * sharers - 1);
}
static_assert(jungle_killer_xp(300, 2) == 200 && jungle_helper_xp(300, 2) == 100);
static_assert(jungle_killer_xp(500, 3) == 300 && jungle_helper_xp(500, 3) == 100);
static_assert(jungle_killer_xp(100, 1) == 100);

// ---------------------------------------------------------------- minions (LoL 2016 style) [U-194]
struct MinionRules {
    std::int32_t acquire_range = 650;      // look for targets this far
    std::int32_t call_for_help_range = 900;// hear "an ally is being attacked" this far
    std::int64_t call_for_help_ms = 2'000; // an attack counts as a call for help this long
    std::int32_t drop_range = 900;         // stop chasing a target further than this
    std::int64_t retarget_ms = 250;        // think 4× per second
    std::int32_t damage_to_champions_pct = 60;  // minion hits on champions do less [X-108]
    std::int64_t repath_ms = 3'000;        // walk the lane on a fresh A* path at least this often [U-196]
    std::int32_t waypoint_reached = 120;   // "arrived" at a lane point
};
inline constexpr MinionRules kMinion;

// LoL 2016 target priority — lower number = more important. A minion switches target only for a
// strictly more important one.
enum class MinionPriority : std::uint8_t {
    ChampionHittingAllyChampion = 1,  // call for help from an allied champion: the enemy champion
    MinionHittingAllyChampion = 2,    // … the enemy minion
    MinionHittingAllyMinion = 3,      // call for help from an allied minion: the enemy minion
    TurretHittingAllyMinion = 4,      // … the enemy turret
    ChampionHittingAllyMinion = 5,    // … the enemy champion
    ClosestMinion = 6,
    ClosestChampion = 7,
    Structure = 8,                    // turrets / inhibitors / Shrine when nothing else is there
    None = 255,
};

// ---------------------------------------------------------------- sight of every kind of unit [U-234]
// (game units, full daylight; night and light change it — see world_clock.hpp / light.hpp)
inline constexpr std::int32_t kSightChampion = 1'200;
inline constexpr std::int32_t kSightMinion = 800;
inline constexpr std::int32_t kSightTurret = 1'700;     // also inhibitors and the Nation Shrine [U-351] (was 1 100)
inline constexpr std::int32_t kSightMonster = 600;
inline constexpr std::int32_t kSightAggressiveMonster = 700;
inline constexpr std::int32_t kSightBoss = 700;
inline constexpr std::int32_t kSightFalcon = 900;       // what a falcon records [U-235]

// ---------------------------------------------------------------- jungle monsters [U-154]
inline constexpr std::int32_t kMonsterLeash = 1'100, kBossLeash = 1'400;  // [X-107]
inline constexpr std::int64_t kMonsterGiveUpMs = 2'500;                    // not hit for this long → reset
inline constexpr std::int32_t kMonsterResetHealPctPerS = 5;
inline constexpr std::int32_t kAggressiveSightRange = 420;                 // [X-97]

}  // namespace em::rules
