// [U-510] em_champion_maker: make a champion without touching a text file.
//   Left: what you edit (tabs). Right: a live preview: the model, its base hitbox and the chosen skill's exact hitbox over time.
//   The result is a normal custom_champions/<name>.champ (+ its model, sounds, pictures in resourcepacks/default/), so it works
//   in the game, on servers and in the Own champions menu like every other own champion.
//   All the rules live in the engine: the text the tool writes goes through the game's own parser after every change.
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <print>
#include <sstream>
#include <string>
#include <vector>

#include "client/anim.hpp"
#include "client/audio.hpp"
#include "client/custom_champions.hpp"
#include "client/locale.hpp"
#include "client/resources.hpp"
#include "client/sprites.hpp"
#include "apps/makers.hpp"
#include "client/tool_shell.hpp"
#include "game/champions/custom.hpp"
#include "game/content.hpp"
#include "game/world.hpp"

using namespace em;
using namespace em::gfx;
namespace fs = std::filesystem;

namespace {

// ================================================================ the draft: exactly what the .champ file will say
using Pairs = std::vector<std::pair<std::string, std::string>>;

struct Draft {
    std::string name = "New Champion", base = "Ember Monk";
    Pairs champ;                     // hp = 600 …
    std::map<char, std::string> copy;  // w = @bolt
    std::map<char, Pairs> skill;     // kind = Bolt …   ("_ultimate" = the rank rules: w_ultimate = 1)
    Pairs other;                     // lines the tool does not edit (model. …): kept as they are
    [[nodiscard]] bool operator==(const Draft&) const = default;
};

void set_kv(Pairs& p, const std::string& k, const std::string& v) {
    std::erase_if(p, [&](const auto& kv) { return kv.first == k; });
    if (!v.empty()) p.push_back({k, v});
}
[[nodiscard]] const std::string* get_kv(const Pairs& p, const std::string& k) {
    for (auto it = p.rbegin(); it != p.rend(); ++it)
        if (it->first == k) return &it->second;
    return nullptr;
}

constexpr std::string_view kSlots = "pwertyqfg";

std::string draft_text(const Draft& d) {
    std::string t = std::format("# made with the champion maker\nbase = {}\nname = {}\n", d.base, d.name);
    for (const auto& [k, v] : d.champ) t += std::format("{} = {}\n", k, v);
    for (char s : kSlots) {
        if (auto c = d.copy.find(s); c != d.copy.end()) t += std::format("{} = {}\n", s, c->second);
        if (auto sk = d.skill.find(s); sk != d.skill.end())
            for (const auto& [k, v] : sk->second) t += k.starts_with("_") ? std::format("{}{} = {}\n", s, k, v) : std::format("{}.{} = {}\n", s, k, v);
    }
    for (const auto& [k, v] : d.other) t += std::format("{} = {}\n", k, v);
    return t;
}

Draft draft_from_text(const std::string& text) {
    Draft d;
    d.base.clear(), d.name.clear();
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (const auto h = line.find(" #"); h != std::string::npos) line.resize(h);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto trim = [](std::string s) {
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
            std::size_t i = 0;
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
            return s.substr(i);
        };
        const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        if (k == "base") d.base = v;
        else if (k == "name") d.name = v;
        else if (k.size() == 1 && kSlots.find(k[0]) != std::string_view::npos) d.copy[k[0]] = v;
        else if (k.size() > 2 && k[1] == '.' && kSlots.find(k[0]) != std::string_view::npos) d.skill[k[0]].push_back({k.substr(2), v});
        else if (k.size() > 2 && k[1] == '_' && kSlots.find(k[0]) != std::string_view::npos) d.skill[k[0]].push_back({k.substr(1), v});
        else if (k.starts_with("model.")) d.other.push_back({k, v});
        else d.champ.push_back({k, v});
    }
    return d;
}

[[nodiscard]] std::string id_of(std::string_view name) {
    std::string id(name);
    for (char& c : id) c = c == ' ' ? '_' : char(std::tolower(static_cast<unsigned char>(c)));
    std::erase(id, '\'');
    return id;
}

// ================================================================ the fields: one row of a table per setting
enum class FT : std::uint8_t { Int, Dec, Bool, Enum, Text, Mask };
struct Field {
    const char* key;
    const char* label;
    FT type;
    int lo, hi, step;
    const char* tip;
    std::function<int(const SkillDef&)> get;
    std::vector<std::string_view> names{};  // Enum / Mask: values in order
    std::function<std::string(const SkillDef&)> get_text{};
    bool names_in_file = true;              // Mask: names in the file (else a number)
};
#define SI(key, label, m, lo, hi, step, tip) Field{key, label, FT::Int, lo, hi, step, tip, [](const SkillDef& s) { return int(s.m); }}
#define SB(key, label, m, tip) Field{key, label, FT::Bool, 0, 1, 1, tip, [](const SkillDef& s) { return int(s.m); }}

const std::vector<std::string_view> kKindNames = {"None", "Bolt", "Wave", "Burst", "Field", "Cone", "Dash", "Wall", "Pillar", "Tornado", "Shield", "Heal", "Haste", "Blink", "Lantern", "WindSense",
                                                  "Flight", "SelfFreeze", "Bloodbend", "Boulder", "Passive", "WaterSpout", "IcePath", "Redirect", "AvatarState", "MagicStance", "StonePush", "Transform", "MoveWater"};
const std::vector<std::string_view> kSubNames = {"Water", "Ice", "Wood", "Blood", "Mud", "Steam", "Air", "Earth", "Lava", "Metal", "EarthArmor", "MetalArmor", "Fire", "BlueFire", "Lightning", "Combustion", "Physical"};
const std::vector<std::string_view> kGeoNames = {"Auto", "Circle", "Ring", "Line", "Rect", "Cone", "Arc", "Square", "Triangle", "Ellipse", "Diamond", "Hexagon", "Star", "Custom"};
const std::vector<std::string_view> kZoneNames = {"Auto", "Centre", "Edge", "Tip", "Band"};
const std::vector<std::string_view> kCcNames = {"Stun", "Root", "Slow", "Silence", "ChiBlock", "Airborne", "Frozen", "Blind", "Burn", "Shield", "Haste", "Flying", "Bloodbound", "Channel", "Swimming", "WaterRide",
                                                "Redirect", "Wet", "Grievous", "Fear", "Disarm", "Subdue", "Hex", "Break", "DebuffImmune", "Polymorph", "Invisible", "Sleep", "Drowsy", "Charm", "Taunt", "Terrify", "None"};
const std::vector<std::string_view> kPassiveNames = {"None", "InnerFlame", "Static", "Rooted", "MetalSkin", "Molten", "Tide", "Frostbite", "LightFeet", "MistForm", "ChiFlow", "SeismicSense", "HeatSense", "FireInHand", "Lantern", "WindSense", "ElementalCycle"};
const std::vector<std::string_view> kHeatNames = {"Usual", "Red 600", "Red 800", "Orange 1000", "Yellow 1400", "White 1600", "Blue 2000", "Blue 2500", "Blue 3000", "Blue 3333"};
const std::vector<std::string_view> kCatNames = {"Champion", "Minion", "SiegeMinion", "SuperMinion", "Monster", "EpicMonster", "Turret", "Inhibitor", "Shrine"};
// [U-534] what the element lists show: every colour of fire by name; the element stays fire
struct ElementChoice { const char* name; const char* sub; int heat; };
constexpr ElementChoice kFireChoices[] = {
    {"Fire - Red 600", "Fire", 1}, {"Fire - Red 800", "Fire", 2}, {"Fire - Orange 1000", "Fire", 0}, {"Fire - Yellow 1400", "Fire", 4}, {"Fire - White 1600", "Fire", 5},
    {"Fire - Blue 2000", "BlueFire", 6}, {"Fire - Blue 2500", "BlueFire", 7}, {"Fire - Blue 3000", "BlueFire", 8}, {"Fire - Blue 3333", "BlueFire", 9}};
[[nodiscard]] std::string element_shown(SubElement sub, int heat) {
    if (sub == SubElement::Fire || sub == SubElement::BlueFire) {
        if (heat == 0) return sub == SubElement::Fire ? "Fire - Orange 1000" : "Fire - Blue 2000";
        for (const ElementChoice& c : kFireChoices)
            if (c.heat == heat) return c.name;
        if (heat == 3) return "Fire - Orange 1000";
    }
    return std::string(kSubNames[std::size_t(sub)]);
}
[[nodiscard]] std::vector<ElementChoice> element_choices() {
    std::vector<ElementChoice> out;
    for (std::string_view n : kSubNames)
        if (n == "Fire") out.insert(out.end(), std::begin(kFireChoices), std::end(kFireChoices));
        else if (n != "BlueFire") out.push_back({n.data(), n.data(), 0});
    return out;
}
const std::vector<std::string_view> kNeeds(std::begin(kNeedNames), std::end(kNeedNames));

struct Page {
    const char* title;
    std::vector<Field> fields;
};

