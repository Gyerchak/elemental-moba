#pragma once
// [U-71][U-76..U-79][U-137] Special skills (F / G), picked in champion select per element.
// Claude idea data (X-62, X-75, X-79..X-81). Add a row = a new special skill.
#include "game/champions/common.hpp"

namespace em::data {

// [U-71][U-76..U-79] Special skills. element = who may pick it (None = non-benders).
struct SpecialRow {
    Element element;
    SkillDef def;
};
constexpr SpecialRow kSpecialRows[] = {
    // earth [U-76]
    {Element::Earth, {.name = "Seismic Sense", .kind = K::Passive, .sub = S::Earth, .passive = PassiveId::SeismicSense,
                      .desc = "Team sees ground movement as waves [U-142]"}},
    {Element::Earth, {.name = "Stone Armour", .kind = K::Shield, .sub = S::EarthArmor, .dmg = 150, .dmg_rank = 50,
                      .ep = 40, .cast_ms = 300, .cd_ms = 40'000, .dur_ms = 5'000, .size_bonus = 2'000,
                      .desc = "Big shield, +20 % model size [U-129]"}},
    {Element::Earth, {.name = "Sandbending", .kind = K::Cone, .sub = S::Earth, .dmg = 90, .dmg_rank = 30, .ep = 40,
                      .range = 500, .width = 320, .cast_ms = 300, .cd_ms = 12'000, .cc = C::Blind, .cc_ms = 1'500,
                      .cc_val = 70, .desc = "Sand blast, blinds (earth version) [U-137][X-75]"}},
    {Element::Earth, {.name = "Magma Bending", .kind = K::Field, .sub = S::Lava, .dmg = 25, .ep = 15, .range = 700,
                      .radius = 180, .cast_ms = 400, .cd_ms = 30'000, .dur_ms = 8'000, .cc = C::Burn, .cc_ms = 1'000,
                      .cc_val = 1'000, .desc = "Lava pool → stone wall [X-56]"}},
    // air [U-77]
    {Element::Air, {.name = "Updraft", .kind = K::Flight, .sub = S::Air, .cast_ms = 0, .cd_ms = 45'000, .dur_ms = 4'000,
                    .cc_val = 25, .desc = "A short lift on the wind: over terrain for 4 s [U-51][U-224]"}},
    {Element::Air, {.name = "Wind Sense", .kind = K::Passive, .sub = S::Air, .passive = PassiveId::WindSense,
                    .desc = "Always on: skills outside vision show as waves [U-141][U-274]"}},
    {Element::Air, {.name = "Sandbending (air)", .kind = K::Cone, .sub = S::Air, .dmg = 54, .dmg_rank = 18, .ep = 24,
                    .range = 450, .width = 280, .cast_ms = 200, .cd_ms = 7'000, .cc = C::Blind, .cc_ms = 1'000,
                    .cc_val = 50, .desc = "Weaker sand blast, lower cooldown [U-137][X-75]"}},
    {Element::Air, {.name = "Chi Blocking", .kind = K::Dash, .sub = S::Air, .dmg = 30, .ad = 40, .range = 250,
                    .width = 80, .speed = 1'500, .cast_ms = 0, .cd_ms = 14'000, .cc = C::Mute, .cc_ms = 1'000,
                    .desc = "Short chi block [U-77]"}},
    // water [U-78]
    {Element::Water, {.name = "Healing Water (self)", .kind = K::Heal, .sub = S::Water, .dmg = 120, .dmg_rank = 40,
                      .ep = 40, .radius = 120, .cast_ms = 500, .cd_ms = 45'000, .ally = true,
                      .desc = "Heal yourself / allies close"}},
    {Element::Water, {.name = "Bloodbending", .kind = K::Bloodbend, .sub = S::Blood, .range = 650, .cast_ms = 300,
                      .cd_ms = 90'000, .cc = C::Bloodbound, .cc_ms = 2'500, .targeted = true, .full_moon_only = true,
                      .desc = "Full moon only: control one enemy (targeted) [U-78]"}},
    {Element::Water, {.name = "Self-Freeze", .kind = K::SelfFreeze, .sub = S::Ice, .cast_ms = 0, .cd_ms = 90'000,
                      .dur_ms = 2'500, .desc = "Freeze yourself: untouchable, no heat [U-143]"}},
    {Element::Water, {.name = "Water Spout", .kind = K::WaterSpout, .sub = S::Water, .cast_ms = 0, .cd_ms = 30'000,
                      .dur_ms = 6'000, .cc_val = 20,
                      .desc = "Ride a water column over water (Korra): faster, cast freely, melee / ground can't reach you [U-189]"}},
    {Element::Water, {.name = "Ice Path", .kind = K::IcePath, .sub = S::Ice, .radius = 160, .cast_ms = 0, .cd_ms = 20'000,
                      .dur_ms = 6'000, .desc = "Freeze the water around you into walkable ice for 6 s [U-188]"}},
    {Element::Water, {.name = "Iceberg", .kind = K::Wall, .sub = S::Ice, .range = 700, .radius = 150, .cast_ms = 400,
                      .cd_ms = 35'000, .dur_ms = 15'000, .desc = "Raise an iceberg ring from the water for 15 s — bends the map; needs a big water source nearby [U-225][U-294]",
                      .env_bend = true}},
    {Element::Water, {.name = "Snow Squall", .kind = K::Cone, .sub = S::Ice, .dmg = 40, .dmg_rank = 15, .ep = 25, .range = 500,
                      .width = 340, .cast_ms = 250, .cd_ms = 14'000, .cost = 40, .cc = C::Blind, .cc_ms = 1'000, .cc_val = 30,
                      .desc = "Snow in the face: weak blind + damage [U-247]"}},
    {Element::Water, {.name = "Lantern", .kind = K::Passive, .sub = S::Physical, .passive = PassiveId::Lantern,
                      .desc = "Carry a lantern: light around you at night — enemies see it too [U-244]"}},
    // fire [U-79]
    {Element::Fire, {.name = "Fire in Hand", .kind = K::Passive, .sub = S::Fire, .passive = PassiveId::FireInHand,
                     .desc = "Carry fire in your hands: strong light at night, but you are easy to see [U-228][U-244]"}},
    {Element::Fire, {.name = "Heat Draw", .kind = K::IcePath, .sub = S::Fire, .radius = 220, .cast_ms = 300, .cd_ms = 25'000,
                     .dur_ms = 0, .desc = "Draw the heat out of lava (→ stone ground) and steam (→ water on the ground; those inside get wet) around you; +20 fuel [U-225][U-292]",
                     .env_bend = true}},
    {Element::Fire, {.name = "Sky Lantern", .kind = K::Lantern, .sub = S::Fire, .range = 900, .speed = 700,
                     .radius = 64, .cast_ms = 200, .cd_ms = 60'000, .cost = 30, .light = 640, .light_ms = 20'000,
                     .desc = "Hanging flame, lights 20 cells for 20 s [X-62]"}},
    {Element::Fire, {.name = "Heat Sense", .kind = K::Passive, .sub = S::Fire, .passive = PassiveId::HeatSense,
                     .desc = "Team sees heat sources as waves [U-143]"}},
    {Element::Fire, {.name = "Lightning Bending", .kind = K::Bolt, .sub = S::Lightning, .dmg = 120, .dmg_rank = 40,
                     .ep = 50, .range = 1'300, .width = 50, .speed = 3'200, .cast_ms = 600, .cd_ms = 35'000,
                     .cost = 30, .pierce = true, .desc = "Long lightning bolt [U-79]"}},
    {Element::Fire, {.name = "Lightning Redirection", .kind = K::Redirect, .sub = S::Lightning, .cast_ms = 0,
                     .cd_ms = 18'000, .dur_ms = 700,
                     .desc = "0.7 s window: enemy lightning that hits you flies on to where your cursor was [U-185]",
                     .turns_caster = false}},
    {Element::Earth, {.name = "Earthshape", .kind = K::Wall, .sub = S::Earth, .range = 700, .radius = 120, .cast_ms = 500,
                      .cd_ms = 30'000, .dur_ms = 20'000, .desc = "Raise a rock outcrop for 20 s — bends the map [U-225]", .env_bend = true}},
    {Element::Earth, {.name = "Lantern", .kind = K::Passive, .sub = S::Physical, .passive = PassiveId::Lantern,
                      .desc = "Carry a lantern: light around you at night — enemies see it too [U-244]"}},
    {Element::Air, {.name = "Sky Ascension (rare)", .kind = K::Flight, .sub = S::Air, .range = 900, .cast_ms = 300, .cd_ms = 90'000,
                    .dur_ms = 8'000, .cc_val = 10,
                    .desc = "True flight over everything; the farther you aim, the higher you fly (40-200) [U-224]",
                    .reach_height = 200}},
    {Element::Air, {.name = "Lantern", .kind = K::Passive, .sub = S::Physical, .passive = PassiveId::Lantern,
                    .desc = "Carry a lantern: light around you at night — enemies see it too [U-244]"}},
    // non-benders
    {Element::None, {.name = "Grapple Hook", .kind = K::Dash, .range = 600, .width = 60, .speed = 1'800, .cast_ms = 100,
                     .cd_ms = 30'000, .desc = "Pull yourself forward"}},
    {Element::None, {.name = "Lantern", .kind = K::Passive, .sub = S::Physical, .passive = PassiveId::Lantern,
                     .desc = "Carry a lantern: light around you at night — enemies see it too [U-244]"}},
    {Element::None, {.name = "Smoke Pellet", .kind = K::Field, .range = 500, .radius = 200, .cast_ms = 0,
                     .cd_ms = 35'000, .dur_ms = 3'000, .cc = C::Blind, .cc_ms = 600, .cc_val = 60,
                     .desc = "Small smoke cloud"}},
    // [U-264] elemental magic stances for non-benders (Claude idea): 10 s of magic → sky events boost them too
    {Element::None, {.name = "Ember Stance", .kind = K::MagicStance, .sub = S::Fire, .cast_ms = 0, .cd_ms = 60'000, .dur_ms = 10'000,
                     .desc = "10 s: your skills and autoattacks become fire magic"}},
    {Element::None, {.name = "Tide Stance", .kind = K::MagicStance, .sub = S::Water, .cast_ms = 0, .cd_ms = 60'000, .dur_ms = 10'000,
                     .desc = "10 s: your skills and autoattacks become water magic"}},
    {Element::None, {.name = "Stone Stance", .kind = K::MagicStance, .sub = S::Earth, .cast_ms = 0, .cd_ms = 60'000, .dur_ms = 10'000,
                     .desc = "10 s: your skills and autoattacks become earth magic"}},
    {Element::None, {.name = "Gale Stance", .kind = K::MagicStance, .sub = S::Air, .cast_ms = 0, .cd_ms = 60'000, .dur_ms = 10'000,
                     .desc = "10 s: your skills and autoattacks become air magic"}},
    // [U-288] metalbending wall (earth; appended at the end so saved picks keep their numbers)
    {Element::Earth, {.name = "Metal Wall", .kind = K::Wall, .sub = S::Metal, .range = 700, .width = 320, .cast_ms = 300,
                      .cd_ms = 30'000, .dur_ms = 4'000,
                      .desc = "Metalbending: the strongest wall (480 HP per piece, stands 6 s); metalbenders break it 2× faster [U-288]"}},
};

}  // namespace em::data
