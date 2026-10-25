#pragma once
// [U-210] Own champions from text files `custom_champions/<file>.champ`. Example file:
//
//     name = Ash Monk           # must be new
//     base = Ember Monk         # copy everything from this champion first
//     hp = 560                  # health (points)          attack, ep_per_level, defence, elemental_defence
//     move_speed = 340          # units / s                attack_range (edge to edge), attack_speed (× 100)
//     colour = ff9933           # hex
//     w = Blaze Ring            # any existing skill by name (champion skills, Q options, specials)
//     y = Volcano              # Y = the ultimate slot [U-291]
//     y_unlock_level = 6       # rank rules per skill: <p|w|e|r|t|y>_ultimate / _unlock_level / _rank_step / _rank_cap
//
// With ANY problem (unknown key, unknown skill, bad number, duplicate name, invalid kit) the file is ignored.
#include <string>
#include <vector>

namespace em {

struct CustomChampionFile {
    std::string file;   // file name
    std::string name;   // champion name (when ok)
    bool ok = false;
    std::string error;  // why it is ignored
};

[[nodiscard]] std::vector<CustomChampionFile> scan_custom_champions();
// Registers the enabled, valid files into the roster. Call once before any match. Returns how many joined.
int load_custom_champions(const std::vector<std::string>& enabled_files);

}  // namespace em
