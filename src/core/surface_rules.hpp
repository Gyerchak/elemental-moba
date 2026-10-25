#pragma once
// [U-269][?-12] Which surfaces each kind of bending can use. Earthbenders need earth under their feet:
// land, dryland, sand, stone, stone roads, forest paths and SHALLOW water (river beds). They can NOT bend on
// wood (bridges), glacier / ice (a form of water), deep water, metal or platinum. Lavabending also works on magma.
// Metalbending uses metal the bender carries → no surface check. Water / fire / air carry their element → none.
#include "core/cell.hpp"
#include "core/elements.hpp"

#include <iterator>

namespace em {

[[nodiscard]] constexpr bool earth_bendable(Surface s) {
    switch (s) {
        case Surface::Land: case Surface::Dryland: case Surface::Sand: case Surface::Stone:
        case Surface::Water: case Surface::StoneRoad: case Surface::ForestPath:
            return true;
        default:
            return false;
    }
}

// May a skill of this sub-element be cast while the caster stands on this surface?
[[nodiscard]] constexpr bool can_bend_on(SubElement sub, Surface s) {
    if (base_element(sub) != Element::Earth) return true;
    if (sub == SubElement::Metal || sub == SubElement::MetalArmor) return true;
    if (sub == SubElement::Lava && s == Surface::Magma) return true;
    return earth_bendable(s);
}

[[nodiscard]] constexpr const char* surface_name(Surface s) {
    constexpr const char* n[] = {"land", "dryland", "glacier", "shallow water", "sand", "stone", "wood", "deep water",
                                 "magma", "metal", "platinum", "stone road", "forest path"};
    static_assert(std::size(n) == std::size_t(Surface::Count));
    return n[std::size_t(s)];
}

static_assert(can_bend_on(SubElement::Earth, Surface::Water) && !can_bend_on(SubElement::Earth, Surface::Glacier));
static_assert(!can_bend_on(SubElement::Earth, Surface::Wood) && can_bend_on(SubElement::Fire, Surface::Wood));
static_assert(can_bend_on(SubElement::Lava, Surface::Magma) && !can_bend_on(SubElement::Earth, Surface::Magma));

}  // namespace em
