#pragma once
// [U-279] Champion: Avatar Kyren — the AVATAR mage type: all four elements, unique, powerful, hard.
// Claude idea, all numbers are placeholders. Edit freely.
//   - one skill per element (water W, earth E, fire R, air Q options), long cooldowns, low HP → hard to play
//   - passive Elemental Cycle: use all 4 elements in a row → the next skill +40 % damage
//   - ultimate Avatar State: fly, +50 % power, +25 % skill size for 6 s
//   - pick rules [U-07][U-08]: max ONE Avatar per team; a normal and a dark version share `unique_group`,
//     so only one of them per game.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kAvatarKyren{.name = "Avatar Kyren", .element = Element::Air, .sub = S::Air, .body = &kSlimBody,
 .range = Range::Ranged, .ep_lvl = 320, .hp = 50'000, .hp_lvl = 8'500, .attack = 5'000, .attack_lvl = 280,
 .def = 2'600, .edef = 2'800, .as = 64, .attack_range = 500,
 .w = {.name = "Tidal Coil", .kind = K::Wave, .sub = S::Water, .dmg = 75, .dmg_rank = 40, .ep = 55, .range = 850, .width = 160,
       .speed = 1'100, .cast_ms = 280, .cd_ms = 11'000, .cc = C::Slow, .cc_ms = 1'200, .cc_val = 30, .knock = 60,
       .desc = "Water wave: pushes and slows; the Avatar's water"},
 .e = {.name = "Stone Bastion", .kind = K::Pillar, .sub = S::Earth, .dmg = 60, .dmg_rank = 35, .ep = 45, .range = 700,
       .radius = 140, .cast_ms = 400, .cd_ms = 14'000, .dur_ms = 2'500, .cc = C::Airborne, .cc_ms = 650,
       .desc = "Earth pillar under the enemy: knock-up; needs earth under the Avatar's feet [U-269]", .ground = true},
 .r = {.name = "Dragon Spiral", .kind = K::Cone, .sub = S::Fire, .dmg = 110, .dmg_rank = 55, .ep = 70, .range = 520,
       .width = 300, .cast_ms = 300, .cd_ms = 13'000, .cc = C::Burn, .cc_ms = 2'000, .cc_val = 1'800,
       .desc = "Fire cone; burns for 2 s", .sweet_pct = 25},
 .t = {.name = "Wind Spiral", .kind = K::Tornado, .sub = S::Air, .dmg = 40, .dmg_rank = 20, .ep = 30, .range = 800, .speed = 400,
       .radius = 140, .cast_ms = 300, .cd_ms = 13'000, .dur_ms = 3'000, .cc = C::Airborne, .cc_ms = 600,
       .desc = "A small tornado: the Avatar's air"},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Avatar State", .kind = K::AvatarState, .sub = S::Air, .cast_ms = 600, .cd_ms = 120'000, .dur_ms = 6'000, .cc_val = 25,
       .desc = "Glowing eyes: fly, +50 % power, +25 % skill size, +25 % move speed for 6 s", .ultimate = true},
 .p = {.name = "Elemental Cycle", .kind = K::Passive, .passive = PassiveId::ElementalCycle,
       .desc = "Use water, earth, fire and air skills in any order → the next skill deals +40 %"},
 .colour = 0x70e0ff, .role = Role::Mage, .avatar = true, .unique_group = 1,
 .q_pool = {"Air Blade", "Rock Throw", "Fire Bolt"}};  // [U-270] Q from three elements

}  // namespace em::data
