#pragma once
// [U-99][U-158][U-162][U-163][U-201][U-207][U-208] Everything the player can set, saved as plain text
// `settings.cfg` (key=value, one item per line) next to where the game is started.
#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/abilities.hpp"
#include "game/loadout/loadout.hpp"

namespace em {

// ---------------------------------------------------------------- key bindings [U-165]
enum class Action : std::uint8_t {
    Q, W, E, R, T, Y, F, G,        // skills (Ctrl + key = learn / rank up); F / G = the specials (keys G / H) [U-289]
    Stop, Stance, Recall, Portal, Shop, Score,
    CamLock, CenterSelf,
    AttackMoveChamp,   // [U-148] A: attack-move, closest champion only — fires at the cursor at once
    AttackMoveAny,     // [U-148] D: attack-move, anything
    ChampsOnly,        // [U-162] X (hold): right click and targeted skills pick champions only
    ToggleAutoAttack,  // [U-158] H
    Chat,
    ExpandStats,       // [U-238] C: all statistics of me and the selected unit
    CancelCast,        // [U-265] Z: cancel the cast / channel
    AttackRange,       // [U-289] ` (hold): my autoattack range circle; Ctrl + ` = always on / off
    SourceForm,        // [U-293] K: waterbenders — water from sources comes as water / ice
    QuickCastAll,      // [U-447] N: "quick cast all" on / off
    Item1, Item2, Item3, Item4, Item5, Item6, Item7, Item8, Item9,  // [U-322] 1 = the special slot
    Count
};
inline constexpr int kActions = int(Action::Count);
[[nodiscard]] const char* action_name(Action a);
[[nodiscard]] const char* action_id(Action a);  // name in settings.cfg

// ---------------------------------------------------------------- saved things
struct BuildStep {      // [U-323] one step of the build creator: an item + a short text ("why / when")
    std::int16_t item = -1;
    std::string text;
};
inline constexpr int kMaxBuildSteps = 12;
struct SavedSetup {     // [U-178] everything prepared before a game: runes, masteries, keystone, build, levelling
    std::string name;
    Loadout loadout;
    std::vector<BuildStep> build;  // [U-323] in buying order (≤ kMaxBuildSteps)
    // [U-324] auto-levelling: 0 = priority list (the ultimate whenever allowed, then these skills), 1 = exact order
    int level_mode = 0;
    std::array<std::int8_t, 6> level_prio{1, 2, 3, 4, 5, 6};  // Slot values Q W E R T Y, most wanted first
    std::array<std::int8_t, 18> level_exact{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};  // Slot per level-up
    std::vector<std::string> champs;  // [U-432] the champions this setup is for: picked FIRST for them in champion select
};
struct ChampPrefs {     // [U-178][U-180] per champion (by name, so the roster order can change)
    std::string champ;
    int setup = -1;     // last full setup used (index into setups), −1 = none
    int q = -1, f = -1, g = -1;
    bool favourite = false;
    std::string skin;   // skin folder name in the resource pack ("" = base)
};
struct ServerEntry {    // [U-212] Minecraft-like server list
    std::string name, address;  // address = host:port
};
struct Build {          // [U-220] own item builds
    std::string name;
    std::string champ;  // "" = any champion
    std::array<std::int16_t, 8> items{-1, -1, -1, -1, -1, -1, -1, -1};
};

enum class WindowMode : std::uint8_t { Fullscreen, Borderless, Windowed };

struct Settings {
    // ---- player
    std::string name = "Hubert";
    std::string version = "official-0.1";  // [U-95]
    // ---- language [U-239]
    std::string language = "en";           // text
    bool voice_follows_text = true;        // voices in the same language as the text …
    std::string voice_language = "en";     // … or this one
    // ---- controls
    bool quick_cast_all = false;           // [U-163] LoL "quick cast all"
    std::array<bool, kSkillSlots> quick_cast{};  // [U-163] per ability slot P…G
    bool auto_buy = false;                 // [U-323] buy the next item of my setup's build when I stand in the shop
    bool auto_level = false;               // [U-324] spend skill points by my setup's levelling order
    bool auto_attack = true;               // [U-158] idle champion hits enemies in range
    bool champs_only_toggle = true;        // X: true = press to switch on / off (default since U-289), false = hold
    bool show_attack_range = false;        // [U-289] Ctrl + ` : the autoattack range circle always on
    int companion = 0;
    int ruleset = 0;                       // [U-416] practice / hosting: 0 Elemental, 1 LoL-like, 2 Dota-like
    int map = 0;                           // [U-420] practice / hosting: index in maps()
    bool show_other_emotes = true;         // [U-414] show other players' own emotes (pictures + sounds)
    std::vector<std::string> own_emotes;   // [U-414] my own emotes in the wheel (folders in custom_emotes/), max 4                     // [U-407] CompanionKind of my travel animal (recall)
    bool attack_move_cursor = false;       // [U-303] attack-move picks the enemy nearest the CURSOR (false = nearest me)
    bool both_buttons_center = true;       // [U-164] left + right click together = camera on me
    std::array<SDL_Scancode, kActions> keys{};
    // ---- window [U-207]
    WindowMode window = WindowMode::Fullscreen;  // true fullscreen by default
    bool confine_mouse = true;             // the mouse can't leave the game window (off = two screens / windowed)
    bool vsync = true;
    // ---- graphics [U-198][U-204][U-208]
    bool dev_mode = false;                 // developer view: top-down, full hitboxes, no textures
    int hitbox_opacity = 10;               // [U-176] game view: hitbox = grey shadow, this % opacity
    bool smooth_fog = true;                // [U-204] soft fog edges (off in dev mode)
    bool damage_numbers = true;            // [U-192]
    bool cc_timers = true;                 // [U-193]
    bool show_paths = true;                // [U-203] my path + allies' paths
    bool show_rewards = true;              // [U-159] "+21g +60xp" on kills
    bool show_names = true;
    bool colourblind = false;
    bool minimap = true;
    bool show_chat = true;
    bool tree_models = true;               // [U-202]
    // ---- skins / resource packs [U-97][U-98][U-197]
    std::string resource_pack = "default";
    bool show_other_skins = true;          // [U-98] default ON
    bool force_own_skins = false;          // [U-98] default OFF
    bool server_skins_only = false;        // [U-98] default OFF
    // ---- sound [U-201]
    int vol_master = 80, vol_music = 40, vol_effects = 80, vol_ui = 60, vol_ambient = 50, vol_voice = 80;
    // ---- lists
    std::vector<std::string> friends;      // [U-182]
    std::vector<ServerEntry> servers;      // [U-212]
    std::vector<SavedSetup> setups;        // [U-178]
    std::vector<ChampPrefs> champs;        // [U-178][U-180]
    std::vector<Build> builds;             // [U-220]
    std::vector<std::string> custom_champions;  // [U-210] enabled custom champion files

    Settings();
    void default_keys();
    bool load(const std::string& path = "settings.cfg");
    void save(const std::string& path = "settings.cfg") const;
    [[nodiscard]] bool quick(int slot) const { return quick_cast_all || (slot >= 0 && slot < kSkillSlots && quick_cast[std::size_t(slot)]); }
    ChampPrefs& prefs(std::string_view champ);  // creates the entry if missing
    [[nodiscard]] bool tilted() const { return !dev_mode; }
};

}  // namespace em
