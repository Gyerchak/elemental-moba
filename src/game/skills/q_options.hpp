#pragma once
// [U-11][U-183] Q options: picked in champion select. They belong to the ELEMENT, not to one champion:
// every bender of that element may pick any of them (non-mages only those that scale with attack [U-110]).
// Claude idea data — add a row here = a new Q option for that element.
#include "game/champions/common.hpp"

namespace em::data {

struct QOptionRow {
    Element element;
    SkillDef def;
};

inline constexpr QOptionRow kQOptions[] = {
    // ---------------------------------------------------------------- water
    // (first made for Frost Hunter)
    {Element::Water, {.name = "Ice Shards", .kind = K::Cone, .sub = S::Ice, .dmg = 80, .dmg_rank = 40, .ep = 50, .ad = 20,
        .range = 480, .width = 300, .cd_ms = 6'000, .cost = 40, .cc = C::Slow, .cc_ms = 1'500, .cc_val = 30,
        .desc = "Ice cone, slows (state change −20 % [U-28])"}},
    // (first made for Frost Hunter)
    {Element::Water, {.name = "Ice Spear", .kind = K::Bolt, .sub = S::Ice, .dmg = 90, .dmg_rank = 45, .ep = 55, .range = 1'000,
        .width = 60, .speed = 1'800, .cast_ms = 300, .cd_ms = 7'000, .cost = 45, .desc = "Long ice spear"}},
    // (first made for Tide Healer)
    {Element::Water, {.name = "Water Whip", .kind = K::Bolt, .sub = S::Water, .dmg = 60, .dmg_rank = 35, .ep = 50, .range = 900,
        .width = 80, .speed = 1'400, .cd_ms = 5'000, .cost = 40,
        .desc = "Water lash; counters one fire skill [U-17]"}},
    // (first made for Tide Healer)
    {Element::Water, {.name = "Water Wave", .kind = K::Wave, .sub = S::Water, .dmg = 50, .dmg_rank = 30, .ep = 40, .range = 750,
        .width = 260, .speed = 900, .cast_ms = 300, .cd_ms = 8'000, .cost = 50, .knock = 140, .pierce = true,
        .desc = "Wide wave that pushes"}},
    // ---------------------------------------------------------------- earth
    // (first made for Iron Sentinel)
    {Element::Earth, {.name = "Metal Cable", .kind = K::Bolt, .sub = S::Metal, .dmg = 60, .dmg_rank = 30, .ad = 70,
        .range = 850, .width = 60, .speed = 1'700, .cast_ms = 300, .cd_ms = 10'000, .cc = C::Root,
        .cc_ms = 1'200, .desc = "Cable that roots the first enemy"}},
    // (first made for Iron Sentinel)
    {Element::Earth, {.name = "Iron Cleave", .kind = K::Cone, .sub = S::Metal, .dmg = 60, .dmg_rank = 35, .ad = 90,
        .range = 300, .width = 320, .cd_ms = 5'000, .desc = "Wide metal swing", .melee = true}},
    // (first made for Magma Herald)
    {Element::Earth, {.name = "Lava Glob", .kind = K::Bolt, .sub = S::Lava, .dmg = 75, .dmg_rank = 40, .ep = 55, .range = 900,
        .width = 90, .speed = 1'000, .cast_ms = 350, .cd_ms = 7'000, .cc = C::Burn, .cc_ms = 2'000, .cc_val = 1'500,
        .desc = "Lava ball, burns"}},
    // (first made for Magma Herald)
    {Element::Earth, {.name = "Magma Spit", .kind = K::Cone, .sub = S::Lava, .dmg = 70, .dmg_rank = 35, .ep = 50, .range = 420,
        .width = 300, .cast_ms = 300, .cd_ms = 6'000, .desc = "Short lava spray"}},
    // (first made for Stone Warden)
    {Element::Earth, {.name = "Rock Throw", .kind = K::Bolt, .sub = S::Earth, .dmg = 70, .dmg_rank = 35, .ep = 40, .ad = 20,
        .range = 900, .width = 90, .speed = 1'200, .cast_ms = 400, .cd_ms = 9'000, .cd_rank = -500,
        .cc = C::Slow, .cc_ms = 1'500, .cc_val = 30, .desc = "Heavy rock, slows; blocks air/fire [U-20]"}},
    // (first made for Stone Warden)
    {Element::Earth, {.name = "Heavy Stone", .kind = K::Boulder, .sub = S::Earth, .dmg = 110, .dmg_rank = 50, .ep = 50,
        .ad = 20, .range = 700, .width = 130, .speed = 1'100, .cast_ms = 450, .cd_ms = 12'000, .cc = C::Stun,
        .cc_ms = 700, .casts = 2, .recast_ms = 6'000,
        .desc = "Cast 1 raises a boulder, cast 2 pushes it; another earthbender can push it too [U-27]"}},
    // ---------------------------------------------------------------- fire
    // (first made for Ember Monk)
    {Element::Fire, {.name = "Fire Bolt", .kind = K::Bolt, .sub = S::Fire, .dmg = 80, .dmg_rank = 40, .ep = 60,
        .range = 950, .width = 70, .speed = 1'500, .cd_ms = 5'000, .cd_rank = -250, .cost = 25,
        .desc = "Fast fireball, burns trees on the way"}},
    // (first made for Ember Monk)
    {Element::Fire, {.name = "Fire Whip", .kind = K::Cone, .sub = S::Fire, .dmg = 70, .dmg_rank = 35, .ep = 55,
        .range = 450, .width = 280, .cd_ms = 4'000, .cost = 20, .desc = "Short wide fire whip; the tip hits +25 % (sweet spot)", .sweet_pct = 25}},
    // (first made for Storm Daughter)
    {Element::Fire, {.name = "Lightning Bolt", .kind = K::Bolt, .sub = S::Lightning, .dmg = 70, .dmg_rank = 40, .ep = 50,
        .ad = 30, .range = 1'000, .width = 50, .speed = 2'600, .cd_ms = 6'000, .cost = 30, .pierce = true,
        .desc = "Piercing lightning; chains through connected water [X-58]"}},
    // (first made for Storm Daughter)
    {Element::Fire, {.name = "Spark Volley", .kind = K::Bolt, .sub = S::Lightning, .dmg = 40, .dmg_rank = 20, .ep = 30,
        .range = 800, .width = 50, .speed = 2'000, .cd_ms = 9'000, .cost = 30, .casts = 3, .recast_ms = 3'000,
        .desc = "3 recasts in 3 s (multicast) [U-106]"}},
    // ---------------------------------------------------------------- air
    // (first made for Mist Dancer)
    {Element::Air, {.name = "Steam Burst", .kind = K::Burst, .sub = S::Steam, .dmg = 85, .dmg_rank = 40, .ep = 60,
        .range = 900, .radius = 180, .cast_ms = 300, .cd_ms = 6'000, .delay_ms = 500,
        .desc = "Steam explosion; leaves fog 3 s [X-57]"}},
    // (first made for Mist Dancer)
    {Element::Air, {.name = "Mist Bolt", .kind = K::Bolt, .sub = S::Air, .dmg = 70, .dmg_rank = 35, .ep = 55, .range = 950,
        .width = 70, .speed = 1'500, .cd_ms = 4'500, .desc = "Bolt of mist"}},
    // (first made for Wind Nomad)
    {Element::Air, {.name = "Air Blade", .kind = K::Bolt, .sub = S::Air, .dmg = 70, .dmg_rank = 35, .ep = 50, .ad = 20,
        .range = 900, .width = 70, .speed = 2'000, .cast_ms = 150, .cd_ms = 4'000, .pierce = true,
        .desc = "Piercing air blade"}},
    // (first made for Wind Nomad)
    {Element::Air, {.name = "Air Gust", .kind = K::Wave, .sub = S::Air, .dmg = 50, .dmg_rank = 25, .ep = 40, .range = 600,
        .width = 240, .speed = 1'300, .cast_ms = 150, .cd_ms = 5'000, .knock = 220, .pierce = true,
        .desc = "Gust that pushes", .env_bend = true}},
    // ---------------------------------------------------------------- non-benders
    // (first made for Chi Blocker)
    {Element::None, {.name = "Chi Strike", .kind = K::Dash, .dmg = 60, .dmg_rank = 30, .ad = 80, .range = 350, .width = 80,
        .speed = 1'500, .cast_ms = 0, .cd_ms = 8'000, .cost = 50, .cc = C::Mute, .cc_ms = 1'500,
        .desc = "Dash strike: blocks chi = no bending 1.5 s [U-77]"}},
    // (first made for Chi Blocker)
    {Element::None, {.name = "Knife Throw", .kind = K::Bolt, .dmg = 60, .dmg_rank = 35, .ad = 90, .range = 850, .width = 50,
        .speed = 2'000, .cast_ms = 150, .cd_ms = 5'000, .cost = 40, .desc = "Thrown knife"}},
    // [U-270] third non-bender option (Claude idea)
    {Element::None, {.name = "Bolas Throw", .kind = K::Bolt, .dmg = 45, .dmg_rank = 25, .ad = 70, .range = 750, .width = 70,
        .speed = 1'600, .cast_ms = 200, .cd_ms = 9'000, .cost = 45, .cc = C::Slow, .cc_ms = 1'500, .cc_val = 35,
        .desc = "Spinning bolas: slows 35 % for 1.5 s"}},
};

}  // namespace em::data
