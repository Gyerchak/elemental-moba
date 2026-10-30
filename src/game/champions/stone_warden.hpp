#pragma once
// [X-59] Champion: Stone Warden (earth tank) — Claude idea, all numbers are placeholders. Edit freely.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kStoneWarden{.name = "Stone Warden", .element = Element::Earth, .sub = S::Earth, .body = &kBroadBody, .hp = 66'000,
 .hp_lvl = 11'000, .attack = 6'400, .def = 3'800, .def_lvl = 480, .edef = 3'400, .ms = 33'000,
 .attack_range = 45,
 .w = {.name = "Earth Wall", .kind = K::Wall, .sub = S::Earth, .range = 700, .width = 320, .cast_ms = 450,
       .cd_ms = 16'000, .dur_ms = 4'000, .desc = "Stone wall, collapses after 4 s [U-24]"},
 .e = {.name = "Pillar Rise", .kind = K::Pillar, .sub = S::Earth, .dmg = 60, .dmg_rank = 30, .ep = 35, .range = 750,
       .radius = 110, .cast_ms = 500, .cd_ms = 14'000, .dur_ms = 3'000, .cc = C::Airborne, .cc_ms = 900,
       .desc = "Raises the ground 2 levels; units on it fly up [U-45]"},
 .r = {.name = "Earthquake", .kind = K::Field, .sub = S::Earth, .dmg = 20, .dmg_rank = 10, .ep = 12, .range = 650,
       .radius = 300, .cast_ms = 600, .cd_ms = 16'000, .dur_ms = 3'000, .cc = C::Slow, .cc_ms = 600, .cc_val = 40,
       .group = true, .desc = "Quake field; other earthbenders can feed it"},
 .t = {.name = "Rock Shield", .kind = K::Shield, .sub = S::EarthArmor, .dmg = 100, .dmg_rank = 50, .ep = 40, .cast_ms = 0,
       .cd_ms = 16'000, .dur_ms = 3'000, .desc = "Stone plates: a shield for 3 s"},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Mountain's Fist", .kind = K::Wave, .sub = S::Earth, .dmg = 200, .dmg_rank = 100, .ep = 70, .ad = 30,
       .range = 1'000, .width = 300, .speed = 900, .cast_ms = 700, .cd_ms = 110'000, .cc = C::Airborne,
       .cc_ms = 1'000, .knock = 150, .pierce = true, .desc = "Rolling earth wave, knocks up", .ground = true, .ultimate = true},
 .p = {.name = "Rooted", .kind = K::Passive, .passive = PassiveId::Rooted, .desc = "+20 % tenacity"},
 .colour = 0xa08050, .role = Role::Tank,
 .q_pool = {"Lift and Throw", "Raise Stone", "Stone Push", "Stone Wall", "Earth Pillar"}};  // [U-270] 3 Q options

}  // namespace em::data
