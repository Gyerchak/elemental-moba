#pragma once
// [X-09] Grid A* (8 directions, no corner cutting) with string pulling. Walls appear at runtime
// → callers re-path when blocked [U-24][U-45].
#include <cstdint>
#include <utility>
#include <vector>

#include "core/grid.hpp"

namespace em {

using PathCells = std::vector<std::pair<std::int16_t, std::int16_t>>;

// Returns false when no path (then `out` holds the best partial path towards the goal).
// [X-94] `clearance` (optional): a cell is usable only if clearance > k → the whole body fits.
bool find_path(const Grid& g, int sx, int sy, int tx, int ty, PathCells& out, int max_nodes = 60'000,
               const std::uint8_t* clearance = nullptr, int k = 0, int* expanded_out = nullptr);

// Straight walk possible between two cells? (every step passes Grid::can_step and the clearance)
[[nodiscard]] bool line_walkable(const Grid& g, int x0, int y0, int x1, int y1, const std::uint8_t* clearance = nullptr,
                                 int k = 0);

}  // namespace em
