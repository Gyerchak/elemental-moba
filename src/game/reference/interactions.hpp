#pragma once
// [U-397] every interaction of the systems as text lines ("## " = a heading), generated from the game's own tables
#include <string>
#include <vector>

namespace em {
[[nodiscard]] std::vector<std::string> interaction_lines();
}  // namespace em
