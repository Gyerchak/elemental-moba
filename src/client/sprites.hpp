#pragma once
// [U-258] Sprite sheets rendered from the Blender placeholder models (tools/art/make_models.py):
//   resourcepacks/<pack>/models/<id>/sheet.png + sheet.txt   (rows = 8 directions, columns = animation frames)
// <id>: champion id ("ember_monk"), minion_melee / _ranged / _siege / _super, a monster ("spirit_wolf"),
// turret / inhibitor / nexus, tree / bush. Missing sheet → the game draws its old placeholder shapes.
// A skin may bring its own model: resourcepacks/<pack>/champions/<id>/skins/<skin>/model/sheet.png + sheet.txt.
#include <SDL3/SDL.h>

#include <map>
#include <memory>
#include <string>

#include "client/draw.hpp"
#include "client/resources.hpp"

namespace em {

inline constexpr float kUnitsPerMetre = 38.9f;  // champion 1.8 m = 70 game units

struct SpriteSheet {
    SDL_Texture* tex = nullptr;
    int px = 128, dirs = 8;
    float ortho_m = 2.6f, ax = 0.5f, ay = 0.8f;
    struct Anim {
        int col = 0, n = 1, fps = 6;
    };
    std::map<std::string, Anim> anims;
};

class Sprites {
public:
    void init(Resources* res) { res_ = res; }
    const SpriteSheet* get(const std::string& id) { return get_dir("models/" + id); }  // nullptr = no sheet for this model
    // a sheet in any pack folder, e.g. "champions/ember_monk/skins/comet/model" (a skin's own model)
    const SpriteSheet* get_dir(const std::string& dir);
    // Draws one frame. (sx, sy) = the ground point on the screen; `scale` = screen px per game unit × size;
    // dir 0…7 (0 = facing screen right, clockwise); t = seconds into the animation; hold = stop on the last frame.
    static void draw(SDL_Renderer* r, const SpriteSheet& s, const std::string& anim, double t, int dir, float sx, float sy,
                     float scale, gfx::Rgba tint = {}, bool hold = false);

private:
    Resources* res_ = nullptr;
    std::map<std::string, std::unique_ptr<SpriteSheet>> cache_;
};

// screen direction 0…7 from a world facing vector (flip = the view is turned 180° for the Earth team)
[[nodiscard]] int sprite_dir(float fx, float fy, bool flip);

}  // namespace em
