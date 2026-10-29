#pragma once
// [X-13][X-59] Champion + skill content (placeholder roster, Claude idea).
// Data-driven: every skill is a SkillDef row; the sim only knows the SkillKind behaviours.
// Numbers go through core resolve() → ranks, levels, items, sky events all apply [U-110..U-114][U-123].
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "core/core.hpp"

namespace em {

// What a skill does in the world. One behaviour = one code path in sim_skills.cpp.
enum class SkillKind : std::uint8_t {
    None,
    Bolt,        // projectile, stops on first enemy (or pierces)
    Wave,        // wide projectile, pierces, pushes
    Burst,       // fluid blob at a point after a delay
    Field,       // fluid zone that stays (damage / CC ticks), can be group-fed [U-135]
    Cone,        // instant fluid fan in front
    Dash,        // caster rushes to a point, hits on the way
    Wall,        // raises a stone / ice wall line [U-18][U-24]
    Pillar,      // raises ground (temp elevation) → airborne [U-45]
    Tornado,     // moving group zone [U-135]
    Shield,      // self (or ally) shield, optional model size [U-129]
    Heal,        // allies in a blob [U-78]
    Haste,       // self move speed
    Blink,       // short teleport
    Lantern,     // light that hangs in the air [U-118][X-62]
    WindSense,   // [U-141] detect skills outside vision as waves
    Flight,      // [U-51][U-77] fly over terrain
    SelfFreeze,  // [U-143] freeze self: untouchable + no heat
    Bloodbend,   // [U-78] targeted, full moon only
    Boulder,     // [U-27] heavy stone: cast 1 raise, cast 2 push (multicast) — allies can push too
    Passive,     // always-on (seismic / heat sense specials, champion passives)
    WaterSpout,  // [U-189] ride a column of water over water: elevated, can cast, can't be hit by melee / ground
    IcePath,     // [U-188][U-189] freeze the water around you → walkable ice for a while
    Redirect,    // [U-185] lightning redirection: a well-timed window sends incoming lightning to your cursor
    AvatarState, // [U-279] fly + power + size for dur_ms
    MagicStance, // [U-264] non-benders: physical skills + autos become this skill's element (magic) for dur_ms
};

// [U-184] Where a skill aims: at the mouse cursor (normal) or where the champion faces (Cassiopeia-like).
enum class Aim : std::uint8_t { Cursor, Facing };

// [U-180] Champion classes for sorting in champion select.
enum class Role : std::uint8_t { Mage, Marksman, Fighter, Tank, Assassin, Support };
[[nodiscard]] constexpr const char* role_name(Role r) {
    switch (r) {
        case Role::Mage: return "Mage";
        case Role::Marksman: return "Marksman";
        case Role::Fighter: return "Fighter";
        case Role::Tank: return "Tank";
        case Role::Assassin: return "Assassin";
        case Role::Support: return "Support";
    }
    return "?";
}

enum class PassiveId : std::uint8_t {
    None,
    InnerFlame,   // skill hits burn
    Static,       // autos chain on water [X-58]
    Rooted,       // +tenacity
    MetalSkin,    // +defence
    Molten,       // lava lasts longer
    Tide,         // heals stronger near water
    Frostbite,    // autos slow
    LightFeet,    // + move speed
    MistForm,     // + skill size
    ChiFlow,      // energy regen on hit
    SeismicSense, // [U-142]
    HeatSense,    // [U-143]
    FireInHand,   // [U-228][U-244] carries fire: strong light around you at night (you are seen too)
    Lantern,      // [U-244] a lantern for non-firebenders: smaller light at night, reveals you
    WindSense,    // [U-141][U-274] always on (like the other senses): skills outside vision show as waves
    ElementalCycle, // [U-279] Avatar: 4 different elements in a row → next skill +40 %
};

struct SkillDef {
    std::string_view name;
    SkillKind kind = SkillKind::None;
    SubElement sub = SubElement::Physical;
    Flow flow = Flow::Plain;
    // damage = dmg + dmg_rank × (rank−1) + ep % EP + ad % attack (+ bonus_ad %)
    std::int32_t dmg = 0, dmg_rank = 0, ep = 0, ad = 0, bonus_ad = 0;
    std::int32_t range = 0, width = 0, speed = 0, radius = 0;  // game units
    std::int32_t cast_ms = 250, cd_ms = 8'000, cd_rank = 0, cost = 0;
    std::int32_t dur_ms = 0, delay_ms = 0, tick_ms = 500;
    StatusKind cc = StatusKind::Count;
    std::int32_t cc_ms = 0, cc_val = 0;
    std::int32_t knock = 0;     // push distance (units)
    bool pierce = false;
    bool group = false;         // [U-135] same-element allies can feed it
    bool ally = false;          // targets allies (heal / shield)
    std::int32_t casts = 1, recast_ms = 0;  // [U-106] multicast
    CastMove move = CastMove::Rooted;       // [U-53]
    std::int32_t light = 0, light_ms = 0;   // [U-116]
    bool targeted = false;                  // [U-109] point-and-click (max 1 per kit [X-43])
    bool full_moon_only = false;            // [U-78]
    std::int32_t size_bonus = 0;            // [U-129] Shield: +model size (100 = 1 %)
    PassiveId passive = PassiveId::None;
    std::string_view desc;
    // ---- [U-184][U-186][U-191] behaviour switches (defaults = the usual case)
    bool turns_caster = true;    // false: cast without turning towards the target
    bool through_walls = false;  // true: passes walls / objects / trees (most skills do not)
    Aim aim = Aim::Cursor;       // Facing: fires where the champion looks, not at the cursor
    bool melee = false;          // melee skill → can't hit flying / water-raised units [U-191]
    bool ground = false;         // ground skill (quake, spikes, pillar…) → can't hit flying units [U-191]
    // [U-216] how high the skill's hitbox reaches (game units above the caster's ground). −1 = default for its kind
    // (melee / ground / dash 40, everything else 160). Raise it to hit flyers, airborne units, raised terrain.
    std::int16_t reach_height = -1;
    bool aim_high = false;       // [U-216] may be aimed upwards (mechanic ready, no skill uses it yet)
    // [U-222] sweet spot: +this % damage in the best part of the hitbox (blast centre 40 % of the radius,
    // cone tip = outer 25 %, bolt = last 20 % of its flight). 0 = no sweet spot. NOT a crit [U-221].
    std::int16_t sweet_pct = 0;
    // ---- [U-225][U-227][U-229][U-244][U-249] more switches
    std::int8_t destroys_trees = -1;  // −1 = by element (fire family / lava yes), 0 no, 1 yes
    std::int8_t burns_ground = -1;    // −1 = by element (fire blasts / fields / cones yes), 0 no, 1 yes
    bool env_bend = false;            // bends the environment (air: moves steam / smoke / sand clouds away)
    bool teleport = false;            // Blink only: true = real teleport; false = a very fast dash [U-229]
    std::uint8_t air_use = 0xff;      // usable while lifted by: bit 1 self, 2 ally, 4 enemy. 0xff = default
                                      // (everything except melee / dashes, which need the same height) [U-249]
    // ---- [U-291] ranks: every skill has its own rule (defaults below; set any of them per skill / champion)
    //   ultimate:  rank 1 at unlock_level (6), then +1 every rank_step levels (12, 18, 24 …)
    //   other:     max rank = rank_cap (2) + level / rank_step (6) → 2 before 6, 3 before 12, 4 before 18 …
    bool ultimate = false;            // a champion may have 0, 1 or 2 ultimates
    std::uint8_t unlock_level = 0;    // first level it can be learned (0 = default: 6 for ultimates, 1 otherwise)
    std::uint8_t rank_step = 0;       // levels per extra rank (0 = default 6)
    std::uint8_t rank_cap = 0;        // non-ultimates: highest rank before the first step (0 = default 2)
    std::string_view passive_desc{};  // [U-291] a skill's OWN passive (works while the skill is learned; PassiveId in .passive)
    std::uint32_t interrupt = 0;      // [U-337] what breaks its cast / channel (InterruptBit, core/channel_rules.hpp); 0 = default
    // [U-312] a single-target projectile (not piercing): which unit categories stop it (bit per UnitCategory,
    // 0 = every one) and on which it flies on when the hit KILLS the target (0 = never: it disappears, the default)
    std::uint16_t stop_on = 0, pass_on_kill = 0;
    std::uint16_t hits = 0;           // [U-311] which categories it can hit at all (0 = every one)
    // [U-313] + % of the TARGET's current / max / missing health (100 = 1 %), added to the damage (or the heal)
    std::int16_t pct_current_hp = 0, pct_max_hp = 0, pct_missing_hp = 0;
    // [U-340] source first, then destination: first click picks the water (later: rock) source, second the target.
    // −1 = default (water bolts / waves / cones / walls yes), 0 no, 1 yes
    std::int8_t source_first = -1;
    // [U-380] wall skills: the wall is built of SEGMENTS 100 units long. 0 = the standard:
    //   wall_thick 1 / 2 / 3 = 33 / 66 / 99 units (0 = 2), wall_level 1 / 2 levels high (0 = 2),
    //   wall_hp per segment (0 = 300; ice 100), + wall_hp_rank per rank above 1
    std::uint8_t wall_thick = 0, wall_level = 0;
    std::int16_t wall_hp = 0, wall_hp_rank = 0;
};

// [U-340] does this skill ask for a source first?
[[nodiscard]] constexpr bool source_first(const SkillDef& d) {
    if (d.source_first >= 0) return d.source_first == 1;
    const bool water = d.sub == SubElement::Water || d.sub == SubElement::Ice;
    if (d.sub == SubElement::Earth && (d.kind == SkillKind::Bolt || d.kind == SkillKind::Wave)) return true;  // [U-378] rock bending
    return water && (d.kind == SkillKind::Bolt || d.kind == SkillKind::Wave || d.kind == SkillKind::Cone || d.kind == SkillKind::Wall);
}

// [U-291] the highest rank a skill may have at this champion level
[[nodiscard]] constexpr int skill_max_rank(const SkillDef& d, int level) {
    const int step = d.rank_step ? d.rank_step : 6;
    if (d.ultimate) {
        const int unlock = d.unlock_level ? d.unlock_level : 6;
        return level < unlock ? 0 : 1 + (level - unlock) / step;
    }
    const int unlock = d.unlock_level ? d.unlock_level : 1;
    return level < unlock ? 0 : (d.rank_cap ? d.rank_cap : 2) + level / step;
}
static_assert(skill_max_rank(SkillDef{.ultimate = true}, 5) == 0 && skill_max_rank(SkillDef{.ultimate = true}, 6) == 1 &&
              skill_max_rank(SkillDef{.ultimate = true}, 12) == 2 && skill_max_rank(SkillDef{.ultimate = true}, 18) == 3);
static_assert(skill_max_rank(SkillDef{}, 1) == 2 && skill_max_rank(SkillDef{}, 5) == 2 && skill_max_rank(SkillDef{}, 6) == 3 &&
              skill_max_rank(SkillDef{}, 12) == 4);
inline constexpr std::uint8_t kAirSelf = 1, kAirAlly = 2, kAirEnemy = 4;

// [U-217] What a right click does. Defaults = a plain single-target hit. Mechanic ready; nothing uses
// the special shapes yet (Aphelios / Twitch-ult-like autos can be built from these).
enum class AutoShape : std::uint8_t { Single, Splash, Line, Chain };
struct AutoAttackDef {
    bool move_while_attacking = false;   // fire at the closest enemy in range while walking
    AutoShape shape = AutoShape::Single;
    std::int16_t splash_radius = 0, splash_pct = 50;  // Splash: units around the target take this %
    std::int16_t line_length = 0, line_width = 60;    // Line: everything on a line behind the target
    std::int8_t chain_count = 0;                      // Chain: jumps to this many more enemies …
    std::int16_t chain_range = 0, chain_pct = 60;     // … within this range, each jump this % of the last
};

// Bridge to the core numbers → resolve() does ranks, levels, items, sky [U-110..U-114][U-123].
[[nodiscard]] constexpr AbilityDef to_ability(const SkillDef& s, Slot slot) {
    const Delivery d = s.targeted ? Delivery::PointClick
                     : (s.kind == SkillKind::Bolt || s.kind == SkillKind::Wave) ? Delivery::Projectile
                                                                                 : Delivery::Skillshot;
    return AbilityDef{
        .slot = slot,
        .sub = s.sub,
        .delivery = d,
        .multicast = {.max_casts = std::uint8_t(s.casts), .recast_window_ms = std::uint16_t(s.recast_ms)},
        .cast_move = s.move,
        .damage = {.base = s.dmg, .per_rank = s.dmg_rank, .ep_pct = std::int16_t(s.ep), .ad_pct = std::int16_t(s.ad),
                   .bonus_ad_pct = std::int16_t(s.bonus_ad)},
        .range = {.base = s.range},
        .width = {.base = s.width},
        .radius = {.base = s.radius},
        .speed = {.base = s.speed},
        .cast_time_ms = {.base = s.cast_ms},
        .cooldown_ms = {.base = s.cd_ms, .per_rank = s.cd_rank},
        .cost = {.base = s.cost},
        .light_radius = {.base = s.light},
        .light_ms = {.base = s.light_ms},
        .changes_water_state = s.sub == SubElement::Ice || s.sub == SubElement::Steam,  // [U-28]
    };
}

struct ChampionDef {
    std::string_view name;
    Element element = Element::None;
    SubElement sub = SubElement::Physical;  // elemental autoattack sub-element
    const BodyDef* body = &kHumanoidBody;
    Range range = Range::Melee;
    ResourceKind resource = ResourceKind::None;
    bool mage = true;           // [U-110] non-mages scale with attack
    std::int32_t ep_lvl = 0;
    std::int32_t hp = 58'000, hp_lvl = 9'500;           // hundredths
    std::int32_t attack = 6'000, attack_lvl = 350;
    std::int32_t def = 3'000, def_lvl = 420, edef = 3'000, edef_lvl = 130;
    std::int32_t as = 65, as_lvl = 200;                 // attacks/s × 100; per level +% × 100
    std::int32_t ms = 33'500;                           // units/s × 100
    std::int32_t attack_range = 40;                     // EDGE to edge [U-133]
    std::int32_t res_max = 0, res_regen = 0;            // per second, hundredths
    // Q is NOT here: Q options belong to the element (game/skills/q_options.hpp) [U-11][U-183]
    SkillDef w, e, r, t, y;                             // [U-70][U-289] y = the sixth skill (key Y)
    SkillDef p;                                         // [U-104]
    bool stance = false;                                // [U-72] Space = switch stance
    std::uint32_t colour = 0xffffff;
    Role role = Role::Mage;                             // [U-180]
    bool auto_projectile = true;                        // [U-186] ranged autos fly as projectiles (melee: never)
    AutoAttackDef autos{};                              // [U-217]
    bool avatar = false;                                 // [U-279][U-07] Avatar type: max one per team
    bool dark = false;                                   // [U-07] a Dark Avatar (avatar = true too); max one per team
    std::uint8_t unique_group = 0;                       // [U-08] normal + dark versions share a group ≠ 0: one per game
    std::array<std::string_view, 3> q_pool{};
    // [U-296] non-benders awaken at the planetary alignment (60:00): this element (None = from the damage taken from
    // champions: rest of ÷ 4 → 0 air, 1 water, 2 earth, 3 fire) and this own set for Q W E R (empty = the element's
    // basic set). Predisposed non-benders can have their own set here.
    Element awaken_element = Element::None;
    std::array<std::string_view, 4> awaken_skills{};           // [U-270] this champion's Q options (names); empty = every Q of its element
};

[[nodiscard]] std::span<const ChampionDef> champions();

// [U-11][U-183] Q options of every element in one table (index = "Q id").
[[nodiscard]] std::span<const SkillDef> q_options();
[[nodiscard]] Element q_option_element(int q);
// May this champion take this Q? (same element; non-mages only attack-scaling skills [U-110])
[[nodiscard]] bool q_allowed(int champ, int q);
[[nodiscard]] int default_q(int champ);  // [U-178] first allowed Q
// [U-71][U-76..U-79] Special skills (F/G) that this element may pick.
[[nodiscard]] std::span<const SkillDef> specials();
// [U-296] the awakened element of a non-bender and its skill for Q (1) … R (4); nullptr = keep the old one
[[nodiscard]] Element awaken_element_for(const ChampionDef& c, std::int64_t damage_taken_from_champions);
[[nodiscard]] const SkillDef* awakened_skill(const ChampionDef& c, Element e, int slot);
[[nodiscard]] bool special_allowed(int special, Element e);
[[nodiscard]] bool special_allowed(int special, const ChampionDef& c);  // [U-279] the Avatar may take any bender special
[[nodiscard]] int find_special(std::string_view name);

[[nodiscard]] constexpr const char* element_name(Element e) {
    switch (e) {
        case Element::Water: return "Water";
        case Element::Earth: return "Earth";
        case Element::Fire: return "Fire";
        case Element::Air: return "Air";
        case Element::None: return "Non-bender";
    }
    return "?";
}

// [U-210] Own champions: add one more to the roster (before any match starts). Returns its index or −1.
int register_champion(const ChampionDef& c);
void reset_custom_champions();
// [U-95] own game versions change numbers in these tables (only from the menu / server start, never in a match)
std::span<ChampionDef> champions_mut();
std::span<SkillDef> q_options_mut();
std::span<SkillDef> specials_mut();
void restore_official_numbers();  // [U-277] drop all own champions (before taking a server's list)
[[nodiscard]] int builtin_champion_count();

// Element of a champion's skills for UI colour.
[[nodiscard]] constexpr std::uint32_t element_colour(Element e) {
    switch (e) {
        case Element::Water: return 0x3aa0ff;
        case Element::Earth: return 0x9a7a3a;
        case Element::Fire: return 0xff5a1e;
        case Element::Air: return 0xe8f0ff;
        case Element::None: return 0xb0b0b0;
    }
    return 0xffffff;
}
[[nodiscard]] constexpr Flow flow_of(SubElement s) {
    switch (base_element(s)) {
        case Element::Water: return Flow::Water;
        case Element::Fire: return Flow::Fire;
        case Element::Earth: return Flow::Earth;
        case Element::Air: return Flow::Air;
        case Element::None: return Flow::Plain;
    }
    return Flow::Plain;
}

}  // namespace em
