#pragma once
// [X-59] Champion: Magma Herald (lava zone mage) — Claude idea, all numbers are placeholders. Edit freely.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kMagmaHerald{.name = "Magma Herald", .element = Element::Earth, .sub = S::Lava, .range = Range::Ranged, .ep_lvl = 300,
 .hp = 55'000, .attack = 5'000, .attack_range = 450,
 .w = {.name = "Lava Pool", .kind = K::Field, .sub = S::Lava, .dmg = 22, .dmg_rank = 10, .ep = 12, .range = 800,
       .radius = 200, .cast_ms = 450, .cd_ms = 15'000, .dur_ms = 8'000, .cc = C::Burn, .cc_ms = 1'000,
       .cc_val = 1'200, .desc = "Lava pool → cools into a stone wall after 8 s [X-56]"},
 .e = {.name = "Ash Cloud", .kind = K::Field, .sub = S::Earth, .range = 800, .radius = 260, .cast_ms = 300,
       .cd_ms = 18'000, .dur_ms = 4'000, .cc = C::Blind, .cc_ms = 600, .cc_val = 70,
       .desc = "Blinding ash, blocks vision"},
 .r = {.name = "Lava Wave", .kind = K::Wave, .sub = S::Lava, .dmg = 100, .dmg_rank = 50, .ep = 60, .range = 900,
       .width = 220, .speed = 800, .cast_ms = 500, .cd_ms = 13'000, .knock = 100, .pierce = true,
       .desc = "Wave of lava, pushes", .ground = true},
 .t = {.name = "Lava Spout", .kind = K::Burst, .sub = S::Lava, .dmg = 80, .dmg_rank = 40, .ep = 50, .range = 700, .radius = 150,
       .cast_ms = 300, .cd_ms = 10'000, .delay_ms = 700, .cc = C::Burn, .cc_ms = 1'500, .cc_val = 1'200,
       .desc = "Lava bursts from the ground after 0.7 s; burns"},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Volcano", .kind = K::Burst, .sub = S::Lava, .dmg = 300, .dmg_rank = 150, .ep = 100, .range = 1'000,
       .radius = 320, .cast_ms = 600, .cd_ms = 110'000, .delay_ms = 1'200, .cc = C::Airborne, .cc_ms = 800,
       .desc = "Eruption at a point", .ultimate = true},
 .p = {.name = "Molten", .kind = K::Passive, .passive = PassiveId::Molten, .desc = "Lava fields last +1 s"},
 .colour = 0xe04010, .role = Role::Mage,
 .q_pool = {"Lava Glob", "Magma Spit", "Rock Throw"}};  // [U-270] 3 Q options

}  // namespace em::data
