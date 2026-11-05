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
// [U-510] Easier: em_champion_maker writes these files. Added for it:
//     body = Humanoid | Slim | Broad | Beast   or   body.part1 = forward side radius swing   (up to 6: the base hitbox)
//     model_size = +10                         # + % model and hitbox
//     w.geo = Square | Triangle | Ellipse | Diamond | Hexagon | Star | Custom    w.poly = 100,0 40,60 -80,60 (Custom: forward,side in % of radius)
//     w.sweet_zone = Band  w.sweet_from = 40  w.sweet_size = 20                  # custom sweet spot: from 40 % of the shape, 20 % wide
//     w.key1 = 0 40        w.key2 = 100 200 200 90 0   (at % of its life, size %, length %, turned, moved)   w.key_loop_ms = 800
//     w.needs = Mud Earth  w.needs_range = 240   (Earth Water Mud Rock Ice Lava Tree Night Day Metal Fire: all must be near)
//     w.vis.sheet = my_skill.png  w.vis.frames = 8  w.vis.fps = 12  w.vis.mode = 0 | 1 | 2   (resourcepacks/.../skills/: stretched to the hitbox)
//     Sounds: resourcepacks/<pack>/sounds/champions/<id>/cast_w.wav
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
