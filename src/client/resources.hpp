#pragma once
// [U-97][U-197][U-199][U-200] Resource packs. Folder layout (a custom pack may override any file; anything
// missing falls back to the "default" pack, and if that is missing too the game draws placeholders):
//
//   resourcepacks/<pack>/pack.txt                         name + description
//   resourcepacks/<pack>/champions/<id>/base/             icon.bmp  splash.bmp  model.txt   (no skin)
//   resourcepacks/<pack>/champions/<id>/skins/<skin>/     icon.bmp  splash.bmp  model.txt  sounds/*.wav
//   resourcepacks/<pack>/models/                          placeholder model descriptions (*.txt)
//   resourcepacks/<pack>/textures/                        ground / trees / bushes / walls (*.png or *.bmp)
//   resourcepacks/<pack>/icons/skills/<slug>.png          skill icons   (slug: "Fire Lord's Mantle" → fire_lords_mantle)
//   resourcepacks/<pack>/icons/items/<slug>.png           item icons
//   resourcepacks/<pack>/screens/<name>.png               menu / loading / victory / defeat pictures
//   resourcepacks/<pack>/models/<champion id>/            sprites rendered from the Blender models [U-258]
//   resourcepacks/<pack>/sounds/                          effects + interface sounds (*.wav)
//   resourcepacks/<pack>/music/                           menu.wav  match.wav
//
// <id> = champion name in lower case with '_' for spaces (e.g. "stone_warden").
#include <SDL3/SDL.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace em {

struct Sound {
    std::vector<float> samples;  // mono, 44 100 Hz
};

class Resources {
public:
    ~Resources();
    void init(SDL_Renderer* r, std::string pack);
    [[nodiscard]] std::vector<std::string> packs() const;          // folders in resourcepacks/
    [[nodiscard]] std::vector<std::string> skins_of(int champ) const;  // skin folders of a champion
    SDL_Texture* champion_icon(int champ, const std::string& skin);
    SDL_Texture* champion_splash(int champ, const std::string& skin);
    SDL_Texture* texture(const std::string& relative);             // e.g. "textures/tree.bmp" (.png tried first)
    SDL_Texture* skill_icon(std::string_view skill_name);          // icons/skills/<slug>.png [U-257]
    SDL_Texture* item_icon(std::string_view item_name);            // icons/items/<slug>.png [U-257]
    [[nodiscard]] static std::string slug(std::string_view name);  // "Fire Lord's Mantle" → "fire_lords_mantle"
    [[nodiscard]] std::string file(const std::string& relative) const { return find(relative); }  // path in the pack (or default), "" = none
    SDL_Texture* texture_from_bmp(std::uint32_t key, const std::vector<std::uint8_t>& bytes);  // network skins
    [[nodiscard]] std::vector<std::uint8_t> skin_upload(int champ, const std::string& skin) const;  // [U-200]
    const Sound* sound(const std::string& name);  // "sounds/<name>.wav"
    const Sound* music(const std::string& name);  // "music/<name>.wav"
    [[nodiscard]] static std::string champion_id(int champ);
    [[nodiscard]] const std::string& pack() const { return pack_; }

private:
    [[nodiscard]] std::string find(const std::string& relative) const;  // pack first, then default
    [[nodiscard]] std::string find_image(const std::string& relative) const;  // .png or .bmp
    SDL_Texture* load_texture(const std::string& path);
    const Sound* load_sound(const std::string& path);
    SDL_Renderer* r_ = nullptr;
    std::string pack_ = "default";
    std::map<std::string, SDL_Texture*> textures_;
    std::map<std::string, std::unique_ptr<Sound>> sounds_;
};

}  // namespace em
