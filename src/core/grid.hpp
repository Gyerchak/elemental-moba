#pragma once
// [U-90] 16 384 game units, 32-unit cells → 512 × 512 grid.
#include <cstdlib>
#include <span>
#include <vector>

#include "core/cell.hpp"

namespace em {

inline constexpr int kMapUnits = 16'384;
inline constexpr int kCellUnits = 32;
inline constexpr int kGridSize = kMapUnits / kCellUnits;
static_assert(kGridSize == 512);

class Grid {
public:
    Grid() : cells_(std::size_t(kGridSize) * kGridSize) {}

    [[nodiscard]] static constexpr bool in_bounds(int x, int y) {
        return unsigned(x) < unsigned(kGridSize) && unsigned(y) < unsigned(kGridSize);
    }
    [[nodiscard]] static constexpr int to_cell(int units) { return units / kCellUnits; }

    [[nodiscard]] Cell& at(int x, int y) { return cells_[index(x, y)]; }
    [[nodiscard]] Cell at(int x, int y) const { return cells_[index(x, y)]; }

    [[nodiscard]] std::span<const Cell> cells() const { return cells_; }

    // [U-93] Can a ground unit step between two neighbouring cells?
    // 1 elevation level difference = walkable, 2+ = not.
    [[nodiscard]] bool can_step(int x0, int y0, int x1, int y1) const {
        if (!in_bounds(x1, y1)) return false;
        const Cell to = at(x1, y1);
        const bool open = to.temp_walkable() || (to.walk() == Walk::Walk && to.wall() == Wall::None &&
                                                 to.tree() != Tree::Alive);
        return open && std::abs(to.effective_elevation() - at(x0, y0).effective_elevation()) <= 1;
    }

private:
    static constexpr std::size_t index(int x, int y) { return std::size_t(y) * kGridSize + std::size_t(x); }

    std::vector<Cell> cells_;  // 512*512*4 B = 1 MiB, row-major
};

}  // namespace em
