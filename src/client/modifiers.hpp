#pragma once
// [U-237][U-253][U-255] Everything that changes a unit right now, in ONE list and ONE order:
//   1. environment (sky: day / night, moon, comet, eclipse, alignment) — always first
//   2. terrain under the unit (brush, water, glacier, wood, magma …)
//   3. statuses (stun, slow, burn, shield …) and keystone / item / ability buffs
// Used by: the icons under the bars, the selection panel, my buff row and the sky box.
#include <cstdint>
#include <string>
#include <vector>

#include "client/draw.hpp"
#include "core/cell.hpp"
#include "core/elements.hpp"
#include "core/world_clock.hpp"

namespace em {

using gfx::Rgba;

struct ModView {
    std::string tag;     // 2–3 letters on the icon
    std::string name;    // short name
    std::string detail;  // what it does, with numbers
    Rgba colour;
    bool debuff = false;
    bool environment = false;
    std::int64_t left_ms = -1;  // −1 = no timer
    int value = 0;
    bool minor = false;         // only information (does not change this unit's power) → not under the bars
};

// Sky effects for a unit of this element (empty for none). Order: day/night, moon, eclipse, comet, alignment.
[[nodiscard]] std::vector<ModView> env_modifiers(Element e, const SkyState& s);
// Terrain under the unit.
[[nodiscard]] std::vector<ModView> terrain_modifiers(const Cell& c, Element e);
// One status / extra buff from the wire (kind < kModWireBase = StatusKind, else ModKind).
[[nodiscard]] ModView wire_modifier(std::uint8_t kind, int value, std::int64_t left_ms);
// Status bits of a NetUnit (no timers).
void status_modifiers(std::uint32_t bits, std::vector<ModView>& out);
// Colour of a status (CC timer bar, icons).
[[nodiscard]] Rgba status_colour(int status_kind);
[[nodiscard]] const char* status_name(int status_kind);

// Next sky event and how long until it (for the sky box). Empty name = nothing left.
struct NextEvent {
    std::string name;
    std::int64_t in_ms = 0;
};
[[nodiscard]] NextEvent next_sky_event(std::int64_t game_ms);

}  // namespace em