const std::vector<Page>& skill_pages() {
    static const std::vector<Page> pages = {
        {"What it is", {
            Field{"name", "Name", FT::Text, 0, 0, 0, "The skill's name in the game", {}, {}, [](const SkillDef& s) { return std::string(s.name); }},
            Field{"desc", "Description", FT::Text, 0, 0, 0, "What the tooltip says", {}, {}, [](const SkillDef& s) { return std::string(s.desc); }},
            Field{"kind", "Type", FT::Enum, 0, 0, 0, "What it does: Bolt flies and stops, Wave flies through, Burst hits at a point after a delay, Field stays, Cone is instant in front, Dash moves you …", [](const SkillDef& s) { return int(s.kind); }, kKindNames},
            Field{"sub", "Element / modifier", FT::Enum, 0, 0, 0, "Decides colour, sounds, resist, every element rule (Physical = no element)", [](const SkillDef& s) { return int(s.sub); }, kSubNames},
            SB("arcane", "Non-elemental magic", arcane, "Magic without an element: chi block and disarm do not stop it"),
            Field{"heat", "Heat of fire", FT::Enum, 0, 0, 0, "Hotter: stronger, faster, brighter, burns longer, melts more. Its colour shows it", [](const SkillDef& s) { return int(s.heat); }, {"Usual", "Red 600", "Red 800", "Orange 1000", "Yellow 1400", "White 1600", "Blue 2000", "Blue 2500", "Blue 3000", "Blue 3333"}},
            Field{"sweet_heat", "Heat in the sweet spot", FT::Enum, 0, 0, 0, "The sweet part of the shape can burn hotter than the rest", [](const SkillDef& s) { return int(s.sweet_heat); }, {"Same", "Red 600", "Red 800", "Orange 1000", "Yellow 1400", "White 1600", "Blue 2000", "Blue 2500", "Blue 3000", "Blue 3333"}},
            Field{"dmg_type", "Damage type", FT::Enum, 0, 0, 0, "Normal, True (ignores defence), Absolute (also ignores reductions)", [](const SkillDef& s) { return int(s.dmg_type); }, {"Normal", "True", "Absolute"}},
            SI("true_pct", "% as that type", true_pct, 0, 100, 5, "This % of the damage is the type above, the rest normal"),
            SB("targeted", "Click on an enemy", targeted, "Point and click instead of aiming (one per kit)"),
            SB("ally", "Hits allies instead", ally, "Heals, shields, buffs: it works on your team"),
            Field{"aim", "Aim", FT::Enum, 0, 0, 0, "At the cursor, or where the champion faces", [](const SkillDef& s) { return int(s.aim); }, {"Cursor", "Facing"}},
            SB("turns_caster", "Turns the caster", turns_caster, "Off: casts without turning"),
            Field{"move", "While casting", FT::Enum, 0, 0, 0, "Rooted, slowed or free to walk during the cast", [](const SkillDef& s) { return int(s.move); }, {"Free", "Rooted", "Slowed"}},
            SB("free_cast", "Inside another cast", free_cast, "Can be used during another cast without stopping it"),
            SB("melee", "Melee skill", melee, "Cannot reach flying or water-raised units"),
            SB("ground", "Ground skill", ground, "Quakes, spikes: cannot hit flying units"),
            SI("reach_height", "Reach height", reach_height, -1, 500, 10, "How high the hitbox reaches (-1 = the usual for its type)"),
            SI("sensed_target", "Sensed targets", sensed_target, -1, 1, 1, "-1 with a sense passive, 0 only visible enemies, 1 also sensed ones"),
            SB("aa_reset", "Resets the autoattack", aa_reset, "The next autoattack is ready at once"),
        }},
        {"Numbers", {
            SI("dmg", "Damage", dmg, 0, 2000, 5, "Base damage (or heal / shield amount)"),
            SI("dmg_rank", "Damage per rank", dmg_rank, 0, 500, 5, "Added for each rank above 1"),
            SI("ep", "% elemental power", ep, 0, 300, 5, "Scaling with elemental power"),
            SI("ad", "% attack", ad, 0, 300, 5, "Scaling with attack"),
            SI("bonus_ad", "% bonus attack", bonus_ad, 0, 300, 5, "Scaling with attack from items"),
            SI("ap", "% ability power", ap, 0, 300, 5, "Scaling with ability power"),
            SI("pct_current_hp", "% current HP", pct_current_hp, 0, 3000, 50, "100 = 1 % of the target's current health"),
            SI("pct_max_hp", "% max HP", pct_max_hp, 0, 3000, 50, "100 = 1 % of the target's maximum health"),
            SI("pct_missing_hp", "% missing HP", pct_missing_hp, 0, 3000, 50, "100 = 1 % of the target's missing health"),
            SI("range", "Range / length", range, 0, 3000, 10, "How far it reaches or flies"),
            SI("range_rank", "Range per rank", range_rank, -500, 500, 10, ""),
            SI("width", "Width", width, 0, 1500, 5, "Width of the projectile, line, cone's far side"),
            SI("width_rank", "Width per rank", width_rank, -300, 300, 5, ""),
            SI("radius", "Radius / size", radius, 0, 1200, 5, "Size of blasts, fields and the polygon shapes"),
            SI("radius_rank", "Radius per rank", radius_rank, -300, 300, 5, ""),
            SI("speed", "Speed", speed, 0, 5000, 50, "Units per second of a projectile or dash"),
            SI("cast_ms", "Cast time (ms)", cast_ms, 0, 4000, 25, "Time before it goes off"),
            SI("cd_ms", "Cooldown (ms)", cd_ms, 0, 200000, 500, ""),
            SI("cd_rank", "Cooldown per rank (ms)", cd_rank, -10000, 10000, 100, "Usually negative"),
            SI("cost", "Cost", cost, 0, 1000, 5, "Resource cost"),
            SI("cost_rank", "Cost per rank", cost_rank, -200, 200, 5, ""),
            SI("dur_ms", "Duration (ms)", dur_ms, 0, 30000, 100, "How long it lasts (fields, buffs, walls, a cone with keys)"),
            SI("dur_ms_rank", "Duration per rank (ms)", dur_ms_rank, -5000, 5000, 100, ""),
            SI("delay_ms", "Delay (ms)", delay_ms, 0, 5000, 25, "Burst: the time from the mark on the ground to the hit"),
            SI("tick_ms", "Tick (ms)", tick_ms, 50, 3000, 50, "Field: how often it hits"),
            SI("casts", "Casts", casts, 1, 10, 1, "More than 1: recast it in the window"),
            SI("recast_ms", "Recast window (ms)", recast_ms, 0, 20000, 100, ""),
            SI("charges", "Charges", charges, 0, 9, 1, "Stored casts"),
            SI("charge_ms", "Charge comes back (ms)", charge_ms, 0, 120000, 500, ""),
            SI("charge_gap_ms", "Between charges (ms)", charge_gap_ms, 0, 5000, 50, ""),
            SI("self_root_ms", "Roots you after (ms)", self_root_ms, -1, 3000, 50, "-1 = the usual: 300 ms for skills that shape the ground"),
            SI("lock_ms", "Locks other skills (ms)", lock_ms, 0, 20000, 100, "Casting it puts your other skills on cooldown this long"),
            Field{"lock_slots", "Which ones it locks", FT::Mask, 0, 0, 0, "Empty = all the others", [](const SkillDef& s) { return int(s.lock_slots); }, {"P", "Q", "W", "E", "R", "T", "Y", "F", "G"}, {}, false},
        }},
        {"Shape", {}},  // own page: the shape buttons
        {"Change with time", {}},  // own page: the keys
        {"Crowd control", {
            Field{"cc", "Effect", FT::Enum, 0, 0, 0, "What a hit does to the target", [](const SkillDef& s) { return int(s.cc); }, kCcNames},
            SI("cc_ms", "Lasts (ms)", cc_ms, 0, 8000, 50, ""),
            SI("cc_ms_rank", "Lasts per rank (ms)", cc_ms_rank, -2000, 2000, 50, ""),
            SI("cc_val", "Strength", cc_val, 0, 2000, 5, "Slow %, burn hundredths of HP per second, blind %, chi block mask (1 water 2 earth 4 fire 8 air, 0 all)"),
            SI("cc_val_rank", "Strength per rank", cc_val_rank, -200, 200, 5, ""),
            SB("cc_sweet_only", "Only on the sweet spot", cc_sweet_only, "The effect and the knock happen only when the sweet spot hits"),
            Field{"cc_rule", "The same effect again", FT::Enum, 0, 0, 0, "Keeps the longer time, adds the times together, or starts the time again", [](const SkillDef& s) { return int(s.cc_rule); }, {"Keeps the longer", "Adds up", "Starts again"}},
            SI("sleep_delay_ms", "Sleep: drowsy first (ms)", sleep_delay_ms, 0, 5000, 50, "Effect Sleep: slowed this long, then asleep; a hit wakes it"),
            SI("drowsy_slow", "Sleep: drowsy slow %", drowsy_slow, 0, 95, 5, ""),
            SI("pin_range", "Pin: reach to a wall", pin_range, 0, 1000, 10, "A wall this close: the target is thrown into it, airborne on the way, then gets the effect above. No wall: no effect"),
            SI("pin_ms", "Pin: held for (ms)", pin_ms, 0, 5000, 50, ""),
            Field{"grapple", "Grapple", FT::Enum, 0, 0, 0, "A projectile that catches the first wall or unit: arms of water, a line of ice, a cable", [](const SkillDef& s) { return int(s.grapple); }, {"No", "Pulls me there", "Pulls the unit to me", "We meet halfway"}},
            SI("grapple_speed", "Grapple speed", grapple_speed, 100, 5000, 50, ""),
            SI("knock", "Push distance", knock, 0, 1200, 10, "Pushes what it hits"),
            SI("knock_rank", "Push per rank", knock_rank, -300, 300, 10, ""),
            SI("dispel", "Dispel", dispel, 0, 2, 1, "1 basic, 2 strong: cleans the caster, strips buffs from enemies hit"),
            SI("tether_ms", "Chain time (ms)", tether_ms, 0, 8000, 50, "A chain holds what it hit; still close when it ends = pulled"),
            SI("tether_radius", "Chain radius", tether_radius, 0, 1500, 10, ""),
            SI("pull", "Chain pull", pull, 0, 1500, 10, ""),
            Field{"cc2", "Chain effect", FT::Enum, 0, 0, 0, "When the chain pulls", [](const SkillDef& s) { return int(s.cc2); }, kCcNames},
            SI("cc2_ms", "Chain effect lasts (ms)", cc2_ms, 0, 8000, 50, ""),
        }},
        {"Projectile", {
            SB("pierce", "Flies through", pierce, "Goes on after a hit"),
            Field{"stop_on", "Stopped by", FT::Mask, 0, 0, 0, "Empty = every unit stops it", [](const SkillDef& s) { return int(s.stop_on); }, kCatNames, {}, false},
            Field{"pass_on_kill", "Flies on after killing", FT::Mask, 0, 0, 0, "These units let it fly on when the hit kills them", [](const SkillDef& s) { return int(s.pass_on_kill); }, kCatNames, {}, false},
            Field{"hits", "Can hit", FT::Mask, 0, 0, 0, "Empty = everything", [](const SkillDef& s) { return int(s.hits); }, kCatNames},
            SB("through_walls", "Through walls", through_walls, "Passes walls, objects and trees"),
            SB("pass_units", "Dash through units", pass_units, "A dash does not stop at units"),
            SB("burrow", "Dash under the ground", burrow, "Cannot be seen or hit on the way, passes walls and units"),
            SB("teleport", "Real teleport", teleport, "Blink: a teleport instead of a very fast dash"),
            SI("control", "Mouse control", control, 0, 2, 1, "While the key is held: 1 steer the projectile, 2 draw a wall"),
            SI("steer_turn", "Steering speed", steer_turn, 0, 8000, 100, ""),
        }},
        {"Terrain & resources", {
            Field{"needs", "Relies on", FT::Mask, 0, 0, 0, "All marked things must be near the caster, or it cannot be cast", [](const SkillDef& s) { return int(s.needs); }, kNeeds},
            SI("needs_range", "How near", needs_range, 32, 1500, 10, "Distance for the things above"),
            SI("source_first", "Pick the source first", source_first, -1, 1, 1, "First click picks the water or rock, second the target. -1 = usual for its type"),
            SB("group", "Allies can feed it", group, "Same-element allies make it grow"),
            SB("env_bend", "Bends the environment", env_bend, "Air: moves steam, smoke, sand clouds away"),
            SI("destroys_trees", "Trees", destroys_trees, -1, 1, 1, "-1 by element (fire burns), 0 no, 1 destroys them"),
            SI("burns_ground", "Burns the ground", burns_ground, -1, 1, 1, "-1 by element, 0 no, 1 yes"),
            SI("wall_thick", "Wall thickness", wall_thick, 0, 3, 1, "33 / 66 / 99 units (0 = usual)"),
            SI("wall_level", "Wall height", wall_level, 0, 2, 1, "1 or 2 levels (0 = usual)"),
            SI("wall_hp", "Wall HP per segment", wall_hp, 0, 3000, 25, "0 = usual"),
            SI("wall_hp_rank", "Wall HP per rank", wall_hp_rank, 0, 1000, 25, ""),
            SI("wall_segs", "Wall segments", wall_segs, 0, 20, 1, "100 units each (0 = from the width)"),
            SB("lift_beside", "Lift the stone beside you", lift_beside, "Boulder: right next to the caster"),
            SB("full_moon_only", "Only at full moon", full_moon_only, ""),
            SI("air_use", "Usable while lifted", air_use, 0, 255, 1, "Bits: 1 by yourself, 2 by an ally, 4 by an enemy (255 = usual)"),
            SI("light", "Light radius", light, 0, 1500, 10, "Gives light while it lasts; fire leaves embers"),
            SI("light_ms", "Embers last (ms)", light_ms, 0, 20000, 100, "How long the light stays after it ends (0 = usual)"),
        }},
        {"Look & sound", {}},  // own page: colour, picture, sound
        {"Ranks & passive", {
            Field{"_ultimate", "Ultimate", FT::Bool, 0, 1, 1, "Rank 1 at level 6, then every 6 levels", [](const SkillDef& s) { return int(s.ultimate); }},
            SI("_unlock_level", "First level", unlock_level, 0, 30, 1, "0 = usual"),
            SI("_rank_step", "Levels per rank", rank_step, 0, 12, 1, "0 = usual (6)"),
            SI("_rank_cap", "Ranks before the first step", rank_cap, 0, 9, 1, "0 = usual (2)"),
            Field{"passive", "Always-on passive", FT::Enum, 0, 0, 0, "Works while the skill is learned", [](const SkillDef& s) { return int(s.passive); }, kPassiveNames},
            Field{"passive_desc", "Passive text", FT::Text, 0, 0, 0, "", {}, {}, [](const SkillDef& s) { return std::string(s.passive_desc); }},
            SI("proc", "Passive trigger", proc, 0, 4, 1, "1 next autoattack, 2 a skill hit, 3 by itself on a timer, 4 HP falls low"),
            SI("proc_cd_ms", "Passive cooldown (ms)", proc_cd_ms, 0, 120000, 500, ""),
            SI("proc_dmg", "Passive damage", proc_dmg, 0, 2000, 5, ""),
            SI("proc_pct_max_hp", "Passive % max HP", proc_pct_max_hp, 0, 3000, 50, "100 = 1 %"),
            SI("proc_heal_pct", "Heal % of that damage", proc_heal_pct, 0, 300, 5, ""),
            SI("proc_heal_max_hp", "Heal % my max HP", proc_heal_max_hp, 0, 3000, 50, "100 = 1 %"),
            SI("proc_hp_pct", "Low HP means under %", proc_hp_pct, 1, 100, 1, ""),
            SI("proc_shield", "Passive shield", proc_shield, 0, 30000, 100, "Hundredths of HP"),
            Field{"proc_cc", "Passive effect", FT::Enum, 0, 0, 0, "", [](const SkillDef& s) { return int(s.proc_cc); }, kCcNames},
            SI("proc_cc_ms", "Passive effect lasts (ms)", proc_cc_ms, 0, 8000, 50, ""),
            SI("size_bonus", "Model size bonus", size_bonus, 0, 5000, 50, "Shield, Transform: 100 = +1 %"),
            SI("buff_attack_pct", "Form: attack %", buff_attack_pct, 0, 300, 5, "Transform"),
            SI("buff_heal_pct", "Form: healing %", buff_heal_pct, 0, 300, 5, "Transform"),
            SI("scepter_dmg_pct", "Scepter: damage %", scepter_dmg_pct, 0, 300, 5, "0 = usual"),
            SI("scepter_cd_pct", "Scepter: cooldown %", scepter_cd_pct, 0, 90, 5, "0 = usual"),
        }},
    };
    return pages;
}
#undef SI
#undef SB

struct ChampField {
    const char* key;
    const char* label;
    int lo, hi, step;
    bool dec;
    const char* tip;
    std::function<int(const ChampionDef&)> get;  // in the unit the file uses (× 100 when dec)
};
const std::vector<ChampField>& champ_fields() {
    static const std::vector<ChampField> f = {
        {"hp", "Health", 100, 3000, 10, false, "", [](const ChampionDef& c) { return c.hp / 100; }},
        {"hp_per_level", "Health per level", 0, 300, 5, false, "", [](const ChampionDef& c) { return c.hp_lvl / 100; }},
        {"attack", "Attack", 0, 300, 1, false, "", [](const ChampionDef& c) { return c.attack / 100; }},
        {"attack_per_level", "Attack per level", 0, 30, 1, false, "", [](const ChampionDef& c) { return c.attack_lvl / 100; }},
        {"ep_per_level", "Elemental power per level", 0, 30, 1, false, "", [](const ChampionDef& c) { return c.ep_lvl / 100; }},
        {"defence", "Defence", 0, 200, 1, false, "", [](const ChampionDef& c) { return c.def / 100; }},
        {"elemental_defence", "Elemental defence", 0, 200, 1, false, "", [](const ChampionDef& c) { return c.edef / 100; }},
        {"move_speed", "Move speed", 100, 800, 5, false, "Units per second", [](const ChampionDef& c) { return c.ms / 100; }},
        {"attack_range", "Attack range", 0, 1200, 5, false, "Edge to edge", [](const ChampionDef& c) { return c.attack_range; }},
        {"attack_speed", "Attack speed", 1, 300, 1, false, "Attacks per second × 100", [](const ChampionDef& c) { return c.as; }},
        {"resource_max", "Resource maximum", 0, 2000, 10, false, "", [](const ChampionDef& c) { return c.res_max / 100; }},
        {"resource_regen", "Resource per second", 0, 100, 1, false, "", [](const ChampionDef& c) { return c.res_regen / 100; }},
        {"model_size", "Model and hitbox size +%", -50, 200, 1, false, "Makes the model and its hitbox bigger together", [](const ChampionDef& c) { return c.model_size / 100; }},
        {"strength", "Strength", 0, 10000, 10, true, "Dota attributes", [](const ChampionDef& c) { return c.str; }},
        {"strength_per_level", "Strength per level", 0, 1000, 5, true, "", [](const ChampionDef& c) { return c.str_lvl; }},
        {"agility", "Agility", 0, 10000, 10, true, "", [](const ChampionDef& c) { return c.agi; }},
        {"agility_per_level", "Agility per level", 0, 1000, 5, true, "", [](const ChampionDef& c) { return c.agi_lvl; }},
        {"intelligence", "Intelligence", 0, 10000, 10, true, "", [](const ChampionDef& c) { return c.intel; }},
        {"intelligence_per_level", "Intelligence per level", 0, 1000, 5, true, "", [](const ChampionDef& c) { return c.int_lvl; }},
    };
    return f;
}

[[nodiscard]] const SkillDef& slot_skill(const ChampionDef& c, char s) {
    return s == 'p' ? c.p : s == 'w' ? c.w : s == 'e' ? c.e : s == 'r' ? c.r : s == 't' ? c.t : s == 'y' ? c.y : s == 'q' ? c.q_own : s == 'f' ? c.f_own : c.g_own;
}

// ================================================================ an imported model sheet (kept in memory until Save)
struct ModelEdit {
    std::string png;      // the picked file
    SpriteSheet view;     // what the preview draws (its numbers can be changed without loading again)
    bool loaded = false;
    bool from_pack = false;
};

// ================================================================ the app
class Maker {
public:
    bool init(const std::string& open_file);
    void frame();
    void run() {
        std::string drop;
        while (win.begin([&](const SDL_Event& e) {
            if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) drop = e.drop.data;
        })) {
            if (!drop.empty()) {
                take_drop(drop);
                drop.clear();
            }
            frame();
            win.dirty = dirty_;
            win.end();
        }
    }
    tool::Window win;
    void start_at(int tab, int page) { tab_ = std::clamp(tab, 0, 2), page_ = std::clamp(page, 0, int(skill_pages().size()) - 1); }
    // a file dropped on the window: by its kind
    void take_drop(const std::string& path) {
        std::string ext = fs::path(path).extension().string();
        for (char& c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
        if (ext == ".wav") got_file("sound_" + std::string(1, slot_), path);
        else if (tab_ == 1 && (ext == ".png" || ext == ".bmp")) got_file("picture_" + std::string(1, slot_), path);
        else got_file("model", path);
    }

private:
    // ---- data
    Draft d_;
    ChampionDef cur_;       // what the game's parser made of the draft (the last valid one)
    std::string error_;
    std::vector<Draft> undo_;
    std::string file_;      // the file it came from ("" = new)
    bool dirty_ = false;
    // ---- view
    int tab_ = 0, page_ = 0;
    char slot_ = 'w';
    float scroll_ = 0;
    std::string tip_, status_;
    std::map<std::string, std::string> typed_;  // text being typed per key
    // preview
    bool playing_ = true;
    float life_ = 0;   // 0 … 1 of the skill
    int speed_ = 100;
    float aim_x_ = 4400, aim_y_ = 3900;
    bool top_down_ = false, edit_corners_ = false;
    int tilt_ = 72;                 // [U-531] how the camera looks at it: 100 = from above
    int turn_from_ = 0;             // [U-531] dragging on the preview turns the model
    float turn_mx_ = -1;
    bool model_special_ = false, sheet_special_ = false;  // [U-536] changing the model's size or how its sheet is read: only when asked for
    int draw_mode_ = 0;             // [U-536] 1: draw the body's hitbox with the mouse, 2: draw the skill's shape
    std::vector<std::pair<float, float>> stroke_;
    bool stroking_ = false;
    void finish_body_drawing(Dir facing, float cx, float cy, int scale_pct);
    void finish_skill_drawing(Dir facing, float cx, float cy);
    float zooms_[3] = {2.6f, 0.6f, 2.6f};  // each tab keeps its own (a model close, a skill from afar)
    int anim_i_ = 0, dir_ = 2;
    double last_t_ = 0;
    std::string anim_key_ = "idle";
    // resources
    std::unique_ptr<Resources> res_;
    std::unique_ptr<Sprites> sprites_;
    Audio audio_;
    ModelEdit model_;
    std::map<std::string, std::string> picked_;  // what → file: "sound_w", "picture_w", "icon", "splash"
    int import_n_ = 0;
    // file dialog
    static void SDLCALL on_files(void* self, const char* const* list, int filter);
    std::mutex dlg_mu_;
    std::string dlg_result_, dlg_what_;
    std::string path_text_;
    void ask_file(const std::string& what, bool picture);
    void got_file(const std::string& what, const std::string& path);
    // picker overlay
    struct Pick { std::string label, value, note; };
    std::string pick_title_, pick_search_;
    std::vector<Pick> pick_items_;
    std::function<void(const std::string&)> pick_done_;
    float pick_scroll_ = 0;
    void open_picker(std::string title, std::vector<Pick> items, std::function<void(const std::string&)> done);
    void draw_picker();
    // ---- work
    void changed(const Draft& before);
    void reparse();
    void new_champion();
    void open_file(const std::string& file);
    void open_builtin(const std::string& name);
    [[nodiscard]] std::string unique_name(const std::string& wanted) const;
    void save_as_new(const std::string& name);
    void overwrite();
    void save();
    std::string opened_builtin_;  // [U-519] opened from the built-in champions
    bool saving_ = false;
    std::string save_name_;
    int overwrite_armed_ = 0;
    // [U-510] a model built from a picture or a 3D file (Blender, a while)
    SDL_Process* build_proc_ = nullptr;
    std::string build_dir_;
    void start_model_build(const std::string& file);
    void poll_model_build();
    void load_model(const std::string& png);
    void load_pack_model();
    void write_sheet(const fs::path& dir) const;
    // ---- drawing
    void toolbar();
    void tab_champion(float x, float& y, float w);
    void tab_skills(float x, float& y, float w);
    void tab_model(float x, float& y, float w);
    void page_shape(float x, float& y, float w);
    void page_time(float x, float& y, float w);
    void page_look(float x, float& y, float w);
    void field_row(const Field& f, float x, float& y, float w);
    void preview(float x, float y, float w, float h);
    void file_row(const std::string& what, const std::string& label, bool picture, float x, float& y, float w);
    bool in_view(float y) const { return y > clip_top_ - 30 && y < clip_bottom_; }
    float clip_top_ = 0, clip_bottom_ = 0;
    [[nodiscard]] int id_for(const std::string& k) const { return int(std::hash<std::string>{}(k) & 0x3fffffff) + 1000; }
    Pairs& kv() { return d_.skill[slot_]; }
    const SkillDef& sk() const { return slot_skill(cur_, slot_); }
    void set_skill(const std::string& k, const std::string& v);
    void set_champ(const std::string& k, const std::string& v);
};

