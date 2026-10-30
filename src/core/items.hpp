#pragma once
// [U-80][U-81][U-113][U-114][U-136] Items. In-game gold shop [?-01]. Data here = placeholder roster.
// [X-46] size / speed / cast items, [X-74] element-defensive items, [X-84] basic stat items.
// Stat units follow stats.hpp: hundredths (100 = 1.00 or 1 %).
#include <array>
#include <cstdint>
#include <string_view>

#include "core/elements.hpp"
#include "core/stats.hpp"

namespace em {

enum class ItemActive : std::uint8_t { None, LotusTile, SpiritFlask, Falcon };
// [U-245] how many a champion may carry
enum class BuyLimit : std::uint8_t { Many, One, OnePerGroup };
enum class ItemGroup : std::uint8_t { None, Boots, ElementWard, Special };  // Special = the special slot (wards, falcons) [U-322]

// [U-322][U-336] the bag: slot 0 = the special slot (key 1), slots 1…8 = items (keys 2…9), slot 9 = boots (no key, no active)
inline constexpr int kItemSlots = 10;
inline constexpr int kSpecialSlot = 0, kBootsSlot = 9;
using Bag = std::array<std::int16_t, kItemSlots>;

struct ItemDef {
    std::string_view name;
    std::int32_t cost = 0;
    std::int32_t health = 0, attack = 0, elemental_power = 0, defence = 0, elemental_defence = 0;
    std::int32_t attack_speed = 0, move_speed = 0, elemental_haste = 0, non_elemental_haste = 0;
    std::int32_t cast_speed = 0, skill_speed = 0, skill_width = 0, skill_length = 0, skill_range = 0, skill_radius = 0;
    std::int32_t elemental_lethality = 0, elemental_penetration = 0, tenacity = 0, cc_amplify = 0, model_size = 0;
    std::int32_t resist_water = 0, resist_earth = 0, resist_fire = 0, resist_air = 0;
    ItemActive active = ItemActive::None;
    std::int32_t active_cd_ms = 0;
    bool unique = false;
    bool unit_collision = false;  // [X-96] (old: everybody body-blocks now [U-367])
    bool ghost = false;           // [U-367] the wearer walks through units (and units through it)
    std::int32_t slow_resist = 0, slow_resist_ice = 0;  // [U-218]
    std::int32_t move_speed_pct = 0;                    // [U-219] % move speed
    bool attack_while_moving = false;                   // [U-217] mechanic ready, no item uses it yet
    std::int32_t wall_damage = 0, wall_damage_stone = 0, wall_damage_ice = 0, wall_damage_metal = 0;  // [U-288]
    BuyLimit limit = BuyLimit::Many;                    // [U-245]
    ItemGroup group = ItemGroup::None;
    // [U-261] build path like LoL: the parts this item is made of (names). `cost` = the FULL price;
    // recipe price = cost − the parts. Buying uses the parts you carry and you pay only the rest.
    std::array<std::string_view, 3> from{};
    // [U-416] classic rule sets (LoL- / Dota-like): vamp + healing reduction; such items are sold only there
    std::int32_t lifesteal = 0, omnivamp = 0, grievous_on_hit = 0;
    bool classic_only = false;
};

inline constexpr std::array kItems = {
    // [X-84] basics
    ItemDef{.name = "Long Spear", .cost = 350, .attack = 1'000},
    ItemDef{.name = "Spirit Pearl", .cost = 435, .elemental_power = 2'000},
    ItemDef{.name = "Ruby Heart", .cost = 400, .health = 15'000},
    ItemDef{.name = "Cloth Armour", .cost = 300, .defence = 1'500},
    ItemDef{.name = "Lotus Cape", .cost = 450, .elemental_defence = 2'500},
    ItemDef{.name = "Swift Dagger", .cost = 300, .attack_speed = 1'200},
    ItemDef{.name = "Sky Boots", .cost = 300, .move_speed = 2'500, .limit = BuyLimit::OnePerGroup, .group = ItemGroup::Boots},
    ItemDef{.name = "Hourglass Charm", .cost = 900, .elemental_haste = 2'000, .non_elemental_haste = 2'000, .from = {"Jade Amulet", "Jade Amulet"}},
    // [X-46] sizes / speeds / cast [U-113][U-114]
    ItemDef{.name = "Tidecaller's Reach", .cost = 1'100, .elemental_power = 2'500, .skill_range = 15'000, .from = {"Spirit Pearl"}},
    ItemDef{.name = "Ember Lens", .cost = 1'000, .elemental_power = 2'500, .skill_speed = 20'000, .from = {"Spirit Pearl"}},
    ItemDef{.name = "Monk's Beads", .cost = 950, .elemental_haste = 1'500, .cast_speed = 3'000, .from = {"Jade Amulet"}},
    ItemDef{.name = "Wide Stance Bracers", .cost = 1'200, .health = 20'000, .skill_width = 4'000, .skill_length = 4'000, .from = {"Ruby Heart"}},
    ItemDef{.name = "Lotus Tile", .cost = 800, .elemental_power = 1'500, .active = ItemActive::LotusTile, .active_cd_ms = 60'000, .limit = BuyLimit::One},
    // [X-74] element-defensive [U-136]: −25 % damage from one element
    ItemDef{.name = "Wet Wraps (vs fire)", .cost = 700, .health = 10'000, .resist_fire = 2'500, .limit = BuyLimit::One},
    ItemDef{.name = "Clay Plate (vs water)", .cost = 700, .health = 10'000, .resist_water = 2'500, .limit = BuyLimit::One},
    ItemDef{.name = "Windbreaker (vs air)", .cost = 700, .health = 10'000, .resist_air = 2'500, .limit = BuyLimit::One},
    ItemDef{.name = "Rootweave (vs earth)", .cost = 700, .health = 10'000, .resist_earth = 2'500, .limit = BuyLimit::One},
    // [U-129][X-82] model size items: bigger = tankier but easier to hit; smaller = the reverse
    ItemDef{.name = "Giant's Belt", .cost = 900, .health = 38'000, .model_size = 1'500, .from = {"Ruby Heart"}},
    ItemDef{.name = "Feather Sash", .cost = 800, .move_speed = 1'500, .model_size = -1'500},
    ItemDef{.name = "Mercury Treads", .cost = 1'100, .elemental_defence = 2'500, .move_speed = 4'500, .tenacity = 3'000, .limit = BuyLimit::OnePerGroup, .group = ItemGroup::Boots, .from = {"Sky Boots", "Lotus Cape"}},
    ItemDef{.name = "Binding Chains", .cost = 1'000, .health = 20'000, .cc_amplify = 2'000, .from = {"Ruby Heart"}},
    ItemDef{.name = "Void Shard", .cost = 1'200, .elemental_power = 4'000, .elemental_penetration = 2'000, .from = {"Spirit Pearl"}},
    // [U-81] unique Spiritual Water Flask
    ItemDef{.name = "Spiritual Water Flask", .cost = 2'800, .health = 30'000, .elemental_power = 5'000,
            .elemental_haste = 2'000, .active = ItemActive::SpiritFlask, .active_cd_ms = 360'000, .unique = true, .limit = BuyLimit::One},
    // [X-96] body blocking item
    ItemDef{.name = "Bulwark Pauldrons", .cost = 1'000, .health = 25'000, .defence = 1'500, .model_size = 500,
            .unit_collision = true, .limit = BuyLimit::One, .from = {"Cloth Armour", "Ruby Heart"}},
    // [U-218] slow resistance
    ItemDef{.name = "Current Striders", .cost = 900, .move_speed = 3'000, .slow_resist = 2'500, .limit = BuyLimit::OnePerGroup, .group = ItemGroup::Boots, .from = {"Sky Boots"}},
    ItemDef{.name = "Seal Fur Boots", .cost = 850, .health = 10'000, .move_speed = 2'500, .slow_resist_ice = 4'000, .limit = BuyLimit::OnePerGroup, .group = ItemGroup::Boots, .from = {"Sky Boots", "Ruby Heart"}},
    // [U-235] vision by falcon: delayed "ghost" information
    ItemDef{.name = "Messenger Falcon", .cost = 400, .active = ItemActive::Falcon, .active_cd_ms = 90'000, .limit = BuyLimit::One,
            .group = ItemGroup::Special},  // [U-322] the special slot
    // [U-219] percent move speed
    ItemDef{.name = "Sky Bison Charm", .cost = 1'300, .elemental_haste = 1'000, .move_speed_pct = 700, .from = {"Jade Amulet"}},
    // ---- [U-261] more placeholder items (Claude idea data). Basic → epic (from basics) → legendary (from epics).
    ItemDef{.name = "Jade Amulet", .cost = 400, .elemental_haste = 1'000},
    ItemDef{.name = "Iron Bracer", .cost = 350, .health = 8'000, .defence = 500},
    ItemDef{.name = "Quartz Chip", .cost = 400, .elemental_power = 1'500, .skill_speed = 5'000},
    ItemDef{.name = "Spirit Staff", .cost = 1'070, .elemental_power = 4'500, .from = {"Spirit Pearl", "Spirit Pearl"}},
    ItemDef{.name = "Kyoshi Fan Blade", .cost = 1'100, .attack = 2'500, .attack_speed = 1'500, .from = {"Long Spear", "Swift Dagger"}},
    ItemDef{.name = "Platinum Mail", .cost = 1'000, .health = 15'000, .defence = 3'000, .from = {"Cloth Armour", "Ruby Heart"}},
    ItemDef{.name = "Glacier Mantle", .cost = 1'100, .health = 10'000, .elemental_defence = 4'000, .from = {"Lotus Cape", "Ruby Heart"}},
    ItemDef{.name = "Prism Focus", .cost = 1'150, .elemental_power = 3'500, .skill_speed = 10'000, .skill_radius = 3'000, .from = {"Quartz Chip", "Spirit Pearl"}},
    ItemDef{.name = "Warden's Bracers", .cost = 950, .health = 18'000, .defence = 1'500, .tenacity = 1'500, .from = {"Iron Bracer", "Cloth Armour"}},
    ItemDef{.name = "Staff of the Moon Spirit", .cost = 3'000, .elemental_power = 9'000, .elemental_haste = 2'500, .limit = BuyLimit::One,
            .from = {"Spirit Staff", "Hourglass Charm"}},
    ItemDef{.name = "Comet Shard Crown", .cost = 3'100, .elemental_power = 8'000, .elemental_penetration = 3'500, .limit = BuyLimit::One,
            .from = {"Void Shard", "Spirit Staff"}},
    ItemDef{.name = "Dragon Bone Halberd", .cost = 3'100, .health = 15'000, .attack = 5'500, .attack_speed = 2'500, .limit = BuyLimit::One,
            .from = {"Kyoshi Fan Blade", "Ruby Heart", "Long Spear"}},
    ItemDef{.name = "Earth King's Bulwark", .cost = 2'900, .health = 45'000, .defence = 4'000, .elemental_defence = 3'000, .limit = BuyLimit::One,
            .from = {"Platinum Mail", "Glacier Mantle"}},
    ItemDef{.name = "Nomad Glider", .cost = 2'800, .elemental_haste = 2'500, .skill_speed = 20'000, .move_speed_pct = 800, .limit = BuyLimit::One,
            .from = {"Feather Sash", "Hourglass Charm"}},
    ItemDef{.name = "Fire Lord's Mantle", .cost = 3'000, .elemental_power = 7'000, .cast_speed = 3'000, .skill_width = 3'000, .limit = BuyLimit::One,
            .from = {"Monk's Beads", "Spirit Staff"}},
    ItemDef{.name = "Tidal Bracers", .cost = 2'900, .health = 30'000, .skill_width = 6'000, .skill_length = 6'000, .limit = BuyLimit::One,
            .from = {"Wide Stance Bracers", "Giant's Belt"}},
    ItemDef{.name = "Lens of the Four Nations", .cost = 3'000, .elemental_power = 6'000, .skill_speed = 15'000, .skill_range = 20'000, .limit = BuyLimit::One,
            .from = {"Tidecaller's Reach", "Prism Focus"}},
    ItemDef{.name = "Mountain Warden Plate", .cost = 2'800, .health = 35'000, .defence = 3'500, .tenacity = 2'500, .limit = BuyLimit::One,
            .from = {"Warden's Bracers", "Platinum Mail"}},
    // [U-288] wall breakers (Claude idea): against all walls, or one kind only
    ItemDef{.name = "Ice Pick", .cost = 400, .attack = 500, .wall_damage_ice = 10'000},
    ItemDef{.name = "Siege Maul", .cost = 1'000, .attack = 1'500, .wall_damage = 5'000, .from = {"Long Spear"}},
    ItemDef{.name = "Lodestone", .cost = 750, .elemental_power = 1'500, .wall_damage_metal = 8'000},
    ItemDef{.name = "Quarry Gloves", .cost = 650, .health = 8'000, .wall_damage_stone = 8'000},
    // [U-416] classic rule sets only (LoL- / Dota-like): lifesteal, omnivamp, healing reduction (appended: numbers stay)
    ItemDef{.name = "Leech Fang", .cost = 900, .attack = 1'000, .lifesteal = 1'000, .classic_only = true},
    ItemDef{.name = "Bloodthirst Blade", .cost = 3'200, .attack = 4'000, .limit = BuyLimit::One, .from = {"Leech Fang", "Long Spear"},
            .lifesteal = 1'800, .classic_only = true},
    ItemDef{.name = "Spirit Drinker", .cost = 2'600, .health = 20'000, .elemental_power = 3'000, .limit = BuyLimit::One,
            .from = {"Spirit Pearl", "Ruby Heart"}, .omnivamp = 1'000, .classic_only = true},
    ItemDef{.name = "Thorned Chain", .cost = 800, .attack = 1'500, .grievous_on_hit = 4'000, .classic_only = true},
    ItemDef{.name = "Withering Charm", .cost = 800, .elemental_power = 2'000, .grievous_on_hit = 4'000, .classic_only = true},
};
inline constexpr std::int32_t kMaxElementResist = 6'000;  // [X-74] 60 %
inline constexpr std::int32_t kMaxSlowResist = 8'000;     // [U-218] 80 %

// ---------------------------------------------------------------- [U-261] build paths
[[nodiscard]] constexpr int item_index(std::string_view name) {
    for (std::size_t i = 0; i < kItems.size(); ++i)
        if (kItems[i].name == name) return int(i);
    return -1;
}
enum class ItemTier : std::uint8_t { Basic, Epic, Legendary };
[[nodiscard]] constexpr bool has_parts(const ItemDef& d) { return !d.from[0].empty(); }
[[nodiscard]] constexpr ItemTier item_tier(const ItemDef& d) {
    if (!has_parts(d)) return ItemTier::Basic;
    for (std::string_view p : d.from)
        if (!p.empty() && item_index(p) >= 0 && has_parts(kItems[std::size_t(item_index(p))])) return ItemTier::Legendary;
    return ItemTier::Epic;
}
[[nodiscard]] constexpr std::int32_t parts_cost(const ItemDef& d) {
    std::int32_t c = 0;
    for (std::string_view p : d.from)
        if (!p.empty() && item_index(p) >= 0) c += kItems[std::size_t(item_index(p))].cost;
    return c;
}
[[nodiscard]] constexpr std::int32_t recipe_cost(const ItemDef& d) { return d.cost - parts_cost(d); }

// What buying `item` costs with these items in the bag: owned parts are used up (also parts of parts),
// you pay the rest. `used` = bag slots that disappear (true = used up).
struct BuyPlan {
    std::int32_t price = 0;
    std::array<bool, kItemSlots> used{};
};
namespace detail {
constexpr std::int32_t take(const Bag& bag, std::array<bool, kItemSlots>& used, int item, bool top) {
    // returns the gold this part saves (its full cost if carried, else what its own parts save)
    if (!top)
        for (std::size_t s = 0; s < bag.size(); ++s)
            if (!used[s] && bag[s] == item) {
                used[s] = true;
                return kItems[std::size_t(item)].cost;
            }
    std::int32_t saved = 0;
    for (std::string_view p : kItems[std::size_t(item)].from)
        if (!p.empty() && item_index(p) >= 0) saved += take(bag, used, item_index(p), false);
    return saved;
}
}  // namespace detail
[[nodiscard]] constexpr BuyPlan plan_buy(const Bag& bag, int item) {
    BuyPlan b;
    b.price = kItems[std::size_t(item)].cost - detail::take(bag, b.used, item, true);
    return b;
}

// every part exists and no recipe is negative
static_assert([] {
    for (const ItemDef& d : kItems) {
        for (std::string_view p : d.from)
            if (!p.empty() && item_index(p) < 0) return false;
        if (recipe_cost(d) < 0) return false;
    }
    return true;
}());
// Dragon Bone Halberd (3100) with a Long Spear (350) and a Swift Dagger (300) in the bag → 2450
static_assert([] {
    Bag bag{};
    bag.fill(-1);
    bag[0] = std::int16_t(item_index("Long Spear"));
    bag[1] = std::int16_t(item_index("Swift Dagger"));
    const BuyPlan p = plan_buy(bag, item_index("Dragon Bone Halberd"));
    return p.price == 3'100 - 650 && p.used[0] && p.used[1];
}());

constexpr void add_item(Stats& s, const ItemDef& i) {
    s.health.bonus += i.health;
    s.attack.bonus += i.attack;
    s.elemental_power.bonus += i.elemental_power;
    s.defence.bonus += i.defence;
    s.elemental_defence.bonus += i.elemental_defence;
    s.attack_speed.bonus += i.attack_speed;
    s.move_speed.bonus += i.move_speed;
    s.elemental_haste += i.elemental_haste;
    s.non_elemental_haste += i.non_elemental_haste;
    s.cast_speed += i.cast_speed;
    s.skill_speed += i.skill_speed;
    s.skill_width += i.skill_width;
    s.skill_length += i.skill_length;
    s.skill_range += i.skill_range;
    s.skill_radius += i.skill_radius;
    s.elemental_lethality += i.elemental_lethality;
    s.elemental_penetration += i.elemental_penetration;
    s.tenacity += i.tenacity;
    s.cc_amplify += i.cc_amplify;
    s.model_size += i.model_size;
    s.slow_resist = std::min(kMaxSlowResist, s.slow_resist + i.slow_resist);            // [U-218]
    s.slow_resist_ice = std::min(kMaxSlowResist, s.slow_resist_ice + i.slow_resist_ice);
    s.move_speed_pct += i.move_speed_pct;
    s.lifesteal += i.lifesteal;  // [U-416]
    s.omnivamp += i.omnivamp;
    s.grievous_on_hit = std::max(s.grievous_on_hit, i.grievous_on_hit);
    s.wall_damage += i.wall_damage;  // [U-288]
    s.wall_damage_stone += i.wall_damage_stone;
    s.wall_damage_ice += i.wall_damage_ice;
    s.wall_damage_metal += i.wall_damage_metal;
    s.resist_water = std::min(kMaxElementResist, s.resist_water + i.resist_water);
    s.resist_earth = std::min(kMaxElementResist, s.resist_earth + i.resist_earth);
    s.resist_fire = std::min(kMaxElementResist, s.resist_fire + i.resist_fire);
    s.resist_air = std::min(kMaxElementResist, s.resist_air + i.resist_air);
}

// [U-136] Damage left after the element resist.
[[nodiscard]] constexpr std::int32_t after_element_resist(std::int32_t dmg, Element e, const Stats& t) {
    const std::int32_t r = e == Element::Water ? t.resist_water
                         : e == Element::Earth ? t.resist_earth
                         : e == Element::Fire  ? t.resist_fire
                         : e == Element::Air   ? t.resist_air
                                               : 0;
    return std::int32_t(std::int64_t(dmg) * (10'000 - std::clamp(r, 0, kMaxElementResist)) / 10'000);
}

// [X-46] Lotus Tile: +40 cast speed for 5 s [U-114].
inline constexpr std::int32_t kLotusCastSpeed = 4'000, kLotusMs = 5'000;

static_assert([] {
    Stats s;
    for (int i = 0; i < 4; ++i) add_item(s, kItems[13]);  // 4 × Wet Wraps → capped 60 %
    return s.resist_fire == kMaxElementResist && after_element_resist(1'000, Element::Fire, s) == 400 &&
           after_element_resist(1'000, Element::Water, s) == 1'000;
}());
static_assert(kItems[13].resist_fire == 2'500 && kItems[22].unique);

}  // namespace em
