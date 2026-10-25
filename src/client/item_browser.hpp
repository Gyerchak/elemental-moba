#pragma once
// [U-220][U-236][U-245] The shop / item browser, LoL-like: categories + search on the left, sorting,
// an icon grid in the middle, and the clicked item's tooltip (stats, how they stack, active, limits)
// with the PRICE AT THE BOTTOM. Used in the main menu (builds) and in the match (buying).
#include <SDL3/SDL.h>

#include <cstdint>
#include <array>
#include <functional>
#include <string>

#include "client/draw.hpp"
#include "core/items.hpp"

namespace em {

enum class ItemCategory : std::uint8_t {
    All, Attack, ElementalPower, AttackSpeed, Health, Defence, ElementalDefence, MoveSpeed, Haste,
    SkillSize, ElementResist, SlowResist, Actives, Count
};

struct ItemBrowser {
    int category = 0;            // ItemCategory
    int sort = 0;                // 0 name, 1 price up, 2 price down
    int selected = -1;           // clicked item
    int tier = -1;               // [U-261] −1 all, 0 basic, 1 epic, 2 legendary
    std::string search;
    const Bag* bag = nullptr;  // [U-261] my items (in the match): prices count the parts I carry

    // Draws the browser in (x, y, w, h). `action` is the button under the tooltip ("Buy", "Add to build");
    // `can` says if it is allowed now; returns the item when the button is pressed, else −1.
    int draw(SDL_Renderer* r, gfx::Ui& ui, float x, float y, float w, float h, std::string_view action,
             const std::function<bool(int)>& can, int gold = -1);
};

// Item icon: the resource pack picture if there is one [U-257], else colour by main stat + two letters [U-236].
void draw_item_icon(SDL_Renderer* r, int item, float x, float y, float size);
// Where item pictures come from (set once by the app; returns nullptr when a pack has none).
void set_item_icon_source(std::function<SDL_Texture*(int item)> f);
// Every stat line of an item, already translated, with the stacking rule [U-219].
std::vector<std::string> item_lines(const ItemDef& d);

}  // namespace em
