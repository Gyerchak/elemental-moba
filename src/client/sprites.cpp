// [U-258] See sprites.hpp.
#include "client/sprites.hpp"
#include "client/anim.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <thread>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace em {

namespace {
std::unique_ptr<SpriteSheet> parse_sheet(const std::string& txt) {
    std::unique_ptr<SpriteSheet> s;
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
            else if (k == "wrap") ls >> s->wrap;  // [U-486]
            else if (k == "use") {
                std::string key, to;
                ls >> key >> to;
                s->use[key] = to;
            }
            else if (k == "anim") {
                std::string name;
                SpriteSheet::Anim a;
                ls >> name >> a.col >> a.n >> a.fps;
                s->anims[name] = a;
            }
        }
    return s;
}
}  // namespace

// [U-505] Decoding a sheet takes 75–400 ms: done on background threads BEFORE the match, never during play. The main
// thread only uploads the finished pictures, a few per frame (a texture can only be made there).
void Sprites::preload(const std::vector<std::string>& dirs) {
    if (!res_) return;
    std::vector<Job> jobs;
    for (const std::string& dir : dirs) {
        const std::string key = res_->pack() + "|" + dir;
        if (cache_.contains(key) || std::ranges::any_of(pending_, [&](const std::string& p) { return p == key; })) continue;
        const std::string txt = res_->file(dir + "/sheet.txt"), png = res_->file(dir + "/sheet.png");
        if (txt.empty() || png.empty()) continue;
        pending_.push_back(key);
        jobs.push_back({key, dir, txt, png});
    }
    if (jobs.empty()) return;
    auto queue = std::make_shared<std::vector<Job>>(std::move(jobs));
    auto next = std::make_shared<std::atomic<std::size_t>>(0);
    const float scale = scale_;
    const int workers = std::clamp(int(std::thread::hardware_concurrency()) - 2, 1, 6);
    for (int w = 0; w < workers; ++w)
        std::thread([this, queue, next, scale] {
            for (std::size_t i = (*next)++; i < queue->size(); i = (*next)++) {
                const Job& j = (*queue)[i];
                Decoded d{j.key, j.dir, parse_sheet(j.txt), SDL_LoadPNG(j.png.c_str())};
                if (d.pic && scale < 0.99f)
                    if (SDL_Surface* small = SDL_ScaleSurface(d.pic, std::max(1, int(float(d.pic->w) * scale)), std::max(1, int(float(d.pic->h) * scale)), SDL_SCALEMODE_LINEAR)) {
                        SDL_DestroySurface(d.pic);
                        d.pic = small;
                    }
                const std::scoped_lock lock(done_mutex_);
                done_.push_back(std::move(d));
            }
        }).detach();
}