void Maker::reparse() {
    ChampionDef out;
    error_ = parse_champion_text(draft_text(d_), out);
    if (error_.empty()) cur_ = out;
}

void Maker::changed(const Draft& before) {
    if (before == d_) return;
    undo_.push_back(before);
    if (undo_.size() > 80) undo_.erase(undo_.begin());
    dirty_ = true;
    reparse();
}

void Maker::set_skill(const std::string& k, const std::string& v) {
    const Draft before = d_;
    set_kv(kv(), k, v);
    changed(before);
}
void Maker::set_champ(const std::string& k, const std::string& v) {
    const Draft before = d_;
    set_kv(d_.champ, k, v);
    changed(before);
}

void Maker::new_champion() {
    d_ = {};
    opened_builtin_.clear();
    file_.clear();
    undo_.clear();
    dirty_ = false;
    model_ = {};
    picked_.clear();
    reparse();
    load_pack_model();
}

// [U-519] a built-in champion: everything copied; "Save as new" makes an own one beside it, "Overwrite" replaces it
void Maker::open_builtin(const std::string& name) {
    d_ = {};
    d_.base = name;
    d_.name = unique_name(name);
    opened_builtin_ = name;
    file_.clear();
    undo_.clear();
    dirty_ = false;
    picked_.clear();
    model_ = {};
    reparse();
    load_pack_model();
    status_ = trf("opened {}", name);
}

[[nodiscard]] std::string Maker::unique_name(const std::string& wanted) const {
    const auto taken = [&](const std::string& n) {
        std::error_code ec;
        if (fs::exists(fs::path("custom_champions") / (n + ".champ"), ec)) return true;
        return std::ranges::any_of(champions(), [&](const ChampionDef& c) { return c.name == n; }) ||
               std::ranges::any_of(scan_custom_champions(), [&](const CustomChampionFile& f) { return f.name == n; });
    };
    std::string base = wanted.substr(0, 20);
    while (!base.empty() && (base.back() == ' ' || std::isdigit(static_cast<unsigned char>(base.back())))) base.pop_back();
    if (!taken(wanted)) return wanted;
    for (int i = 2; i < 100; ++i)
        if (const std::string n = std::format("{} {}", base, i); !taken(n)) return n;
    return wanted;
}

void Maker::save_as_new(const std::string& name) {
    d_.name = unique_name(name.empty() ? d_.name : name);  // never over another champion
    set_kv(d_.champ, "replaces", "");
    file_.clear();
    opened_builtin_.clear();
    reparse();
    save();
}

void Maker::overwrite() {
    if (!opened_builtin_.empty()) {  // own file with the built-in name: it replaces the built-in champion everywhere
        d_.name = opened_builtin_;
        set_kv(d_.champ, "replaces", "1");
        reparse();
    }
    save();
    if (error_.empty()) opened_builtin_.clear();
}

void Maker::open_file(const std::string& file) {
    opened_builtin_.clear();
    std::ifstream in(fs::path("custom_champions") / file, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), {});
    d_ = draft_from_text(text);
    file_ = file;
    undo_.clear();
    dirty_ = false;
    picked_.clear();
    model_ = {};
    reparse();
    load_pack_model();
    status_ = trf("opened {}", file);
}

bool Maker::init(const std::string& open) {
    if (!win.embedded() && !win.open("Elemental MOBA - Champion maker", 1600, 900)) return false;
    std::error_code ec;
    fs::create_directories("resourcepacks/_maker", ec);
    res_ = std::make_unique<Resources>();
    res_->init(win.ren(), "_maker");
    sprites_ = std::make_unique<Sprites>();
    sprites_->init(res_.get());
    audio_.init();
    if (!open.empty()) open_file(open);
    else new_champion();
    return true;
}

void SDLCALL Maker::on_files(void* self, const char* const* list, int) {
    auto* m = static_cast<Maker*>(self);
    if (!list || !*list) return;
    const std::scoped_lock lock(m->dlg_mu_);
    m->dlg_result_ = *list;
}

void Maker::ask_file(const std::string& what, bool picture) {
    {
        const std::scoped_lock lock(dlg_mu_);
        dlg_what_ = what;
        dlg_result_.clear();
    }
    static const SDL_DialogFileFilter kPic[] = {{"PNG / BMP", "png;bmp"}};
    static const SDL_DialogFileFilter kWav[] = {{"WAV", "wav"}};
    static const SDL_DialogFileFilter kModel[] = {{"Picture / 3D", "png;jpg;jpeg;webp;glb;gltf;fbx;obj"}};
    SDL_ShowOpenFileDialog(on_files, this, win.win(), what == "build" ? kModel : picture ? kPic : kWav, 1, nullptr, false);
}

void Maker::got_file(const std::string& what, const std::string& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        status_ = trf("file not found: {}", path);
        return;
    }
    if (what == "model") return load_model(path);
    if (what == "build") return start_model_build(path);
    picked_[what] = path;
    if (what.starts_with("sound")) {
        // a copy under a new name in the tool's own pack, so the preview always plays the new file
        const std::string rel = std::format("m{}_{}", ++import_n_, what);
        fs::create_directories("resourcepacks/_maker/sounds", ec);
        fs::copy_file(path, fs::path("resourcepacks/_maker/sounds") / (rel + ".wav"), fs::copy_options::overwrite_existing, ec);
        audio_.play(res_->sound(rel), SoundCat::Effects);
        picked_["play_" + what] = rel;
    } else if (what.starts_with("picture")) {
        const std::string rel = std::format("m{}_{}.png", ++import_n_, what);
        fs::create_directories("resourcepacks/_maker/skills", ec);
        fs::copy_file(path, fs::path("resourcepacks/_maker/skills") / rel, fs::copy_options::overwrite_existing, ec);
        picked_["show_" + what] = rel;
    }
    dirty_ = true;
    status_ = trf("picked {}", fs::path(path).filename().string());
}

// [U-510] Blender makes the sheet in the background; the tool stays usable
void Maker::start_model_build(const std::string& file) {
    if (build_proc_) {
        status_ = tr("a model is being built already");
        return;
    }
    build_dir_ = std::format("resourcepacks/_maker/models/m{}_built", ++import_n_);
    const char* args[] = {"python3", "tools/art/model_from_file.py", file.c_str(), build_dir_.c_str(), nullptr};
    build_proc_ = SDL_CreateProcess(args, false);
    status_ = build_proc_ ? tr("Blender is building the model: this takes some minutes, you can keep working") : std::string(SDL_GetError());
}

void Maker::poll_model_build() {
    if (!build_proc_) return;
    int code = 0;
    if (!SDL_WaitProcess(build_proc_, false, &code)) return;
    SDL_DestroyProcess(build_proc_);
    build_proc_ = nullptr;
    std::error_code ec;
    const fs::path png = fs::path(build_dir_) / "sheet.png";
    if (code != 0 || !fs::exists(png, ec)) {
        status_ = tr("the model could not be built from this file");
        return;
    }
    load_model(png.string());
}

// Reads sheet.txt next to the picture when there is one; a lone picture becomes a simple sheet (the numbers can be set).
void Maker::load_model(const std::string& png) {
    std::error_code ec;
    const std::string dir = std::format("m{}_model", ++import_n_);
    const fs::path to = fs::path("resourcepacks/_maker/models") / dir;
    fs::create_directories(to, ec);
    fs::copy_file(png, to / "sheet.png", fs::copy_options::overwrite_existing, ec);
    const fs::path txt = fs::path(png).parent_path() / "sheet.txt";
    bool had_txt = fs::exists(txt, ec);
    if (had_txt) {
        fs::copy_file(txt, to / "sheet.txt", fs::copy_options::overwrite_existing, ec);
    } else {
        int pw = 0, ph = 0;
        if (SDL_Texture* t = res_->texture("models/" + dir + "/sheet.png")) {
            float fw = 0, fh = 0;
            SDL_GetTextureSize(t, &fw, &fh);
            pw = int(fw), ph = int(fh);
        }
        const int dirs = ph > 0 && pw > ph && ph % 8 == 0 ? 8 : 1;
        const int px = std::max(1, ph / dirs), cols = std::max(1, pw / px);
        std::ofstream out(to / "sheet.txt");
        out << std::format("frame_px {}\ndirs {}\northo_m 2.6\nanchor 0.5 0.85\nanim idle 0 {} 8\n", px, dirs, cols);
    }
    sprites_ = std::make_unique<Sprites>();
    sprites_->init(res_.get());
    const SpriteSheet* s = sprites_->get_dir("models/" + dir);
    if (!s) {
        status_ = tr("this picture could not be read as a model");
        return;
    }
    model_.png = png, model_.view = *s, model_.loaded = true, model_.from_pack = false;
    dirty_ = true;
    anim_key_ = "idle";
    status_ = had_txt ? tr("model loaded with its sheet.txt") : tr("picture loaded: set its frames below");
}

// What the champion has already: its own model, else its base's (only a preview: the game draws its own model only)
void Maker::load_pack_model() {
    model_ = {};
    sprites_ = std::make_unique<Sprites>();
    sprites_->init(res_.get());
    const SpriteSheet* s = sprites_->get(id_of(d_.name));
    if (!s) s = sprites_->get(id_of(d_.base));
    if (s) model_.view = *s, model_.loaded = true, model_.from_pack = true;
}

void Maker::write_sheet(const fs::path& dir) const {
    const SpriteSheet& s = model_.view;
    std::ofstream out(dir / "sheet.txt");
    out << "# made with the champion maker\n" << std::format("frame_px {}\ndirs {}\northo_m {:.3f}\nanchor {:.3f} {:.3f}\n", s.px, s.dirs, s.ortho_m, s.ax, s.ay);
    if (s.wrap > 0) out << std::format("wrap {}\n", s.wrap);
    for (const auto& [k, a] : s.anims) out << std::format("anim {} {} {} {}\n", k, a.col, a.n, a.fps);
    for (const auto& [k, a] : s.use) out << std::format("use {} {}\n", k, a);
}

void Maker::save() {
    if (!error_.empty()) {
        status_ = tr("fix the problem first: it would not load in the game");
        return;
    }
    std::error_code ec;
    const std::string id = id_of(d_.name);
    const Draft before = d_;
    // ---- what was picked goes to its final place
    const fs::path pack = "resourcepacks/default";
    if (model_.loaded && !model_.from_pack) {
        fs::create_directories(pack / "models" / id, ec);
        fs::copy_file(model_.png, pack / "models" / id / "sheet.png", fs::copy_options::overwrite_existing, ec);
        write_sheet(pack / "models" / id);
    } else if (model_.loaded && model_.from_pack && fs::exists(pack / "models" / id / "sheet.txt", ec)) {
        write_sheet(pack / "models" / id);
    }
    for (const auto& [what, path] : picked_) {
        if (what.starts_with("play_") || what.starts_with("show_")) continue;
        if (what.starts_with("sound_")) {
            fs::create_directories(pack / "sounds/champions" / id, ec);
            fs::copy_file(path, pack / "sounds/champions" / id / ((what == "sound_death" ? std::string("death") : "cast_" + what.substr(6)) + ".wav"), fs::copy_options::overwrite_existing, ec);
        } else if (what.starts_with("picture_")) {
            fs::create_directories(pack / "skills", ec);
            const std::string name = std::format("{}_{}.png", id, what.substr(8));
            fs::copy_file(path, pack / "skills" / name, fs::copy_options::overwrite_existing, ec);
            set_kv(d_.skill[what[8]], "vis.sheet", name);
        } else if (what == "icon" || what == "splash") {
            fs::create_directories(pack / "champions" / id / "base", ec);
            fs::copy_file(path, pack / "champions" / id / "base" / (what + fs::path(path).extension().string()), fs::copy_options::overwrite_existing, ec);
        }
    }
    reparse();
    // ---- the file
    fs::create_directories("custom_champions", ec);
    const std::string file = d_.name + ".champ";
    std::ofstream out(fs::path("custom_champions") / file, std::ios::binary);
    out << draft_text(d_);
    if (!out) {
        status_ = trf("could not write {}", file);
        return;
    }
    file_ = file;
    dirty_ = false;
    status_ = trf("saved custom_champions/{}: switch it on in the game under Own champions", file);
}

void Maker::open_picker(std::string title, std::vector<Pick> items, std::function<void(const std::string&)> done) {
    pick_title_ = std::move(title), pick_items_ = std::move(items), pick_done_ = std::move(done);
    pick_search_.clear();
    pick_scroll_ = 0;
    win.ui.focus = 99;  // ready to type a search
}

void Maker::draw_picker() {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    const float pw = std::min(760.f, win.w - 80), ph = win.h - 120, x = (win.w - pw) / 2, y = 60;
    fill_rect(r, 0, 0, win.w, win.h, {0, 0, 0, 170});
    fill_rect(r, x, y, pw, ph, {30, 32, 44, 255});
    rect(r, x, y, pw, ph, {255, 190, 110, 255});
    text(r, x + 14, y + 10, pick_title_, {255, 220, 150}, 2);
    text_field(r, ui, 99, x + 14, y + 52, pw - 140, pick_search_, tr("Search"));
    if (button(r, ui, x + pw - 110, y + 50, 96, 30, tr("Close"))) {
        pick_done_ = {};
        ui.focus = -1;
        return;
    }
    std::string low = pick_search_;
    for (char& c : low) c = char(std::tolower(static_cast<unsigned char>(c)));
    const float top = y + 92, bottom = y + ph - 10;
    const SDL_Rect clip{int(x), int(top), int(pw), int(bottom - top)};
    SDL_SetRenderClipRect(r, &clip);
    pick_scroll_ = std::max(0.f, pick_scroll_ - ui.wheel * 40);
    float ry = top - pick_scroll_;
    std::function<void(const std::string&)> chosen;
    std::string value;
    for (const Pick& p : pick_items_) {
        std::string l = p.label + " " + p.note;
        for (char& c : l) c = char(std::tolower(static_cast<unsigned char>(c)));
        if (!low.empty() && l.find(low) == std::string::npos) continue;
        if (ry > top - 30 && ry < bottom && hover(ui, x, top, pw, bottom - top)) {
            if (button(r, ui, x + 14, ry, 300, 26, p.label)) chosen = pick_done_, value = p.value;
        } else if (ry > top - 30 && ry < bottom) {
            button(r, ui, x + 14, ry, 300, 26, p.label);
        }
        if (ry > top - 30 && ry < bottom) text(r, x + 324, ry + 8, fit_text(p.note, pw - 340), {180, 180, 195});
        ry += 30;
    }
    SDL_SetRenderClipRect(r, nullptr);
    if (chosen) {
        pick_done_ = {};
        ui.focus = -1;
        chosen(value);
    }
}

