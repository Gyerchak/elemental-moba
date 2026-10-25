// [U-03] Placeholder visualisation: default map → PPM image (1 pixel = 1 cell).
// Usage: map_preview out.ppm   then   magick out.ppm out.png
#include <array>
#include <cstdint>
#include <cstdio>
#include <print>
#include <vector>

#include "map/default_map.hpp"

namespace {

using Rgb = std::array<std::uint8_t, 3>;

Rgb surface_colour(em::Surface s) {
    using enum em::Surface;
    switch (s) {
        case Land: return {118, 170, 72};
        case Dryland: return {186, 160, 104};
        case Glacier: return {222, 240, 250};
        case Water: return {96, 170, 230};
        case Sand: return {232, 212, 150};
        case Stone: return {140, 136, 128};
        case Wood: return {150, 100, 60};
        case DeepWater: return {24, 64, 140};
        case Magma: return {230, 80, 20};
        case Metal: return {170, 175, 185};
        case Platinum: return {225, 228, 235};
        case Count: break;
    }
    return {255, 0, 255};
}

Rgb mix(Rgb a, Rgb b, int b_pct) {
    Rgb r;
    for (int i = 0; i < 3; ++i) r[i] = std::uint8_t((a[i] * (100 - b_pct) + b[i] * b_pct) / 100);
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "map.ppm";
    const em::Grid g = em::make_default_map();

    std::vector<std::uint8_t> px;
    px.reserve(std::size_t(em::kGridSize) * em::kGridSize * 3);
    for (int y = 0; y < em::kGridSize; ++y)
        for (int x = 0; x < em::kGridSize; ++x) {
            const em::Cell c = g.at(x, y);
            Rgb col = surface_colour(c.surface());
            if (c.tree() == em::Tree::Alive) col = {34, 92, 40};
            else if (c.bush() == em::Bush::Alive) col = {70, 130, 50};
            // [U-06] team tint: Fire Nation red, Earth Nation brown-green.
            const bool fire = em::side_of(x, y) == em::Team::FireNation;
            col = mix(col, fire ? Rgb{200, 40, 20} : Rgb{90, 110, 30}, 10);
            px.insert(px.end(), col.begin(), col.end());
        }

    std::FILE* f = std::fopen(out, "wb");
    if (!f) {
        std::println(stderr, "cannot open {}", out);
        return 1;
    }
    std::print(f, "P6\n{} {}\n255\n", em::kGridSize, em::kGridSize);
    std::fwrite(px.data(), 1, px.size(), f);
    std::fclose(f);
    std::println("wrote {} ({}x{})", out, em::kGridSize, em::kGridSize);
}
