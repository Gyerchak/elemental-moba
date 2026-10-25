#pragma once
// [U-134] Elemental combos and [U-135] group bending. Numbers: [X-72][X-73].
// A "zone" skill (tornado, fire field, water pool, quake, …) can be fed by other skills:
//  - a skill of ANOTHER element flying through it → a combo [X-72];
//  - a skill of the SAME element from an ally → the zone grows (group bending) [X-73].
#include <algorithm>
#include <cstdint>

#include "core/elements.hpp"

namespace em {

enum class Combo : std::uint8_t {
    None,
    Firestorm,  // fire through air: the fire skill grows +50 % size and damage
    Steam,      // fire meets water: steam cloud = vision fog 3 s [X-57]
    Lava,       // fire meets earth: lava pool, burns, becomes a stone wall after 8 s [X-56]
    IceShards,  // water through air: water freezes → ice, slows 40 %
    Mud,        // water meets earth: mud field, slows 50 %
    Sandstorm,  // air meets earth: sandstorm, blinds (sight −70 %)
};

// [X-72] Symmetric table of element pairs.
[[nodiscard]] constexpr Combo combo_of(Element a, Element b) {
    if (a == b || a == Element::None || b == Element::None) return Combo::None;
    auto is = [&](Element x, Element y) { return (a == x && b == y) || (a == y && b == x); };
    if (is(Element::Fire, Element::Air)) return Combo::Firestorm;
    if (is(Element::Fire, Element::Water)) return Combo::Steam;
    if (is(Element::Fire, Element::Earth)) return Combo::Lava;
    if (is(Element::Water, Element::Air)) return Combo::IceShards;
    if (is(Element::Water, Element::Earth)) return Combo::Mud;
    if (is(Element::Air, Element::Earth)) return Combo::Sandstorm;
    return Combo::None;
}

// [X-73] Group bending: every ally of the same element who feeds the zone
// adds +25 % size, +20 % damage and +1 s life. Max 4 feeds (5 benders together).
inline constexpr int kMaxGroupFeeds = 4;
[[nodiscard]] constexpr int group_size_pct(int feeds) { return 100 + 25 * std::clamp(feeds, 0, kMaxGroupFeeds); }
[[nodiscard]] constexpr int group_damage_pct(int feeds) { return 100 + 20 * std::clamp(feeds, 0, kMaxGroupFeeds); }
[[nodiscard]] constexpr int group_extra_ms(int feeds) { return 1'000 * std::clamp(feeds, 0, kMaxGroupFeeds); }

static_assert(combo_of(Element::Air, Element::Fire) == Combo::Firestorm && combo_of(Element::Fire, Element::Air) == Combo::Firestorm);
static_assert(combo_of(Element::Water, Element::Water) == Combo::None && combo_of(Element::Earth, Element::Water) == Combo::Mud);
static_assert(group_size_pct(2) == 150 && group_size_pct(9) == 200 && group_damage_pct(4) == 180);

}  // namespace em
