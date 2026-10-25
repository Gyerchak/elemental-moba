#pragma once
// [U-90..U-94] One map cell packed into 32 bits (compressed layout from
// zrodla/map/mapplan.txt). Every accessor is constexpr and branch-free.
#include <algorithm>
#include <cstdint>
#include <utility>

namespace em {

// [U-92] 11 surfaces → 4 bits.
enum class Surface : std::uint8_t {
    Land, Dryland, Glacier, Water, Sand, Stone, Wood, DeepWater, Magma, Metal, Platinum,
    StoneRoad, ForestPath,  // [U-269] earth-based: roads along the lanes, paths through the jungle
    Count
};
static_assert(std::to_underlying(Surface::Count) <= 16);

// [?-15][X-31] The 2-bit fog fields describe how much THIS cell blocks a sense.
// Per-team "what is visible now" lives in separate per-team bitmaps [X-10].
enum class SenseBlock : std::uint8_t { None, Partial, Full, Brush };

// [X-32] Meaning of the 2 walkable bits (plan gives only the size).
enum class Walk : std::uint8_t { Blocked, Walk, FlyOnly, BenderOnly };

// [U-94] "ice/stone temp wall" 2 bits.
enum class Wall : std::uint8_t { None, Ice, Stone, Metal };  // [U-288] Metal (was Reserved)

// [U-94] "tree / burned / destroyed tree" 2 bits (land only).
enum class Tree : std::uint8_t { None, Alive, Burned, Destroyed };

// [X-30] Bushes are in the plan (U-14, U-89) but have no cell field.
// Uses 2 of the 3 reserved bits (29-30).
enum class Bush : std::uint8_t { None, Alive, Burned };

inline constexpr int kMinElevation = -2;  // [U-93]
inline constexpr int kMaxElevation = 6;
inline constexpr std::uint32_t kNoTempElevation = 15;  // [U-94] "15 = none"

class Cell {
public:
    using Raw = std::uint32_t;

private:  // helpers first: clang needs them defined before constexpr use
    template <unsigned Len>
    static constexpr Raw mask = (Raw{1} << Len) - 1;

    template <unsigned Off, unsigned Len>
    [[nodiscard]] constexpr Raw get() const {
        static_assert(Off + Len <= 32);
        return (raw_ >> Off) & mask<Len>;
    }
    template <unsigned Off, unsigned Len>
    constexpr void set(Raw v) {
        static_assert(Off + Len <= 32);
        raw_ = (raw_ & ~(mask<Len> << Off)) | ((v & mask<Len>) << Off);
    }
    static constexpr int clamp_level(int level) { return std::clamp(level, kMinElevation, kMaxElevation); }


public:

    // Default = plain walkable land at elevation 0, no temp elevation.
    constexpr Cell() {
        set_elevation(0);
        clear_temp_elevation();
        set_walk(Walk::Walk);
    }
    constexpr explicit Cell(Raw raw) : raw_{raw} {}

    [[nodiscard]] constexpr Raw raw() const { return raw_; }

    // bits 0-3
    [[nodiscard]] constexpr Surface surface() const { return Surface(get<0, 4>()); }
    constexpr void set_surface(Surface s) { set<0, 4>(std::to_underlying(s)); }

    // bits 4-7: level + 2
    [[nodiscard]] constexpr int elevation() const { return int(get<4, 4>()) + kMinElevation; }
    constexpr void set_elevation(int level) { set<4, 4>(Raw(clamp_level(level) - kMinElevation)); }

    // bits 8-11: level + 2, 15 = none
    [[nodiscard]] constexpr bool has_temp_elevation() const { return get<8, 4>() != kNoTempElevation; }
    [[nodiscard]] constexpr int temp_elevation() const { return int(get<8, 4>()) + kMinElevation; }
    constexpr void set_temp_elevation(int level) { set<8, 4>(Raw(clamp_level(level) - kMinElevation)); }
    constexpr void clear_temp_elevation() { set<8, 4>(kNoTempElevation); }

    // Elevation that counts right now (temp overrides base) [U-45].
    [[nodiscard]] constexpr int effective_elevation() const {
        return has_temp_elevation() ? temp_elevation() : elevation();
    }

    // bits 12-17, 25-28: sense blockers
    [[nodiscard]] constexpr SenseBlock vision_fog() const { return SenseBlock(get<12, 2>()); }
    constexpr void set_vision_fog(SenseBlock f) { set<12, 2>(std::to_underlying(f)); }
    [[nodiscard]] constexpr SenseBlock sound_fog() const { return SenseBlock(get<14, 2>()); }
    constexpr void set_sound_fog(SenseBlock f) { set<14, 2>(std::to_underlying(f)); }
    [[nodiscard]] constexpr SenseBlock skill_fog() const { return SenseBlock(get<16, 2>()); }
    constexpr void set_skill_fog(SenseBlock f) { set<16, 2>(std::to_underlying(f)); }
    [[nodiscard]] constexpr SenseBlock seismic_fog() const { return SenseBlock(get<25, 2>()); }
    constexpr void set_seismic_fog(SenseBlock f) { set<25, 2>(std::to_underlying(f)); }
    [[nodiscard]] constexpr SenseBlock heat_fog() const { return SenseBlock(get<27, 2>()); }
    constexpr void set_heat_fog(SenseBlock f) { set<27, 2>(std::to_underlying(f)); }

    // bits 18-19
    [[nodiscard]] constexpr Walk walk() const { return Walk(get<18, 2>()); }
    constexpr void set_walk(Walk w) { set<18, 2>(std::to_underlying(w)); }

    // bits 20-21
    [[nodiscard]] constexpr Wall wall() const { return Wall(get<20, 2>()); }
    constexpr void set_wall(Wall w) { set<20, 2>(std::to_underlying(w)); }

    // bit 22
    [[nodiscard]] constexpr bool temp_walkable() const { return get<22, 1>() != 0; }
    constexpr void set_temp_walkable(bool b) { set<22, 1>(b ? 1u : 0u); }

    // bits 23-24
    [[nodiscard]] constexpr Tree tree() const { return Tree(get<23, 2>()); }
    constexpr void set_tree(Tree t) { set<23, 2>(std::to_underlying(t)); }

    // bits 29-30 [X-30]
    [[nodiscard]] constexpr Bush bush() const { return Bush(get<29, 2>()); }
    constexpr void set_bush(Bush b) { set<29, 2>(std::to_underlying(b)); }
    // bit 31 stays reserved [U-94]

    friend constexpr bool operator==(Cell, Cell) = default;

    Raw raw_ = 0;
};

static_assert(sizeof(Cell) == 4, "[U-91] 32 bits per cell");

// Compile-time round-trip check of the layout.
static_assert([] {
    Cell c;
    c.set_surface(Surface::Platinum);
    c.set_elevation(-2);
    c.set_temp_elevation(6);
    c.set_vision_fog(SenseBlock::Brush);
    c.set_heat_fog(SenseBlock::Partial);
    c.set_wall(Wall::Ice);
    c.set_tree(Tree::Burned);
    c.set_bush(Bush::Alive);
    c.set_temp_walkable(true);
    return c.surface() == Surface::Platinum && c.elevation() == -2 && c.temp_elevation() == 6 &&
           c.effective_elevation() == 6 && c.vision_fog() == SenseBlock::Brush &&
           c.heat_fog() == SenseBlock::Partial && c.wall() == Wall::Ice && c.tree() == Tree::Burned &&
           c.bush() == Bush::Alive && c.temp_walkable() && c.walk() == Walk::Walk;
}());

}  // namespace em
