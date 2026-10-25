#pragma once
// [X-59] Champion: Frost Hunter (ice skirmisher) — Claude idea, all numbers are placeholders. Edit freely.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kFrostHunter{.name = "Frost Hunter", .element = Element::Water, .sub = S::Ice, .resource = ResourceKind::Water,
 .hp = 58'000, .attack = 6'400, .attack_lvl = 380, .as = 70, .attack_range = 45, .res_max = 30'000,
 .res_regen = 300,
 .w = {.name = "Ice Slide", .kind = K::Dash, .sub = S::Ice, .range = 500, .width = 80, .speed = 1'500, .cast_ms = 0,
       .cd_ms = 11'000, .cost = 30, .desc = "Slide on ice"},
 .e = {.name = "Frost Ring", .kind = K::Burst, .sub = S::Ice, .dmg = 70, .dmg_rank = 35, .ep = 45, .radius = 260,
       .cast_ms = 200, .cd_ms = 13'000, .cost = 50, .cc = C::Frozen, .cc_ms = 1'000,
       .desc = "Freezes around you (frozen = no heat [U-143])"},
 .r = {.name = "Ice Wall", .kind = K::Wall, .sub = S::Ice, .range = 650, .width = 280, .cast_ms = 250,
       .cd_ms = 15'000, .cost = 50, .dur_ms = 2'500, .desc = "Ice wall; needs a big water source nearby [U-294]"},
 .t = {.name = "Frost Trap", .kind = K::Field, .sub = S::Ice, .dmg = 12, .dmg_rank = 6, .ep = 8, .ad = 10, .range = 650,
       .radius = 160, .cast_ms = 200, .cd_ms = 13'000, .dur_ms = 3'000, .cc = C::Slow, .cc_ms = 600, .cc_val = 45,
       .desc = "Icy ground: slows 45 %"},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Glacier Spike", .kind = K::Pillar, .sub = S::Ice, .dmg = 220, .dmg_rank = 110, .ep = 80,
       .range = 800, .radius = 180, .cast_ms = 500, .cd_ms = 100'000, .cost = 100, .dur_ms = 2'500,
       .cc = C::Frozen, .cc_ms = 1'500, .desc = "Ice pillar that freezes", .ultimate = true},
 .p = {.name = "Frostbite", .kind = K::Passive, .passive = PassiveId::Frostbite, .desc = "Autoattacks slow 15 %"},
 .colour = 0xa0e8ff, .role = Role::Fighter,
 .q_pool = {"Ice Shards", "Ice Spear", "Water Whip"}};  // [U-270] 3 Q options

}  // namespace em::data
