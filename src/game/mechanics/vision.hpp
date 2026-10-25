#pragma once
// [U-75][X-10] Fog of war per team on the 512 × 512 grid. Shared by server (who may see what)
// and client (what to draw dark). Rays from every viewer to the edge of its sight square.
//  - Night: a cell is seen if dist ≤ sight × max(ambient %, light % of that cell) [U-108][U-115][U-117].
//  - Trees, stone walls, steam fog (vision fog Full) and higher ground block the ray (the blocker itself is seen).
//  - Brush cells are seen only from inside the same brush [X-31]; units hide only if fully inside [U-140].
#include <cstdint>
#include <span>
#include <vector>

#include "core/grid.hpp"
#include "core/light.hpp"

namespace em {

struct Viewer {
    std::int32_t x = 0, y = 0;  // game units
    std::int32_t sight = 0;     // game units, full (day) sight
    // [U-150][X-101] eyes: full sight only inside the field of view around the facing; behind
    // the eyes only a short "body sense" radius. fov_cos = cos(half angle) in Q14; ≤ −16384 = all around.
    std::int32_t fx = 16'384, fy = 0;
    std::int32_t fov_cos = -16'384;
    std::int32_t near = 0;
};
inline constexpr std::int32_t kEyeFovCos = -4'240;  // [X-101] 210° field of view (half angle 105°)
inline constexpr std::int32_t kBodySense = 160;     // [X-101] units seen / felt behind you
// [U-152][X-104] Vision starts at the EYES (the head, in front of the body centre), so seeing into a
// bush needs the eyes inside it. Beasts carry their head further forward.
[[nodiscard]] constexpr std::int32_t eye_offset(bool beast, std::int32_t size_pct) { return (beast ? 40 : 20) * size_pct / 100; }

// [U-241] A cloud (steam, smoke, sand, snow): blocks vision for every team like a wall, but you can walk in.
// Puddle [U-292]: water lying on the ground (cooled steam). It is NOT a cloud for the eyes — it never blocks vision.
// IcePatch [U-293]: frozen water on the ground (slippery), also never blocks vision.
// Arrival [U-350]: the light at the far portal while someone channels towards it (never blocks vision).
enum class CloudKind : std::uint8_t { Steam, Smoke, Sand, Snow, Puddle, IcePatch, Arrival };
struct Cloud {
    std::int32_t x = 0, y = 0, radius = 0;  // game units
    std::int64_t until_ms = 0;
    CloudKind kind = CloudKind::Steam;
    std::uint32_t owner = 0;  // who made it (portal arrivals end with the channel)
};
// cells covered by any cloud (512 × 512, 1 = blocked)
void cloud_mask(std::span<const Cloud> clouds, std::vector<std::uint8_t>& out);

class VisionMap {
public:
    VisionMap() : vis_(std::size_t(kGridSize) * kGridSize, 0) {}
    // `clouds` (optional): cloud_mask() cells, blocking like walls
    void compute(const Grid& g, std::span<const Viewer> viewers, const LightMap& light, int ambient_pct,
                 const std::vector<std::uint8_t>* clouds = nullptr);
    [[nodiscard]] bool cell(int cx, int cy) const {
        return Grid::in_bounds(cx, cy) && vis_[std::size_t(cy) * kGridSize + std::size_t(cx)] != 0;
    }
    [[nodiscard]] bool at_units(std::int32_t x, std::int32_t y) const {
        return x >= 0 && y >= 0 && cell(x / kCellUnits, y / kCellUnits);
    }
    [[nodiscard]] const std::vector<std::uint8_t>& raw() const { return vis_; }

private:
    void cast(const Grid& g, const Viewer& v, const LightMap& light, int ambient_pct, const std::vector<std::uint8_t>* clouds);
    std::vector<std::uint8_t> vis_;
};

}  // namespace em
