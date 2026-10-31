#pragma once
// [U-95] Own game versions: a copy of the official game with a few changed numbers (balance tests).
//
//   versions/<name>/version.txt
//       description = A small balance test
//       (any number: 570, +40, -20%, +30%, *1.5 — relative to the official value [U-482])
//       champion.Ember Monk.hp = 570              # champion stat (same keys as own champions: hp, attack, ep_per_level,
//                                                 #   defence, elemental_defence, move_speed, attack_range, attack_speed)
//       skill.Fire Bolt.damage = 90               # skill number: damage, damage_per_rank, ep, ad, range, width, speed,
//                                                 #   radius, cast_ms, cooldown_ms, cost, duration_ms, cc_ms, cc_value,
//                                                 #   ultimate (0/1), unlock_level, rank_step, rank_cap  [U-291]
//
//       mods = lol                                # [U-478] content that exists ONLY in this version (items with .mod = "lol")
//       resource_pack = my_pack                   # [U-478] a pack from resourcepacks/ that comes with the version
//   versions/<name>/pack/                         # [U-478] the version's OWN assets (a resource pack: icons/items/…)
//
// A skill change applies to every skill with that name (champion skills, Q options, specials).
// The server tells joining players its version; a player who has the same folder switches to it.
// Any mistake (unknown champion / skill / key, bad number) = the version is refused with the reason.
#include <string>
#include <string_view>
#include <vector>

namespace em {

inline constexpr const char* kOfficialVersion = "official-0.1";

[[nodiscard]] std::vector<std::string> list_versions();  // official first, then versions/<name>/ folders
// Switch the game's numbers to this version. Returns "" when fine, else why it failed (the official numbers stay).
std::string apply_version(const std::string& name, int* changes = nullptr);
[[nodiscard]] const std::string& current_version();
[[nodiscard]] std::string version_description(const std::string& name);
// [U-478] the active version's mods and assets
[[nodiscard]] bool mod_enabled(std::string_view mod);
[[nodiscard]] std::string version_pack_dir();     // "versions/<name>/pack" when it exists, else ""
[[nodiscard]] const std::string& version_linked_pack();  // `resource_pack = …` of the version ("" = none)

}  // namespace em
