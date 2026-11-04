// [U-501] See postfx.hpp.
#include "client/postfx.hpp"

#include <algorithm>

#include "client/shaders/shaders_spv.hpp"

namespace em {

namespace {
SDL_GPUShader* make_shader(SDL_GPUDevice* dev, const std::uint32_t* code, std::size_t bytes) {
    SDL_GPUShaderCreateInfo info{};
    info.code = reinterpret_cast<const Uint8*>(code);
    info.code_size = bytes;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = 1;
    info.num_uniform_buffers = 1;
    return SDL_CreateGPUShader(dev, &info);
}
SDL_GPURenderState* make_state(SDL_Renderer* r, SDL_GPUShader* sh) {
    SDL_GPURenderStateCreateInfo info{};
    info.fragment_shader = sh;
    return SDL_CreateGPURenderState(r, &info);
}
SDL_Texture* make_target(SDL_Renderer* r, int w, int h) {
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, std::max(1, w), std::max(1, h));
    if (t) {
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    }
    return t;
}
}  // namespace

PostFx::~PostFx() {
    if (scene_) SDL_DestroyTexture(scene_);
    if (small_a_) SDL_DestroyTexture(small_a_);
    if (small_b_) SDL_DestroyTexture(small_b_);
    for (SDL_GPURenderState* s : {bright_, blur_h_, blur_v_, grade_})
        if (s) SDL_DestroyGPURenderState(s);
    if (dev_)
        for (SDL_GPUShader* s : {sh_bright_, sh_blur_, sh_grade_})
            if (s) SDL_ReleaseGPUShader(dev_, s);
}

bool PostFx::init(SDL_Renderer* r) {
    r_ = r;
    dev_ = static_cast<SDL_GPUDevice*>(SDL_GetPointerProperty(SDL_GetRendererProperties(r), SDL_PROP_RENDERER_GPU_DEVICE_POINTER, nullptr));
    if (!dev_) return false;
    sh_bright_ = make_shader(dev_, shaders::k_bright, sizeof(shaders::k_bright));
    sh_blur_ = make_shader(dev_, shaders::k_blur, sizeof(shaders::k_blur));
    sh_grade_ = make_shader(dev_, shaders::k_grade, sizeof(shaders::k_grade));
    if (!sh_bright_ || !sh_blur_ || !sh_grade_) {
        SDL_Log("post-processing off: %s", SDL_GetError());
        return false;
    }
    bright_ = make_state(r, sh_bright_);
    blur_h_ = make_state(r, sh_blur_);
    blur_v_ = make_state(r, sh_blur_);
    grade_ = make_state(r, sh_grade_);
    ok_ = bright_ && blur_h_ && blur_v_ && grade_;
    if (!ok_) SDL_Log("post-processing off: %s", SDL_GetError());
    return ok_;
}

void PostFx::targets(int w, int h) {
    if (w == w_ && h == h_ && scene_) return;
    w_ = w, h_ = h;
    for (SDL_Texture** t : {&scene_, &small_a_, &small_b_})
        if (*t) SDL_DestroyTexture(*t), *t = nullptr;
    scene_ = make_target(r_, w, h);
    small_a_ = make_target(r_, w / 4, h / 4);
    small_b_ = make_target(r_, w / 4, h / 4);
    const struct { float x, y, a, b; } hor{4.f / float(std::max(1, w)), 0, 0, 0}, ver{0, 4.f / float(std::max(1, h)), 0, 0};
    SDL_SetGPURenderStateFragmentUniforms(blur_h_, 0, &hor, sizeof(hor));
    SDL_SetGPURenderStateFragmentUniforms(blur_v_, 0, &ver, sizeof(ver));
    const struct { float threshold, knee, a, b; } br{0.72f, 0.2f, 0, 0};
    SDL_SetGPURenderStateFragmentUniforms(bright_, 0, &br, sizeof(br));
}

void PostFx::begin(int w, int h) {
    if (!ok_) return;
    targets(w, h);
    SDL_SetRenderTarget(r_, scene_);
}

void PostFx::pass(SDL_Texture* src, SDL_Texture* dst, SDL_GPURenderState* st) {
    SDL_SetRenderTarget(r_, dst);
    SDL_SetGPURenderState(r_, st);
    SDL_RenderTexture(r_, src, nullptr, nullptr);
    SDL_SetGPURenderState(r_, nullptr);
}

void PostFx::end(const PostParams& p) {
    if (!ok_ || !scene_) return;
    if (p.bloom > 0) {
        pass(scene_, small_a_, bright_);
        pass(small_a_, small_b_, blur_h_);
        pass(small_b_, small_a_, blur_v_);
    }
    const struct {
        float tint[4];
        float texel[2];
        float sharpen, contrast;
        float saturation, vignette, a, b;
    } g{{p.tint[0], p.tint[1], p.tint[2], 1}, {1.f / float(std::max(1, w_)), 1.f / float(std::max(1, h_))}, p.sharpen, p.contrast, p.saturation, p.vignette, 0, 0};
    SDL_FlushRenderer(r_);
    SDL_SetGPURenderStateFragmentUniforms(grade_, 0, &g, sizeof(g));
    pass(scene_, nullptr, grade_);
    if (p.bloom > 0) {  // the glow on top: additive, scaled up smoothly
        SDL_SetTextureBlendMode(small_a_, SDL_BLENDMODE_ADD);
        const float s = std::clamp(p.bloom, 0.f, 1.f);
        SDL_SetTextureColorModFloat(small_a_, s, s, s);
        SDL_RenderTexture(r_, small_a_, nullptr, nullptr);
        SDL_SetTextureBlendMode(small_a_, SDL_BLENDMODE_NONE);
        SDL_SetTextureColorModFloat(small_a_, 1, 1, 1);
    }
}

}  // namespace em
