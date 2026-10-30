#pragma once
// [X-07] Match layout on the default map (cells, Fire Nation side). Earth Nation = point mirror [U-83].
// Point symmetry maps the Fire TOP lane onto the Earth BOT lane, so Earth's structures in lane L
// are the mirror of Fire's structures in lane (lanes − 1 − L). Docs: docs/match-layout.md.
#include <array>
#include <cstdint>
#include <span>
#include <utility>

namespace em {

using CellPt = std::pair<int, int>;
[[nodiscard]] constexpr CellPt mirror(CellPt p) { return {511 - p.first, 511 - p.second}; }

// [U-420] the positions of a map are in its MapDef (game/match/maps.hpp); the original map is "elemental_world"
// (game/match/maps.cpp).

// Fire side structures per lane: tier 1 (outer), tier 2, tier 3 (base), inhibitor.
struct LaneStructs {
    CellPt t1, t2, t3, inhib;  // t3 = {-1, -1}: a lane with two turrets (small maps)
};
}  // namespace em
