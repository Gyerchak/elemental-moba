// Resource packs. See resources.hpp.
#include "client/resources.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "game/content.hpp"

namespace em {

namespace fs = std::filesystem;

Resources::~Resources() {
    for (auto& [k, t] : textures_)
        if (t) SDL_DestroyTexture(t);
}

void Resources::init(SDL_Renderer* r, std::string pack) {
    r_ = r;
    pack_ = std::move(pack);
}

std::string Resources::champion_id(int champ) {
    if (champ < 0 || champ >= int(champions().size())) return "unknown";
    std::string id(champions()[std::size_t(champ)].name);
    for (char& c : id) c = c == ' ' ? '_' : char(std::tolower(static_cast<unsigned char>(c)));
    std::erase(id, '\'');
    return id;
}

std::string Resources::find(const std::string& relative) const {
    for (const std::string& p : {pack_, std::string("default")}) {
        const fs::path f = fs::path("resourcepacks") / p / relative;
        std::error_code ec;
        if (fs::exists(f, ec)) return f.string();
    }
    return {};
}

std::vector<std::string> Resources::packs() const {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator("resourcepacks", ec))
        if (e.is_directory()) out.push_back(e.path().filename().string());
    std::ranges::sort(out);
    return out;
}

std::vector<std::string> Resources::skins_of(int champ) const {
    std::vector<std::string> out;
    for (const std::string& p : {pack_, std::string("default")}) {
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(fs::path("resourcepacks") / p / "champions" / champion_id(champ) / "skins", ec))
            if (e.is_directory() && std::ranges::find(out, e.path().filename().string()) == out.end())
                out.push_back(e.path().filename().string());
    }
    std::ranges::sort(out);
    return out;
}

SDL_Texture* Resources::load_texture(const std::string& path) {
    if (path.empty()) return nullptr;
    if (auto it = textures_.find(path); it != textures_.end()) return it->second;
    SDL_Texture* t = nullptr;
    if (SDL_Surface* s = path.ends_with(".png") ? SDL_LoadPNG(path.c_str()) : SDL_LoadBMP(path.c_str())) {  // [U-257] PNG or BMP
        t = SDL_CreateTextureFromSurface(r_, s);
        SDL_DestroySurface(s);
        if (t) SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    }
    textures_[path] = t;  // remember failures too (no retry every frame)
    return t;
}

// [U-257] an image may be .png (preferred, smaller) or .bmp — whatever the name asks for, both are tried
std::string Resources::find_image(const std::string& relative) const {
    const std::string stem = relative.ends_with(".bmp") || relative.ends_with(".png") ? relative.substr(0, relative.size() - 4) : relative;
    for (const std::string& p : {pack_, std::string("default")})
        for (const char* ext : {".png", ".bmp"}) {
            const fs::path f = fs::path("resourcepacks") / p / (stem + ext);
            std::error_code ec;
            if (fs::exists(f, ec)) return f.string();
        }
    return {};
}

SDL_Texture* Resources::texture(const std::string& relative) { return load_texture(find_image(relative)); }

std::string Resources::slug(std::string_view name) {
    std::string id;
    for (char c : name) {
        if (c == '\'') continue;
        id += c == ' ' || c == '-' ? '_' : char(std::tolower(static_cast<unsigned char>(c)));
    }
    return id;
}
SDL_Texture* Resources::skill_icon(std::string_view skill_name) { return texture("icons/skills/" + slug(skill_name) + ".png"); }
SDL_Texture* Resources::item_icon(std::string_view item_name) { return texture("icons/items/" + slug(item_name) + ".png"); }

SDL_Texture* Resources::champion_icon(int champ, const std::string& skin) {
    const std::string base = "champions/" + champion_id(champ) + "/";
    if (!skin.empty())
        if (SDL_Texture* t = texture(base + "skins/" + skin + "/icon.bmp")) return t;
    return texture(base + "base/icon.bmp");
}

