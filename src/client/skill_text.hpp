#pragma once
// [U-168][U-187] Every number of a skill / champion as readable lines — used by the HUD tooltips
// and the champion wiki screen, so both always show the same.
#include <string>
#include <vector>

#include "game/content.hpp"

namespace em {

std::vector<std::string> skill_lines(const SkillDef& s);      // all fields that are not default, + the damage formula
std::vector<std::string> champion_lines(const ChampionDef& c); // base stats, growth per level, autoattack, body

}  // namespace em
