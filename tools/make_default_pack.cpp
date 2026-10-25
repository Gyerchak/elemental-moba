// [U-197][U-198][U-199][U-201][U-226] Generates the placeholder DEFAULT resource pack:
//   resourcepacks/default/{pack.txt, champions/<id>/base/{icon,splash}.bmp + model.txt, champions/<id>/skins/shadow/…,
//   sounds/*.wav (casts, hits, autos, deaths, pings, emotes, ui, anim/<slot>.wav, en/ + pl/ voice), music/{menu,match}.wav}
// Usage: make_default_pack [output root]   (default: resourcepacks/default). Real art replaces these files later.
#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <vector>

#include "client/anim.hpp"
#include "game/content.hpp"

namespace fs = std::filesystem;
using namespace em;

namespace {

std::string champ_id(const ChampionDef& c) {
    std::string id(c.name);
    for (char& ch : id) ch = ch == ' ' ? '_' : char(std::tolower(static_cast<unsigned char>(ch)));
    std::erase(id, '\'');
    return id;
}

// ---- images (software renderer on a surface)
void save_image(const fs::path& path, int w, int h, std::uint32_t colour, Element el, std::string_view label, bool dark) {
    SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* r = SDL_CreateSoftwareRenderer(s);
    const Uint8 cr = Uint8(colour >> 16), cg = Uint8(colour >> 8), cb = Uint8(colour);
    for (int y = 0; y < h; ++y) {  // vertical gradient
        const float t = float(y) / float(h);
        const float k = dark ? 0.35f : 1.f;
        SDL_SetRenderDrawColor(r, Uint8(float(cr) * (0.35f + 0.65f * (1 - t)) * k), Uint8(float(cg) * (0.35f + 0.65f * (1 - t)) * k),
                               Uint8(float(cb) * (0.35f + 0.65f * (1 - t)) * k), 255);
        SDL_RenderLine(r, 0, float(y), float(w), float(y));
    }
    // element badge in the middle
    const std::uint32_t ec = element_colour(el);
    SDL_SetRenderDrawColor(r, Uint8(ec >> 16), Uint8(ec >> 8), Uint8(ec), 255);
    const float cx = float(w) / 2, cy = float(h) * 0.45f, rad = float(std::min(w, h)) * 0.28f;
    for (int i = 0; i < 360; i += 2) {
        const float a = float(i) * 0.01745f;
        SDL_RenderLine(r, cx, cy, cx + std::cos(a) * rad, cy + std::sin(a) * rad);
    }
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    const float sc = w >= 200 ? 2.f : 1.f;
    SDL_SetRenderScale(r, sc, sc);
    std::string l(label);
    SDL_RenderDebugText(r, (float(w) / sc - float(l.size()) * 8) / 2, float(h) * 0.82f / sc, l.c_str());
    SDL_RenderPresent(r);
    fs::create_directories(path.parent_path());
    SDL_SaveBMP(s, path.string().c_str());
    SDL_DestroyRenderer(r);
    SDL_DestroySurface(s);
}

// ---- sounds (16-bit mono 22 050 Hz WAV)
void save_wav(const fs::path& path, const std::vector<float>& smp) {
    fs::create_directories(path.parent_path());
    std::ofstream o(path, std::ios::binary);
    const std::uint32_t rate = 22'050, n = std::uint32_t(smp.size()), data = n * 2;
    auto u32 = [&](std::uint32_t v) { o.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](std::uint16_t v) { o.write(reinterpret_cast<const char*>(&v), 2); };
    o.write("RIFF", 4);
    u32(36 + data);
    o.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(1);
    u32(rate);
    u32(rate * 2);
    u16(2);
    u16(16);
    o.write("data", 4);
    u32(data);
    for (float f : smp) {
        const auto v = std::int16_t(std::clamp(f, -1.f, 1.f) * 32'000);
        o.write(reinterpret_cast<const char*>(&v), 2);
    }
}
std::vector<float> tone(float seconds, float f0, float f1, float noise, float decay, unsigned seed = 1) {
    const int n = int(seconds * 22'050);
    std::vector<float> s(static_cast<std::size_t>(n));
    unsigned x = seed * 2'654'435'761u + 1;
    float ph = 0;
    for (int i = 0; i < n; ++i) {
        const float t = float(i) / 22'050.f;
        const float f = f0 + (f1 - f0) * t / seconds;
        ph += 6.2831853f * f / 22'050.f;
        x = x * 1'664'525u + 1'013'904'223u;
        const float nz = (float(x >> 8) / float(1u << 24)) * 2 - 1;
        const float env = std::exp(-t * decay) * std::min(1.f, t * 200);
        s[std::size_t(i)] = (std::sin(ph) * (1 - noise) + nz * noise) * env * 0.6f;
    }
    return s;
}
std::vector<float> music(float seconds, std::initializer_list<int> chords, float tempo) {
    const int n = int(seconds * 22'050);
    std::vector<float> s(std::size_t(n), 0.f);
    const std::vector<int> ch(chords);
    for (int i = 0; i < n; ++i) {
        const float t = float(i) / 22'050.f;
        const int step = int(t * tempo / 60.f);
        const int root = ch[std::size_t(step / 4) % ch.size()];
        float v = 0;
        for (int k : {0, 4, 7}) {
            const float f = 110.f * std::pow(2.f, float(root + k) / 12.f);
            v += std::sin(6.2831853f * f * t) * 0.12f;
        }
        const float beat = std::fmod(t * tempo / 60.f, 1.f);
        v += std::sin(6.2831853f * 55.f * std::pow(2.f, float(root) / 12.f) * t) * 0.25f * std::exp(-beat * 6);
        s[std::size_t(i)] = v * 0.7f;
    }
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    const fs::path root = argc > 1 ? argv[1] : "resourcepacks/default";
    SDL_Init(0);
    fs::create_directories(root);
    std::ofstream(root / "pack.txt") << "name=Default (placeholder)\ndescription=Generated placeholder art, sounds and music. Replace any file.\n";
    for (const ChampionDef& c : champions()) {
        const fs::path base = root / "champions" / champ_id(c) / "base";
        save_image(base / "icon.bmp", 64, 64, c.colour, c.element, "", false);
        save_image(base / "splash.bmp", 240, 360, c.colour, c.element, c.name, false);
        std::ofstream(base / "model.txt") << "# [U-198] placeholder model: body circles from the hitbox, height in game units\n"
                                          << "body=" << (c.body == &kBroadBody ? "broad" : c.body == &kSlimBody ? "slim" : "humanoid")
                                          << "\nheight=70\ncolour=" << std::hex << c.colour << std::dec << "\n";
        // one example skin per champion: "shadow" (darker) [U-97]
        const fs::path skin = root / "champions" / champ_id(c) / "skins" / "shadow";
        save_image(skin / "icon.bmp", 64, 64, c.colour, c.element, "", true);
        save_image(skin / "splash.bmp", 240, 360, c.colour, c.element, "shadow skin", true);
    }
    // sounds [U-201]
    const fs::path snd = root / "sounds";
    const char* els[] = {"Water", "Earth", "Fire", "Air", "Non-bender"};
    const float base_f[] = {520, 140, 300, 900, 220};
    const float noise[] = {0.3f, 0.5f, 0.7f, 0.6f, 0.2f};
    for (int i = 0; i < 5; ++i) {
        save_wav(snd / std::format("cast_{}.wav", els[i]), tone(0.35f, base_f[i], base_f[i] * 1.6f, noise[i], 6, unsigned(i + 1)));
        save_wav(snd / std::format("hit_{}.wav", els[i]), tone(0.18f, base_f[i] * 0.8f, base_f[i] * 0.5f, noise[i] + 0.1f, 14, unsigned(i + 7)));
    }
    save_wav(snd / "hit_auto.wav", tone(0.10f, 260, 180, 0.4f, 25));
    save_wav(snd / "auto_ranged.wav", tone(0.12f, 700, 400, 0.3f, 18));
    save_wav(snd / "death.wav", tone(0.25f, 300, 90, 0.5f, 8));
    save_wav(snd / "death_champion.wav", tone(0.9f, 400, 70, 0.3f, 3));
    for (int p = 0; p < 6; ++p) save_wav(snd / std::format("ping_{}.wav", p), tone(0.22f, 880 + float(p) * 110, 1'100 + float(p) * 110, 0.f, 9));
    for (std::string_view e : kEmoteNames) save_wav(snd / std::format("emote_{}.wav", e), tone(0.5f, 330, 660, 0.1f, 4));
    save_wav(snd / "ui_click.wav", tone(0.05f, 1'200, 900, 0.f, 50));
    for (int a = 0; a < int(AnimSlot::Count); ++a)  // [U-226] one placeholder sound per animation slot
        save_wav(snd / "anim" / std::format("{}.wav", anim_name(AnimSlot(a))), tone(0.2f, 200 + float(a) * 20, 160 + float(a) * 15, 0.3f, 10, unsigned(a + 20)));
    for (const char* lang : {"en", "pl"})  // [U-239] voice folders per language
        save_wav(snd / lang / "voice_select.wav", tone(0.4f, lang[0] == 'e' ? 400.f : 450.f, 500, 0.05f, 4));
    // music [U-197]
    save_wav(root / "music" / "menu.wav", music(24, {0, 5, 3, 7}, 84));
    save_wav(root / "music" / "match.wav", music(32, {0, 3, 8, 7}, 112));
    std::println("default pack written to {}", root.string());
    SDL_Quit();
}
