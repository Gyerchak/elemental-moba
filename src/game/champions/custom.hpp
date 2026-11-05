#pragma once
// [U-210][U-277] Parse one `.champ` text (format: client/custom_champions.hpp). Returns "" when fine, else the reason.
// The parsed champion is NOT registered — call register_champion() for that.
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "game/content.hpp"

namespace em {

[[nodiscard]] [[nodiscard]] std::vector<std::pair<std::string_view, std::string_view>> skill_template_list();  // [U-510] @name, what it is
std::string parse_champion_text(const std::string& text, ChampionDef& out);

}  // namespace em
