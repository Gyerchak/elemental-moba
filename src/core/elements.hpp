#pragma once
// [U-12..U-29] Elements, sub-elements and who-blocks-what rules.
#include <cstdint>

namespace em {

// [U-65]
enum class Element : std::uint8_t { Water, Earth, Fire, Air, None };

// [U-66..U-69]
enum class SubElement : std::uint8_t {
    // water
    Water, Ice, Wood, Blood, Mud, Steam,
    // air (Steam shared with water) [U-67]
    Air,
    // earth (Mud shared with water) [U-68]
    Earth, Lava, Metal, EarthArmor, MetalArmor,
    // fire
    Fire, BlueFire, Lightning, Combustion,
    // non-benders
    Physical,
};

[[nodiscard]] constexpr Element base_element(SubElement s) {
    switch (s) {
        case SubElement::Water: case SubElement::Ice: case SubElement::Wood:
        case SubElement::Blood: case SubElement::Mud: case SubElement::Steam:
            return Element::Water;
        case SubElement::Air: return Element::Air;
        case SubElement::Earth: case SubElement::Lava: case SubElement::Metal:
        case SubElement::EarthArmor: case SubElement::MetalArmor:
            return Element::Earth;
        case SubElement::Fire: case SubElement::BlueFire: case SubElement::Lightning:
        case SubElement::Combustion:
            return Element::Fire;
        case SubElement::Physical: return Element::None;
    }
    return Element::None;
}

struct AttackInfo {
    SubElement sub;
    bool elevated = false;              // [U-22] cast from higher ground / flying
    bool from_above = false;            // [U-21] water attack from the up side
    bool unstoppable_stone = false;     // [U-48]
    bool unstoppable_ice = false;       // [U-48]
};

enum class BlockResult : std::uint8_t {
    None,       // attack passes
    Partial,    // attack passes weakened [X-35: 50 % placeholder]
    Blocked,    // attack stopped, blocker survives
    Countered,  // [U-17] 1 skill blocks 1 skill, both consumed
};

inline constexpr int kPartialBlockPassPct = 50;  // [X-35]

// A blocking SKILL (not a wall) meets an attack.
[[nodiscard]] constexpr BlockResult skill_blocks(Element blocker, const AttackInfo& a) {
    const Element att = base_element(a.sub);
    switch (blocker) {
        case Element::Water:
            if (att == Element::Fire) return BlockResult::Countered;            // [U-17]
            return BlockResult::None;                                            // [U-19] not air/stone
        case Element::Earth:
            if (att == Element::Air || att == Element::Fire)                     // [U-20]
                return a.elevated ? BlockResult::None : BlockResult::Blocked;    // [U-22]
            if (att == Element::Water)
                return a.from_above ? BlockResult::None : BlockResult::Partial;  // [U-20][U-21]
            return BlockResult::None;
        default:
            return BlockResult::None;  // [X-36] air/fire/none blocking skills: not in plan yet
    }
}

enum class WallKind : std::uint8_t { Stone, Ice };

enum class WallHit : std::uint8_t { PassesOver, Stopped, Melts, BreaksThrough };

// An attack meets a terrain wall.
[[nodiscard]] constexpr WallHit wall_hit(WallKind w, const AttackInfo& a) {
    const Element att = base_element(a.sub);
    if (w == WallKind::Ice) {
        if (a.unstoppable_ice) return WallHit::BreaksThrough;                    // [U-48]
        if (att == Element::Fire || att == Element::Water) return WallHit::Melts;  // [U-15][U-16]
    } else {
        if (a.unstoppable_stone) return WallHit::BreaksThrough;                  // [U-48]
        if ((att == Element::Air || att == Element::Fire) && a.elevated) return WallHit::PassesOver;  // [U-22]
    }
    if (att == Element::Water && a.from_above) return WallHit::PassesOver;        // [U-21]
    return WallHit::Stopped;
}

// [U-28] Changing water state nerfs EVERY aspect (radius, range, damage...) by 20 %.
inline constexpr int kStateChangeNerfPct = 20;
[[nodiscard]] constexpr int state_change_nerf(int value) { return value * (100 - kStateChangeNerfPct) / 100; }

// [U-44] Map walls = 6 × typical ability strength.
inline constexpr int kTypicalAbilityStrength = 100;  // [X-37] placeholder unit
inline constexpr int kMapWallStrength = 6 * kTypicalAbilityStrength;
// [U-18] Ice wall weaker / shorter. [X-38] numbers are placeholders.
inline constexpr int kStoneWallStrength = 2 * kTypicalAbilityStrength;
inline constexpr int kIceWallStrength = kTypicalAbilityStrength;
inline constexpr int kStoneWallLifeMs = 4'000;  // [U-24] collapse time
inline constexpr int kIceWallLifeMs = 2'500;

static_assert(skill_blocks(Element::Water, {.sub = SubElement::Fire}) == BlockResult::Countered);
static_assert(skill_blocks(Element::Water, {.sub = SubElement::Earth}) == BlockResult::None);
static_assert(skill_blocks(Element::Earth, {.sub = SubElement::Air, .elevated = true}) == BlockResult::None);
static_assert(wall_hit(WallKind::Ice, {.sub = SubElement::BlueFire}) == WallHit::Melts);
static_assert(state_change_nerf(100) == 80);

}  // namespace em
