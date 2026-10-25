#pragma once
// [U-237][U-255] Modifiers that are NOT statuses (status.hpp): keystone / item / ability buffs the server sends
// for my own champion. On the wire (NetMe::status) a kind < kModWireBase is a StatusKind, a kind ≥ kModWireBase is
// kModWireBase + ModKind. Environment modifiers (sky) and terrain modifiers are worked out by the client itself
// from the clock and the map, so they cost no bandwidth.
#include <cstdint>

namespace em {

enum class ModKind : std::uint8_t {
    SurgeStacks,    // keystone Elemental Surge: hits counted on one champion (value = count)
    StoneheartCd,   // keystone Stoneheart: cooling down
    ChiRush,        // keystone Chi Rush: next autoattack +30 %
    Lotus,          // item active: +cast speed
    Flask,          // item active: immortal healing window
    Untargetable,   // cannot be targeted
    WindSense,      // active wind sense
    Bigger,         // temporary size bonus (value = %)
    SkillStolen,    // a skill was stolen (value = slot)
    AvatarState,    // [U-279]
    Cycle,          // [U-279] Elemental Cycle: elements used (value = count), or READY (value = 4)
    MagicStance,    // [U-264] non-bender magic stance (value = SubElement)
    SourceIce,      // [U-293] waterbender: water from sources comes as ice
    Count
};

inline constexpr std::uint8_t kModWireBase = 64;

}  // namespace em
