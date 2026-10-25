#pragma once
// [U-30..U-35] Day/night, moon, one-time sky events. Integer math only
// (deterministic on every machine [X-04]). Timings: docs/time-and-sky.md.
#include <cstdint>

namespace em {

struct SkyState {
    std::int64_t real_ms = 0;
    int game_day = 0;           // 0-based
    int minute_of_day = 0;      // 0..1439 game minutes
    int second_of_day = 0;      // 0..86399 game seconds (smooth light)
    bool is_day = false;        // 06:00-18:00
    int moon_age_days = 0;      // 0 = new, 15 = full
    int moon_light_pct = 0;     // 0..100
    bool full_moon = false;
    bool new_moon = false;
    bool solar_eclipse = false;
    bool comet_landed = false;  // true from the impact moment on
    bool planetary_alignment = false;
};

class WorldClock {
public:
    static constexpr int kGameMinutesPerRealSecond = 12;     // [U-30]
    static constexpr int kMinutesPerDay = 24 * 60;
    static constexpr int kDayStart = 6 * 60, kDayEnd = 18 * 60;
    static constexpr int kMoonCycleDays = 30;                // [U-32]
    static constexpr int kFullMoonAge = kMoonCycleDays / 2;  // day 15 = 30:00 real
    static constexpr int kEclipseDays[] = {12, 17};          // [U-35] 25:00 and 35:00 = noon
    static constexpr std::int64_t kCometMs = 30 * 60'000;    // [U-34][U-266] the comet starts at 30:00
    static constexpr std::int64_t kAlignStartMs = 60 * 60'000;  // [U-128] late game
    static constexpr std::int64_t kAlignEndMs = 66 * 60'000;    // [U-33] 3 game days

    constexpr explicit WorldClock(std::uint32_t tick_hz = 128) : hz_{tick_hz} {}  // [U-102]

    [[nodiscard]] constexpr std::uint32_t tick_hz() const { return hz_; }

    [[nodiscard]] constexpr SkyState at_tick(std::uint64_t tick) const {
        return at_ms(std::int64_t(tick * 1000 / hz_));
    }

