// [U-258] See sprites.hpp.
#include "client/sprites.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

namespace em {

const SpriteSheet* Sprites::get_dir(const std::string& dir) {
    const std::string key = (res_ ? res_->pack() : std::string()) + "|" + dir;  // (a pack switch finds other files)
    if (auto it = cache_.find(key); it != cache_.end()) return it->second.get();
    std::unique_ptr<SpriteSheet> s;
    const std::string txt = res_ ? res_->file(dir + "/sheet.txt") : std::string();
    if (!txt.empty()) {
        s = std::make_unique<SpriteSheet>();
        std::ifstream in(txt);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ls(line);
            std::string k;
            ls >> k;
            if (k == "frame_px") ls >> s->px;
            else if (k == "dirs") ls >> s->dirs;
            else if (k == "ortho_m") ls >> s->ortho_m;
            else if (k == "anchor") ls >> s->ax >> s->ay;
            else if (k == "anim") {
                std::string name;
                SpriteSheet::Anim a;
                ls >> name >> a.col >> a.n >> a.fps;
                s->anims[name] = a;
            }
        }
        s->tex = res_->texture(dir + "/sheet.png");
        if (!s->tex || s->anims.empty()) s.reset();
    }
    const SpriteSheet* p = s.get();
    cache_[key] = std::move(s);
    return p;
}

void Sprites::draw(SDL_Renderer* r, const SpriteSheet& s, const std::string& anim, double t, int dir, float sx, float sy, float scale,
                   gfx::Rgba tint, bool hold) {
    auto it = s.anims.find(anim);
    if (it == s.anims.end()) it = s.anims.find("idle");
    if (it == s.anims.end()) it = s.anims.begin();
    const SpriteSheet::Anim& a = it->second;
    int f = int(std::max(0.0, t) * a.fps);
    f = hold ? std::min(f, a.n - 1) : f % std::max(1, a.n);
    const int row = s.dirs > 1 ? ((dir % s.dirs) + s.dirs) % s.dirs : 0;
    // half a pixel inside the frame: the smooth filter must never read the NEIGHBOUR frame (thin lines / specks) [U-338]
    const SDL_FRect src{float((a.col + f) * s.px) + 0.5f, float(row * s.px) + 0.5f, float(s.px) - 1.f, float(s.px) - 1.f};
    const float size = s.ortho_m * kUnitsPerMetre * scale;  // the frame width on screen
    const SDL_FRect dst{sx - size * s.ax, sy - size * s.ay, size, size};
    SDL_SetTextureColorMod(s.tex, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(s.tex, tint.a);
    SDL_RenderTexture(r, s.tex, &src, &dst);
    SDL_SetTextureColorMod(s.tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(s.tex, 255);
}

int sprite_dir(float fx, float fy, bool flip) {
    if (fx == 0 && fy == 0) return 2;  // towards the camera
    if (flip) fx = -fx, fy = -fy;
    const float a = std::atan2(fy, fx);  // screen y grows downwards = world +y
    return (int(std::lround(a / 0.785398f)) + 8) % 8;
}

}  // namespace em
