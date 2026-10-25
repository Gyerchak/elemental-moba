#pragma once
// [U-210][U-277] Parse one `.champ` text (format: client/custom_champions.hpp). Returns "" when fine, else the reason.
// The parsed champion is NOT registered — call register_champion() for that.
#include <string>

#include "game/content.hpp"

namespace em {

[[nodiscard]] std::string parse_champion_text(const std::string& text, ChampionDef& out);

}  // namespace em
