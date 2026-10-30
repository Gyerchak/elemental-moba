#pragma once
// [X-59] Champion: Iron Sentinel (metal fighter) — Claude idea, all numbers are placeholders. Edit freely.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kIronSentinel{.name = "Iron Sentinel", .element = Element::Earth, .sub = S::Metal, .body = &kBroadBody, .mage = false,
 .hp = 62'000, .hp_lvl = 10'500, .attack = 6'800, .attack_lvl = 420, .def = 3'600, .ms = 34'000,
 .attack_range = 50,
 .w = {.name = "Iron Armour", .kind = K::Shield, .sub = S::MetalArmor, .dmg = 120, .dmg_rank = 60, .bonus_ad = 120,
       .cast_ms = 0, .cd_ms = 18'000, .dur_ms = 4'000, .size_bonus = 1'500,
       .desc = "Shield + 15 % bigger model [U-129]"},
 .e = {.name = "Charge", .kind = K::Dash, .sub = S::Metal, .dmg = 50, .dmg_rank = 30, .ad = 60, .range = 500,
       .width = 120, .speed = 1'400, .cast_ms = 0, .cd_ms = 12'000, .cc = C::Stun, .cc_ms = 600, .knock = 80,
       .desc = "Charge, stuns what you hit"},
 .r = {.name = "Spike Field", .kind = K::Field, .sub = S::Metal, .dmg = 25, .dmg_rank = 12, .ad = 30, .range = 600,
       .radius = 250, .cd_ms = 14'000, .dur_ms = 3'000, .cc = C::Slow, .cc_ms = 600, .cc_val = 35,
       .desc = "Metal spikes from the ground"},
 .t = {.name = "Steel Barricade", .kind = K::Wall, .sub = S::Metal, .range = 650, .width = 220, .cast_ms = 300, .cd_ms = 20'000,
       .dur_ms = 3'000, .desc = "A short metal wall (the strongest walls [U-288])"},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Iron Prison", .kind = K::Wall, .sub = S::Metal, .dmg = 150, .dmg_rank = 75, .ad = 80, .range = 700,
       .radius = 160, .cast_ms = 500, .cd_ms = 100'000, .dur_ms = 3'000, .cc = C::Root, .cc_ms = 1'000,
       .desc = "Ring of metal walls around a point", .ultimate = true},
 .p = {.name = "Metal Skin", .kind = K::Passive, .passive = PassiveId::MetalSkin, .desc = "+15 defence"},
 .colour = 0xb8c0c8, .role = Role::Fighter,
 .q_pool = {"Lift and Throw", "Raise Stone", "Stone Push", "Stone Wall", "Earth Pillar", "Metal Cable"}};  // [U-270] 3 Q options

}  // namespace em::data
