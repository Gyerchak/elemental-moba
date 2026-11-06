#pragma once
// [U-518] Own maps: custom_maps/<id>.emap = one file with everything a map is: the settings as text lines, then every cell of
// the terrain (run-length coded). The map maker (em_map_maker) writes them, the game and servers read them; a server sends its
// maps to everyone who joins. A file with the id of a built-in map replaces that map; removing the file brings the original back.
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "game/match/maps.hpp"

namespace em {

struct MapDoc {
    std::string id = "new_map", name = "New map", desc, pack;
    int team_size = 5;
    CellPt base{40, 471}, fountain{18, 493}, nexus{46, 465}, pole = kNoCell, portal = kNoCell, spirit_island = kNoCell;
    std::vector<std::vector<CellPt>> lanes;
    std::vector<LaneStructs> lane_structs;
    std::vector<CellPt> nexus_turrets, camps, aggro_camps, buff_camps, dark_spirits;
    std::vector<std::pair<CellPt, std::string>> epics;
    struct Spot {
        CellPt cell;
        std::string monster;
        bool mirrored = true;
    };
    std::vector<Spot> spots;
    bool epic_islands = false, exact_terrain = true;
    MinionWaves minions;
    std::uint16_t features = 0;
    std::vector<std::uint32_t> cells;  // kGridSize², row by row
};

[[nodiscard]] MapDoc doc_of(const MapDef& m);                                // a map of the game as an editable document
[[nodiscard]] std::string doc_to_blob(const MapDoc& d);                      // the file's bytes
[[nodiscard]] std::string doc_from_blob(std::string_view blob, MapDoc& out);  // "" = fine, else why not
[[nodiscard]] std::string map_id_of(std::string_view name);                  // "My Map!" → "my_map"

void load_custom_maps();                                                     // custom_maps/*.emap (sorted)
void set_custom_maps(const std::vector<std::string>& blobs);                 // a server's maps, in the server's order
[[nodiscard]] const std::vector<std::string>& custom_map_blobs();            // what is loaded (sent to players who join)
[[nodiscard]] bool is_builtin_map_id(std::string_view id);

}  // namespace em