// ---------------------------------------------------------------- one setting of a skill
void Maker::field_row(const Field& f, float x, float& y, float w) {
    const float rowh = f.type == FT::Mask ? 56.f : f.type == FT::Text ? 40.f : 28.f;
    if (!in_view(y)) {
        y += rowh;
        return;
    }
    Ui& ui = win.ui;
    SDL_Renderer* r = win.ren();
    if (hover(ui, x, y, w, rowh) && f.tip && *f.tip) tip_ = tr(f.tip);
    const bool changed_here = get_kv(kv(), f.key) != nullptr;
    switch (f.type) {
        case FT::Int: {
            int v = f.get(sk());
            if (slider(r, ui, id_for(std::string(1, slot_) + f.key), x, y, w - 56, v, f.lo, f.hi, tr(f.label))) {
                if (f.step > 1) v = v / f.step * f.step;
                set_skill(f.key, std::to_string(v));
            }
            if (button(r, ui, x + w - 52, y, 24, 22, "-")) set_skill(f.key, std::to_string(std::clamp(f.get(sk()) - f.step, f.lo, f.hi)));
            if (button(r, ui, x + w - 26, y, 24, 22, "+")) set_skill(f.key, std::to_string(std::clamp(f.get(sk()) + f.step, f.lo, f.hi)));
            break;
        }
        case FT::Bool: {
            const bool on = f.get(sk()) != 0;
            if (button(r, ui, x, y, w, 24, std::format("{}: {}", tr(f.label), on ? tr("ON") : tr("OFF")), on)) set_skill(f.key, on ? "0" : "1");
            break;
        }
        case FT::Enum: {
            if (std::string_view(f.key) == "sub") {  // [U-534] every colour of fire by name; the element stays fire
                if (button(r, ui, x, y, w, 24, std::format("{}: {}", tr(f.label), tr(element_shown(sk().sub, sk().heat))))) {
                    std::vector<Pick> items;
                    for (const ElementChoice& c : element_choices()) items.push_back({tr(c.name), c.name, ""});
                    open_picker(tr(f.label), std::move(items), [this](const std::string& val) {
                        for (const ElementChoice& c : element_choices())
                            if (val == c.name) {
                                const bool fire = std::string_view(c.sub) == "Fire" || std::string_view(c.sub) == "BlueFire";
                                set_skill("sub", c.sub);
                                if (fire || sk().heat) set_skill("heat", std::string(kHeatNames[std::size_t(c.heat)]));
                            }
                    });
                }
                break;
            }
            const int v = std::clamp(f.get(sk()), 0, int(f.names.size()) - 1);
            if (button(r, ui, x, y, w, 24, std::format("{}: {}", tr(f.label), tr(f.names[std::size_t(v)])))) {
                std::vector<Pick> items;
                for (std::string_view n : f.names) items.push_back({tr(n), std::string(n), ""});
                const std::string key = f.key;
                open_picker(tr(f.label), std::move(items), [this, key](const std::string& val) { set_skill(key, val); });
            }
            break;
        }
        case FT::Text: {
            const std::string key = std::string(1, slot_) + f.key;
            auto [it, fresh] = typed_.try_emplace(key, f.get_text ? f.get_text(sk()) : "");
            if (ui.focus != id_for(key)) it->second = f.get_text ? f.get_text(sk()) : "";
            const std::string before = it->second;
            y += 12;
            text_field(r, ui, id_for(key), x, y, w, it->second, tr(f.label));
            if (it->second != before) {
                std::string v = it->second;
                if (std::string(f.key) == "name") std::erase_if(v, [](char c) { return !(std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '\'' || c == '_' || c == '-'); });
                set_skill(f.key, v);
            }
            y -= 12;
            break;
        }
        case FT::Mask: {
            text(r, x, y + 2, tr(f.label), {210, 210, 220});
            const int v = f.get(sk());
            float bx = x, by = y + 16;
            const float bw = std::min(92.f, (w - 8) / 6);
            for (std::size_t i = 0; i < f.names.size(); ++i) {
                if (bx + bw > x + w) bx = x, by += 20;
                const bool on = (v >> i) & 1;
                if (button(r, ui, bx, by, bw - 2, 18, tr(f.names[i]), on)) {
                    const int nv = v ^ (1 << i);
                    if (f.names_in_file) {
                        std::string list;
                        for (std::size_t j = 0; j < f.names.size(); ++j)
                            if ((nv >> j) & 1) list += (list.empty() ? "" : " ") + std::string(f.names[j]);
                        set_skill(f.key, list.empty() ? " " : list);  // (a space: the line stays and says "nothing")
                    } else {
                        set_skill(f.key, std::to_string(nv));
                    }
                }
                bx += bw;
            }
            break;
        }
        default: break;
    }
    if (changed_here) fill_rect(r, x - 5, y + 2, 2, rowh - 8, {255, 190, 90, 255});  // marked: differs from what it copied
    y += rowh;
}

// ---------------------------------------------------------------- the Shape page
void Maker::page_shape(float x, float& y, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    text(r, x, y, fit_text(tr("Hitbox shape: click one. The preview shows the exact hitbox."), w), {255, 220, 150});
    y += 18;
    const float cell = (w - 12) / 7;
    for (std::size_t i = 0; i < kGeoNames.size(); ++i) {
        const float bx = x + cell * float(i % 7), by = y + 62 * float(i / 7);
        const bool sel = sk().geo == Geo(i);
        if (button(r, ui, bx, by, cell - 4, 56, "", sel)) set_skill("geo", std::string(kGeoNames[i]));
        const float cx = bx + (cell - 4) / 2, cy = by + 24;
        View v;
        v.cx = 0, v.cy = 0, v.zoom = 0.09f, v.sw = cx * 2, v.sh = cy * 2;
        const Geo g = Geo(i);
        const Rgba c = sel ? Rgba{255, 210, 140, 255} : Rgba{200, 205, 225, 255};
        if (geo_is_poly(g)) {
            const SkillDef& d = sk();
            const FxKind k = g == Geo::Triangle ? FxKind::Burst : FxKind::Burst;
            outline_poly(r, v, fx_poly(k, 0, 0, 180, 240, 300, Dir{kOne, 0}, g, d.poly.data(), d.npoly), c, 1.5f);
        } else if (g == Geo::Auto) {
            text(r, cx - 10, cy - 8, "?", c, 2);
        } else {
            const Shape s = fx_shape(g == Geo::Cone || g == Geo::Arc || g == Geo::Line ? FxKind::Cone : FxKind::Burst, g == Geo::Cone || g == Geo::Arc || g == Geo::Line ? -170 : 0, 0, 170,
                                     g == Geo::Line ? 100 : 240, 340, Dir{kOne, 0}, Flow::Plain, 1, 0, std::uint8_t(g), g == Geo::Ring || g == Geo::Arc ? 80 : 0);
            outline_shape(r, v, s, c, 1.f);
        }
        const std::string lab = fit_text(tr(kGeoNames[i]), cell - 8);
        text(r, cx - text_w(lab) / 2, by + 44, lab, {225, 225, 235});
    }
    y += 62 * 2 + 6;
    auto num = [&](const char* key, const char* label, int v, int lo, int hi, const char* tip) {
        if (hover(ui, x, y, w, 26) && *tip) tip_ = tr(tip);
        if (slider(r, ui, id_for(std::string(1, slot_) + key), x, y, w - 56, v, lo, hi, tr(label))) set_skill(key, std::to_string(v));
        y += 28;
    };
    const Geo g = sk().geo;
    num("range", "Range / length", sk().range, 0, 3000, "How far it reaches or flies");
    num("width", "Width", sk().width, 0, 1500, "Width of the projectile, line, cone's far side");
    num("radius", "Radius / size", sk().radius, 0, 1200, "Size of blasts, fields and the polygon shapes");
    if (g == Geo::Ring || g == Geo::Arc) num("inner", "Hole in the middle", sk().inner, 0, 1000, "What is inside the hole is not hit");
    if (hover(ui, x, y, w, 26)) tip_ = tr("Hold the button in the preview and draw any shape: it becomes the exact hitbox of this skill");
    if (button(r, ui, x, y, w, 26, draw_mode_ == 2 ? tr("Now draw in the preview (click again to cancel)") : tr("Draw the shape with the mouse"), draw_mode_ == 2)) draw_mode_ = draw_mode_ == 2 ? 0 : 2;
    y += 32;
    if (g == Geo::Custom) {
        text(r, x, y + 6, trf("{} corners", sk().npoly / 2), {210, 210, 220});
        if (button(r, ui, x + 120, y, 240, 24, edit_corners_ ? tr("Click in the preview to add corners") : tr("Draw the corners in the preview"), edit_corners_)) edit_corners_ = !edit_corners_;
        if (button(r, ui, x + 370, y, 90, 24, tr("Clear"))) set_skill("poly", "");
        y += 30;
    } else {
        edit_corners_ = false;
    }
    y += 6;
    text(r, x, y, fit_text(tr("Sweet spot: more damage in one part of the hitbox"), w), {255, 180, 240});
    y += 18;
    num("sweet_pct", "Bonus damage %", sk().sweet_pct, 0, 300, "0 = no sweet spot");
    const SweetZone z = sk().sweet_zone;
    const char* const zones[] = {"Auto", "Centre", "Edge", "Tip", "Custom"};
    const float zw = (w - 8) / 5;
    for (int i = 0; i < 5; ++i)
        if (button(r, ui, x + zw * float(i), y, zw - 4, 24, tr(zones[i]), z == SweetZone(i))) set_skill("sweet_zone", std::string(kZoneNames[std::size_t(i)]));
    y += 30;
    num("sweet_size", "Its size % (0 = usual)", sk().sweet_size, 0, 100, "How much of the shape");
    if (z == SweetZone::Band) num("sweet_from", "Starts at %", sk().sweet_from, 0, 100, "Where the band begins, from the middle (or the caster)");
}

// ---------------------------------------------------------------- the time page
void Maker::page_time(float x, float& y, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    const SkillDef& d = sk();
    text(r, x, y, fit_text(tr("The hitbox over the skill's life. Between two keys it changes evenly."), w), {255, 220, 150});
    y += 20;
    int n = d.nkeys;
    auto write_key = [&](int i, int at, int size, int len, int rot, int fwd) {
        set_skill(std::format("key{}", i + 1), std::format("{} {} {} {} {}", at, size, len, rot, fwd));
    };
    for (int i = 0; i < n; ++i) {
        const SkillDef::HitKey k = d.keys[std::size_t(i)];
        const std::string base = std::format("{}key{}", slot_, i);
        text(r, x, y + 2, trf("Key {}", i + 1), {255, 200, 120});
        y += 16;
        int at = k.at, size = k.size, len = k.len, rot = k.rot, fwd = k.fwd;
        bool ch = false;
        ch |= slider(r, ui, id_for(base + "a"), x, y, w, at, 0, 100, tr("At % of its life"));
        y += 24;
        ch |= slider(r, ui, id_for(base + "s"), x, y, w, size, 0, 500, tr("Size %"));
        y += 24;
        ch |= slider(r, ui, id_for(base + "l"), x, y, w, len, 0, 500, tr("Length %"));
        y += 24;
        ch |= slider(r, ui, id_for(base + "r"), x, y, w, rot, -360, 360, tr("Turned (degrees)"));
        y += 24;
        ch |= slider(r, ui, id_for(base + "f"), x, y, w, fwd, -1000, 1000, tr("Moved along the aim"));
        y += 28;
        if (ch) write_key(i, at, size, len, rot, fwd);
    }
    if (n < 6 && button(r, ui, x, y, 160, 28, tr("Add a key"))) {
        if (n == 0) write_key(0, 0, 100, 100, 0, 0), write_key(1, 100, 100, 100, 0, 0);
        else write_key(n, 100, d.keys[std::size_t(n - 1)].size, d.keys[std::size_t(n - 1)].len, d.keys[std::size_t(n - 1)].rot, d.keys[std::size_t(n - 1)].fwd);
    }
    if (n > 0 && button(r, ui, x + 170, y, 170, 28, tr("Remove the last key"))) {
        const Draft before = d_;
        set_kv(kv(), std::format("key{}", n), "");
        if (n == 1) set_kv(kv(), "key_loop_ms", "");
        changed(before);
        // (a skill copied from another one keeps its keys: an empty draft line cannot remove those)
    }
    y += 36;
    int loop = d.key_loop_ms;
    if (hover(ui, x, y, w, 26)) tip_ = tr("Above 0: the keys repeat with this period (a pulsing or spinning hitbox)");
    if (slider(r, ui, id_for(std::string(1, slot_) + "loop"), x, y, w, loop, 0, 5000, tr("Repeats every (ms)"))) set_skill("key_loop_ms", std::to_string(loop / 50 * 50));
    y += 30;
    text(r, x, y, fit_text(tr("Quick:"), w), {210, 210, 220});
    const float qx = x + 60;
    if (button(r, ui, qx, y - 4, 110, 24, tr("Grows"))) write_key(0, 0, 40, 40, 0, 0), write_key(1, 100, 150, 150, 0, 0);
    if (button(r, ui, qx + 114, y - 4, 110, 24, tr("Shrinks"))) write_key(0, 0, 150, 150, 0, 0), write_key(1, 100, 40, 40, 0, 0);
    if (button(r, ui, qx + 228, y - 4, 110, 24, tr("Sweeps"))) write_key(0, 0, 100, 100, -60, 0), write_key(1, 100, 100, 100, 60, 0);
    if (button(r, ui, qx + 342, y - 4, 110, 24, tr("Pulses"))) {
        write_key(0, 0, 70, 70, 0, 0), write_key(1, 50, 130, 130, 0, 0), write_key(2, 100, 70, 70, 0, 0);
        set_skill("key_loop_ms", "800");
    }
    y += 30;
    text(r, x, y, fit_text(tr("Cast stages (a skill with more casts): set Casts on the Numbers page, each cast may have its own shape"), w), {150, 150, 170});
    y += 18;
    for (int st = 1; st <= 3 && st <= d.casts; ++st) {
        const SkillDef::CastStage& s = d.stages[std::size_t(st - 1)];
        text(r, x, y + 4, trf("Cast {}", st), {255, 200, 120});
        const std::string pre = std::format("stage{}.", st);
        if (button(r, ui, x + 70, y, 150, 24, std::format("{}: {}", tr("Type"), tr(kKindNames[std::size_t(s.kind)])))) {
            std::vector<Pick> items;
            for (std::string_view nm : kKindNames) items.push_back({tr(nm), std::string(nm), ""});
            open_picker(tr("Type"), std::move(items), [this, pre](const std::string& val) { set_skill(pre + "kind", val); });
        }
        if (button(r, ui, x + 226, y, 150, 24, std::format("{}: {}", tr("Shape"), tr(kGeoNames[std::size_t(s.geo)])))) {
            std::vector<Pick> items;
            for (std::string_view nm : kGeoNames) items.push_back({tr(nm), std::string(nm), ""});
            open_picker(tr("Shape"), std::move(items), [this, pre](const std::string& val) { set_skill(pre + "geo", val); });
        }
        y += 28;
        int dp = s.dmg_pct, rg = s.range, wd = s.width, rd = s.radius;
        const std::string base = std::format("{}st{}", slot_, st);
        if (slider(r, ui, id_for(base + "d"), x, y, w, dp, 0, 500, tr("Damage %"))) set_skill(pre + "dmg_pct", std::to_string(dp));
        y += 24;
        if (slider(r, ui, id_for(base + "g"), x, y, w, rg, 0, 3000, tr("Range (0 = the skill's)"))) set_skill(pre + "range", std::to_string(rg));
        y += 24;
        if (slider(r, ui, id_for(base + "w"), x, y, w, wd, 0, 1500, tr("Width (0 = the skill's)"))) set_skill(pre + "width", std::to_string(wd));
        y += 24;
        if (slider(r, ui, id_for(base + "r"), x, y, w, rd, 0, 1200, tr("Radius (0 = the skill's)"))) set_skill(pre + "radius", std::to_string(rd));
        y += 28;
    }
}

