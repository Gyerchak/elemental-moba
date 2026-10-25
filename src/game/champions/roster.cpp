// [X-13][X-59] The champion roster and the special-skill list. One file per champion in this folder.
#include <array>
#include <vector>

#include "game/champions/ember_monk.hpp"
#include "game/champions/storm_daughter.hpp"
#include "game/champions/stone_warden.hpp"
#include "game/champions/iron_sentinel.hpp"
#include "game/champions/magma_herald.hpp"
#include "game/champions/tide_healer.hpp"
#include "game/champions/frost_hunter.hpp"
#include "game/champions/wind_nomad.hpp"
#include "game/champions/mist_dancer.hpp"
#include "game/champions/chi_blocker.hpp"
#include "game/champions/avatar_kyren.hpp"
#include "game/skills/q_options.hpp"
#include "game/skills/specials.hpp"

namespace em {
namespace {

constexpr std::array kRoster{data::kEmberMonk, data::kStormDaughter, data::kStoneWarden, data::kIronSentinel, data::kMagmaHerald, data::kTideHealer, data::kFrostHunter, data::kWindNomad, data::kMistDancer, data::kChiBlocker, data::kAvatarKyren};

constexpr auto kSpecials = [] {
    std::array<SkillDef, std::size(data::kSpecialRows)> a{};
    for (std::size_t i = 0; i < a.size(); ++i) a[i] = data::kSpecialRows[i].def;
    return a;
}();

constexpr auto kQ = [] {
    std::array<SkillDef, std::size(data::kQOptions)> a{};
    for (std::size_t i = 0; i < a.size(); ++i) a[i] = data::kQOptions[i].def;
    return a;
}();

// [X-43] A kit with this Q: max 1 targeted skill; non-mages scale with attack [U-110].
constexpr bool kit_ok_with(const ChampionDef& c, const SkillDef& q) {
    Kit kit{to_ability(c.p, Slot::P), to_ability(q, Slot::Q), to_ability(c.w, Slot::W),
            to_ability(c.e, Slot::E), to_ability(c.r, Slot::R), to_ability(c.t, Slot::T), to_ability(c.y, Slot::Y)};
    return kit_ok(kit, c.mage);
}
constexpr bool q_ok(const ChampionDef& c, std::size_t q) {
    if (!c.q_pool[0].empty() && std::ranges::find(c.q_pool, data::kQOptions[q].def.name) == c.q_pool.end()) return false;  // [U-270]
    return (c.avatar || data::kQOptions[q].element == c.element) && kit_ok_with(c, data::kQOptions[q].def);  // [U-279]
}
// every champion has at least one Q it may take; every kit is valid without Q too
constexpr bool roster_ok() {
    for (const ChampionDef& c : kRoster) {
        int n = 0;
        for (std::size_t q = 0; q < std::size(data::kQOptions); ++q) n += q_ok(c, q);
        if (n != 3) return false;  // [U-270] every built-in champion: exactly 3 Q options (names must exist + fit the kit)
    }
    return true;
}
static_assert(roster_ok());

}  // namespace

// The roster in use: the built-in champions, then own champions [U-210]. Reserved so pointers stay valid.
std::vector<ChampionDef>& roster_vec() {
    static std::vector<ChampionDef> v = [] {
        std::vector<ChampionDef> r;
        r.reserve(64);
        r.assign(kRoster.begin(), kRoster.end());
        return r;
    }();
    return v;
}

std::span<const ChampionDef> champions() { return roster_vec(); }
int builtin_champion_count() { return int(kRoster.size()); }
void reset_custom_champions() { roster_vec().resize(kRoster.size()); }  // [U-277] (pointers to built-ins stay valid)
int register_champion(const ChampionDef& c) {
    auto& v = roster_vec();
    if (v.size() >= 64) return -1;
    Kit kit{to_ability(c.p, Slot::P), to_ability(c.w, Slot::W), to_ability(c.e, Slot::E), to_ability(c.r, Slot::R), to_ability(c.t, Slot::T),
            to_ability(c.y, Slot::Y)};
    if (!kit_ok(kit, c.mage)) return -1;
    for (const ChampionDef& o : v)
        if (o.name == c.name) return -1;
    v.push_back(c);
    return int(v.size()) - 1;
}
// [U-95] the tables in use are copies, so an own game version can change numbers (game/versions.cpp)
auto& specials_store() {
    static auto a = kSpecials;
    return a;
}
auto& q_store() {
    static auto a = kQ;
    return a;
}
std::span<const SkillDef> specials() { return specials_store(); }
std::span<const SkillDef> q_options() { return q_store(); }
std::span<SkillDef> specials_mut() { return specials_store(); }
std::span<SkillDef> q_options_mut() { return q_store(); }
std::span<ChampionDef> champions_mut() { return roster_vec(); }
void restore_official_numbers() {  // built-in champions, Q options and specials back to the code's numbers
    auto& v = roster_vec();
    for (std::size_t i = 0; i < kRoster.size(); ++i) v[i] = kRoster[i];
    specials_store() = kSpecials;
    q_store() = kQ;
}
Element q_option_element(int q) {
    return q >= 0 && q < int(kQ.size()) ? data::kQOptions[std::size_t(q)].element : Element::None;
}
bool q_allowed(int champ, int q) {
    if (champ < 0 || champ >= int(roster_vec().size()) || q < 0 || q >= int(kQ.size())) return false;
    return q_ok(roster_vec()[std::size_t(champ)], std::size_t(q));
}
int default_q(int champ) {
    for (int q = 0; q < int(kQ.size()); ++q)
        if (q_allowed(champ, q)) return q;
    return -1;
}

bool special_allowed(int special, Element e) {
    if (special < 0 || special >= int(std::size(data::kSpecialRows))) return false;
    return data::kSpecialRows[std::size_t(special)].element == e;
}
bool special_allowed(int special, const ChampionDef& c) {
    if (special < 0 || special >= int(std::size(data::kSpecialRows))) return false;
    if (c.avatar) return data::kSpecialRows[std::size_t(special)].element != Element::None;  // [U-279] any bender special
    return special_allowed(special, c.element);
}
int find_special(std::string_view name) {
    for (int i = 0; i < int(std::size(data::kSpecialRows)); ++i)
        if (data::kSpecialRows[std::size_t(i)].def.name == name) return i;
    return -1;
}

}  // namespace em

