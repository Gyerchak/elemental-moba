#pragma once
// [U-111][U-119..U-121] Champion XP, levels, skill ranks and game phases.
// No level cap. Every level needs more XP than the one before; XP income grows
// until 60:00 (late game) and then stays flat → long games still give levels, only slower.
#include <algorithm>
#include <cstdint>

#include "core/abilities.hpp"

namespace em {

// [U-121] Game phases, a bit longer than LoL; 60:00 is deep late game [X-66].
enum class GamePhase : std::uint8_t { Early, Mid, Late };
inline constexpr std::int64_t kMidStartMs = 18 * 60'000, kLateStartMs = 40 * 60'000;  // alignment at 60:00 = deep late game [U-128]

[[nodiscard]] constexpr GamePhase phase_at(std::int64_t ms) {
    return ms < kMidStartMs ? GamePhase::Early : ms < kLateStartMs ? GamePhase::Mid : GamePhase::Late;
}

// [U-119] Level 18 at the standard game time [?-18]. [U-120][U-121] Level 27 at 60:00.
inline constexpr std::int64_t kStandardGameMs = 35 * 60'000;
inline constexpr std::int64_t kLateGameMs = 60 * 60'000;

// [X-44] XP from level n to n+1: 280 + 100 × (n − 1)  (same as LoL up to 18).
[[nodiscard]] constexpr std::int64_t xp_to_next(std::uint32_t level) {
    const std::int64_t n = level < 1 ? 1 : level;
    return 280 + 100 * (n - 1);
}

// Total XP needed to reach `level` from level 1.
[[nodiscard]] constexpr std::int64_t total_xp_to(std::uint32_t level) {
    std::int64_t sum = 0;
    for (std::uint32_t n = 1; n < level; ++n) sum += xp_to_next(n);
    return sum;
}

[[nodiscard]] constexpr std::uint32_t level_at_xp(std::int64_t xp) {
    std::uint32_t level = 1;
    while (xp >= xp_to_next(level)) xp -= xp_to_next(level++);
    return level;
}

// [X-64] Tuning target for minion / kill / passive XP (M5). XP per minute for an
// average player grows from 331 (0:00) by 11.07 each minute up to 995 (60:00), then stays.
inline constexpr std::int64_t kXpRateStart = 331, kXpRateGrowthCenti = 1'107;  // per min, per min²/100

// Expected total XP of an average player at time `ms`.
[[nodiscard]] constexpr std::int64_t expected_xp_at(std::int64_t ms) {
    const std::int64_t t = std::min(ms, kLateGameMs);  // ms
    std::int64_t xp = kXpRateStart * t / 60'000 + kXpRateGrowthCenti * t / 60'000 * t / 60'000 / 200;
    if (ms > kLateGameMs) xp += (kXpRateStart + kXpRateGrowthCenti * 60 / 100) * (ms - kLateGameMs) / 60'000;
    return xp;
}

[[nodiscard]] constexpr std::uint32_t expected_level_at(std::int64_t ms) { return level_at_xp(expected_xp_at(ms)); }

// [U-120] Ultimate T gets a rank at levels 9, 18, 27 (27 = LoL's "level 16" moment),
// then every 9 levels more (36, 45…, no cap [U-111]) [?-20]. Every other skill
// (P…R, F, G) can take a point at any level — there are more skills to learn.
inline constexpr std::uint32_t kUltLevelStep = 9;

// [U-271] the step is per champion (ChampionDef::ult_step), 9 by default
[[nodiscard]] constexpr std::uint32_t max_rank(Slot slot, std::uint32_t level, std::uint32_t ult_step = kUltLevelStep) {
    return slot == Slot::T ? level / (ult_step ? ult_step : kUltLevelStep) : level;
}

[[nodiscard]] constexpr bool can_rank_up(Slot slot, std::uint32_t current_rank, std::uint32_t level, std::uint32_t ult_step = kUltLevelStep) {
    return current_rank < max_rank(slot, level, ult_step);
}

// [X-45] Optional soft cap (Claude idea, OFF by default): after level 30 every level
// gives only 50 % of the normal per-level stat growth. Turn on per server.
inline constexpr std::uint32_t kSoftCapLevel = 30;
[[nodiscard]] constexpr std::int64_t growth_levels(std::uint32_t level, bool soft_cap) {  // in hundredths of a level
    const std::int64_t l = level < 1 ? 0 : level - 1;
    if (!soft_cap || level <= kSoftCapLevel) return l * 100;
    return std::int64_t(kSoftCapLevel - 1) * 100 + std::int64_t(level - kSoftCapLevel) * 50;
}

// Checks.
static_assert(!can_rank_up(Slot::T, 0, 8) && can_rank_up(Slot::T, 0, 9) && !can_rank_up(Slot::T, 1, 17));
static_assert(can_rank_up(Slot::T, 1, 18) && can_rank_up(Slot::T, 2, 27) && can_rank_up(Slot::T, 3, 36));
static_assert(can_rank_up(Slot::Q, 0, 1) && can_rank_up(Slot::F, 5, 6) && can_rank_up(Slot::Y, 0, 1));
static_assert(can_rank_up(Slot::T, 0, 6, 6) && !can_rank_up(Slot::T, 1, 11, 6));  // [U-271] a champion with step 6
static_assert(xp_to_next(1) == 280 && xp_to_next(17) == 1'880 && xp_to_next(18) == 1'980);
static_assert(total_xp_to(18) == 18'360 && total_xp_to(27) == 39'780);
static_assert(level_at_xp(total_xp_to(18)) == 18 && level_at_xp(total_xp_to(18) - 1) == 17);
static_assert(phase_at(0) == GamePhase::Early && phase_at(30 * 60'000) == GamePhase::Mid &&
              phase_at(60 * 60'000) == GamePhase::Late);
static_assert([] {  // every level needs more XP than the one before [U-119]
    for (std::uint32_t n = 1; n < 200; ++n)
        if (xp_to_next(n + 1) <= xp_to_next(n)) return false;
    return true;
}());
// Expected pace: 15:00 → 10, 35:00 → 18, 60:00 → 27, 90:00 → 36, 120:00 → 43 (slower and slower).
static_assert(expected_level_at(15 * 60'000) == 10 && expected_level_at(kStandardGameMs) == 18);
static_assert(expected_level_at(kLateGameMs) == 27 && expected_level_at(90 * 60'000) == 36);
static_assert(expected_level_at(120 * 60'000) == 43);
static_assert(growth_levels(40, false) == 3'900 && growth_levels(40, true) == 2'900 + 500 && growth_levels(30, true) == 2'900);

}  // namespace em
