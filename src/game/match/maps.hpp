#pragma once
// [U-420] Maps. Every map is ONE MapDef in the same format: its terrain generator, the bases, the lanes (minion
// paths) and the structures on them, jungle camps, epic monsters, portals, the minion waves and its own resource pack
// (resourcepacks/<pack>/ — tried before the default pack while that map is played). Fire Nation side; the Earth Nation
// side is always the point mirror. Add a map = add a MapDef in maps.cpp.
#include <span>
#include <string_view>
#include <vector>

#include "core/grid.hpp"
#include "game/match/layout.hpp"

namespace em {

struct EpicSpot {
    CellPt cell;
    std::string_view monster;  // a name from kMonsters
};
// [U-420] minion waves of a map (the minion logic itself is the same everywhere)
struct MinionWaves {
    int melee = 3, ranged = 3;
    int siege_every = 3;               // a siege minion every N-th wave
    std::int64_t first_ms = 65'000, period_ms = 30'000;
};
inline constexpr CellPt kNoCell{-1, -1};

struct MapDef {
    std::string_view id, name, desc;
    Grid (*make)();                    // the terrain
    int team_size = 5;                 // players per team (bots fill up to this)
    std::string_view pack;             // its resource pack folder
    CellPt base, fountain, nexus;
    CellPt pole = kNoCell, portal = kNoCell, spirit_island = kNoCell;
    std::vector<std::vector<CellPt>> lanes;  // waypoints Fire base → Earth base; minions walk them
    std::vector<LaneStructs> lane_structs;   // per lane (Fire side); t3 = kNoCell → no third turret
    std::vector<CellPt> nexus_turrets, camps, aggro_camps, buff_camps, dark_spirits;
    std::vector<EpicSpot> epics;             // one each (not mirrored)
    bool epic_islands = false;               // epics stand on sea islands with a ford (the elemental world)
    MinionWaves minions;
};

[[nodiscard]] std::span<const MapDef> maps();
[[nodiscard]] const MapDef& cur_map();
void set_map(int index);                     // clamped; the server and every client set the same one
[[nodiscard]] int map_index(std::string_view id);  // −1 = unknown
[[nodiscard]] inline const std::vector<CellPt>& lane_path(int lane) { return cur_map().lanes[std::size_t(lane) % cur_map().lanes.size()]; }
[[nodiscard]] inline int lane_count() { return int(cur_map().lanes.size()); }

}  // namespace em
