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
    static constexpr int kFullMoonAge = kMoonCycleDays / 2;  // 30 min after the new moon: 55:00 real
    // [U-515] the match starts at the beginning of the night (18:00)
    static constexpr int kStartMinute = 18 * 60;
    // [U-515] the exact new moon is at 25:00 real (sunrise); that day's sun is eclipsed: 25:00-26:00, darkest at noon 25:30.
    // An eclipse needs a new moon, so the next one comes a whole moon cycle later (85:00).
    static constexpr std::int64_t kNewMoonMs = 25 * 60'000;
    static constexpr std::int64_t kMoonCycleMs = 60 * 60'000;
    static constexpr std::int64_t kCometMs = 29 * 60'000;    // [U-34][U-266][U-515] the comet starts at 29:00
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
        const std::int64_t game_min = kStartMinute + ms * kGameMinutesPerRealSecond / 1000;  // [U-515]
        s.game_day = int(game_min / kMinutesPerDay);
        s.minute_of_day = int(game_min % kMinutesPerDay);
        s.second_of_day = int((std::int64_t(kStartMinute) * 60 + ms * kGameMinutesPerRealSecond * 60 / 1000) % (std::int64_t(kMinutesPerDay) * 60));
        s.is_day = s.minute_of_day >= kDayStart && s.minute_of_day < kDayEnd;

        // Triangle 0 % → 100 % → 0 %, smooth over game minutes. Age 0 starts exactly at the new moon [U-515]
        const std::int64_t cycle_min = std::int64_t(kMoonCycleDays) * kMinutesPerDay;
        const std::int64_t half = cycle_min / 2;
        const std::int64_t new_moon_min = kStartMinute + kNewMoonMs * kGameMinutesPerRealSecond / 1000;
        const std::int64_t age_min = ((game_min - new_moon_min) % cycle_min + cycle_min) % cycle_min;
        s.moon_age_days = int(age_min / kMinutesPerDay);
        const std::int64_t from_full = age_min > half ? age_min - half : half - age_min;
        s.moon_light_pct = int(100 - from_full * 100 / half);
        s.full_moon = s.moon_age_days == kFullMoonAge;
        s.new_moon = s.moon_age_days == 0;

        s.solar_eclipse = s.is_day && s.new_moon;  // [U-515] only a new moon can cover the sun
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
// [U-515] real time of noon of a game day (game day 0 is the 6 h from the 18:00 start to midnight)
[[nodiscard]] constexpr std::int64_t eclipse_noon_ms(int game_day) { return std::int64_t(game_day) * 120'000 - 30'000; }
[[nodiscard]] constexpr int vision_range_pct(const SkyState& s) {
    const std::int64_t night = kNightVisionMinPct + (kNightVisionMaxPct - kNightVisionMinPct) * s.moon_light_pct / 100;
    std::int64_t pct = night + (100 - night) * sunlight1000(s.second_of_day) / 1000;
    if (s.solar_eclipse) {  // V-curve: 100 % of the normal light at the edges → new-moon dark in the middle
        const std::int64_t mid = eclipse_noon_ms(s.game_day);
        const std::int64_t d = s.real_ms > mid ? s.real_ms - mid : mid - s.real_ms;
        const std::int64_t f = d >= 30'000 ? 1000 : d * 1000 / 30'000;
        pct = kNightVisionMinPct + (pct - kNightVisionMinPct) * f / 1000;
    }
    return int(pct);
}

// Compile-time checks against the plan. Real minutes: 0:00 = 18:00, 0:30 = midnight, 1:00 = 06:00, 1:30 = noon.
static_assert(WorldClock::at_ms(0).minute_of_day == 18 * 60 && !WorldClock::at_ms(0).is_day);   // [U-515] the night begins
static_assert(WorldClock::at_ms(60'000).minute_of_day == 6 * 60);                               // 1 min = 12 h [U-30]
static_assert(WorldClock::at_ms(25 * 60'000).new_moon && WorldClock::at_ms(25 * 60'000).moon_light_pct == 0);  // [U-515] exactly
static_assert(!WorldClock::at_ms(25 * 60'000 - 1'000).new_moon);
static_assert(WorldClock::at_ms(25 * 60'000).solar_eclipse && WorldClock::at_ms(25 * 60'000 + 59'000).solar_eclipse);
static_assert(!WorldClock::at_ms(25 * 60'000 - 1'000).solar_eclipse && !WorldClock::at_ms(26 * 60'000).solar_eclipse);
static_assert(!WorldClock::at_ms(35 * 60'000).solar_eclipse);
static_assert(vision_range_pct(WorldClock::at_ms(25 * 60'000 + 30'000)) == kNightVisionMinPct);  // eclipse middle = noon
static_assert(!WorldClock::at_ms(29 * 60'000 - 1).comet_landed && WorldClock::at_ms(29 * 60'000).comet_landed);  // [U-515]
static_assert(WorldClock::at_ms(55 * 60'000).full_moon && WorldClock::at_ms(55 * 60'000).moon_light_pct == 100);
static_assert(WorldClock::at_ms(60 * 60'000).planetary_alignment && !WorldClock::at_ms(66 * 60'000).planetary_alignment);  // [U-128]
// [U-153] smooth: noon full, 18:00 still fairly bright, 21:30 before the new moon dark, 08:30 full
static_assert(vision_range_pct(WorldClock::at_ms(90'000)) == 100);                                       // 12:00
static_assert(vision_range_pct(WorldClock::at_ms(72'500)) == 100);                                       // 08:30
static_assert(vision_range_pct(WorldClock::at_ms(120'000)) > 60 && vision_range_pct(WorldClock::at_ms(120'000)) < 100);  // 18:00
static_assert(vision_range_pct(WorldClock::at_ms(25 * 60'000 - 42'500)) < 15);                           // 21:30

}  // namespace em
