#pragma once
// [U-83..U-89] Default map generator. Layout explained in docs/map-layout.md.
#include "core/grid.hpp"

namespace em {

enum class Team : std::uint8_t { FireNation, EarthNation, Neutral };  // [U-06] + jungle monsters [U-82]

// Fire Nation half = y > x (bottom-left base), Earth Nation half = y < x.
[[nodiscard]] constexpr Team side_of(int x, int y) { return y > x ? Team::FireNation : Team::EarthNation; }

[[nodiscard]] Grid make_default_map();
[[nodiscard]] Grid make_twisted_grove();  // [U-421] the 3 v 3 two-lane map (map/twisted_grove.cpp)

}  // namespace em