void Maker::file_row(const std::string& what, const std::string& label, bool picture, float x, float& y, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    if (button(r, ui, x, y, 240, 26, label)) ask_file(what, picture);
    const auto it = picked_.find(what);
    const std::string shown = it != picked_.end() ? fs::path(it->second).filename().string() : tr("(no file picked)");
    text(r, x + 250, y + 8, fit_text(shown, w - 330), it != picked_.end() ? Rgba{170, 240, 180} : Rgba{150, 150, 170});
    if (!picture && it != picked_.end()) {
        if (const auto p = picked_.find("play_" + what); p != picked_.end() && button(r, ui, x + w - 70, y, 70, 26, tr("Play"))) audio_.play(res_->sound(p->second), SoundCat::Effects);
    }
    y += 30;
}

// ---------------------------------------------------------------- look + sound
void Maker::page_look(float x, float& y, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    const SkillDef::Visual& v = sk().vis;
    auto num = [&](const char* key, const char* label, int val, int lo, int hi, const char* tip) {
        if (hover(ui, x, y, w, 26) && *tip) tip_ = tr(tip);
        if (slider(r, ui, id_for(std::string(1, slot_) + key), x, y, w - 56, val, lo, hi, tr(label))) set_skill(key, std::to_string(val));
        y += 28;
    };
    text(r, x, y, fit_text(tr("The colour and effects are drawn on the exact hitbox, so they always match its size."), w), {255, 220, 150});
    y += 20;
    num("vis.size", "How big it looks %", v.size_pct, 10, 400, "Like a skin for the hitbox: only the look changes, it hits with the real size. The developer view shows the real size");
    {   // colour: R G B
        int cr = int(v.colour >> 16 & 255), cg = int(v.colour >> 8 & 255), cb = int(v.colour & 255);
        bool ch = false;
        const std::string b(1, slot_);
        ch |= slider(r, ui, id_for(b + "cr"), x, y, w - 90, cr, 0, 255, tr("Colour: red"));
        fill_rect(r, x + w - 80, y, 70, 72, hex(v.colour ? v.colour : element_colour(base_element(sk().sub))));
        rect(r, x + w - 80, y, 70, 72, {200, 200, 210, 255});
        y += 24;
        ch |= slider(r, ui, id_for(b + "cg"), x, y, w - 90, cg, 0, 255, tr("Colour: green"));
        y += 24;
        ch |= slider(r, ui, id_for(b + "cb"), x, y, w - 90, cb, 0, 255, tr("Colour: blue"));
        y += 28;
        if (ch) set_skill("vis.colour", std::format("{:06x}", cr << 16 | cg << 8 | cb));
        if (button(r, ui, x, y, 200, 22, tr("The element's own colour"))) set_skill("vis.colour", "000000");
        y += 28;
    }
    num("vis.fill", "Fill strength (0 = usual)", v.fill, 0, 255, "");
    num("vis.glow", "Glow", v.glow, 0, 255, "A soft light around the shape");
    num("vis.trail", "Trail", v.trail, 0, 11, "A fading trail behind moving skills");
    num("vis.flash", "Flash at start", v.flash, 0, 1, "");
    num("vis.particles", "Sparks", v.particles, 0, 40, "");
    num("vis.mark_ms", "Mark on the ground (ms)", v.mark_ms, 0, 20000, "It stays this long after the skill ends");
    y += 6;
    text(r, x, y, fit_text(tr("A picture of the skill: it is always stretched to the hitbox"), w), {255, 220, 150});
    y += 18;
    const std::string pic = "picture_" + std::string(1, slot_);
    file_row(pic, tr("Pick a picture (PNG)"), true, x, y, w);
    num("vis.frames", "Frames side by side", v.frames, 1, 64, "How many pictures are in a row in the file");
    num("vis.fps", "Frames per second", v.fps, 1, 60, "");
    const char* const modes[] = {"Loops", "Plays once over its life", "Loops and grows with its life"};
    const float mw = (w - 8) / 3;
    for (int i = 0; i < 3; ++i)
        if (button(r, ui, x + mw * float(i), y, mw - 4, 24, tr(modes[i]), v.mode == i)) set_skill("vis.mode", std::to_string(i));
    y += 34;
    text(r, x, y, fit_text(tr("Sound when it is cast (WAV)"), w), {255, 220, 150});
    y += 18;
    file_row("sound_" + std::string(1, slot_), tr("Pick a sound"), false, x, y, w);
}

// ---------------------------------------------------------------- tab: the skills
void Maker::tab_skills(float x, float& y, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    const float cw = (w - 8) / float(kSlots.size());
    for (std::size_t i = 0; i < kSlots.size(); ++i) {
        const char s = kSlots[i];
        const std::string key(1, char(std::toupper(static_cast<unsigned char>(s))));
        const bool sel = slot_ == s;
        if (button(r, ui, x + cw * float(i), y, cw - 4, 44, "", sel)) slot_ = s, scroll_ = 0;
        text(r, x + cw * float(i) + 6, y + 6, key, {255, 220, 150}, 2);
        const std::string nm = fit_text(tr(slot_skill(cur_, s).name), cw - 16);
        text(r, x + cw * float(i) + 6, y + 28, nm, {225, 225, 235});
    }
    y += 50;
    if (slot_ == 'q' || slot_ == 'f' || slot_ == 'g') {
        text(r, x, y, fit_text(tr("Its own skill for this key: only this champion can pick it (empty = it uses the shared ones)."), w), {150, 150, 170});
        y += 18;
    }
    if (button(r, ui, x, y, 190, 26, tr("Start from a skill"))) {
        std::vector<Pick> items;
        for (const auto& [nm, ds] : skill_template_list()) items.push_back({std::string(nm), std::string(nm), tr(ds)});
        for (const ChampionDef& c : champions())
            for (const SkillDef* s : {&c.p, &c.w, &c.e, &c.r, &c.t, &c.y})
                if (!s->name.empty()) items.push_back({std::string(s->name), std::string(s->name), std::format("{} - {}", c.name, tr(s->desc))});
        for (const SkillDef& s : q_options()) items.push_back({std::string(s.name), std::string(s.name), std::format("Q - {}", tr(s.desc))});
        for (const SkillDef& s : specials()) items.push_back({std::string(s.name), std::string(s.name), tr(s.desc)});
        const char sl = slot_;
        open_picker(tr("Start from a skill (it can be changed after)"), std::move(items), [this, sl](const std::string& v) {
            const Draft before = d_;
            d_.copy[sl] = v;
            d_.skill[sl].clear();
            changed(before);
        });
    }
    if (button(r, ui, x + 196, y, 190, 26, tr("Put it on another key"))) {
        std::vector<Pick> items;
        for (char s : kSlots)
            if (s != slot_) items.push_back({std::string(1, char(std::toupper(static_cast<unsigned char>(s)))), std::string(1, s), std::string(slot_skill(cur_, s).name)});
        const char sl = slot_;
        open_picker(tr("Swap with the skill on this key"), std::move(items), [this, sl](const std::string& v) {
            const char other = v[0];
            const Draft before = d_;
            // both skills become explicit copies by name, then their changes move with them
            const std::string a(slot_skill(cur_, sl).name), b(slot_skill(cur_, other).name);
            Pairs ka = d_.skill[sl], kb = d_.skill[other];
            const auto ca = d_.copy.find(sl), cb = d_.copy.find(other);
            const std::string sa = ca != d_.copy.end() ? ca->second : std::string(slot_skill(champions()[std::size_t(std::max<std::ptrdiff_t>(0, std::ranges::find_if(champions(), [&](const ChampionDef& c) { return c.name == d_.base; }) - champions().begin()))], sl).name);
            const std::string sb = cb != d_.copy.end() ? cb->second : std::string(slot_skill(champions()[std::size_t(std::max<std::ptrdiff_t>(0, std::ranges::find_if(champions(), [&](const ChampionDef& c) { return c.name == d_.base; }) - champions().begin()))], other).name);
            d_.copy.erase(sl), d_.copy.erase(other);
            if (!sb.empty()) d_.copy[sl] = sb;
            if (!sa.empty()) d_.copy[other] = sa;
            d_.skill[sl] = kb, d_.skill[other] = ka;
            changed(before);
            slot_ = other;
        });
    }
    if (button(r, ui, x + 392, y, 120, 26, tr("Empty this key"))) {
        const Draft before = d_;
        d_.copy.erase(slot_);
        d_.skill[slot_].clear();
        set_kv(d_.skill[slot_], "kind", "None");
        set_kv(d_.skill[slot_], "name", "Empty");
        changed(before);
    }
    y += 34;
    const auto& pages = skill_pages();
    const float pw = (w - 4) / 5;
    for (std::size_t i = 0; i < pages.size(); ++i) {
        const float bx = x + pw * float(i % 5), by = y + 26 * float(i / 5);
        if (button(r, ui, bx, by, pw - 3, 24, tr(pages[i].title), page_ == int(i))) page_ = int(i), scroll_ = 0;
    }
    y += 26 * float((pages.size() + 4) / 5) + 6;
    // ---- what the page holds, scrolled
    clip_top_ = y;
    clip_bottom_ = win.h - 56;
    const SDL_Rect clip{int(x - 8), int(clip_top_), int(w + 16), int(clip_bottom_ - clip_top_)};
    SDL_SetRenderClipRect(r, &clip);
    if (hover(ui, x, clip_top_, w, clip_bottom_ - clip_top_)) scroll_ = std::max(0.f, scroll_ - ui.wheel * 44);
    float py = clip_top_ + 4 - scroll_;
    const bool inside = hover(ui, x - 8, clip_top_, w + 16, clip_bottom_ - clip_top_) || ui.drag >= 0 || ui.focus >= 0;
    const bool keep_click = ui.clicked;
    if (!inside) ui.clicked = false;
    const std::string title = pages[std::size_t(page_)].title;
    if (title == "Shape") page_shape(x, py, w);
    else if (title == "Change with time") page_time(x, py, w);
    else if (title == "Look & sound") page_look(x, py, w);
    else
        for (const Field& f : pages[std::size_t(page_)].fields) field_row(f, x, py, w);
    if (std::string(pages[std::size_t(page_)].title) == "Ranks & passive") {  // numbers the skill gives while learned
        py += 6;
        text(r, x, py, fit_text(tr("Stats it gives while it is learned"), w), {255, 220, 150});
        py += 18;
        for (std::size_t i = 0; i < sk().learned.size(); ++i) {
            const SkillDef::Learned& l = sk().learned[i];
            const std::string name = l.key == StatKey::Count ? tr("(none)") : std::string(kStatKeyNames[int(l.key)]);
            if (button(r, ui, x, py, 200, 24, name)) {
                std::vector<Pick> items;
                for (int k = 0; k < int(StatKey::Count); ++k) items.push_back({std::string(kStatKeyNames[k]), std::string(kStatKeyNames[k]), ""});
                open_picker(tr("Stat"), std::move(items), [this](const std::string& v) { set_skill("learned." + v, "100"); });
            }
            if (l.key != StatKey::Count) {
                int b = l.base;
                const std::string k(kStatKeyNames[int(l.key)]);
                if (slider(r, ui, id_for(std::string(1, slot_) + "ln" + k), x + 210, py, w - 210, b, -5000, 5000, tr("Value"))) set_skill("learned." + k, std::to_string(b));
            }
            py += 28;
        }
    }
    SDL_SetRenderClipRect(r, nullptr);
    if (!inside) ui.clicked = keep_click;
    y = clip_bottom_;
}

// ---------------------------------------------------------------- tab: the champion
void Maker::tab_champion(float x, float& y, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    clip_top_ = y;
    clip_bottom_ = win.h - 56;
    const SDL_Rect clip{int(x - 8), int(clip_top_), int(w + 16), int(clip_bottom_ - clip_top_)};
    SDL_SetRenderClipRect(r, &clip);
    if (hover(ui, x, clip_top_, w, clip_bottom_ - clip_top_)) scroll_ = std::max(0.f, scroll_ - ui.wheel * 44);
    float py = clip_top_ + 16 - scroll_;
    {
        const Draft before = d_;
        text_field(r, ui, 10, x, py, 300, d_.name, tr("Champion name"));
        if (d_.name != before.name) changed(before);
    }
    if (button(r, ui, x + 320, py, w - 320, 28, std::format("{}: {}", tr("Starts from"), d_.base))) {
        std::vector<Pick> items;
        for (int i = 0; i < builtin_champion_count(); ++i) {
            const ChampionDef& c = champions()[std::size_t(i)];
            items.push_back({std::string(c.name), std::string(c.name), std::format("{} - {}", tr(element_name(c.element)), tr(role_name(c.role)))});
        }
        open_picker(tr("Start from a champion: everything is copied, then you change what you want"), std::move(items), [this](const std::string& v) {
            const Draft before = d_;
            d_.base = v;
            changed(before);
            if (model_.from_pack || !model_.loaded) load_pack_model();
        });
    }
    py += 38;
    auto choose = [&](const char* key, const char* label, std::string_view shown, std::vector<std::string_view> values) {
        if (button(r, ui, x, py, w, 24, std::format("{}: {}", tr(label), tr(shown)))) {
            std::vector<Pick> items;
            for (std::string_view n : values) items.push_back({tr(n), std::string(n), ""});
            const std::string k = key;
            open_picker(tr(label), std::move(items), [this, k](const std::string& v) { set_champ(k, v); });
        }
        py += 28;
    };
    const std::vector<std::string_view> elems{"Water", "Earth", "Fire", "Air", "None"}, roles{"Mage", "Marksman", "Fighter", "Tank", "Assassin", "Support"},
        res{"Water", "Fuel", "None", "Ammo", "Energy", "Rage", "Mana"}, attrs{"None", "Strength", "Agility", "Intelligence", "Universal"}, ranges{"Melee", "Ranged"};
    choose("element", "Element", elems[std::size_t(cur_.element)], elems);
    if (button(r, ui, x, py, w, 24, std::format("{}: {}", tr("Autoattack element"), tr(element_shown(cur_.sub, cur_.auto_heat))))) {  // [U-534]
        std::vector<Pick> items;
        for (const ElementChoice& c : element_choices()) items.push_back({tr(c.name), c.name, ""});
        open_picker(tr("Autoattack element"), std::move(items), [this](const std::string& val) {
            for (const ElementChoice& c : element_choices())
                if (val == c.name) {
                    const Draft before = d_;
                    set_kv(d_.champ, "sub", c.sub);
                    set_kv(d_.champ, "auto_heat", std::to_string(c.heat));
                    changed(before);
                }
        });
    }
    py += 28;
    choose("role", "Role", roles[std::size_t(cur_.role)], roles);
    choose("range", "Range", ranges[cur_.range == Range::Ranged ? 1 : 0], ranges);
    choose("resource", "Resource", res[std::size_t(cur_.resource)], res);
    choose("attr", "Primary attribute (Dota)", attrs[std::min<std::size_t>(cur_.attr, 4)], attrs);
    if (hover(ui, x, py, w, 24)) tip_ = tr("Mages scale with elemental power, others with attack or ability power");
    if (button(r, ui, x, py, w / 2 - 4, 24, std::format("{}: {}", tr("Mage"), cur_.mage ? tr("ON") : tr("OFF")), cur_.mage)) set_champ("mage", cur_.mage ? "0" : "1");
    if (button(r, ui, x + w / 2, py, w / 2 - 4, 24, std::format("{}: {}", tr("Can crit"), cur_.crits ? tr("ON") : tr("OFF")), cur_.crits)) set_champ("crits", cur_.crits ? "0" : "1");
    py += 34;
    text(r, x, py, fit_text(tr("Stats"), w, 2), {255, 220, 150}, 2);
    py += 26;
    for (const ChampField& f : champ_fields()) {
        if (in_view(py)) {
            if (hover(ui, x, py, w, 26) && *f.tip) tip_ = tr(f.tip);
            int v = f.get(cur_);
            if (slider(r, ui, id_for(std::string("c") + f.key), x, py, w - 56, v, f.lo, f.hi, tr(f.label)))
                set_champ(f.key, f.dec ? std::format("{:.2f}", v / 100.0) : std::to_string(v));
            auto step = [&](int s) {
                const int nv = std::clamp(f.get(cur_) + s, f.lo, f.hi);
                set_champ(f.key, f.dec ? std::format("{:.2f}", nv / 100.0) : std::to_string(nv));
            };
            if (button(r, ui, x + w - 52, py, 24, 22, "-")) step(-f.step);
            if (button(r, ui, x + w - 26, py, 24, 22, "+")) step(f.step);
        }
        py += 26;
    }
    py += 8;
    text(r, x, py, fit_text(tr("Autoattack"), w, 2), {255, 220, 150}, 2);
    py += 26;
    {
        const std::vector<std::string_view> shapes{"Single", "Splash", "Line", "Chain"};
        choose("autos.shape", "Shape", shapes[std::size_t(cur_.autos.shape)], shapes);
        auto as = [&](const char* key, const char* label, int v, int lo, int hi) {
            if (in_view(py) && slider(r, ui, id_for(std::string("a") + key), x, py, w - 56, v, lo, hi, tr(label))) set_champ(key, std::to_string(v));
            py += 26;
        };
        const AutoAttackDef& a = cur_.autos;
        if (button(r, ui, x, py, w / 2 - 4, 24, std::format("{}: {}", tr("Attacks while walking"), a.move_while_attacking ? tr("ON") : tr("OFF")), a.move_while_attacking)) set_champ("autos.move_while_attacking", a.move_while_attacking ? "0" : "1");
        if (button(r, ui, x + w / 2, py, w / 2 - 4, 24, std::format("{}: {}", tr("Can hit walls"), a.hits_walls ? tr("ON") : tr("OFF")), a.hits_walls)) set_champ("autos.hits_walls", a.hits_walls ? "0" : "1");
        py += 28;
        as("autos.splash_radius", "Splash radius", a.splash_radius, 0, 1000);
        as("autos.splash_pct", "Splash %", a.splash_pct, 0, 100);
        as("autos.line_length", "Line length", a.line_length, 0, 2000);
        as("autos.chain_count", "Chain jumps", a.chain_count, 0, 10);
        as("autos.chain_range", "Chain range", a.chain_range, 0, 2000);
        as("autos.chain_pct", "Chain %", a.chain_pct, 0, 100);
        as("autos.wall_pct", "Damage to walls %", a.wall_pct, 0, 1000);
    }
    py += 8;
    {   // [U-535] its very own skills for these keys
        text(r, x, py, fit_text(tr("Its own Q, F and G: only this champion can pick them"), w, 2), {255, 220, 150}, 2);
        py += 26;
        const float kw = w / 3;
        for (int i = 0; i < 3; ++i) {
            const char sl = "qfg"[i];
            const SkillDef& own = slot_skill(cur_, sl);
            if (button(r, ui, x + kw * float(i), py, kw - 4, 26, fit_text(std::format("{}: {}", char(std::toupper(static_cast<unsigned char>(sl))), own.name.empty() ? tr("design it") : std::string(own.name)), kw - 12))) {
                tab_ = 1, slot_ = sl, page_ = 0, scroll_ = 0;
                SDL_SetRenderClipRect(r, nullptr);
                y = clip_bottom_;
                return;
            }
        }
        py += 34;
    }
    text(r, x, py, fit_text(tr("Q options (shared skills of the element, at most 6)"), w, 2), {255, 220, 150}, 2);
    py += 26;
    for (std::size_t i = 0; i < cur_.q_pool.size(); ++i) {
        const std::string shown = cur_.q_pool[i].empty() ? tr("(free)") : std::string(cur_.q_pool[i]);
        const float bx = x + (w / 3) * float(i % 3), by = py + 28 * float(i / 3);
        if (button(r, ui, bx, by, w / 3 - 4, 24, shown)) {
            std::vector<Pick> items;
            for (const SkillDef& s : q_options()) items.push_back({std::string(s.name), std::string(s.name), tr(s.desc)});
            const std::string k = std::format("q{}", i + 1);
            open_picker(tr("Q option"), std::move(items), [this, k](const std::string& v) { set_champ(k, v); });
        }
    }
    py += 66;
    text(r, x, py, fit_text(tr("Talents (feature Talents): levels 10, 15, 20, 25, left and right"), w, 2), {255, 220, 150}, 2);
    py += 26;
    for (int t = 0; t < 4; ++t)
        for (int s = 0; s < 2; ++s) {
            const SkillDef::Learned& l = talent_of(cur_, t, s);
            const float bx = x + (w / 2) * float(s);
            const std::string pre = std::format("talent{}.{}.", t + 1, s == 0 ? "left" : "right");
            const std::string shown = std::format("{}: {} {}", 10 + 5 * t, kStatKeyNames[int(l.key)], l.base);
            if (button(r, ui, bx, py, w / 2 - 4, 24, fit_text(shown, w / 2 - 12))) {
                std::vector<Pick> items;
                for (int k = 0; k < int(StatKey::Count); ++k) items.push_back({std::string(kStatKeyNames[k]), std::string(kStatKeyNames[k]), ""});
                open_picker(tr("Stat"), std::move(items), [this, pre](const std::string& v) {
                    const Draft before = d_;
                    std::erase_if(d_.champ, [&](const auto& kv) { return kv.first.starts_with(pre); });
                    d_.champ.push_back({pre + v, "1000"});
                    changed(before);
                });
            }
            if (s == 1) py += 28;
        }
    SDL_SetRenderClipRect(r, nullptr);
    y = clip_bottom_;
}