    [[nodiscard]] static constexpr SkyState at_ms(std::int64_t ms) {
        SkyState s;
        s.real_ms = ms;
        const std::int64_t game_min = ms * kGameMinutesPerRealSecond / 1000;  // starts at 00:00 [U-31]
        s.game_day = int(game_min / kMinutesPerDay);
        s.minute_of_day = int(game_min % kMinutesPerDay);
        s.second_of_day = int(ms * kGameMinutesPerRealSecond * 60 / 1000 % (std::int64_t(kMinutesPerDay) * 60));
        s.is_day = s.minute_of_day >= kDayStart && s.minute_of_day < kDayEnd;

        s.moon_age_days = s.game_day % kMoonCycleDays;
        // Triangle 0 % → 100 % → 0 %, smooth over game minutes.
        const std::int64_t cycle_min = std::int64_t(kMoonCycleDays) * kMinutesPerDay;
        const std::int64_t half = cycle_min / 2;
        const std::int64_t age_min = game_min % cycle_min;
        const std::int64_t from_full = age_min > half ? age_min - half : half - age_min;
        s.moon_light_pct = int(100 - from_full * 100 / half);
        s.full_moon = s.moon_age_days == kFullMoonAge;
        s.new_moon = s.moon_age_days == 0;

        for (int d : kEclipseDays) s.solar_eclipse |= s.is_day && s.game_day == d;
        s.comet_landed = ms >= kCometMs;
        s.planetary_alignment = ms >= kAlignStartMs && ms < kAlignEndMs;
        return s;
    }

private:
    std::uint32_t hz_;
};

// [U-108] Night vision range depends on the moon phase. [U-115] New moon ≈ 0.
// [U-153][X-106] Light changes SMOOTHLY like a real day (smoothstep curves, no switch at 06:00 / 18:00):
//   full daylight 08:30–15:30, dusk 15:30 → 21:30, full night 21:30–02:30, dawn 02:30 → 08:30.
//   Night level = moonlight: 5 % (new moon) … 85 % (full moon) [X-39].
// [X-40] Solar eclipse: the light dims smoothly to new-moon darkness at its middle and comes back.
inline constexpr int kNightVisionMinPct = 5, kNightVisionMaxPct = 85;
inline constexpr int kDawnStartS = 2 * 3600 + 1800, kDayFullS = 8 * 3600 + 1800;   // 02:30, 08:30
inline constexpr int kDuskStartS = 15 * 3600 + 1800, kNightFullS = 21 * 3600 + 1800;  // 15:30, 21:30

// smoothstep on 0..1000 → 0..1000
[[nodiscard]] constexpr std::int64_t smooth1000(std::int64_t t) {
    t = t < 0 ? 0 : t > 1000 ? 1000 : t;
    return t * t * (3000 - 2 * t) / 1'000'000;
}
// 0 = full night, 1000 = full day
[[nodiscard]] constexpr std::int64_t sunlight1000(int sec) {
    if (sec >= kDayFullS && sec <= kDuskStartS) return 1000;
    if (sec > kDawnStartS && sec < kDayFullS)
        return smooth1000(std::int64_t(sec - kDawnStartS) * 1000 / (kDayFullS - kDawnStartS));
    if (sec > kDuskStartS && sec < kNightFullS)
        return 1000 - smooth1000(std::int64_t(sec - kDuskStartS) * 1000 / (kNightFullS - kDuskStartS));
    return 0;
}
[[nodiscard]] constexpr int vision_range_pct(const SkyState& s) {
    const std::int64_t night = kNightVisionMinPct + (kNightVisionMaxPct - kNightVisionMinPct) * s.moon_light_pct / 100;
    std::int64_t pct = night + (100 - night) * sunlight1000(s.second_of_day) / 1000;
    if (s.solar_eclipse) {  // V-curve: 100 % of the normal light at the edges → new-moon dark in the middle
        const std::int64_t mid = std::int64_t(s.game_day) * 120'000 + 60'000;
        const std::int64_t d = s.real_ms > mid ? s.real_ms - mid : mid - s.real_ms;
        const std::int64_t f = d >= 30'000 ? 1000 : d * 1000 / 30'000;
        pct = kNightVisionMinPct + (pct - kNightVisionMinPct) * f / 1000;
    }
    return int(pct);
}

// Compile-time checks against the plan.
static_assert(vision_range_pct(WorldClock::at_ms(0)) == kNightVisionMinPct);            // new moon night
static_assert(vision_range_pct(WorldClock::at_ms(30 * 60'000)) == kNightVisionMaxPct);  // full moon night
static_assert(!WorldClock::at_ms(0).is_day && WorldClock::at_ms(0).new_moon);       // [U-31]
static_assert(WorldClock::at_ms(30 * 60'000).full_moon);                            // [U-32]
static_assert(WorldClock::at_ms(30 * 60'000).moon_light_pct == 100);
static_assert(WorldClock::at_ms(25 * 60'000).solar_eclipse);                        // [U-35]
static_assert(WorldClock::at_ms(35 * 60'000).solar_eclipse);
static_assert(!WorldClock::at_ms(25 * 60'000 + 31'000).solar_eclipse);              // ends at sunset
static_assert(WorldClock::at_ms(60 * 60'000).planetary_alignment && !WorldClock::at_ms(66 * 60'000).planetary_alignment);  // [U-128]
static_assert(WorldClock::at_ms(60'000).minute_of_day == 12 * 60);                  // 1 min = 12 h [U-30]
// [U-153] smooth: noon full, 18:00 still fairly bright, 21:30 dark, 07:00 half-way up
static_assert(vision_range_pct(WorldClock::at_ms(60'000)) == 100);                   // 12:00
static_assert(vision_range_pct(WorldClock::at_ms(42'500)) == 100);                   // 08:30
static_assert(vision_range_pct(WorldClock::at_ms(90'000)) > 60 && vision_range_pct(WorldClock::at_ms(90'000)) < 100);  // 18:00
static_assert(vision_range_pct(WorldClock::at_ms(107'500)) < 15);                   // 21:30, new-moon week: dark
static_assert(vision_range_pct(WorldClock::at_ms(25 * 60'000)) == kNightVisionMinPct);   // eclipse middle

}  // namespace em
