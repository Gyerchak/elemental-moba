#pragma once
// [U-501] Post-processing of the WORLD (the HUD stays crisp), on the Vulkan SDL GPU renderer with custom SPIR-V shaders:
//   the world is drawn into a texture → bright pass + blur (bloom) → grade (sharpen, contrast, saturation, tint, vignette).
#include <SDL3/SDL.h>

namespace em {

struct PostParams {
    float tint[3] = {1, 1, 1};
    float bloom = 0;       // 0 = no bloom; strength of the glow added on top
    float sharpen = 0, contrast = 1, saturation = 1, vignette = 0;
    bool grade = false;    // false = copy the world as it is (only scaled)
};

class PostFx {
public:
    ~PostFx();
    bool init(SDL_Renderer* r);          // false = shaders unavailable: the game simply draws without post-processing
    // the world is drawn into the scene texture from here on; [U-502] scale < 1 = a scaled-down image (lower quality)
    void begin(int w, int h, float scale);
    // [U-502] animated water + lava over the ground: `mask` (r water, g lava) drawn like the ground layer
    void draw_water(SDL_Texture* mask, const SDL_FRect& src, const SDL_FRect& dst, bool flip, float time, float cells);
    void end(const PostParams& p);       // applies the effects and draws the result to the window
    [[nodiscard]] bool ready() const { return ok_; }

private:
    void targets(int w, int h);
    void pass(SDL_Texture* src, SDL_Texture* dst, SDL_GPURenderState* st);
    SDL_Renderer* r_ = nullptr;
    SDL_GPUDevice* dev_ = nullptr;
    SDL_GPUShader* sh_bright_ = nullptr, *sh_blur_ = nullptr, *sh_grade_ = nullptr, *sh_water_ = nullptr;
    SDL_GPURenderState* bright_ = nullptr, *blur_h_ = nullptr, *blur_v_ = nullptr, *grade_ = nullptr, *water_ = nullptr;
    float scale_ = 1;
    SDL_Texture* scene_ = nullptr, *small_a_ = nullptr, *small_b_ = nullptr;
    int w_ = 0, h_ = 0;
    bool ok_ = false;
};

}  // namespace em
