#pragma once
// [X-59] Champion: Wind Nomad (air assassin) — Claude idea, all numbers are placeholders. Edit freely.
#include "game/champions/common.hpp"

namespace em::data {

inline constexpr ChampionDef kWindNomad{.name = "Wind Nomad", .element = Element::Air, .sub = S::Air, .body = &kSlimBody, .hp = 53'000, .hp_lvl = 9'000,
 .attack = 6'200, .as = 70, .ms = 35'000, .attack_range = 45,
 .w = {.name = "Air Scooter", .kind = K::Haste, .sub = S::Air, .cast_ms = 0, .cd_ms = 9'000, .dur_ms = 2'500,
       .cc_val = 45, .desc = "+45 % move speed"},
 .e = {.name = "Air Step", .kind = K::Blink, .sub = S::Air, .range = 320, .cast_ms = 0, .cd_ms = 8'000,
       .desc = "Short blink — 2 charges, one back every 9 s [U-433]", .charges = 2, .charge_ms = 9'000},
 .r = {.name = "Tornado", .kind = K::Tornado, .sub = S::Air, .dmg = 25, .dmg_rank = 12, .ep = 15, .range = 900,
       .speed = 350, .radius = 150, .cast_ms = 300, .cd_ms = 12'000, .dur_ms = 4'000, .cc = C::Airborne,
       .cc_ms = 700, .group = true, .desc = "Moving tornado; grows when allied airbenders feed it [U-135]"},
 .t = {.name = "Gust Push", .kind = K::Wave, .sub = S::Air, .dmg = 40, .dmg_rank = 20, .ep = 30, .ad = 20, .range = 600,
       .width = 220, .speed = 1'300, .cast_ms = 200, .cd_ms = 10'000, .knock = 200, .pierce = true, .desc = "Pushes enemies back"},  // [U-289][U-291] the extra skill (Claude idea); the ultimate is Y
 .y = {.name = "Hurricane", .kind = K::Tornado, .sub = S::Air, .dmg = 70, .dmg_rank = 35, .ep = 40, .range = 1'200,
       .speed = 300, .radius = 300, .cast_ms = 500, .cd_ms = 90'000, .dur_ms = 6'000, .cc = C::Airborne,
       .cc_ms = 1'000, .group = true, .desc = "Huge tornado", .env_bend = true, .ultimate = true},
 .p = {.name = "Light Feet", .kind = K::Passive, .passive = PassiveId::LightFeet, .desc = "+15 move speed"},
 .colour = 0xf0f0d0, .role = Role::Assassin,
 .q_pool = {"Air Blade", "Air Gust", "Mist Bolt"}};  // [U-270] 3 Q options

}  // namespace em::data