int Sprites::pump(int max_uploads) {
    for (int n = 0; n < max_uploads; ++n) {
        Decoded d;
        {
            const std::scoped_lock lock(done_mutex_);
            if (done_.empty()) break;
            d = std::move(done_.back());
            done_.pop_back();
        }
        std::erase(pending_, d.key);
        std::unique_ptr<SpriteSheet> s = std::move(d.sheet);
        if (d.pic) {
            if (s && !s->anims.empty())
                if (SDL_Texture* t = SDL_CreateTextureFromSurface(res_->renderer(), d.pic)) {
                    SDL_SetTextureScaleMode(t, scale_ < 0.99f ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
                    s->tex = t, s->owned = true, s->tex_scale = scale_ < 0.99f ? scale_ : 1.f;
                }
            SDL_DestroySurface(d.pic);
        }
        if (!s || !s->tex) {  // [U-499] bigger than the GPU allows …: say so instead of failing silently
            if (std::error_code ec; std::filesystem::create_directories("logs", ec), true)
                if (std::ofstream lg("logs/models.txt", std::ios::app); lg) lg << d.dir << "/sheet.png could not be loaded: " << SDL_GetError() << '\n';
            s.reset();
        }
        cache_[d.key] = std::move(s);
    }
    return int(pending_.size());
}

const SpriteSheet* Sprites::get_dir(const std::string& dir) {
    const std::string key = (res_ ? res_->pack() : std::string()) + "|" + dir;  // (a pack switch finds other files)
    if (auto it = cache_.find(key); it != cache_.end()) return it->second.get();
    while (std::ranges::any_of(pending_, [&](const std::string& p) { return p == key; })) {  // [U-505] (being decoded: wait for it)
        pump(8);
        if (auto it = cache_.find(key); it != cache_.end()) return it->second.get();
        SDL_Delay(1);
    }
    std::unique_ptr<SpriteSheet> s;
    const std::string txt = res_ ? res_->file(dir + "/sheet.txt") : std::string();
    if (!txt.empty()) {
        s = parse_sheet(txt);
        if (scale_ < 0.99f) {  // [U-502] a scaled-down copy of the original (the full size never goes to the GPU)
            const std::string png = res_->file(dir + "/sheet.png");
            if (SDL_Surface* full = png.empty() ? nullptr : SDL_LoadPNG(png.c_str())) {
                if (SDL_Surface* small = SDL_ScaleSurface(full, std::max(1, int(float(full->w) * scale_)), std::max(1, int(float(full->h) * scale_)), SDL_SCALEMODE_LINEAR)) {
                    if (SDL_Texture* t = SDL_CreateTextureFromSurface(res_->renderer(), small)) {
                        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
                        s->tex = t, s->owned = true, s->tex_scale = scale_;
                    }
                    SDL_DestroySurface(small);
                }
                SDL_DestroySurface(full);
            }
        }
        if (!s->tex) s->tex = res_->texture(dir + "/sheet.png");
        if (!s->tex && !s->anims.empty())  // [U-499] e.g. bigger than the GPU allows: say so (logs/models.txt) instead of failing silently
            if (std::error_code ec; std::filesystem::create_directories("logs", ec), true)
                if (std::ofstream lg("logs/models.txt", std::ios::app); lg) lg << dir << "/sheet.png could not be loaded: " << SDL_GetError() << '\n';
        if (!s->tex || s->anims.empty()) s.reset();
    }
    const SpriteSheet* p = s.get();
    cache_[key] = std::move(s);
    return p;
}

void Sprites::set_scale(float s) {
    if (std::abs(s - scale_) < 0.001f) return;
    scale_ = s;
    for (auto& [k, sh] : cache_)
        if (sh && sh->owned && sh->tex) SDL_DestroyTexture(sh->tex);
    cache_.clear();
}

const SpriteSheet::Anim* SpriteSheet::find(std::string key) const {
    for (int guard = 0; guard < 12 && !key.empty(); ++guard) {
        if (auto u = use.find(key); u != use.end())
            if (auto a = anims.find(u->second); a != anims.end()) return &a->second;
        if (auto a = anims.find(key); a != anims.end()) return &a->second;
        key = anim_fallback(key);
    }
    if (auto a = anims.find("idle"); a != anims.end()) return &a->second;
    return anims.empty() ? nullptr : &anims.begin()->second;
}

void Sprites::draw(SDL_Renderer* r, const SpriteSheet& s, const std::string& anim, double t, int dir, float sx, float sy, float scale,
                   gfx::Rgba tint, bool hold) {
    const SpriteSheet::Anim* ap = s.find(anim);
    if (!ap) return;
    const SpriteSheet::Anim& a = *ap;
    const double pos = std::max(0.0, t) * a.fps;
    int f = int(pos);
    const float mix = float(pos - double(f));  // [U-501] how far towards the next frame
    const int last = std::max(1, a.n);
    int next = f + 1;
    if (hold) f = std::min(f, last - 1), next = std::min(next, last - 1);
    else f %= last, next %= last;
    const int row = s.dirs > 1 ? ((dir % s.dirs) + s.dirs) % s.dirs : 0;
    const float size = s.ortho_m * kUnitsPerMetre * scale;  // the frame width on screen
    // [U-389] whole pixels: fractional positions made thin lines shimmer while moving / zoomed out
    const SDL_FRect dst{std::round(sx - size * s.ax), std::round(sy - size * s.ay), std::round(size), std::round(size)};
    if (!std::isfinite(dst.x) || !std::isfinite(dst.y) || !(size > 0) || size > 20'000.f || std::abs(dst.x) > 60'000.f || std::abs(dst.y) > 60'000.f)
        return;  // [U-467] a bad position must never become a screen-wide texture
    auto frame_rect = [&](int fr) {
        // half a pixel inside the frame: the smooth filter must never read the NEIGHBOUR frame (thin lines / specks) [U-338]
        // [U-486] wrapped sheets: frame i sits in block i / wrap (each block = `dirs` rows), column i % wrap
        const int i = a.col + fr, cx = s.wrap > 0 ? i % s.wrap : i, block = s.wrap > 0 ? i / s.wrap : 0;
        const float k = s.tex_scale, fp = float(s.px) * k;  // [U-502] (the sheet may be a scaled copy)
        return SDL_FRect{float(cx) * fp + 0.5f, float(block * s.dirs + row) * fp + 0.5f, fp - 1.f, fp - 1.f};
    };
    SDL_SetTextureColorMod(s.tex, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(s.tex, tint.a);
    const SDL_FRect src = frame_rect(f);
    const SDL_FPoint pivot{size * s.ax, size * s.ay * 0.7f};
    if (tilt != 0) SDL_RenderTextureRotated(r, s.tex, &src, &dst, tilt, &pivot, SDL_FLIP_NONE);
    else SDL_RenderTexture(r, s.tex, &src, &dst);
    if (smooth && next != f && mix > 0.02f) {  // [U-501] the next frame fades in over this one
        const SDL_FRect src2 = frame_rect(next);
        SDL_SetTextureAlphaMod(s.tex, std::uint8_t(float(tint.a) * mix));
        if (tilt != 0) SDL_RenderTextureRotated(r, s.tex, &src2, &dst, tilt, &pivot, SDL_FLIP_NONE);
        else SDL_RenderTexture(r, s.tex, &src2, &dst);
    }
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
