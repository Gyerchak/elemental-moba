// Prints the sky every 30 real seconds for one moon cycle + the alignment (0:00–66:00) [U-30..U-35, U-107, U-108, U-122..U-126].
// Around each eclipse it prints every 3 s to show the drop to 0 % and the 6 s fire mute.
#include <print>
#include <string>

#include "core/core.hpp"  // whole core, so every compile-time check runs

// 3 s steps inside the eclipses (24:30–25:30, 34:30–35:30), else 30 s.
constexpr int k_step(int s) {
    const bool ecl = (s >= 24 * 60 + 30 && s < 25 * 60 + 30) || (s >= 34 * 60 + 30 && s < 35 * 60 + 30);
    return ecl ? 3 : 30;
}

int main() {
    using namespace em;
    std::println("real   day  game   light  moon%  vision%  fire%  fsize  water%  wsize  events");
    for (int s = 0; s <= 66 * 60; s += k_step(s)) {
        const SkyState k = WorldClock::at_ms(std::int64_t(s) * 1000);
        std::string ev;
        if (k.new_moon) ev += "new-moon ";
        if (k.full_moon) ev += "FULL-MOON ";
        if (k.solar_eclipse) ev += "ECLIPSE ";
        if (k.comet_landed && k.real_ms == WorldClock::kCometMs) ev += "COMET ";
        if (k.planetary_alignment) ev += "ALIGNMENT ";
        if (sky_boost(Element::Fire, k).muted) ev += "FIRE-MUTED ";
        const SkyBoost f = sky_boost(Element::Fire, k), w = sky_boost(Element::Water, k);
        std::println("{:02}:{:02}  {:>3}  {:02}:{:02}  {:<5}  {:>5}  {:>7}  {:>5}  {:>5}  {:>6}  {:>5}  {}", s / 60,
                     s % 60, k.game_day, k.minute_of_day / 60, k.minute_of_day % 60, k.is_day ? "day" : "night",
                     k.moon_light_pct, vision_range_pct(k), f.power_pct, f.size_pct, w.power_pct, w.size_pct, ev);
    }
}
