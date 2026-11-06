#pragma once
// [U-546] Minions, monsters and structures are bots that use the game's mechanics: a game version can set them like champions.
//   unit.<name>.<key> = value   (versions/<name>/version.txt, the unit maker writes it)
//   hp, attack, defence, elemental_defence, move_speed, attack_speed, attack_range, size: % of the usual; body = Humanoid / Slim / Broad / Beast;
//   skill1, skill2, skill3 = the name of any skill: the unit casts it by itself at the enemies near it.
#include <array>
#include <string>
#include <vector>

namespace em {

struct SkillDef;
struct BodyDef;
struct UnitTweak {
    std::string name;
    int hp = 100, attack = 100, defence = 100, elemental_defence = 100, move_speed = 100, attack_speed = 100, attack_range = 100, size = 100;
    const BodyDef* body = nullptr;
    std::array<const SkillDef*, 3> skills{};
};
[[nodiscard]] std::vector<UnitTweak>& unit_tweaks();
[[nodiscard]] const UnitTweak* tweak_of(const std::string& name);
[[nodiscard]] std::vector<std::string> tweakable_units();  // every name there is: minions, structures, monsters

}  // namespace em