namespace em {

// ---------------------------------------------------------------- [U-296] awakening of non-benders
namespace {
// the really basic set per element (Q W E R), by skill name (Claude idea; existing skills)
constexpr std::array<std::array<std::string_view, 4>, 4> kAwakenSets = {{
    {"Water Whip", "Healing Water", "Ice Slide", "Whirlpool"},  // Water
    {"Rock Throw", "Earth Wall", "Pillar Rise", "Earthquake"},   // Earth
    {"Fire Bolt", "Fire Whip", "Jet Propulsion", "Blaze Ring"},  // Fire
    {"Air Gust", "Air Scooter", "Air Step", "Tornado"},          // Air
}};
const SkillDef* skill_by_name(std::string_view name) {
    for (const ChampionDef& c : champions())
        for (const SkillDef* s : {&c.p, &c.w, &c.e, &c.r, &c.t, &c.y})
            if (s->name == name) return s;
    for (const SkillDef& s : q_options())
        if (s.name == name) return &s;
    for (const SkillDef& s : specials())
        if (s.name == name) return &s;
    return nullptr;
}
}  // namespace

Element awaken_element_for(const ChampionDef& c, std::int64_t dmg) {
    if (c.awaken_element != Element::None) return c.awaken_element;
    static constexpr Element kByRest[4] = {Element::Air, Element::Water, Element::Earth, Element::Fire};
    return kByRest[std::size_t((dmg % 4 + 4) % 4)];
}

const SkillDef* awakened_skill(const ChampionDef& c, Element e, int slot) {
    if (slot < 1 || slot > 4 || e == Element::None) return nullptr;
    const std::string_view own = c.awaken_skills[std::size_t(slot - 1)];
    if (!own.empty())
        if (const SkillDef* s = skill_by_name(own)) return s;
    return skill_by_name(kAwakenSets[std::size_t(e)][std::size_t(slot - 1)]);
}

}  // namespace em