// ---------------------------------------------------------------- tab: model, hitbox, animations, sounds
void Maker::tab_model(float x, float& y, float w) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    clip_top_ = y;
    clip_bottom_ = win.h - 56;
    const SDL_Rect clip{int(x - 8), int(clip_top_), int(w + 16), int(clip_bottom_ - clip_top_)};
    SDL_SetRenderClipRect(r, &clip);
    if (hover(ui, x, clip_top_, w, clip_bottom_ - clip_top_)) scroll_ = std::max(0.f, scroll_ - ui.wheel * 44);
    float py = clip_top_ + 6 - scroll_;
    // ---- the base hitbox
    text(r, x, py, tr("Hitbox"), {255, 220, 150}, 2);
    py += 24;
    text(r, x, py, fit_text(tr("The circles the game hits. Shown on the model in the preview."), w), {170, 170, 190});
    py += 18;
    {   // [U-536] its size alone: the model and its animations stay as they are
        int hs = cur_.hitbox_pct;
        if (hover(ui, x, py, w, 26)) tip_ = tr("Only the hitbox changes: the model and its animations stay as they are");
        if (slider(r, ui, id_for("hitbox_size"), x, py, w, hs, 20, 300, tr("Hitbox size %"))) set_champ("hitbox_size", std::to_string(hs));
        py += 28;
        if (hover(ui, x, py, w, 28)) tip_ = tr("Hold the button in the preview and draw around the body: it becomes the circles of the hitbox");
        if (button(r, ui, x, py, w, 28, draw_mode_ == 1 ? tr("Now draw in the preview (click again to cancel)") : tr("Draw the hitbox with the mouse"), draw_mode_ == 1)) draw_mode_ = draw_mode_ == 1 ? 0 : 1;
        py += 34;
    }
    const char* const bodies[] = {"Humanoid", "Slim", "Broad", "Beast"};
    const float bw = (w - 8) / 4;
    for (int i = 0; i < 4; ++i)
        if (button(r, ui, x + bw * float(i), py, bw - 4, 24, tr(bodies[i]))) {
            const Draft before = d_;
            std::erase_if(d_.champ, [](const auto& kv) { return kv.first.starts_with("body"); });
            d_.champ.push_back({"body", bodies[i]});
            changed(before);
        }
    py += 30;
    const BodyDef body = *cur_.body;
    for (int i = 0; i < std::min<int>(body.n + 1, 6); ++i) {
        const BodyPart bp = i < body.n ? body.parts[std::size_t(i)] : BodyPart{0, 0, 16, 0};
        text(r, x, py + 2, trf("Circle {}", i + 1), {255, 200, 120});
        py += 16;
        int fw = bp.fwd, sd = bp.side, rd = bp.r, sw = bp.swing;
        const std::string b = std::format("body{}", i);
        bool ch = false;
        ch |= slider(r, ui, id_for(b + "f"), x, py, w, fw, -150, 150, tr("Forward"));
        py += 22;
        ch |= slider(r, ui, id_for(b + "s"), x, py, w, sd, -150, 150, tr("Sideways"));
        py += 22;
        ch |= slider(r, ui, id_for(b + "r"), x, py, w, rd, 2, 150, tr("Radius"));
        py += 22;
        ch |= slider(r, ui, id_for(b + "w"), x, py, w, sw, -30, 30, tr("Swing when walking"));
        py += 26;
        if (ch) {
            const Draft before = d_;
            std::erase_if(d_.champ, [](const auto& kv) { return kv.first == "body"; });
            // all circles written out, so the hitbox is exactly what is shown
            for (int j = 0; j < std::max<int>(body.n, i + 1); ++j) {
                const BodyPart o = j == i ? BodyPart{std::int16_t(fw), std::int16_t(sd), std::int16_t(rd), std::int16_t(sw)} : j < body.n ? body.parts[std::size_t(j)] : BodyPart{};
                set_kv(d_.champ, std::format("body.part{}", j + 1), std::format("{} {} {} {}", o.fwd, o.side, o.r, o.swing));
            }
            changed(before);
        }
    }
    py += 8;
    text(r, x, py, fit_text(tr("Model: a sprite sheet (PNG). With a sheet.txt beside it, its animations come along."), w), {255, 220, 150});
    py += 20;
    if (button(r, ui, x, py, 240, 28, tr("Load a model file"))) ask_file("model", true);
    if (button(r, ui, x + 250, py, 200, 28, tr("Use what it has already"))) load_pack_model();
    py += 32;
    if (hover(ui, x, py, w, 28)) tip_ = tr("A full-body picture or a 3D file (.glb .gltf .fbx .obj): Blender renders all 8 directions and the animations");
    if (button(r, ui, x, py, 450, 28, build_proc_ ? tr("Blender is building the model...") : tr("Build a model from a picture or a 3D file"), build_proc_ != nullptr)) ask_file("build", true);
    py += 44;
    text_field(r, ui, 11, x, py, w - 90, path_text_, tr("or paste a file path (or drop a file on this window)"));
    if (button(r, ui, x + w - 80, py, 80, 28, tr("Load"))) got_file("model", path_text_);
    py += 38;
    if (model_.loaded) {
        SpriteSheet& s = model_.view;
        text(r, x, py, model_.from_pack ? tr("Showing the model it already has (or its base's)") : trf("Loaded: {}", fs::path(model_.png).filename().string()), {170, 240, 180});
        py += 18;
        auto ival = [&](const char* id, const char* label, int& v, int lo, int hi) {
            if (slider(r, ui, id_for(std::string("m") + id), x, py, w, v, lo, hi, tr(label))) dirty_ = true;
            py += 24;
        };
        auto fval = [&](const char* id, const char* label, float& v, float scale, int lo, int hi) {
            int iv = int(std::lround(v * scale));
            if (slider(r, ui, id_for(std::string("m") + id), x, py, w, iv, lo, hi, tr(label))) v = float(iv) / scale, dirty_ = true;
            py += 24;
        };
        // [U-536] special settings: they can put a model out of step with its hitbox or break its animations, so they are closed at first
        if (hover(ui, x, py, w, 26)) tip_ = tr("Only if the model should look bigger or smaller than its hitbox");
        if (button(r, ui, x, py, w, 24, std::format("{}: {}", tr("Special: change the size of the model"), model_special_ ? tr("ON") : tr("OFF")), model_special_)) model_special_ = !model_special_;
        py += 28;
        if (model_special_) {
            int ms = cur_.model_scale;
            if (slider(r, ui, id_for("model_scale"), x, py, w, ms, 20, 300, tr("Size of the model %"))) set_champ("model_scale", std::to_string(ms));
            py += 26;
            fval("ax", "Feet: left - right %", s.ax, 100.f, 0, 100);
            fval("ay", "Feet: top - bottom %", s.ay, 100.f, 0, 100);
            text(r, x, py, fit_text(tr("Line the feet up with the hitbox in the preview: they belong together."), w), {150, 150, 170});
            py += 22;
        }
        if (hover(ui, x, py, w, 26)) tip_ = tr("Only for a sheet you made yourself: wrong numbers here break the animations");
        if (button(r, ui, x, py, w, 24, std::format("{}: {}", tr("Special: how the picture file is read"), sheet_special_ ? tr("ON") : tr("OFF")), sheet_special_)) sheet_special_ = !sheet_special_;
        py += 28;
        if (sheet_special_) {
            ival("px", "Frame size (pixels)", s.px, 16, 1024);
            ival("dirs", "Rows (directions)", s.dirs, 1, 8);
            ival("wrap", "Frames per row (0 = one long row)", s.wrap, 0, 64);
            fval("or", "Model size in the world", s.ortho_m, 100.f, 20, 1200);
        }
        text(r, x, py, fit_text(tr("Animations in the file"), w, 2), {255, 220, 150}, 2);
        py += 26;
        std::string remove;
        for (auto& [k, a] : s.anims) {
            if (in_view(py)) {
                if (button(r, ui, x, py, 130, 22, k, anim_key_ == k)) anim_key_ = k;
                int col = a.col, n = a.n, fps = a.fps;
                const std::string b = "an" + k;
                const float third = (w - 170) / 3;
                auto small = [&](const char* id, float sx, int& v, int lo, int hi, const char* lab) {
                    text(r, sx, py + 6, tr(lab), {200, 200, 210});
                    const float bar = sx + 46, bl = third - 84;  // label, bar, number: each has its own place
                    if (ui.clicked && hover(ui, bar, py, bl, 22)) ui.drag = id_for(b + id), ui.clicked = false;
                    if (!ui.down && ui.drag == id_for(b + id)) ui.drag = -1;
                    if (ui.drag == id_for(b + id)) v = lo + int(std::clamp((ui.mx - bar) / bl, 0.f, 1.f) * float(hi - lo)), dirty_ = true;
                    fill_rect(r, bar, py + 9, bl, 4, {60, 60, 80, 255});
                    fill_rect(r, bar + bl * float(v - lo) / float(std::max(1, hi - lo)) - 3, py + 3, 6, 16, {240, 240, 240, 255});
                    text(r, bar + bl + 6, py + 6, std::to_string(v), {255, 255, 255});
                };
                small("c", x + 136, col, 0, 256, "start");
                small("n", x + 136 + third, n, 1, 128, "count");
                small("f", x + 136 + 2 * third, fps, 1, 60, "fps");
                a.col = col, a.n = n, a.fps = fps;
                if (button(r, ui, x + w - 24, py, 22, 22, "x")) remove = k;
            }
            py += 26;
        }
        if (!remove.empty()) s.anims.erase(remove), dirty_ = true;
        if (button(r, ui, x, py, 200, 24, tr("Add an animation"))) {
            std::vector<Pick> items;
            for (std::string_view k : kAnimKeys) items.push_back({std::string(k), std::string(k), ""});
            for (char sl : std::string_view("qwerty")) items.push_back({std::format("cast_{}", sl), std::format("cast_{}", sl), ""});
            for (std::string_view k : kEmoteNames) items.push_back({std::format("emote_{}", k), std::format("emote_{}", k), ""});
            open_picker(tr("Which animation"), std::move(items), [this](const std::string& v) {
                model_.view.anims[v] = {0, 1, 8};
                anim_key_ = v;
                dirty_ = true;
            });
        }
        py += 34;
        text(r, x, py, fit_text(tr("What each action plays (empty = the usual fallback)"), w, 2), {255, 220, 150}, 2);
        py += 26;
        std::vector<std::string> actions(std::begin(kAnimKeys), std::end(kAnimKeys));
        for (char sl : std::string_view("qwerty")) actions.push_back(std::format("cast_{}", sl));
        const float cw = (w - 4) / 2;
        int col = 0;
        for (const std::string& act : actions) {
            if (in_view(py)) {
                const auto u = s.use.find(act);
                const SpriteSheet::Anim* a = s.find(act);
                std::string plays = u != s.use.end() ? u->second : s.anims.contains(act) ? act : tr("fallback");
                if (button(r, ui, x + cw * float(col), py, cw - 4, 22, fit_text(std::format("{} > {}", act, plays), cw - 12), anim_key_ == act)) {
                    anim_key_ = act;
                    std::vector<Pick> items{{tr("(usual fallback)"), "", ""}};
                    for (const auto& [k, an] : s.anims) items.push_back({k, k, std::format("{} {}", an.n, tr("frames"))});
                    open_picker(trf("{} plays", act), std::move(items), [this, act](const std::string& v) {
                        if (v.empty()) model_.view.use.erase(act);
                        else model_.view.use[act] = v;
                        dirty_ = true;
                    });
                }
                (void)a;
            }
            if (++col == 2) col = 0, py += 26;
        }
        if (col) py += 26;
        py += 8;
    }
    // ---- pictures and sounds of the champion
    text(r, x, py, fit_text(tr("Pictures and sounds"), w, 2), {255, 220, 150}, 2);
    py += 26;
    file_row("icon", tr("Icon (PNG)"), true, x, py, w);
    file_row("splash", tr("Big picture (PNG)"), true, x, py, w);
    file_row("sound_death", tr("Death sound (WAV)"), false, x, py, w);
    for (char s : kSlots) file_row("sound_" + std::string(1, s), trf("Sound of {} (WAV)", char(std::toupper(static_cast<unsigned char>(s)))), false, x, py, w);
    SDL_SetRenderClipRect(r, nullptr);
    y = clip_bottom_;
}

