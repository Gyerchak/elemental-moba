#pragma once
// [X-59] Champion: Tide Healer (water support) — Claude idea, all numbers are placeholders. Edit freely.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kTideHealer{.name = "Tide Healer", .element = Element::Water, .sub = S::Water, .range = Range::Ranged,
 .resource = ResourceKind::Water, .ep_lvl = 250, .hp = 52'000, .attack = 4'800, .attack_range = 500,
 .res_max = 30'000, .res_regen = 300,
 .w = {.name = "Healing Water", .kind = K::Heal, .sub = S::Water, .dmg = 70, .dmg_rank = 35, .ep = 40, .range = 700,
       .radius = 220, .cast_ms = 300, .cd_ms = 12'000, .cost = 60, .ally = true, .desc = "Heals allies [U-78]"},
 .e = {.name = "Ice Wall", .kind = K::Wall, .sub = S::Ice, .range = 700, .width = 300, .cast_ms = 300,
       .cd_ms = 14'000, .cost = 50, .dur_ms = 2'500, .desc = "Ice wall: weaker, melts; needs a big water source nearby [U-18][U-15][U-294]"},
 .r = {.name = "Whirlpool", .kind = K::Field, .sub = S::Water, .dmg = 18, .dmg_rank = 9, .ep = 10, .range = 800,
       .radius = 240, .cast_ms = 400, .cd_ms = 14'000, .cost = 60, .dur_ms = 3'000, .cc = C::Slow, .cc_ms = 600,
       .cc_val = 40, .group = true, .desc = "Whirlpool; other waterbenders can feed it"},
 .t = {.name = "Purifying Wave", .kind = K::Heal, .sub = S::Water, .dmg = 50, .dmg_rank = 25, .ep = 30, .range = 600,
       .radius = 260, .cast_ms = 250, .cd_ms = 14'000, .cost = 50, .ally = true, .desc = "A wave of clean water heals allies"},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Tsunami", .kind = K::Wave, .sub = S::Water, .dmg = 180, .dmg_rank = 90, .ep = 70, .range = 1'400,
       .width = 520, .speed = 800, .cast_ms = 600, .cd_ms = 110'000, .cost = 100, .cc = C::Slow, .cc_ms = 2'000,
       .cc_val = 50, .knock = 250, .pierce = true, .desc = "Giant wave", .ultimate = true},
 .p = {.name = "Tide", .kind = K::Passive, .passive = PassiveId::Tide, .desc = "Heals +30 % near water"},
 .colour = 0x40a8ff, .role = Role::Support,
 .q_pool = {"Water Whip", "Water Wave", "Ice Shards"}};  // [U-270] 3 Q options

}  // namespace em::data
