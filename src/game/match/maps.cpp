// [U-420][U-421] The maps (one MapDef each, same format). The first one is the original map.
#include "game/match/maps.hpp"

#include <algorithm>

#include "map/default_map.hpp"

namespace em {

namespace {
std::vector<MapDef> make_maps() {
    std::vector<MapDef> m;
    // ---- the original map: 3 lanes, sea along the diagonal, poles with portals, epic sea islands [X-07]
    m.push_back(MapDef{
        .id = "elemental_world", .name = "Elemental World",
        .desc = "5 v 5, three lanes, the sea between the nations, poles with spirit portals, epic monsters on sea islands",
        .make = make_default_map, .team_size = 5, .pack = "map_elemental_world",
        .base = {40, 471}, .fountain = {18, 493}, .nexus = {46, 465},
        .pole = {28, 28}, .portal = {10, 10}, .spirit_island = {256, 256},
        .lanes = {{{40, 450}, {28, 400}, {28, 250}, {28, 100}, {50, 50}, {100, 28}, {250, 28}, {400, 28}, {450, 40}},
                  {{62, 449}, {110, 401}, {160, 351}, {210, 301}, {256, 256}, {301, 210}, {351, 160}, {401, 110}, {449, 62}},
                  {{61, 471}, {111, 483}, {261, 483}, {411, 483}, {461, 461}, {483, 411}, {483, 261}, {483, 111}, {471, 61}}},
        .lane_structs = {{{28, 175}, {28, 290}, {30, 385}, {32, 420}},
                         {{196, 315}, {150, 361}, {104, 407}, {80, 431}},
                         {{336, 483}, {221, 483}, {126, 483}, {91, 480}}},
        .nexus_turrets = {{60, 458}, {53, 451}},
        .camps = {{110, 300}, {150, 410}, {230, 430}, {80, 360}, {190, 360}},
        .aggro_camps = {{130, 250}, {270, 400}, {60, 250}},
        .buff_camps = {{150, 300}, {174, 442}},
        .dark_spirits = {{26, 14}, {14, 26}},
        .epics = {{{170, 170}, "Lion Turtle"}, {{341, 341}, "Sun Dragons"}, {{299, 299}, "Face Stealer"}},
        .epic_islands = true,
        .minions = {.melee = 3, .ranged = 3, .siege_every = 3, .first_ms = 65'000, .period_ms = 30'000},
    });
    // ---- [U-421] Twisted Grove: 3 v 3, two lanes (top / bottom), a jungle between them, the Canyon Crawler Queen
    // in the middle pit (a Claude idea in the spirit of LoL's old two-lane map). Bases west (Fire) and east (Earth).
    m.push_back(MapDef{
        .id = "twisted_grove", .name = "Twisted Grove",
        .desc = "3 v 3, two lanes around a dark jungle, the Canyon Crawler Queen in the centre pit",
        .make = make_twisted_grove, .team_size = 3, .pack = "map_twisted_grove",
        .base = {52, 256}, .fountain = {28, 256}, .nexus = {64, 256},
        .lanes = {{{72, 236}, {100, 168}, {170, 150}, {256, 146}, {342, 150}, {412, 168}, {440, 236}},
                  {{72, 275}, {99, 343}, {169, 361}, {255, 365}, {341, 361}, {411, 343}, {439, 275}}},
        .lane_structs = {{{176, 150}, {118, 160}, kNoCell, {86, 205}},
                         {{175, 361}, {117, 351}, kNoCell, {85, 306}}},
        .nexus_turrets = {{78, 246}, {78, 266}},
        .camps = {{150, 215}, {150, 296}, {210, 256}},
        .aggro_camps = {{205, 200}, {205, 312}},
        .buff_camps = {{120, 256}},
        .epics = {{{256, 256}, "Canyon Crawler Queen"}},
        .minions = {.melee = 3, .ranged = 3, .siege_every = 3, .first_ms = 45'000, .period_ms = 30'000},
    });
    return m;
}
int g_map = 0;
}  // namespace

std::span<const MapDef> maps() {
    static const std::vector<MapDef> all = make_maps();
    return all;
}
const MapDef& cur_map() { return maps()[std::size_t(g_map)]; }
void set_map(int index) { g_map = std::clamp(index, 0, int(maps().size()) - 1); }
int map_index(std::string_view id) {
    for (std::size_t i = 0; i < maps().size(); ++i)
        if (maps()[i].id == id) return int(i);
    return -1;
}

}  // namespace em
