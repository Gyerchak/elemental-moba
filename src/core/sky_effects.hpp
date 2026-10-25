#pragma once
// [U-107][U-122..U-126] Sky events give buffs / nerfs to different elemental mages:
// power, skill sizes, XP speed, muting. Numbers are placeholders (X-50..X-53, X-67).
#include <cstdint>

#include "core/elements.hpp"
#include "core/world_clock.hpp"

namespace em {

inline constexpr std::int64_t kCometBuffMs = 4 * 60'000;  // [X-50] 2 game days
inline constexpr std::int64_t kEclipseHalfMs = 30'000;     // eclipse = day time of one game day (1:00 real)
inline constexpr std::int64_t kEclipseMuteMs = 6'000;      // [U-125] fire muted 6 s in the middle

// Everything a sky moment does to one element. All in %, 100 = no change.
struct SkyBoost {
    int power_pct = 100;  // multiplies ability damage + elemental autoattacks
    int size_pct = 100;   // range + length [U-123]
    int width_pct = 100, radius_pct = 100, speed_pct = 100;  // [U-123][U-264]
    int dur_pct = 100;    // [U-264] how long the skill lasts (fields, shields, buffs, walls…)
    int dot_pct = 100;    // [U-264] how long damage over time lasts (burns, burning fields)
    int xp_pct = 100;     // XP gain speed [U-122]
    bool muted = false;   // no bending abilities, no autoattacks [U-125]
};

// [U-124] 0 ms before/after the eclipse middle → 100 %, middle → 0 %, linear between.
[[nodiscard]] constexpr std::int64_t eclipse_mid_ms(const SkyState& s) {
    return std::int64_t(s.game_day) * 120'000 + 60'000;  // noon of that game day
}
[[nodiscard]] constexpr int eclipse_factor_pct(const SkyState& s) {
    if (!s.solar_eclipse) return 100;
    const std::int64_t d = s.real_ms - eclipse_mid_ms(s);
    return int((d < 0 ? -d : d) * 100 / kEclipseHalfMs);
}

[[nodiscard]] constexpr SkyBoost sky_boost(Element e, const SkyState& s) {
    SkyBoost b;
    const bool comet = s.comet_landed && s.real_ms < WorldClock::kCometMs + kCometBuffMs;
    switch (e) {
        case Element::Fire:
            if (s.is_day) b.power_pct += 5;  // [X-53]
            if (comet) {                     // [U-123] comet > full moon [X-67]
                b.power_pct += 75;
                b.size_pct += 40, b.width_pct += 40, b.radius_pct += 40, b.speed_pct += 40;
                b.xp_pct += 40;
            }
            break;
        case Element::Water:
            if (!s.is_day) b.power_pct += 5;  // [X-53]
            if (s.full_moon) {                // [U-123] [X-67]
                b.power_pct += 50;
                b.size_pct += 25, b.width_pct += 25, b.radius_pct += 25, b.speed_pct += 25;
                b.xp_pct += 25;
            }
            if (s.new_moon) b.power_pct -= 10;  // [X-51]
            break;
        case Element::Earth:
        case Element::Air:
        case Element::None:
            break;
    }
    // [U-126][U-264] every MAGIC skill of every bender (not physical ones, not non-benders):
    // +25 % power, width, radius, speed, duration and damage-over-time duration
    if (s.planetary_alignment && e != Element::None) {
        b.power_pct += 25;
        b.width_pct += 25, b.radius_pct += 25, b.speed_pct += 25, b.dur_pct += 25, b.dot_pct += 25;
    }
    if (e == Element::Fire && s.solar_eclipse) {  // [U-124][U-125][X-52] Black Sun
        b.power_pct = b.power_pct * eclipse_factor_pct(s) / 100;
        const std::int64_t d = s.real_ms - eclipse_mid_ms(s);
        b.muted = d >= -kEclipseMuteMs / 2 && d < kEclipseMuteMs / 2;
    }
    return b;
}

[[nodiscard]] constexpr int element_power_pct(Element e, const SkyState& s) { return sky_boost(e, s).power_pct; }

// [U-139] Sun height: firebenders regenerate fuel faster by day, most at noon.
// [X-76] Triangle: +0 % at sunrise (06:00) → +30 % at noon → +0 % at sunset (18:00). Eclipse = no sun.
inline constexpr int kNoonFuelBonusPct = 30;
[[nodiscard]] constexpr int sun_height_pct(const SkyState& s) {  // 0 at horizon / night, 100 at noon
    if (!s.is_day || s.solar_eclipse) return 0;
    const int from_noon = s.minute_of_day > 720 ? s.minute_of_day - 720 : 720 - s.minute_of_day;
    return 100 - from_noon * 100 / 360;
}
[[nodiscard]] constexpr int fuel_regen_pct(const SkyState& s) { return 100 + kNoonFuelBonusPct * sun_height_pct(s) / 100; }

// [U-138] Comet: unlimited fuel for firebenders. [X-77] = the comet buff window 30:00–34:00 [U-266].
[[nodiscard]] constexpr bool unlimited_fuel(const SkyState& s) {
    return s.comet_landed && s.real_ms < WorldClock::kCometMs + kCometBuffMs;
}


// [U-78] Bloodbending special skill only during full moon.
[[nodiscard]] constexpr bool bloodbending_allowed(const SkyState& s) { return s.full_moon; }

// Checks. Eclipse #1 24:30–25:30, middle 25:00. The comet lands at 30:00 [U-266].
constexpr SkyState sky_at_s(int sec) { return WorldClock::at_ms(std::int64_t(sec) * 1000); }
static_assert(eclipse_factor_pct(sky_at_s(24 * 60 + 30)) == 100 && eclipse_factor_pct(sky_at_s(24 * 60 + 45)) == 50);
static_assert(eclipse_factor_pct(sky_at_s(25 * 60)) == 0 && eclipse_factor_pct(sky_at_s(25 * 60 + 15)) == 50);
static_assert(element_power_pct(Element::Fire, sky_at_s(25 * 60)) == 0 && sky_boost(Element::Fire, sky_at_s(25 * 60)).muted);
static_assert(sky_boost(Element::Fire, sky_at_s(24 * 60 + 57)).muted && !sky_boost(Element::Fire, sky_at_s(25 * 60 + 3)).muted);
static_assert(!sky_boost(Element::Water, sky_at_s(25 * 60)).muted);
// 25:15: half eclipse × (100 + 5 day) = 52 %. 31:00 (day): 100 + 5 day + 75 comet = 180 %.
static_assert(element_power_pct(Element::Fire, sky_at_s(25 * 60 + 15)) == 52);
static_assert(sky_boost(Element::Fire, sky_at_s(31 * 60)).power_pct == 180 && sky_boost(Element::Fire, sky_at_s(31 * 60)).size_pct == 140);
// Full moon night: 100 + 5 night + 50 = 155 %.
static_assert(element_power_pct(Element::Water, sky_at_s(30 * 60)) == 155);
static_assert(sky_boost(Element::Earth, sky_at_s(61 * 60)).power_pct == 125 && sky_boost(Element::Earth, sky_at_s(61 * 60)).dot_pct == 125);
static_assert(sky_boost(Element::None, sky_at_s(61 * 60)).power_pct == 100);  // non-benders: no alignment
static_assert(fuel_regen_pct(sky_at_s(60)) == 130 && fuel_regen_pct(sky_at_s(30)) == 100);  // noon / sunrise
static_assert(fuel_regen_pct(sky_at_s(45)) == 115 && fuel_regen_pct(sky_at_s(0)) == 100);   // 09:00 / night
static_assert(unlimited_fuel(sky_at_s(31 * 60)) && !unlimited_fuel(sky_at_s(34 * 60)) && !unlimited_fuel(sky_at_s(29 * 60)));

}  // namespace em
