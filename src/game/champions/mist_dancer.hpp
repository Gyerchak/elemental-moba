#pragma once
// [X-59] Champion: Mist Dancer (air / steam utility) — Claude idea, all numbers are placeholders. Edit freely.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kMistDancer{.name = "Mist Dancer", .element = Element::Air, .sub = S::Steam, .body = &kSlimBody, .range = Range::Ranged,
 .ep_lvl = 250, .hp = 51'000, .attack = 5'000, .attack_range = 480,
 .w = {.name = "Mist Veil", .kind = K::Field, .sub = S::Steam, .range = 700, .radius = 260, .cast_ms = 250,
       .cd_ms = 16'000, .dur_ms = 4'000, .cc = C::Blind, .cc_ms = 600, .cc_val = 50,
       .desc = "Fog that blocks vision"},
 .e = {.name = "Cloud Hop", .kind = K::Blink, .sub = S::Air, .range = 400, .cast_ms = 0, .cd_ms = 10'000,
       .desc = "Blink"},
 .r = {.name = "Pressure Wave", .kind = K::Wave, .sub = S::Air, .dmg = 90, .dmg_rank = 45, .ep = 55, .range = 800,
       .width = 280, .speed = 1'200, .cast_ms = 300, .cd_ms = 12'000, .knock = 260, .pierce = true,
       .desc = "Pushes enemies away", .env_bend = true},
 .t = {.name = "Fog Bank", .kind = K::Field, .sub = S::Steam, .range = 700, .radius = 230, .cast_ms = 250, .cd_ms = 15'000,
       .dur_ms = 3'000, .cc = C::Blind, .cc_ms = 600, .cc_val = 50, .desc = "A bank of fog: blinds inside"},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Monsoon", .kind = K::Tornado, .sub = S::Steam, .dmg = 60, .dmg_rank = 30, .ep = 40, .range = 1'100,
       .speed = 260, .radius = 320, .cast_ms = 500, .cd_ms = 100'000, .dur_ms = 6'000, .cc = C::Slow,
       .cc_ms = 600, .cc_val = 40, .group = true, .desc = "Steam storm", .ultimate = true},
 .p = {.name = "Mist Form", .kind = K::Passive, .passive = PassiveId::MistForm, .desc = "+10 % skill size"},
 .colour = 0xd8e8f0, .role = Role::Support,
 .q_pool = {"Steam Burst", "Mist Bolt", "Air Gust"}};  // [U-270] 3 Q options

}  // namespace em::data
