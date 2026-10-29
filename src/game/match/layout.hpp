#pragma once
// [X-07] Match layout on the default map (cells, Fire Nation side). Earth Nation = point mirror [U-83].
// Point symmetry maps the Fire TOP lane onto the Earth BOT lane, so Earth's structures in lane L
// are the mirror of Fire's structures in lane 2−L. Docs: docs/match-layout.md.
#include <array>
#include <cstdint>
#include <span>
#include <utility>

namespace em {

using CellPt = std::pair<int, int>;
[[nodiscard]] constexpr CellPt mirror(CellPt p) { return {511 - p.first, 511 - p.second}; }

inline constexpr CellPt kBaseCell{40, 471};      // [U-06] Fire Nation base centre (stone core) — shop zone
inline constexpr CellPt kFountainCell{18, 493};  // spawn + heal [X-12]
inline constexpr CellPt kNexusCell{46, 465};     // "Nation Shrine" [X-07]
inline constexpr CellPt kNorthPoleCell{28, 28};  // [U-84]
inline constexpr CellPt kPortalCell{10, 10};     // [X-54][U-355] spirit portal: the far NW corner (~300 units in); SE = mirror
inline constexpr CellPt kSpiritIsland{256, 256}; // [X-86] Face Stealer pit, centre of the mid bridge

// Lane waypoints from the Fire base to the Earth base. 0 top, 1 mid, 2 bot.
inline constexpr std::array<CellPt, 9> kTopLane{{{40, 450}, {28, 400}, {28, 250}, {28, 100}, {50, 50},
                                                 {100, 28}, {250, 28}, {400, 28}, {450, 40}}};
inline constexpr std::array<CellPt, 9> kMidLane{{{62, 449}, {110, 401}, {160, 351}, {210, 301}, {256, 256},
                                                 {301, 210}, {351, 160}, {401, 110}, {449, 62}}};
inline constexpr std::array<CellPt, 9> kBotLane{{{61, 471}, {111, 483}, {261, 483}, {411, 483}, {461, 461},
                                                 {483, 411}, {483, 261}, {483, 111}, {471, 61}}};
[[nodiscard]] constexpr const std::array<CellPt, 9>& lane_path(int lane) {
    return lane == 0 ? kTopLane : lane == 1 ? kMidLane : kBotLane;
}

// Fire side structures per lane: tier 1 (outer), tier 2, tier 3 (base), inhibitor.
struct LaneStructs {
    CellPt t1, t2, t3, inhib;
};
inline constexpr std::array<LaneStructs, 3> kLaneStructs{{
    {{28, 175}, {28, 290}, {30, 385}, {32, 420}},     // top: the north pole (28,28) is between both t1 [U-84]
    {{196, 315}, {150, 361}, {104, 407}, {80, 431}},  // mid
    {{336, 483}, {221, 483}, {126, 483}, {91, 480}},  // bot
}};
inline constexpr std::array<CellPt, 2> kNexusTurrets{{{60, 458}, {53, 451}}};

// [U-82] Jungle camps (Fire side; mirrored for Earth side). Cleared around on match start.
inline constexpr std::array<CellPt, 5> kCamps{{{110, 300}, {150, 410}, {230, 430}, {80, 360}, {190, 360}}};
// [X-97] aggressive camps: cursed spirit, angry beast, bandits — attack whoever comes close
inline constexpr std::array<CellPt, 3> kAggroCamps{{{130, 250}, {270, 400}, {60, 250}}};
// [U-366] buff camps (Fire side; mirrored): Platypus Bear (Ember buff), Knowledge Seeker (Wan Shi Tong's Wisdom)
inline constexpr std::array<CellPt, 2> kBuffCamps{{{205, 310}, {110, 440}}};
// [U-364] epic monsters on islands in the sea (one each, not mirrored), reached by a shallow ford
inline constexpr CellPt kLionTurtleCell{170, 170};
inline constexpr CellPt kDragonsCell{341, 341};

}  // namespace em
