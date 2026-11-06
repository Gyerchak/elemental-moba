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
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <string_view>
#include <vector>

namespace em {

inline constexpr const char* kOfficialVersion = "official-0.1";

// [U-535] the lines of a version file, one by one (what the item maker and the loadout maker preview)
struct ItemDef;
std::string apply_item_line(const std::string& name, const std::string& key, const std::string& value);
std::string apply_unit_line(const std::string& name, const std::string& key, const std::string& value);  // [U-546]
std::string apply_setup_text(const std::string& kind, const std::string& name, const std::string& key, const std::string& value);
[[nodiscard]] std::vector<std::string> item_number_keys();
[[nodiscard]] std::int32_t item_number(const ItemDef& d, const std::string& key);
[[nodiscard]] std::vector<std::string> list_versions();  // official first, then versions/<name>/ folders
// Switch the game's numbers to this version. Returns "" when fine, else why it failed (the official numbers stay).
std::string apply_version(const std::string& name, int* changes = nullptr);
[[nodiscard]] const std::string& current_version();
[[nodiscard]] std::string version_description(const std::string& name);
[[nodiscard]] std::string next_server_version_name(const std::string& server_name, const std::string& date);  // [U-525]
[[nodiscard]] std::string version_id(const std::string& name);  // [U-375] computed from its files
[[nodiscard]] std::vector<std::pair<std::string, std::string>> version_files(const std::string& name);  // [U-374] relative path, text
bool store_version_files(const std::string& name, const std::vector<std::pair<std::string, std::string>>& files);
// [U-478] the active version's mods and assets
[[nodiscard]] bool mod_enabled(std::string_view mod);
[[nodiscard]] std::string version_pack_dir();     // "versions/<name>/pack" when it exists, else ""
[[nodiscard]] const std::string& version_linked_pack();  // `resource_pack = …` of the version ("" = none)

}  // namespace em
