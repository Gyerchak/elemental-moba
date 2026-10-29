#pragma once
// [U-288] Player-made walls have HP (per cell: a hit breaks a hole, the wall is divided). Every element is more or
// less effective against each kind; items add wall damage. Metal walls: the strongest and longest lasting.
// Claude idea numbers. Effectiveness in % of the hit's damage.
#include <cstdint>

#include "core/cell.hpp"
#include "core/elements.hpp"

namespace em {

[[nodiscard]] constexpr std::int32_t wall_max_hp(Wall w) {
    switch (w) {
        case Wall::Ice: return 90;     // [U-359] much easier to break than before (160 / 240 / 480)
        case Wall::Stone: return 140;
        case Wall::Metal: return 300;
        default: return 0;
    }
}
inline constexpr int kMetalWallLifePct = 150;
// [U-380] the wall standard for every wall skill: segments 100 units long, 33 / 66 / 99 thick, 1 or 2 levels high,
// HP per segment 300 (ice 100) — per skill / per rank in SkillDef. (wall_max_hp above = single cells: lava rocks,
// thrown rocks; natural rocks × 2.)
inline constexpr int kWallSegLen = 100, kWallThickStep = 33, kWallSegHp = 300, kIceWallSegHp = 100;
struct WallSpec {
    int thick = 66;   // units
    int level = 2;    // levels above the ground
    int hp = kWallSegHp;
};  // metal stands 1.5 × the skill's duration
// the wall a skill raises: ice from ice, metal from metalbending, else stone
[[nodiscard]] constexpr Wall wall_kind_of(SubElement s) {
    return s == SubElement::Ice ? Wall::Ice : (s == SubElement::Metal || s == SubElement::MetalArmor) ? Wall::Metal : Wall::Stone;
}

[[nodiscard]] constexpr int wall_effect_pct(Wall w, SubElement s) {
    using S = SubElement;
    switch (w) {
        case Wall::Ice:  // fire melts it; water / ice reshape it slowly
            if (s == S::Lava) return 300;
            if (s == S::Fire || s == S::BlueFire || s == S::Combustion) return 250;
            if (s == S::Lightning) return 100;
            if (s == S::Ice || s == S::Water) return 50;
            if (base_element(s) == Element::Earth) return 120;
            if (base_element(s) == Element::Air) return 60;
            return 100;  // physical
        case Wall::Stone:  // earthbenders crush stone; lava too
            if (s == S::Lava) return 150;
            if (s == S::Metal || s == S::MetalArmor) return 100;
            if (base_element(s) == Element::Earth) return 150;
            if (s == S::Lightning) return 40;
            if (base_element(s) == Element::Fire) return 60;
            if (base_element(s) == Element::Water) return 50;
            if (base_element(s) == Element::Air) return 50;
            return 75;
        case Wall::Metal:  // only metalbenders (and lava) are good against it
            if (s == S::Metal || s == S::MetalArmor) return 200;
            if (s == S::Lava) return 150;
            if (s == S::Lightning) return 60;
            if (base_element(s) == Element::Fire) return 50;
            if (base_element(s) == Element::Earth) return 60;
            if (base_element(s) == Element::Water || base_element(s) == Element::Air) return 30;
            return 40;
        default: return 0;
    }
}

// [U-288] a wall is also an elemental SOURCE: casting next to one (own or enemy) draws from it
[[nodiscard]] constexpr bool wall_feeds(Wall w, SubElement s) {
    switch (w) {
        case Wall::Stone: return s == SubElement::Earth || s == SubElement::Lava;
        case Wall::Ice: return s == SubElement::Ice || s == SubElement::Water;
        case Wall::Metal: return s == SubElement::Metal || s == SubElement::MetalArmor;
        default: return false;
    }
}
inline constexpr int kWallSourceBonusPct = 15;  // the skill deals +15 % …
inline constexpr int kWallSourceCost = 40;      // … and the wall cell loses 40 HP
inline constexpr int kWallSourceCells = 4;      // within 4 cells of the caster

static_assert(wall_effect_pct(Wall::Ice, SubElement::Fire) > wall_effect_pct(Wall::Stone, SubElement::Fire));
static_assert(wall_effect_pct(Wall::Metal, SubElement::Metal) > wall_effect_pct(Wall::Metal, SubElement::Earth));
static_assert(wall_max_hp(Wall::Metal) > wall_max_hp(Wall::Stone) && wall_max_hp(Wall::Stone) > wall_max_hp(Wall::Ice));

}  // namespace em