SDL_Texture* Resources::champion_splash(int champ, const std::string& skin) {
    const std::string base = "champions/" + champion_id(champ) + "/";
    if (!skin.empty())
        if (SDL_Texture* t = texture(base + "skins/" + skin + "/splash.bmp")) return t;
    return texture(base + "base/splash.bmp");
}

SDL_Texture* Resources::texture_from_bmp(std::uint32_t key, const std::vector<std::uint8_t>& bytes) {
    const std::string id = "net:" + std::to_string(key) + ":" + std::to_string(bytes.size());
    if (auto it = textures_.find(id); it != textures_.end()) return it->second;
    SDL_Texture* t = nullptr;
    const bool png = bytes.size() > 4 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G';
    if (SDL_IOStream* io = SDL_IOFromConstMem(bytes.data(), bytes.size()))
        if (SDL_Surface* s = png ? SDL_LoadPNG_IO(io, true) : SDL_LoadBMP_IO(io, true)) {
            t = SDL_CreateTextureFromSurface(r_, s);
            SDL_DestroySurface(s);
        }
    textures_[id] = t;
    return t;
}

std::vector<std::uint8_t> Resources::skin_upload(int champ, const std::string& skin) const {
    if (skin.empty()) return {};
    const std::string f = find_image("champions/" + champion_id(champ) + "/skins/" + skin + "/splash.bmp");
    if (f.empty()) return {};
    std::ifstream in(f, std::ios::binary);
    std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(in)), {});
    return b.size() <= 512 * 1024 ? b : std::vector<std::uint8_t>{};
}

const Sound* Resources::load_sound(const std::string& path) {
    if (path.empty()) return nullptr;
    if (auto it = sounds_.find(path); it != sounds_.end()) return it->second.get();
    std::unique_ptr<Sound> snd;
    SDL_AudioSpec spec{};
    Uint8* buf = nullptr;
    Uint32 len = 0;
    if (SDL_LoadWAV(path.c_str(), &spec, &buf, &len)) {
        const SDL_AudioSpec want{SDL_AUDIO_F32, 1, 44'100};
        Uint8* out = nullptr;
        int out_len = 0;
        if (SDL_ConvertAudioSamples(&spec, buf, int(len), &want, &out, &out_len)) {
            snd = std::make_unique<Sound>();
            snd->samples.assign(reinterpret_cast<float*>(out), reinterpret_cast<float*>(out) + out_len / 4);
            SDL_free(out);
        }
        SDL_free(buf);
    }
    const Sound* p = snd.get();
    sounds_[path] = std::move(snd);
    return p;
}

// [U-414] a WAV received over the network (another player's own emote)
const Sound* Resources::sound_from_wav(std::uint32_t key, const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) return nullptr;
    const std::string id = "net:" + std::to_string(key) + ":" + std::to_string(bytes.size());
    if (auto it = sounds_.find(id); it != sounds_.end()) return it->second.get();
    std::unique_ptr<Sound> snd;
    SDL_AudioSpec spec{};
    Uint8* buf = nullptr;
    Uint32 len = 0;
    if (SDL_IOStream* io = SDL_IOFromConstMem(bytes.data(), bytes.size()); io && SDL_LoadWAV_IO(io, true, &spec, &buf, &len)) {
        const SDL_AudioSpec want{SDL_AUDIO_F32, 1, 44'100};
        Uint8* out = nullptr;
        int out_len = 0;
        if (SDL_ConvertAudioSamples(&spec, buf, int(len), &want, &out, &out_len)) {
            snd = std::make_unique<Sound>();
            snd->samples.assign(reinterpret_cast<float*>(out), reinterpret_cast<float*>(out) + std::min(out_len / 4, 44'100 * 4));  // ≤ 4 s
            SDL_free(out);
        }
        SDL_free(buf);
    }
    const Sound* p = snd.get();
    sounds_[id] = std::move(snd);
    return p;
}

const Sound* Resources::sound(const std::string& name) { return load_sound(find("sounds/" + name + ".wav")); }
const Sound* Resources::music(const std::string& name) { return load_sound(find("music/" + name + ".wav")); }

}  // namespace em
