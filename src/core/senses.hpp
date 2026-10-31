#pragma once
// [U-141..U-143] Senses that are NOT vision. They never show WHAT something is,
// only waves where something is / moves. All numbers: [X-79..X-81].
#include <cstdint>

#include "core/elements.hpp"

namespace em {

enum class WaveKind : std::uint8_t {
    Seismic,         // [U-142] a ground unit moves (Rek'Sai-like tremor), team-shared
    SeismicBending,  // [U-142] someone ELSE bent the earth → "terrain changed" wave
    Heat,            // [U-143] heat source, waves going outward, team-shared
    Wind,            // [U-141] a skill moves outside vision, only for the airbender with Wind Sense
    Sound,           // [U-151] footsteps heard from outside vision
    SoundLoud,       // [U-151] fighting / casting / explosions heard from outside vision
};

struct SenseWave {
    WaveKind kind = WaveKind::Seismic;
    std::int32_t x = 0, y = 0;  // game units (centre of the wave, not the exact unit)
    std::int32_t dx = 0, dy = 0;  // Wind: direction the skill moves (Q14), else 0
};

// [U-76][U-79][U-77] The senses are SPECIAL SKILLS (F/G picks): seismic sense = earth,
// heat sense = fire, Wind Sense ("sense skills") = air. [?-25] Seismic / heat work passively
// (always on) while picked; Wind Sense is an active.
// [X-80] Seismic sense numbers.
inline constexpr std::int32_t kSeismicRange = 1'400;
inline constexpr std::int32_t kSeismicPeriodMs = 500;
// [X-81] Heat sense numbers.
inline constexpr std::int32_t kHeatRange = 1'200;
inline constexpr std::int32_t kHeatPeriodMs = 750;
// [X-79] Wind Sense (air special skill F/G): active, 10 s.
inline constexpr std::int32_t kWindSenseRange = 2'400;
inline constexpr std::int32_t kWindSenseMs = 10'000;
inline constexpr std::int32_t kWindPeriodMs = 250;

// [U-151][X-102] Hearing: every champion hears what it cannot see. Team-shared like vision.
// [U-466] the SENSE hears only footsteps (a unit that stands still is silent, even while fighting), within half the
// champions' full day vision (1 200 → 600); explosions / fields / storms the same range. kHearLoud = how far the
// client's audio carries (sound volume), not the sense.
inline constexpr std::int32_t kHearRange = 600;
inline constexpr std::int32_t kHearLoud = 1'600;
inline constexpr std::int32_t kSoundPeriodMs = 400;

// [U-142] Cannot sense flying things. [?-26] Only things that MOVE make tremors.
[[nodiscard]] constexpr bool seismic_feels(bool flying, bool airborne, bool moving) {
    return !flying && !airborne && moving;
}
// [U-143] Frozen / self-frozen = no heat. [?-26] Heat shows still units too.
[[nodiscard]] constexpr bool heat_feels(bool frozen) { return !frozen; }

static_assert(!seismic_feels(true, false, true) && !seismic_feels(false, true, true) && seismic_feels(false, false, true));
static_assert(!heat_feels(true) && heat_feels(false));

}  // namespace em
