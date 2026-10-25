#pragma once
// [X-59] Champion: Chi Blocker (non-bender) — Claude idea, all numbers are placeholders. Edit freely.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kChiBlocker{.name = "Chi Blocker", .element = Element::None, .sub = S::Physical, .body = &kSlimBody,
 .resource = ResourceKind::Energy, .mage = false, .hp = 56'000, .attack = 6'600, .attack_lvl = 420, .as = 72,
 .as_lvl = 300, .ms = 35'000, .attack_range = 40, .res_max = 20'000, .res_regen = 1'000,
 .w = {.name = "Acrobatics", .kind = K::Haste, .cast_ms = 0, .cd_ms = 12'000, .cost = 50, .dur_ms = 2'000,
       .cc_val = 40, .desc = "+40 % move speed"},
 .e = {.name = "Pressure Point", .kind = K::Cone, .dmg = 50, .dmg_rank = 30, .ad = 60, .range = 250, .width = 220,
       .cast_ms = 100, .cd_ms = 11'000, .cost = 60, .cc = C::Silence, .cc_ms = 1'200, .desc = "Silences", .melee = true},
 .r = {.name = "Smoke Bomb", .kind = K::Field, .range = 600, .radius = 260, .cast_ms = 150, .cd_ms = 16'000,
       .cost = 50, .dur_ms = 3'500, .cc = C::Blind, .cc_ms = 600, .cc_val = 60, .desc = "Smoke, blocks vision"},
 .t = {.name = "Pressure Jab", .kind = K::Bolt, .dmg = 40, .dmg_rank = 25, .ad = 70, .range = 500, .width = 60, .speed = 1'800,
       .cast_ms = 100, .cd_ms = 9'000, .cost = 40, .cc = C::Slow, .cc_ms = 1'000, .cc_val = 30, .desc = "A thrown dart at a nerve: slows"},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Paralysis Flurry", .kind = K::Dash, .dmg = 200, .dmg_rank = 100, .ad = 150, .range = 450,
       .width = 120, .speed = 1'200, .cast_ms = 0, .cd_ms = 90'000, .cost = 80, .cc = C::Stun, .cc_ms = 1'500,
       .desc = "Dash; paralyses what you hit", .ultimate = true},
 .p = {.name = "Chi Flow", .kind = K::Passive, .passive = PassiveId::ChiFlow, .desc = "Hits give energy"},
 .stance = true,  // [U-72] Space: fists (melee) ↔ knives (ranged)
 .colour = 0x8a5030, .role = Role::Assassin,
 .q_pool = {"Chi Strike", "Knife Throw", "Bolas Throw"}};  // [U-270] 3 Q options

}  // namespace em::data
