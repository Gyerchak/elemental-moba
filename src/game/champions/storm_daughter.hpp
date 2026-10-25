#pragma once
// [X-59] Champion: Storm Daughter (lightning carry) — Claude idea, all numbers are placeholders. Edit freely.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kStormDaughter{.name = "Storm Daughter", .element = Element::Fire, .sub = S::Lightning, .range = Range::Ranged,
 .resource = ResourceKind::Rage, .ep_lvl = 200, .hp = 52'000, .hp_lvl = 8'800, .attack = 6'000,  // [U-268] rage (Claude idea)
 .attack_lvl = 300, .as = 68, .as_lvl = 300, .attack_range = 520, .res_max = 10'000, .res_regen = 800,  // regen = drain out of combat
 .w = {.name = "Ion Field", .kind = K::Field, .sub = S::Lightning, .dmg = 12, .dmg_rank = 6, .ep = 8, .range = 800,
       .radius = 240, .cd_ms = 13'000, .cost = 35, .dur_ms = 2'500, .cc = C::Slow, .cc_ms = 600, .cc_val = 35,
       .desc = "Crackling field, slows"},
 .e = {.name = "Static Step", .kind = K::Blink, .sub = S::Lightning, .range = 380, .cast_ms = 0, .cd_ms = 16'000,
       .cost = 25, .desc = "Short blink"},
 .r = {.name = "Thunder Lance", .kind = K::Bolt, .sub = S::Lightning, .dmg = 110, .dmg_rank = 50, .ep = 60,
       .ad = 40, .range = 1'500, .width = 80, .speed = 3'000, .cast_ms = 500, .cd_ms = 14'000, .cost = 40,
       .cc = C::Stun, .cc_ms = 900, .desc = "Long bolt, stuns"},
 .t = {.name = "Static Snare", .kind = K::Burst, .sub = S::Lightning, .dmg = 50, .dmg_rank = 25, .ep = 35, .range = 750,
       .radius = 130, .cast_ms = 250, .cd_ms = 11'000, .cost = 25, .delay_ms = 500, .cc = C::Root, .cc_ms = 800,
       .desc = "Lightning snare at a point: roots after 0.5 s"},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Lightning Storm", .kind = K::Field, .sub = S::Lightning, .dmg = 60, .dmg_rank = 30, .ep = 35,
       .range = 1'200, .radius = 420, .cast_ms = 500, .cd_ms = 100'000, .cost = 60, .dur_ms = 4'000,
       .tick_ms = 500, .cc = C::Slow, .cc_ms = 600, .cc_val = 20, .desc = "Big storm, strikes every 0.5 s", .ultimate = true},
 .p = {.name = "Static", .kind = K::Passive, .passive = PassiveId::Static,
       .desc = "Autoattacks chain to units in the same water [X-58]"},
 .colour = 0xffd040, .role = Role::Marksman,
 .q_pool = {"Lightning Bolt", "Spark Volley", "Fire Bolt"}};  // [U-270] 3 Q options

}  // namespace em::data
