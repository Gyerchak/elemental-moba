// Shop / item browser. See item_browser.hpp.
#include "client/item_browser.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <vector>

#include "client/locale.hpp"
#include "core/stat_rules.hpp"
#include "game/world.hpp"

namespace em {

using namespace gfx;

namespace {
const char* kCatNames[] = {"All", "Attack", "Elemental power", "Attack speed", "Health", "Defence", "Elemental defence",
                           "Move speed", "Haste", "Skill size", "Element resist", "Slow resist", "Actives"};
static_assert(std::size(kCatNames) == std::size_t(ItemCategory::Count));

bool in_category(const ItemDef& d, ItemCategory c) {
    switch (c) {
        case ItemCategory::All: return true;
        case ItemCategory::Attack: return d.attack > 0;
        case ItemCategory::ElementalPower: return d.elemental_power > 0 || d.elemental_penetration > 0;
        case ItemCategory::AttackSpeed: return d.attack_speed > 0;
        case ItemCategory::Health: return d.health > 0;
        case ItemCategory::Defence: return d.defence > 0;
        case ItemCategory::ElementalDefence: return d.elemental_defence > 0;
        case ItemCategory::MoveSpeed: return d.move_speed > 0 || d.move_speed_pct > 0;
        case ItemCategory::Haste: return d.elemental_haste > 0 || d.non_elemental_haste > 0 || d.cast_speed > 0;
        case ItemCategory::SkillSize: return d.skill_range || d.skill_width || d.skill_length || d.skill_radius || d.skill_speed;
        case ItemCategory::ElementResist: return d.resist_fire || d.resist_water || d.resist_air || d.resist_earth;
        case ItemCategory::SlowResist: return d.slow_resist || d.slow_resist_ice || d.tenacity;
        case ItemCategory::Actives: return d.active != ItemActive::None || d.unique;
        case ItemCategory::Count: break;
    }
    return false;
}
std::string lower(std::string_view s) {
    std::string o(s);
    for (char& c : o) c = char(std::tolower(static_cast<unsigned char>(c)));
    return o;
}
}  // namespace

std::vector<std::string> item_lines(const ItemDef& d) {
    std::vector<std::string> L;
    auto add = [&](std::int32_t v, std::string_view what, std::string_view stack, bool pct = false) {
        if (!v) return;
        L.push_back(std::format("{:+}{} {}   ({})", v / 100, pct ? "%" : "", tr(what), tr(stacking_text(stack))));
    };
    add(d.health, "health", "health");
    add(d.attack, "attack", "attack");
    add(d.elemental_power, "elemental power", "");
    add(d.attack_speed, "attack speed", "attack speed", true);
    add(d.defence, "defence", "defence");
    add(d.elemental_defence, "elemental defence", "elemental defence");
    add(d.move_speed, "move speed", "move speed");
    add(d.move_speed_pct, "move speed", "move speed %", true);
    add(d.wall_damage, "damage to walls", "", true);  // [U-288]
    add(d.wall_damage_stone, "damage to stone walls", "", true);
    add(d.wall_damage_ice, "damage to ice walls", "", true);
    add(d.wall_damage_metal, "damage to metal walls", "", true);
    add(d.elemental_haste, "elemental haste", "haste");
    add(d.non_elemental_haste, "non-elemental haste", "haste");
    add(d.cast_speed, "cast speed", "cast speed");
    add(d.skill_range, "skill range", "");
    add(d.skill_speed, "skill speed", "");
    add(d.skill_width, "skill width", "");
    add(d.skill_length, "skill length", "");
    add(d.skill_radius, "skill radius", "");
    add(d.elemental_penetration, "elemental penetration", "penetration", true);
    add(d.elemental_lethality, "elemental lethality", "penetration");
    add(d.tenacity, "tenacity", "tenacity", true);
    add(d.cc_amplify, "crowd-control duration", "", true);
    add(d.model_size, "model size (bigger hitbox)", "model size", true);
    add(d.slow_resist, "resistance to non-ice slows", "slow resist", true);
    add(d.slow_resist_ice, "resistance to ice slows", "slow resist", true);
    if (d.resist_fire) L.push_back(std::format("−{}% {}", d.resist_fire / 100, tr("fire damage taken")));
    if (d.resist_water) L.push_back(std::format("−{}% {}", d.resist_water / 100, tr("water damage taken")));
    if (d.resist_air) L.push_back(std::format("−{}% {}", d.resist_air / 100, tr("air damage taken")));
    if (d.resist_earth) L.push_back(std::format("−{}% {}", d.resist_earth / 100, tr("earth damage taken")));
    if (d.unit_collision) L.push_back(tr("Your body blocks other units (unit collision)."));
    if (d.active == ItemActive::LotusTile) L.push_back(tr("Active: +40 cast speed for 5 s. Cooldown 60 s."));
    if (d.active == ItemActive::SpiritFlask)
        L.push_back(tr("Active: 60 s strong healing + you cannot die; if you would, 3 s down, then 5 × 5 % max HP. Cooldown 6 min."));
    // [U-416..U-482] the newer numbers: vamp, crits, powers, on-hit, ANY stat by name (flat or +%)
    add(d.lifesteal, "lifesteal", "", true);
    add(d.omnivamp, "omnivamp", "", true);
    add(d.spellvamp, "spellvamp", "", true);
    add(d.grievous_on_hit, "grievous wounds on hit (less healing)", "", true);
    add(d.crit_chance, "crit chance (champions that crit)", "", true);
    add(d.crit_damage, "crit damage", "", true);
    add(d.arcane_power, "arcane skill damage", "", true);
    add(d.martial_power, "martial skill damage", "", true);
    add(d.skill_amp, "skill damage (all)", "", true);
    add(d.on_hit_dmg, "on-hit damage", "");
    add(d.on_hit_max_hp, "on-hit % of the target's max HP", "", true);
    for (const ItemDef::More& m : d.more) {
        if (m.key == StatKey::Count) continue;
        std::string nm(kStatKeyNames[int(m.key)]);
        std::ranges::replace(nm, '_', ' ');
        L.push_back(m.pct ? std::format("+{:g}% {}", m.value / 100.0, nm) : std::format("{:+g} {}", m.value / 100.0, nm));
    }
    if (d.scepter) L.push_back(tr("Upgrades your ultimate: +30% damage, -20% cooldown, +15% size."));
    if (d.true_sight) L.push_back(std::format("{} {}", tr("True sight: sees invisible enemies within"), d.true_sight));
    if (d.active == ItemActive::Shadow) L.push_back(tr("Active: invisible for 10 s, +20% speed (attacking or casting ends it). Cooldown 25 s."));
    if (d.feature) L.push_back(tr("Only in matches with its feature switched on."));
    if (!d.mod.empty()) L.push_back(std::format("{} '{}'", tr("Mod item: only in game versions with the mod"), d.mod));
    if (d.limit == BuyLimit::One || d.unique) L.push_back(tr("Limit: only one."));
    if (d.limit == BuyLimit::OnePerGroup) L.push_back(tr("Limit: one item of this kind (e.g. one pair of boots)."));
    return L;
}

namespace {
std::function<SDL_Texture*(int)> g_item_icon;
}
void set_item_icon_source(std::function<SDL_Texture*(int)> f) { g_item_icon = std::move(f); }

void draw_item_icon(SDL_Renderer* r, int item, float x, float y, float s) {
    if (item < 0 || item >= int(kItems.size())) return;
    const ItemDef& d = kItems[std::size_t(item)];
    if (SDL_Texture* t = g_item_icon ? g_item_icon(item) : nullptr) {  // [U-257]
        const SDL_FRect dst{x, y, s, s};
        SDL_RenderTexture(r, t, nullptr, &dst);
        if (d.active != ItemActive::None) fill_rect(r, x + s - 9, y + 3, 6, 6, {255, 230, 90, 255});
        rect(r, x, y, s, s, item_tier(d) == ItemTier::Legendary ? Rgba{255, 200, 90} : item_tier(d) == ItemTier::Epic ? Rgba{190, 120, 255} : Rgba{200, 190, 150});
        return;
    }
    Rgba c{120, 120, 130, 255};
    if (d.elemental_power) c = {150, 90, 230, 255};
    if (d.attack) c = {230, 120, 60, 255};
    if (d.health || d.defence) c = {80, 170, 90, 255};
    if (d.elemental_defence) c = {70, 140, 220, 255};
    if (d.move_speed || d.move_speed_pct) c = {230, 220, 120, 255};
    if (d.resist_fire || d.resist_water || d.resist_air || d.resist_earth) c = {90, 200, 200, 255};
    fill_rect(r, x, y, s, s, mixc(c, {0, 0, 0, 255}, 55));
    fill_rect(r, x + 3, y + 3, s - 6, s - 6, mixc(c, {0, 0, 0, 255}, 15));
    std::string ini;
    for (std::size_t i = 0; i < d.name.size() && ini.size() < 2; ++i)
        if (i == 0 || d.name[i - 1] == ' ') ini += d.name[i];
    const float sc = s >= 44 ? 2.f : 1.f;
    text(r, x + s / 2 - text_w(ini, sc) / 2, y + s / 2 - 4 * sc, ini, {255, 255, 255}, sc);
    if (d.active != ItemActive::None) fill_rect(r, x + s - 9, y + 3, 6, 6, {255, 230, 90, 255});
    rect(r, x, y, s, s, {200, 190, 150});
}

// ---------------------------------------------------------------- [U-547] what an item gives, as numbers, and what that is worth
namespace {
struct ItemStat { const char* name; std::int32_t ItemDef::*field; };
constexpr ItemStat kItemStats[] = {
    {"health", &ItemDef::health}, {"attack", &ItemDef::attack}, {"elemental power", &ItemDef::elemental_power}, {"defence", &ItemDef::defence},
    {"elemental defence", &ItemDef::elemental_defence}, {"attack speed", &ItemDef::attack_speed}, {"move speed", &ItemDef::move_speed}, {"move speed %", &ItemDef::move_speed_pct},
    {"elemental haste", &ItemDef::elemental_haste}, {"non-elemental haste", &ItemDef::non_elemental_haste}, {"cast speed", &ItemDef::cast_speed}, {"skill speed", &ItemDef::skill_speed},
    {"skill width", &ItemDef::skill_width}, {"skill length", &ItemDef::skill_length}, {"skill range", &ItemDef::skill_range}, {"skill radius", &ItemDef::skill_radius},
    {"elemental lethality", &ItemDef::elemental_lethality}, {"elemental penetration", &ItemDef::elemental_penetration}, {"tenacity", &ItemDef::tenacity},
    {"crowd-control duration", &ItemDef::cc_amplify}, {"fire damage taken", &ItemDef::resist_fire}, {"water damage taken", &ItemDef::resist_water},
    {"air damage taken", &ItemDef::resist_air}, {"earth damage taken", &ItemDef::resist_earth}, {"resistance to slows", &ItemDef::slow_resist},
    {"damage to walls", &ItemDef::wall_damage}, {"lifesteal", &ItemDef::lifesteal}, {"omnivamp", &ItemDef::omnivamp}, {"spellvamp", &ItemDef::spellvamp},
    {"crit chance", &ItemDef::crit_chance}, {"crit damage", &ItemDef::crit_damage}, {"on-hit damage", &ItemDef::on_hit_dmg}, {"skill damage (all)", &ItemDef::skill_amp},
};
struct StatAmount { std::string name; double points; };
[[nodiscard]] std::vector<StatAmount> stats_of(const ItemDef& d) {
    std::vector<StatAmount> out;
    for (const ItemStat& st : kItemStats)
        if (const std::int32_t v = d.*st.field; v > 0) out.push_back({st.name, v / 100.0});
    for (const ItemDef::More& m : d.more)
        if (m.key != StatKey::Count && m.value > 0) {
            std::string n(kStatKeyNames[int(m.key)]);
            std::ranges::replace(n, '_', ' ');
            out.push_back({m.pct ? n + " %" : n, m.value / 100.0});
        }
    return out;
}
// What one point of a stat costs: the cheapest item that gives ONLY that stat sets the price, as in every game with an item shop.
// A stat no such item gives is priced by what is left over in items that give it among stats with a known price.
[[nodiscard]] double stat_price(const std::string& name) {
    static std::vector<std::pair<std::string, double>> prices;
    static std::size_t built_for = 0;
    if (built_for != kItems.size()) {
        built_for = kItems.size();
        prices.clear();
        auto find = [&](const std::string& n) { return std::ranges::find_if(prices, [&](const auto& p) { return p.first == n; }); };
        for (const ItemDef& d : kItems) {
            const auto st = stats_of(d);
            if (st.size() != 1 || d.cost <= 0 || st[0].points <= 0) continue;
            const double each = double(d.cost) / st[0].points;
            if (auto it = find(st[0].name); it == prices.end()) prices.push_back({st[0].name, each});
            else it->second = std::min(it->second, each);
        }
        for (int pass = 0; pass < 2; ++pass)
            for (const ItemDef& d : kItems) {
                const auto st = stats_of(d);
                double known = 0;
                const StatAmount* unknown = nullptr;
                int missing = 0;
                for (const StatAmount& a : st)
                    if (auto it = find(a.name); it != prices.end()) known += it->second * a.points;
                    else unknown = &a, ++missing;
                if (missing == 1 && d.cost > known && unknown->points > 0) prices.push_back({unknown->name, (double(d.cost) - known) / unknown->points});
            }
    }
    const auto it = std::ranges::find_if(prices, [&](const auto& p) { return p.first == name; });
    return it != prices.end() ? it->second : 0.0;
}
[[nodiscard]] bool counts(const std::vector<std::string>& chosen, const std::string& name) { return chosen.empty() || std::ranges::find(chosen, name) != chosen.end(); }
[[nodiscard]] double stat_amount(const ItemDef& d, const std::vector<std::string>& chosen) {
    double sum = 0;
    for (const StatAmount& a : stats_of(d))
        if (counts(chosen, a.name)) sum += chosen.empty() ? a.points * stat_price(a.name) / 30.0 : a.points;  // (all stats together: on one scale)
    return sum;
}
[[nodiscard]] double gold_efficiency(const ItemDef& d, const std::vector<std::string>& chosen) {  // % of the price the chosen stats are worth
    if (d.cost <= 0) return 0;
    double worth = 0;
    for (const StatAmount& a : stats_of(d))
        if (counts(chosen, a.name)) worth += a.points * stat_price(a.name);
    return worth * 100.0 / double(d.cost);
}
}  // namespace

int ItemBrowser::draw(SDL_Renderer* r, Ui& ui, float x, float y, float w, float h, std::string_view action,
                      const std::function<bool(int)>& can, int gold) {
    fill_rect(r, x, y, w, h, {12, 12, 20, 240});
    rect(r, x, y, w, h, {150, 130, 90});
    // left: search + categories
    text_field(r, ui, 70, x + 8, y + 22, 170, search, tr("Search"));
    float cy = y + 60;
    for (int c = 0; c < int(ItemCategory::Count); ++c) {
        if (button(r, ui, x + 8, cy, 170, 22, tr(kCatNames[c]), category == c)) category = c;
        cy += 25;
    }
    // [U-261] tiers: basic → epic → legendary
    cy += 6;
    const char* tiers[] = {"All tiers", "Basic", "Epic", "Legendary"};
    for (int t = -1; t < 3; ++t) {
        if (button(r, ui, x + 8 + float((t + 1) % 2) * 86, cy + float((t + 1) / 2) * 25, 84, 22, tr(tiers[t + 1]), tier == t)) tier = t;
    }
    // top: sorting (no "recommended" — on purpose [U-220])
    const char* sorts[] = {"Name A–Z", "Price low–high", "Price high–low", "Most stats", "Gold efficiency"};
    const float sort_w = std::min(134.f, (w - 190 - 150 - 170) / 5);
    for (int s = 0; s < 5; ++s)
        if (button(r, ui, x + 190 + float(s) * (sort_w + 4), y + 8, sort_w, 22, fit_text(tr(sorts[s]), sort_w - 8), sort == s)) sort = s;
    if (button(r, ui, x + 190 + 5 * (sort_w + 4), y + 8, 166, 22, fit_text(sort_stats.empty() ? tr("Stats: all") : trf("Stats: {} chosen", sort_stats.size()), 158), choosing_stats)) choosing_stats = !choosing_stats;
    if (gold >= 0) text(r, x + w - 150, y + 14, std::format("{} {}", tr("gold"), gold), {255, 215, 80}, 1.5f);
    // grid
    std::vector<int> list;
    for (int i = 0; i < int(kItems.size()); ++i)
        if ((classic || !kItems[std::size_t(i)].classic_only) && item_available(kItems[std::size_t(i)], features, 0) && in_category(kItems[std::size_t(i)], ItemCategory(category)) && (tier < 0 || int(item_tier(kItems[std::size_t(i)])) == tier) &&
            (search.empty() || lower(kItems[std::size_t(i)].name).find(lower(search)) != std::string::npos))
            list.push_back(i);
    if (sort == 0) std::ranges::sort(list, {}, [](int i) { return kItems[std::size_t(i)].name; });
    if (sort == 1) std::ranges::sort(list, {}, [](int i) { return kItems[std::size_t(i)].cost; });
    if (sort == 2) std::ranges::sort(list, std::greater<>{}, [](int i) { return kItems[std::size_t(i)].cost; });
    if (sort == 3) std::ranges::stable_sort(list, std::greater<>{}, [&](int i) { return stat_amount(kItems[std::size_t(i)], sort_stats); });
    if (sort == 4) std::ranges::stable_sort(list, std::greater<>{}, [&](int i) { return gold_efficiency(kItems[std::size_t(i)], sort_stats); });
    const float gx = x + 190, gy = y + 40, tip_w = 340, gw = w - 200 - tip_w;
    const int cols = std::max(1, int(gw / 64));
    for (std::size_t k = 0; k < list.size(); ++k) {
        const int i = list[k];
        const float ix = gx + float(int(k) % cols) * 64, iy = gy + float(int(k) / cols) * (sort >= 3 ? 88 : 76);
        draw_item_icon(r, i, ix, iy, 52);
        const int price = bag ? plan_buy(*bag, i).price : kItems[std::size_t(i)].cost;  // [U-261] parts I carry count
        text(r, ix, iy + 55, std::format("{}", price), gold < 0 || gold >= price ? Rgba{255, 215, 80} : Rgba{150, 120, 80});
        if (sort == 3) text(r, ix, iy + 67, std::format("{:.0f}", stat_amount(kItems[std::size_t(i)], sort_stats)), {170, 230, 255});  // [U-547]
        if (sort == 4) text(r, ix, iy + 67, std::format("{:.0f} %", gold_efficiency(kItems[std::size_t(i)], sort_stats)), {170, 230, 255});
        if (selected == i) rect(r, ix - 2, iy - 2, 56, 56, {255, 255, 255});
        if (hover(ui, ix, iy, 52, 52) && ui.clicked) {
            selected = i;
            ui.clicked = false;
        }
    }
    if (choosing_stats) {  // [U-547] the stats that count for both sorts
        std::vector<std::string> names;
        for (const ItemDef& d : kItems)
            for (const StatAmount& a2 : stats_of(d))
                if (std::ranges::find(names, a2.name) == names.end()) names.push_back(a2.name);
        std::ranges::sort(names);
        const float px = gx, py = gy, pw = gw, ph = h - 48;
        fill_rect(r, px, py, pw, ph, {22, 24, 34, 250});
        rect(r, px, py, pw, ph, {255, 190, 110, 255});
        if (button(r, ui, px + 8, py + 8, 150, 24, tr("All of them"), sort_stats.empty())) sort_stats.clear();
        const int ncol = std::max(1, int((pw - 16) / 200));
        for (std::size_t k = 0; k < names.size(); ++k) {
            const float bx = px + 8 + float(int(k) % ncol) * 200, by = py + 40 + float(int(k) / ncol) * 26;
            const bool on = std::ranges::find(sort_stats, names[k]) != sort_stats.end();
            if (button(r, ui, bx, by, 194, 24, fit_text(tr(names[k]), 186), on)) {
                if (on) std::erase(sort_stats, names[k]);
                else sort_stats.push_back(names[k]);
            }
        }
    }
    // tooltip of the CLICKED item, price at the bottom [U-245]
    int result = -1;
    const float tx = x + w - tip_w - 6;
    fill_rect(r, tx, y + 40, tip_w, h - 48, {18, 18, 28, 255});
    if (selected >= 0) {
        const ItemDef& d = kItems[std::size_t(selected)];
        draw_item_icon(r, selected, tx + 8, y + 48, 56);
        text(r, tx + 72, y + 52, tr(d.name), {255, 230, 150}, 1.5f);
        text(r, tx + 72, y + 72, fit_text(trf("Gold efficiency: {:.0f} %", gold_efficiency(d, {})), tip_w - 80), {170, 230, 255});  // [U-547]
        float ly = y + 112;
        for (const std::string& l : item_lines(d)) {
            text(r, tx + 8, ly, fit_text(l, tip_w - 16), {225, 225, 235});
            ly += 14;
        }
        if (SDL_GetModState() & SDL_KMOD_ALT) {  // [U-520] Alt: the parts in words, with what each one gives
            ly += 4;
            for (std::string_view p : d.from) {
                const int pi = item_index(p);
                if (pi < 0) continue;
                text(r, tx + 8, ly, fit_text(std::format("{} ({})", tr(kItems[std::size_t(pi)].name), kItems[std::size_t(pi)].cost), tip_w - 16), {255, 215, 120});
                ly += 14;
                for (const std::string& l : item_lines(kItems[std::size_t(pi)])) {
                    text(r, tx + 16, ly, fit_text(l, tip_w - 24), {190, 190, 200});
                    ly += 14;
                }
            }
        } else if (has_parts(d)) {
            text(r, tx + 8, ly, tr("hold Alt: what the parts give"), {150, 150, 170});
            ly += 14;
        }
        // [U-261] build path: what it is made of (+ recipe), and what it builds into — click an icon to open it
        ly += 6;
        if (has_parts(d)) {
            text(r, tx + 8, ly, std::format("{} ({} {}):", tr("Built from"), tr("recipe"), recipe_cost(d)), {200, 200, 160});
            ly += 14;
            float px = tx + 8;
            for (std::string_view p : d.from) {
                const int pi = item_index(p);
                if (pi < 0) continue;
                const bool owned = bag && std::ranges::find(*bag, std::int16_t(pi)) != bag->end();
                draw_item_icon(r, pi, px, ly, 36);
                if (owned) rect(r, px - 2, ly - 2, 40, 40, {120, 255, 120});
                text(r, px, ly + 38, std::format("{}", kItems[std::size_t(pi)].cost), {255, 215, 80});
                if (hover(ui, px, ly, 36, 36) && ui.clicked) selected = pi, ui.clicked = false;
                px += 46;
            }
            ly += 54;
        }
        std::vector<int> into;
        for (int k = 0; k < int(kItems.size()); ++k)
            for (std::string_view p : kItems[std::size_t(k)].from)
                if (p == d.name && std::ranges::find(into, k) == into.end()) into.push_back(k);
        if (!into.empty()) {
            text(r, tx + 8, ly, tr("Builds into:"), {200, 200, 160});
            ly += 14;
            float px = tx + 8;
            for (int k : into) {
                if (px + 36 > tx + tip_w) break;
                draw_item_icon(r, k, px, ly, 30);
                if (hover(ui, px, ly, 30, 30) && ui.clicked) selected = k, ui.clicked = false;
                px += 36;
            }
        }
        const int price = bag ? plan_buy(*bag, selected).price : d.cost;
        text(r, tx + 8, y + h - 74, price != d.cost ? std::format("{}: {}  ({} {})", tr("Price"), price, tr("full"), d.cost)
                                                    : std::format("{}: {}", tr("Price"), d.cost), {255, 215, 80}, 2);
        const bool ok = can ? can(selected) : true;
        if (!action.empty() && button(r, ui, tx + 8, y + h - 46, tip_w - 16, 34, action, false, ok)) result = selected;
    } else {
        text(r, tx + 8, y + 52, tr("Click an item to see it."), {170, 170, 190});
    }
    return result;
}

}  // namespace em
