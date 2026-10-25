#pragma once
// [X-59] Champion: Ember Monk (fire burst mage) — Claude idea, all numbers are placeholders. Edit freely.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kEmberMonk{.name = "Ember Monk", .element = Element::Fire, .sub = S::Fire, .range = Range::Ranged,
 .resource = ResourceKind::Fuel, .ep_lvl = 300, .hp = 54'000, .hp_lvl = 9'000, .attack = 5'200,
 .attack_range = 480, .res_max = 10'000, .res_regen = 500,
 .w = {.name = "Blaze Ring", .kind = K::Field, .sub = S::Fire, .dmg = 18, .dmg_rank = 8, .ep = 10, .range = 850,
       .radius = 220, .cd_ms = 12'000, .cost = 35, .dur_ms = 3'000, .tick_ms = 500, .cc = C::Burn, .cc_ms = 1'000,
       .cc_val = 1'000, .group = true, .desc = "Fire field; other firebenders can feed it (wildfire)"},
 .e = {.name = "Jet Propulsion", .kind = K::Dash, .sub = S::Fire, .dmg = 40, .dmg_rank = 20, .ep = 30, .range = 420,
       .width = 90, .speed = 1'600, .cast_ms = 0, .cd_ms = 14'000, .cd_rank = -1'000, .cost = 30,
       .desc = "Fly a short way on fire jets [U-51]"},
 .r = {.name = "Combustion", .kind = K::Burst, .sub = S::Combustion, .dmg = 120, .dmg_rank = 60, .ep = 75,
       .range = 1'100, .radius = 190, .cast_ms = 350, .cd_ms = 10'000, .cost = 40, .delay_ms = 700,
       .desc = "Delayed explosion at a point; the centre hits +30 % (sweet spot)", .sweet_pct = 30},
 .t = {.name = "Flame Kick", .kind = K::Cone, .sub = S::Fire, .dmg = 60, .dmg_rank = 30, .ep = 40, .range = 300, .width = 200,
       .cast_ms = 150, .cd_ms = 7'000, .cost = 20, .desc = "Short fiery kick in front", .melee = true},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Dragon's Breath", .kind = K::Cone, .sub = S::BlueFire, .dmg = 250, .dmg_rank = 125, .ep = 90,
       .range = 700, .width = 520, .cast_ms = 400, .cd_ms = 90'000, .cd_rank = -10'000, .cost = 60,
       .cc = C::Burn, .cc_ms = 3'000, .cc_val = 3'000, .desc = "Huge blue fire cone, burns", .ultimate = true},
 .p = {.name = "Inner Flame", .kind = K::Passive, .passive = PassiveId::InnerFlame,
       .desc = "Skill hits burn for 2 s"},
 .colour = 0xff6a2a, .role = Role::Mage,
 .q_pool = {"Fire Bolt", "Fire Whip", "Spark Volley"}};  // [U-270] 3 Q options

}  // namespace em::data