// ---------------------------------------------------------------- [U-536] hitboxes drawn with the mouse
namespace {
[[nodiscard]] double dist_to_segment(double px, double py, double ax, double ay, double bx, double by) {
    const double dx = bx - ax, dy = by - ay, l = dx * dx + dy * dy;
    const double t = l > 0 ? std::clamp(((px - ax) * dx + (py - ay) * dy) / l, 0.0, 1.0) : 0.0;
    return std::hypot(px - ax - t * dx, py - ay - t * dy);
}
[[nodiscard]] bool inside_outline(const std::vector<std::pair<double, double>>& p, double x, double y) {
    bool in = false;
    for (std::size_t i = 0, j = p.size() - 1; i < p.size(); j = i++)
        if ((p[i].second > y) != (p[j].second > y) && x < (p[j].first - p[i].first) * (y - p[i].second) / (p[j].second - p[i].second) + p[i].first) in = !in;
    return in;
}
// keeps the points that matter (Douglas-Peucker)
void simplify(const std::vector<std::pair<double, double>>& p, std::size_t a, std::size_t b, double eps, std::vector<bool>& keep) {
    if (b <= a + 1) return;
    double best = -1;
    std::size_t at = a;
    for (std::size_t i = a + 1; i < b; ++i) {
        const double d = dist_to_segment(p[i].first, p[i].second, p[a].first, p[a].second, p[b].first, p[b].second);
        if (d > best) best = d, at = i;
    }
    if (best > eps) {
        keep[at] = true;
        simplify(p, a, at, eps, keep);
        simplify(p, at, b, eps, keep);
    }
}
}  // namespace

// The outline becomes the circles the game tests: the biggest circle that fits, then the biggest in what is left (at most 6).
void Maker::finish_body_drawing(Dir f, float cx, float cy, int scale_pct) {
    draw_mode_ = 0;
    if (stroke_.size() < 6) return status_ = tr("Too small: hold the button and draw around the body."), void();
    const double fx = double(f.x) / kOne, fy = double(f.y) / kOne, sc = 100.0 / std::max(1, scale_pct);
    std::vector<std::pair<double, double>> o;
    for (const auto& [wx, wy] : stroke_) {
        const double dx = double(wx) - cx, dy = double(wy) - cy;
        o.push_back({(dx * fx + dy * fy) * sc, (-dx * fy + dy * fx) * sc});  // forward, sideways
    }
    double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
    for (const auto& [a, b] : o) x0 = std::min(x0, a), x1 = std::max(x1, a), y0 = std::min(y0, b), y1 = std::max(y1, b);
    const double step = std::max(1.0, std::max(x1 - x0, y1 - y0) / 80);
    struct P { double x, y, edge; bool covered; };
    std::vector<P> pts;
    for (double yy = y0; yy <= y1; yy += step)
        for (double xx = x0; xx <= x1; xx += step)
            if (inside_outline(o, xx, yy)) {
                double e = 1e9;
                for (std::size_t i = 0, j = o.size() - 1; i < o.size(); j = i++) e = std::min(e, dist_to_segment(xx, yy, o[i].first, o[i].second, o[j].first, o[j].second));
                pts.push_back({xx, yy, e, false});
            }
    std::vector<BodyPart> parts;
    while (parts.size() < 6) {
        const P* best = nullptr;
        for (const P& p : pts)
            if (!p.covered && p.edge >= 2 && (!best || p.edge > best->edge)) best = &p;
        if (!best) break;
        const double r0 = std::min(best->edge, 150.0), bx = best->x, by = best->y;
        parts.push_back({std::int16_t(std::clamp(std::lround(bx), -300L, 300L)), std::int16_t(std::clamp(std::lround(by), -300L, 300L)), std::int16_t(std::max(2L, std::lround(r0))), 0});
        for (P& p : pts)
            if (std::hypot(p.x - bx, p.y - by) <= r0) p.covered = true;
    }
    if (parts.empty()) return status_ = tr("Too small: hold the button and draw around the body."), void();
    const Draft before = d_;
    std::erase_if(d_.champ, [](const auto& kv) { return kv.first.starts_with("body"); });
    for (std::size_t i = 0; i < parts.size(); ++i) set_kv(d_.champ, std::format("body.part{}", i + 1), std::format("{} {} {} 0", parts[i].fwd, parts[i].side, parts[i].r));
    changed(before);
    status_ = trf("The hitbox is now {} circles", parts.size());
    stroke_.clear();
}

// The outline becomes the corners of the skill's own shape, and its size.
void Maker::finish_skill_drawing(Dir f, float cx, float cy) {
    draw_mode_ = 0;
    if (stroke_.size() < 6) return status_ = tr("Too small: hold the button and draw the shape."), void();
    const double fx = double(f.x) / kOne, fy = double(f.y) / kOne;
    std::vector<std::pair<double, double>> o;
    double ext = 1;
    for (const auto& [wx, wy] : stroke_) {
        const double dx = double(wx) - cx, dy = double(wy) - cy;
        o.push_back({dx * fx + dy * fy, -dx * fy + dy * fx});
        ext = std::max(ext, std::max(std::abs(o.back().first), std::abs(o.back().second)));
    }
    std::vector<bool> keep;
    for (double eps = ext / 200;; eps *= 1.4) {
        keep.assign(o.size(), false);
        keep.front() = keep.back() = true;
        simplify(o, 0, o.size() - 1, eps, keep);
        if (std::ranges::count(keep, true) <= 24) break;
    }
    std::string list;
    const int radius = int(std::ceil(ext));
    for (std::size_t i = 0; i < o.size(); ++i)
        if (keep[i]) list += std::format("{},{} ", std::clamp(int(std::lround(o[i].first * 100 / radius)), -127, 127), std::clamp(int(std::lround(o[i].second * 100 / radius)), -127, 127));
    const Draft before = d_;
    set_kv(kv(), "geo", "Custom");
    set_kv(kv(), "poly", list);
    set_kv(kv(), "radius", std::to_string(radius));
    set_kv(kv(), "width", std::to_string(radius * 2));
    changed(before);
    stroke_.clear();
}

// ---------------------------------------------------------------- the preview
void Maker::preview(float x, float y, float w, float h) {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    const SDL_Rect clip{int(x), int(y), int(w), int(h)};
    fill_rect(r, x, y, w, h, {34, 40, 34, 255});
    SDL_SetRenderClipRect(r, &clip);
    const float cwx = 4000, cwy = 4000;
    View v;
    float& zoom_ = zooms_[std::clamp(tab_, 0, 2)];
    v.cx = cwx, v.cy = cwy, v.zoom = zoom_;
    v.sw = (x + w / 2) * 2, v.sh = (y + h * 0.55f) * 2;
    v.gy = float(tilt_) / 100.f, v.hz = tilt_ >= 100 ? 0.f : 0.7f;  // [U-531]
    for (int i = -40; i <= 40; ++i) {  // the ground: a cell grid
        const float a = cwx + float(i) * 96;
        line(r, v.sx(a), v.sy(cwy - 3840), v.sx(a), v.sy(cwy + 3840), {60, 68, 60, 255});
        line(r, v.sx(cwx - 3840), v.sy(a), v.sx(cwx + 3840), v.sy(a), {60, 68, 60, 255});
    }
    const bool over = hover(ui, x, y, w, h) && pick_done_ == nullptr;
    if (over) zoom_ = std::clamp(zoom_ * (1.f + ui.wheel * 0.08f), 0.15f, 4.f);
    const double dt = std::clamp(win.now_s - last_t_, 0.0, 0.1);
    last_t_ = win.now_s;
    const SkillDef& d = sk();
    const bool show_skill = tab_ == 1;
    // ---- the aim
    if (over && show_skill && (ui.down || ui.clicked) && !edit_corners_ && !draw_mode_) aim_x_ = v.wx(ui.mx), aim_y_ = v.wy(ui.my);
    const std::int32_t ux = std::int32_t(cwx), uy = std::int32_t(cwy);
    // [U-531] away from the skills the model faces where you turned it, and the hitbox turns with it
    const double dir_a = double(dir_) * 3.14159265358979 / 4;
    const Dir aim = show_skill ? dir_of(std::int64_t(aim_x_) - ux, std::int64_t(aim_y_) - uy)
                               : dir_of(std::int64_t(std::lround(std::cos(dir_a) * 1000)), std::int64_t(std::lround(std::sin(dir_a) * 1000)));
    // ---- the model and its hitbox [U-536] two numbers: the hitbox size does not touch the model
    const int both_pct = std::max(10, 100 + cur_.model_size / 100), hit_pct = std::max(10, both_pct * cur_.hitbox_pct / 100);
    const Shape body = place_body(*cur_.body, ux, uy, aim, 0, hit_pct);
    if (!show_skill && !draw_mode_ && over && pick_done_ == nullptr) {  // [U-531] drag sideways: the model turns
        if (ui.clicked && ui.my > y + 64) turn_mx_ = ui.mx, turn_from_ = dir_, ui.clicked = false;
        if (turn_mx_ >= 0 && ui.down) dir_ = ((turn_from_ + int((ui.mx - turn_mx_) / 36)) % 8 + 8) % 8;
    }
    if (!ui.down) turn_mx_ = -1;
    if (draw_mode_ == 1) {  // [U-536] draw the outline of the body
        if (over && ui.clicked && ui.my > y + 64) stroking_ = true, stroke_.clear(), ui.clicked = false;
        if (stroking_) {
            const std::pair<float, float> p{v.wx(ui.mx), v.wy(ui.my)};
            if (stroke_.empty() || std::hypot(p.first - stroke_.back().first, p.second - stroke_.back().second) > 2.f) stroke_.push_back(p);
            if (!ui.down) {
                stroking_ = false;
                finish_body_drawing(aim, cwx, cwy, hit_pct);
            }
        }
    }
    {
        fill_shape(r, v, body, {255, 255, 255, 40});  // [U-522] one clean shape
        outline_shape(r, v, body, {255, 230, 80, 255}, 1.5f);
    }
    if (model_.loaded) {
        const int dir = tab_ == 2 ? dir_ : sprite_dir(float(aim.x), float(aim.y), false);
        Sprites::draw(r, model_.view, anim_key_, win.now_s, dir, v.sx(cwx), v.sy(cwy), v.zoom * float(both_pct * cur_.model_scale / 100) / 100.f);
    }
    // ---- the chosen skill over its life
    if (show_skill) {
        float life_s = 0.5f;
        const float range = float(d.range), speed = float(std::max(1, d.speed));
        FxKind fk = FxKind::Burst;
        switch (d.kind) {
            case SkillKind::Bolt: case SkillKind::Dash: fk = FxKind::Bolt, life_s = range / speed; break;
            case SkillKind::Wave: fk = FxKind::Wave, life_s = range / speed; break;
            case SkillKind::Tornado: fk = FxKind::Tornado, life_s = std::max(0.5f, float(d.dur_ms) / 1000.f); break;
            case SkillKind::Cone: fk = FxKind::Cone, life_s = d.nkeys && d.dur_ms > 0 ? float(d.dur_ms) / 1000.f : 0.25f; break;
            case SkillKind::Field: case SkillKind::Heal: fk = FxKind::Field, life_s = std::max(0.3f, float(d.dur_ms) / 1000.f); break;
            default: fk = FxKind::Burst, life_s = float(d.delay_ms + 300) / 1000.f; break;
        }
        life_s = std::max(0.1f, life_s) + 0.001f;
        if (playing_) {
            life_ += float(dt) / life_s * float(speed_) / 100.f;
            if (life_ > 1.25f) life_ = 0;  // a short rest between the plays
        }
        const float q = std::clamp(life_, 0.f, 1.f);
        const HitPose pose = d.nkeys ? hit_pose(d, d.key_loop_ms > 0 ? std::int32_t(std::fmod(q * life_s * 1000.f, float(d.key_loop_ms)) * 1000.f / float(d.key_loop_ms)) : std::int32_t(q * 1000)) : HitPose{};
        const bool moves = fk == FxKind::Bolt || fk == FxKind::Wave;
        const bool caster = from_caster(fk, d.geo);
        const float dist = std::hypot(aim_x_ - float(ux), aim_y_ - float(uy));
        const float clamped = range > 0 ? std::min(dist, range) : 0;
        const float afx = float(aim.x) / kOne, afy = float(aim.y) / kOne;
        // where the shape is, as in the engine
        float sx = float(ux), sy = float(uy);
        if (moves) sx += afx * range * q, sy += afy * range * q;
        else if (!caster) sx += afx * clamped, sy += afy * clamped;
        sx += afx * float(pose.fwd), sy += afy * float(pose.fwd);
        std::int32_t radius = moves ? std::max(12, d.width / 2) : d.radius;
        radius = std::max<std::int32_t>(1, radius * pose.size / 100);
        const std::int32_t width = d.width * pose.size / 100;
        const std::int32_t length = (fk == FxKind::Cone || caster ? d.range : d.range) * pose.len / 100;
        Dir sd = aim;
        if (pose.rot) {
            const Dir t = kDirs[std::size_t((pose.rot * 64 / 360 % 64 + 64) % 64)];
            sd = Dir{std::int32_t((std::int64_t(aim.x) * t.x - std::int64_t(aim.y) * t.y) / kOne), std::int32_t((std::int64_t(aim.x) * t.y + std::int64_t(aim.y) * t.x) / kOne)};
        }
        const Rgba col = d.vis.colour ? hex(d.vis.colour) : hex(element_colour(base_element(d.sub)));
        const std::uint8_t alpha = d.vis.fill ? d.vis.fill : fk == FxKind::Field ? 70 : 130;
        const float tick_q = float(win.now_s * 128);
        const Shape s = fx_shape(fk, std::int32_t(sx), std::int32_t(sy), radius, width, length, sd, flow_of(d.sub), 7, std::uint32_t(tick_q), std::uint8_t(d.geo), d.inner);
        const bool poly = geo_is_poly(d.geo);
        const Poly pl = poly ? fx_poly(fk, std::int32_t(sx), std::int32_t(sy), radius, width, length, sd, d.geo, d.poly.data(), d.npoly) : Poly{};
        // range
        if (range > 0) ground_ring(r, v, float(ux), float(uy), range, {255, 255, 255, 60}, 96);
        const bool armed = fk != FxKind::Burst || q * life_s * 1000.f >= float(d.delay_ms);
        // its picture: always the size of the hitbox
        bool pictured = false;
        const auto shown = picked_.find("show_picture_" + std::string(1, slot_));
        const std::string pic_rel = shown != picked_.end() ? shown->second : std::string(d.vis.sheet);
        if (!pic_rel.empty())
            if (SDL_Texture* pic = res_->texture("skills/" + pic_rel)) {
                const float grow = d.vis.mode == 2 ? q : 1.f;
                const float hf = (caster ? float(length) / 2 : float(radius)) * grow, hs = (width > 0 ? float(width) / 2 : float(radius)) * grow;
                const float off = caster ? float(length) / 2 : 0.f;
                const float ddx = float(sd.x) / kOne, ddy = float(sd.y) / kOne;
                const int frame = d.vis.mode == 1 ? int(q * float(d.vis.frames)) : int(win.now_s * double(d.vis.fps));
                ground_picture(r, v, pic, sx + ddx * off, sy + ddy * off, ddx, ddy, hf, hs, frame, d.vis.frames);
                pictured = true;
            }
        if (life_ <= 1.f) {
            if (poly) {
                if (armed && !pictured) fill_poly(r, v, pl, with_alpha(col, alpha));
                outline_poly(r, v, pl, mixc(col, {255, 255, 255, 255}, 40), 2);
            } else {
                if (armed && !pictured) fill_shape(r, v, s, with_alpha(col, alpha));
                outline_shape(r, v, s, mixc(col, {255, 255, 255, 255}, 40), 2);
            }
            if (d.inner > 0 && (d.geo == Geo::Ring || d.geo == Geo::Arc)) ground_ring(r, v, sx, sy, float(d.inner), {0, 0, 0, 160}, 40);
            if (d.sweet_pct > 0) {
                const SweetZone z = sweet_zone_for(fk, d.geo, d.sweet_zone);
                const Shape sw = sweet_shape(fk, d.geo, z, d.sweet_size, std::int32_t(sx), std::int32_t(sy), radius, width, length, sd, d.sweet_from);
                if (!sw.empty()) {
                    fill_shape(r, v, sw, {255, 80, 220, 70});
                    outline_shape(r, v, sw, {255, 120, 230, 230}, 1.5f);
                } else if (z == SweetZone::Tip) {
                    const float tip_from = range * float(100 - sweet_size_for(z, d.sweet_size)) / 100.f;
                    line(r, v.sx(float(ux) + afx * tip_from), v.sy(float(uy) + afy * tip_from), v.sx(float(ux) + afx * range), v.sy(float(uy) + afy * range), {255, 120, 230, 255});
                }
            }
        }
        // the test dummies: red when the hitbox touches them now, as in the game
        for (int i = 0; i < 5; ++i) {
            const Dir dd = kDirs[std::size_t(i * 13 + 4) & 63];
            const float rr = 240.f + float(i) * 110.f;
            const std::int32_t tx = ux + std::int32_t(float(dd.x) / kOne * rr), ty = uy + std::int32_t(float(dd.y) / kOne * rr);
            const Circle c{tx, ty, 30};
            const bool hit = life_ <= 1.f && armed && (poly ? touches(pl, c) : touches(s, c));
            ground_fill(r, v, float(tx), float(ty), 30, hit ? Rgba{255, 70, 60, 200} : Rgba{120, 120, 135, 170}, 16);
            ground_ring(r, v, float(tx), float(ty), 30, {220, 220, 230, 200}, 16);
        }
        if (const int look = d.vis.size_pct; look != 100) {  // [U-538] how big it looks, around the real hitbox
            const Shape ls = fx_shape(fk, std::int32_t(sx), std::int32_t(sy), radius * look / 100, width * look / 100, length * look / 100, sd, flow_of(d.sub), 7, std::uint32_t(tick_q), std::uint8_t(d.geo), d.inner * look / 100);
            if (!poly) outline_shape(r, v, ls, {255, 255, 255, 120}, 1.f);
        }
        if (draw_mode_ == 2) {  // [U-536] draw the shape of the skill
            if (over && ui.clicked && ui.my > y + 64) stroking_ = true, stroke_.clear(), ui.clicked = false;
            if (stroking_) {
                const std::pair<float, float> p{v.wx(ui.mx), v.wy(ui.my)};
                if (stroke_.empty() || std::hypot(p.first - stroke_.back().first, p.second - stroke_.back().second) > 2.f) stroke_.push_back(p);
                if (!ui.down) {
                    stroking_ = false;
                    finish_skill_drawing(sd, sx, sy);
                }
            }
        }
        // corners being drawn
        if (edit_corners_ && d.geo == Geo::Custom) {
            for (int i = 0; i + 1 < d.npoly; i += 2) {
                const float px = sx + (afx * float(d.poly[std::size_t(i)]) - afy * float(d.poly[std::size_t(i + 1)])) * float(radius) / 100.f;
                const float py = sy + (afy * float(d.poly[std::size_t(i)]) + afx * float(d.poly[std::size_t(i + 1)])) * float(radius) / 100.f;
                fill_circle(r, v.sx(px), v.sy(py), 5, {255, 230, 120, 255}, 10);
            }
            if (over && (ui.clicked || ui.rclicked) && radius > 0) {
                ui.clicked = false;
                std::string list;
                const int have = ui.rclicked ? std::max(0, d.npoly - 2) : d.npoly;
                for (int i = 0; i + 1 < have; i += 2) list += std::format("{},{} ", d.poly[std::size_t(i)], d.poly[std::size_t(i + 1)]);
                if (!ui.rclicked && d.npoly < 48) {
                    const float mx = v.wx(ui.mx) - sx, my = v.wy(ui.my) - sy;
                    const int fw = std::clamp(int((mx * afx + my * afy) * 100.f / float(radius)), -127, 127);
                    const int sd2 = std::clamp(int((-mx * afy + my * afx) * 100.f / float(radius)), -127, 127);
                    list += std::format("{},{}", fw, sd2);
                }
                set_skill("poly", list);
                ui.rclicked = false;
            }
        }
        // ---- the timeline
        SDL_SetRenderClipRect(r, nullptr);
        const float ty = y + h - 40;
        fill_rect(r, x, ty - 6, w, 46, {22, 24, 32, 240});
        if (button(r, ui, x + 8, ty, 70, 28, playing_ ? tr("Pause") : tr("Play"), playing_)) playing_ = !playing_;
        const float bx = x + 90, bw = w - 330;
        fill_rect(r, bx, ty + 12, bw, 4, {70, 70, 90, 255});
        fill_rect(r, bx, ty + 12, bw * q, 4, {255, 190, 90, 255});
        for (int i = 0; i < d.nkeys; ++i) fill_rect(r, bx + bw * float(d.keys[std::size_t(i)].at) / 100.f - 2, ty + 6, 4, 16, {255, 240, 170, 255});
        fill_rect(r, bx + bw * q - 4, ty + 4, 8, 20, {240, 240, 240, 255});
        if (ui.clicked && hover(ui, bx - 6, ty, bw + 12, 28)) ui.drag = 9001, ui.clicked = false;
        if (!ui.down && ui.drag == 9001) ui.drag = -1;
        if (ui.drag == 9001) life_ = std::clamp((ui.mx - bx) / bw, 0.f, 1.f), playing_ = false;
        text(r, bx + bw + 10, ty + 8, std::format("{:.2f} / {:.2f} s", q * life_s, life_s), {230, 230, 235});
        int sp = speed_;
        const float spx = x + w - 130;
        if (button(r, ui, spx, ty, 36, 28, "-")) sp = std::max(10, sp - 10);
        text(r, spx + 40, ty + 8, std::format("{} %", sp), {230, 230, 235});
        if (button(r, ui, spx + 88, ty, 36, 28, "+")) sp = std::min(300, sp + 10);
        speed_ = sp;
        SDL_SetRenderClipRect(r, &clip);
    } else {
        // ---- the model's direction
        SDL_SetRenderClipRect(r, nullptr);
        const float ty = y + h - 40;
        fill_rect(r, x, ty - 6, w, 46, {22, 24, 32, 240});
        text(r, x + 8, ty + 8, tr("Direction"), {230, 230, 235});
        for (int i = 0; i < 8; ++i)
            if (button(r, ui, x + 90 + float(i) * 34, ty, 30, 28, std::to_string(i), dir_ == i)) dir_ = i;
        text(r, x + 380, ty + 8, trf("Playing: {}", anim_key_), {230, 230, 235});
        SDL_SetRenderClipRect(r, &clip);
    }
    SDL_SetRenderClipRect(r, nullptr);
    rect(r, x, y, w, h, {90, 90, 110, 255});
    for (std::size_t i = 1; i < stroke_.size() && (stroking_ || draw_mode_); ++i)
        line(r, v.sx(stroke_[i - 1].first), v.sy(stroke_[i - 1].second), v.sx(stroke_[i].first), v.sy(stroke_[i].second), {255, 240, 120, 255});
    if (button(r, ui, x + w - 130, y + 8, 122, 24, tilt_ >= 100 ? tr("From above") : tr("Game camera"), false)) tilt_ = tilt_ >= 100 ? 72 : 100;
    slider(r, ui, 9101, x + w - 460, y + 36, 450, tilt_, 40, 100, tr("Angle of the camera"));  // [U-531]
    text(r, x + 10, y + 10, show_skill ? tr("Click to aim. Wheel = zoom. Yellow = the body's hitbox, pink = the sweet spot.") : (draw_mode_ == 1 ? tr("Hold and draw the outline of the body: it becomes the hitbox.") : tr("Wheel = zoom. Drag = turn the model. Yellow = the hitbox of the body.")), {200, 200, 210});
}

// ---------------------------------------------------------------- the bar on top
void Maker::toolbar() {
    SDL_Renderer* r = win.ren();
    Ui& ui = win.ui;
    text(r, 14, 12, tr("Champion maker"), {255, 200, 120}, 2);
    float x = std::max(240.f, 14 + text_w(tr("Champion maker"), 2) + 16);
    if (button(r, ui, x, 8, 90, 28, tr("New"))) new_champion();
    x += 94;
    if (button(r, ui, x, 8, 90, 28, tr("Open"))) {
        std::vector<Pick> items;
        for (const CustomChampionFile& f : scan_custom_champions()) items.push_back({f.file, "f:" + f.file, f.ok ? f.name : f.error});
        for (int i = 0; i < builtin_champion_count(); ++i) {
            const ChampionDef& c = champions()[std::size_t(i)];
            items.push_back({std::string(c.name), "b:" + std::string(c.name), std::format("{} - {}", tr("built in"), tr(element_name(c.element)))});
        }
        open_picker(tr("Open a champion: your own or a built-in one"), std::move(items), [this](const std::string& v) {
            if (v.starts_with("f:")) open_file(v.substr(2));
            else open_builtin(v.substr(2));
        });
    }
    x += 94;
    if (hover(ui, x, 8, 170, 28)) tip_ = tr("A new champion beside the old one: nothing is overwritten");
    if (button(r, ui, x, 8, 170, 28, dirty_ ? tr("Save as new *") : tr("Save as new"), dirty_)) {
        saving_ = true;
        save_name_ = file_.empty() && opened_builtin_.empty() ? d_.name : unique_name(d_.name);
        ui.focus = 98;
    }
    x += 174;
    const bool can_over = !file_.empty() || !opened_builtin_.empty();
    if (can_over && hover(ui, x, 8, 170, 28)) tip_ = !opened_builtin_.empty() ? trf("Replaces the built-in {} with this one; remove the file to get the original back", opened_builtin_) : trf("Writes over {}", file_);
    if (button(r, ui, x, 8, 170, 28, overwrite_armed_ > 0 ? tr("Click again to overwrite") : tr("Overwrite"), overwrite_armed_ > 0, can_over)) {
        if (overwrite_armed_ > 0) overwrite_armed_ = 0, overwrite();
        else overwrite_armed_ = 240;
    }
    if (overwrite_armed_ > 0) --overwrite_armed_;
    x += 174;
    if (button(r, ui, x, 8, 170, 28, tr("Test in practice"))) {
        if (file_.empty() && opened_builtin_.empty()) save_as_new(d_.name);
        else if (dirty_ || file_.empty()) overwrite();
        if (error_.empty() && !file_.empty()) {
            if (win.embedded()) {  // [U-529] the same window goes straight into practice
                win.exit.practice_file = file_;
                win.leave();
            } else {
                const char* base = SDL_GetBasePath();
                const std::string exe = (fs::path(base ? base : "") / "elemental_moba").string();
                const char* args[] = {exe.c_str(), "--practice-with", file_.c_str(), nullptr};
                if (SDL_Process* p = SDL_CreateProcess(args, false)) SDL_DestroyProcess(p);
                else status_ = SDL_GetError();
            }
        }
    }
    x += 174;
    if (button(r, ui, x, 8, 80, 28, tr("Undo"), false, !undo_.empty()) && !undo_.empty()) {
        d_ = undo_.back();
        undo_.pop_back();
        reparse();
    }
    if (button(r, ui, win.w - 120, 8, 110, 28, loc::current() == "en" ? "EN" : "PL")) loc::load(tool::next_language(loc::current()));
    const char* const tabs[] = {"Champion", "Skills", "Hitbox, model, sounds"};
    float tx = 14;
    for (int i = 0; i < 3; ++i) {
        const float tw = 200;
        if (button(r, ui, tx, 44, tw, 32, tr(tabs[i]), tab_ == i)) tab_ = i, scroll_ = 0;
        tx += tw + 4;
    }
}

void Maker::frame() {
    Ui& ui = win.ui;
    tip_.clear();
    {
        std::string got, what;
        {
            const std::scoped_lock lock(dlg_mu_);
            if (!dlg_result_.empty()) got = std::move(dlg_result_), what = dlg_what_, dlg_result_.clear();
        }
        if (!got.empty()) got_file(what, got);
    }
    const bool modal = pick_done_ != nullptr || saving_;
    const bool real_clicked = ui.clicked, real_r = ui.rclicked;
    if (modal) ui.clicked = false, ui.rclicked = false;
    toolbar();
    const float panel = std::min(760.f, win.w * 0.5f), x = 14, w = panel - 28;
    float y = 84;
    if (tab_ == 0) tab_champion(x, y, w);
    else if (tab_ == 1) tab_skills(x, y, w);
    else tab_model(x, y, w);
    preview(panel, 44, win.w - panel - 6, win.h - 44 - 50);
    // ---- the bottom line: what you are pointing at, what happened, what is wrong
    SDL_Renderer* r = win.ren();
    fill_rect(r, 0, win.h - 50, win.w, 50, {16, 17, 24, 255});
    if (!error_.empty()) {
        text(r, 14, win.h - 42, fit_text(trf("Cannot be used yet: {}", error_), win.w - 28), {255, 120, 110});
        text(r, 14, win.h - 24, fit_text(tip_.empty() ? status_ : tip_, win.w - 28), {200, 200, 210});
    } else {
        text(r, 14, win.h - 42, fit_text(tip_, win.w - 28), {230, 230, 200});
        text(r, 14, win.h - 24, fit_text(status_.empty() ? tr("Everything here is checked by the game's own rules after every change.") : status_, win.w - 28), {170, 220, 170});
    }
    poll_model_build();
    if (saving_) {
        ui.clicked = real_clicked;
        const float dw = 520, dx = (win.w - dw) / 2, dy = win.h / 2 - 90;
        fill_rect(r, 0, 0, win.w, win.h, {0, 0, 0, 170});
        fill_rect(r, dx, dy, dw, 170, {30, 32, 44, 255});
        rect(r, dx, dy, dw, 170, {255, 190, 110, 255});
        text(r, dx + 16, dy + 12, tr("Save as a new champion"), {255, 220, 150}, 2);
        text_field(r, ui, 98, dx + 16, dy + 62, dw - 32, save_name_, tr("Name of the new champion"));
        if (button(r, ui, dx + 16, dy + 110, 200, 36, tr("Save")) || ui.enter) {
            saving_ = false;
            ui.focus = -1;
            save_as_new(save_name_);
        }
        if (button(r, ui, dx + 226, dy + 110, 140, 36, tr("Close"))) {
            saving_ = false;
            ui.focus = -1;
        }
    } else if (modal) {
        ui.clicked = real_clicked, ui.rclicked = real_r;
        draw_picker();
    }
    if (ui.enter && ui.focus >= 0 && ui.focus != 99) ui.focus = -1;
}

}  // namespace

namespace em::makers {

int champion_main(int argc, char** argv) {
    tool::go_to_game_folder();
    std::string open_file;
    int start_tab = 0, start_page = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--tab" && i + 1 < argc) start_tab = std::atoi(argv[++i]);       // opens on this tab (0 champion, 1 skills, 2 model)
        else if (a == "--page" && i + 1 < argc) start_page = std::atoi(argv[++i]);  // … and this skill page
        else open_file = a;
    }
    loc::load(tool::language_of_game());
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::println(stderr, "SDL_Init: {}", SDL_GetError());
        return 1;
    }
    {
        auto maker = std::make_unique<Maker>();
        if (!maker->init(open_file)) {
            SDL_Quit();
            return 1;
        }
        Maker* m = maker.get();
        m->start_at(start_tab, start_page);
        m->run();
    }
    SDL_Quit();
    return 0;
}

// [U-529] inside the window of the game
tool::ToolExit champion_embedded(SDL_Window* win, SDL_Renderer* ren) {
    auto maker = std::make_unique<Maker>();
    maker->win.attach(win, ren);
    if (!maker->init("")) return {};
    maker->run();
    return maker->win.exit;
}

}  // namespace em::makers
