// Settings file + default key bindings. See settings.hpp.
#include "client/settings.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace em {

namespace {
struct ActionInfo {
    const char* id;
    const char* name;
    SDL_Scancode key;
};
// [U-148][U-158][U-162] defaults: A champion attack-move, D attack-move, X champions only, H autoattack on/off
constexpr ActionInfo kInfo[kActions] = {
    {"skill_q", "Skill Q", SDL_SCANCODE_Q},       {"skill_w", "Skill W", SDL_SCANCODE_W},
    {"skill_e", "Skill E", SDL_SCANCODE_E},       {"skill_r", "Skill R", SDL_SCANCODE_R},
    {"skill_t", "Skill T", SDL_SCANCODE_T},       {"skill_y", "Ultimate Y", SDL_SCANCODE_Y},  // [U-289][U-291]
    {"skill_f", "Special 1 (G)", SDL_SCANCODE_G}, {"skill_g", "Special 2 (H)", SDL_SCANCODE_H},
    {"stop", "Stop", SDL_SCANCODE_S},
    {"stance", "Switch stance (with Ctrl)", SDL_SCANCODE_SPACE}, {"recall", "Recall", SDL_SCANCODE_B},
    {"portal", "Spirit portal", SDL_SCANCODE_V},  {"shop", "Shop", SDL_SCANCODE_I},  // [U-291] I like "item"
    {"score", "Scoreboard (hold)", SDL_SCANCODE_TAB}, {"cam_lock", "Camera lock", SDL_SCANCODE_U},
    {"center_self", "Centre camera on me", SDL_SCANCODE_SPACE},
    {"attack_move_champ", "Attack-move: champions only", SDL_SCANCODE_A},
    {"attack_move_any", "Attack-move: anything", SDL_SCANCODE_D},
    {"champs_only", "Target champions only", SDL_SCANCODE_X},
    {"toggle_autoattack", "Autoattack on / off", SDL_SCANCODE_J},
    {"chat", "Chat", SDL_SCANCODE_RETURN},
    {"expand_stats", "Show all statistics", SDL_SCANCODE_C},
    {"cancel_cast", "Cancel cast / channel", SDL_SCANCODE_Z},
    {"attack_range", "Attack range circle (hold; Ctrl = on / off)", SDL_SCANCODE_GRAVE},
    {"source_form", "Waterbenders: water source as water / ice", SDL_SCANCODE_K},
    {"item1", "Item 1", SDL_SCANCODE_1}, {"item2", "Item 2", SDL_SCANCODE_2}, {"item3", "Item 3", SDL_SCANCODE_3},
    {"item4", "Item 4", SDL_SCANCODE_4}, {"item5", "Item 5", SDL_SCANCODE_5}, {"item6", "Item 6", SDL_SCANCODE_6},
    {"item7", "Item 7", SDL_SCANCODE_7}, {"item8", "Item 8", SDL_SCANCODE_8}, {"item9", "Item 9", SDL_SCANCODE_9},
};

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}
int to_int(const std::string& s, int fallback = -1) {
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    return end && *end == 0 && !s.empty() ? int(v) : fallback;
}
}  // namespace

const char* action_name(Action a) { return kInfo[int(a)].name; }
const char* action_id(Action a) { return kInfo[int(a)].id; }

Settings::Settings() {
    default_keys();
    // a starter setup so "pick a full setup" has something to show [U-178]
    setups.push_back({"Mage (default)", {.keystone = 0, .tree = 2, .nodes = {0, 1, 0}, .runes = {0, 0, 1}}});
    setups.push_back({"Fighter (default)", {.keystone = 4, .tree = 0, .nodes = {1, 0, 0}, .runes = {1, 1, 0}}});
    setups.push_back({"Tank (default)", {.keystone = 1, .tree = 1, .nodes = {0, 0, 0}, .runes = {1, 2, 0}}});
    servers.push_back({"My computer", "127.0.0.1:7777"});
}

void Settings::default_keys() {
    for (int i = 0; i < kActions; ++i) keys[std::size_t(i)] = kInfo[i].key;
}

ChampPrefs& Settings::prefs(std::string_view champ) {
    for (ChampPrefs& p : champs)
        if (p.champ == champ) return p;
    champs.push_back({.champ = std::string(champ)});
    return champs.back();
}

bool Settings::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    bool lists_cleared = false;
    int file_version = 1;
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos || line.starts_with('#')) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        const bool b = v == "1";
        auto clear_lists = [&] {  // the file has lists → drop the built-in starters
            if (lists_cleared) return;
            lists_cleared = true;
            setups.clear();
            servers.clear();
        };
        if (k == "settings_version") file_version = to_int(v, 1);
        else if (k == "name") name = v;
        else if (k == "language") language = v;
        else if (k == "voice_follows_text") voice_follows_text = b;
        else if (k == "voice_language") voice_language = v;
        else if (k == "quick_cast_all") quick_cast_all = b;
        else if (k.starts_with("quick_cast_") && k.size() == 12) {
            const int s = k[11] - '0';
            if (s >= 0 && s < kSkillSlots) quick_cast[std::size_t(s)] = b;
        } else if (k == "auto_attack") auto_attack = b;
        else if (k == "champs_only_toggle") champs_only_toggle = b;
        else if (k == "show_attack_range") show_attack_range = b;
        else if (k == "attack_move_cursor") attack_move_cursor = b;
        else if (k == "companion") companion = std::clamp(to_int(v, 0), 0, 5);
        else if (k == "both_buttons_center") both_buttons_center = b;
        else if (k == "window") window = WindowMode(std::clamp(to_int(v, 0), 0, 2));
        else if (k == "confine_mouse") confine_mouse = b;
        else if (k == "vsync") vsync = b;
        else if (k == "dev_mode") dev_mode = b;
        else if (k == "hitbox_opacity") hitbox_opacity = std::clamp(to_int(v, 10), 0, 100);
        else if (k == "smooth_fog") smooth_fog = b;
        else if (k == "damage_numbers") damage_numbers = b;
        else if (k == "cc_timers") cc_timers = b;
        else if (k == "show_paths") show_paths = b;
        else if (k == "show_rewards") show_rewards = b;
        else if (k == "show_names") show_names = b;
        else if (k == "colourblind") colourblind = b;
        else if (k == "minimap") minimap = b;
        else if (k == "show_chat") show_chat = b;
        else if (k == "tree_models") tree_models = b;
        else if (k == "resource_pack") resource_pack = v;
        else if (k == "show_other_skins") show_other_skins = b;
        else if (k == "force_own_skins") force_own_skins = b;
        else if (k == "server_skins_only") server_skins_only = b;
        else if (k == "vol_master") vol_master = std::clamp(to_int(v, 80), 0, 100);
        else if (k == "vol_music") vol_music = std::clamp(to_int(v, 40), 0, 100);
        else if (k == "vol_effects") vol_effects = std::clamp(to_int(v, 80), 0, 100);
        else if (k == "vol_ui") vol_ui = std::clamp(to_int(v, 60), 0, 100);
        else if (k == "vol_ambient") vol_ambient = std::clamp(to_int(v, 50), 0, 100);
        else if (k == "vol_voice") vol_voice = std::clamp(to_int(v, 80), 0, 100);
        else if (k == "friend") friends.push_back(v);
        else if (k == "custom_champion") custom_champions.push_back(v);
        else if (k == "server") {
            clear_lists();
            const auto p = split(v, '|');
            if (p.size() == 2) servers.push_back({p[0], p[1]});
        } else if (k == "setup") {
            clear_lists();
            const auto p = split(v, '|');
            if (p.size() == 9) {
                SavedSetup s{p[0], {}};
                s.loadout.keystone = std::int8_t(to_int(p[1]));
                s.loadout.tree = std::int8_t(to_int(p[2]));
                for (int i = 0; i < 3; ++i) s.loadout.nodes[std::size_t(i)] = std::int8_t(to_int(p[3 + std::size_t(i)]));
                for (int i = 0; i < 3; ++i) s.loadout.runes[std::size_t(i)] = std::int8_t(to_int(p[6 + std::size_t(i)]));
                s.loadout = validated(s.loadout);
                setups.push_back(s);
            }
        } else if (k == "champ") {
            const auto p = split(v, '|');
            if (p.size() == 7) {
                ChampPrefs& c = prefs(p[0]);
                c.setup = to_int(p[1]);
                c.q = to_int(p[2]);
                c.f = to_int(p[3]);
                c.g = to_int(p[4]);
                c.favourite = p[5] == "1";
                c.skin = p[6];
            }
        } else if (k == "build") {
            const auto p = split(v, '|');
            if (p.size() == 10) {
                Build bd{p[0], p[1], {}};
                for (int i = 0; i < 8; ++i) bd.items[std::size_t(i)] = std::int16_t(to_int(p[2 + std::size_t(i)]));
                builds.push_back(bd);
            }
        } else if (k.starts_with("key_")) {
            for (int i = 0; i < kActions; ++i)
                if (k.substr(4) == kInfo[i].id) {
                    const SDL_Scancode sc = SDL_GetScancodeFromName(v.c_str());
                    if (sc != SDL_SCANCODE_UNKNOWN) keys[std::size_t(i)] = sc;
                }
        }
    }
    // [U-251] files from before version 2 had "centre on me" on F1; F1…F12 are the teammates now → Space
    if (file_version < 2 && keys[std::size_t(Action::CenterSelf)] == SDL_SCANCODE_F1) keys[std::size_t(Action::CenterSelf)] = SDL_SCANCODE_SPACE;
    // [U-289] version 3: six skills (Q W E R T Y), specials on G / H, camera lock U, autoattack J, shop O, X switches.
    // Keys still on an OLD default move to the new one; keys the player chose stay. Quick-cast flags move with
    // their skills (slot 6 was special F → now 7, slot 7 was G → now 8).
    if (file_version < 3) {
        auto move = [&](Action a, SDL_Scancode old_key) {  // (Shop: P → I; the < 4 rule below also covers O → I)
            if (keys[std::size_t(a)] == old_key) keys[std::size_t(a)] = kInfo[int(a)].key;
        };
        move(Action::F, SDL_SCANCODE_F);
        move(Action::G, SDL_SCANCODE_G);
        move(Action::CamLock, SDL_SCANCODE_Y);
        move(Action::ToggleAutoAttack, SDL_SCANCODE_H);
        move(Action::Shop, SDL_SCANCODE_P);
        quick_cast[8] = quick_cast[7];
        quick_cast[7] = quick_cast[6];
        quick_cast[6] = false;
        champs_only_toggle = true;
    }
    if (file_version < 4 && keys[std::size_t(Action::Shop)] == SDL_SCANCODE_O) keys[std::size_t(Action::Shop)] = SDL_SCANCODE_I;  // [U-291]
    return true;
}

void Settings::save(const std::string& path) const {
    // [U-383] written to a temporary file, then renamed: a crash while saving never leaves a half file
    struct Commit {
        std::string path;
        ~Commit() {
            std::error_code ec;
            std::filesystem::rename(path + ".tmp", path, ec);
        }
    } commit{path};
    std::ofstream out(path + ".tmp");
    if (!out) return;
    out << "# elemental-moba settings (key=value). Delete this file to get the defaults back.\n";
    out << "settings_version=4\n# ---- player\nname=" << name << '\n';
    out << "# ---- language\nlanguage=" << language << "\nvoice_follows_text=" << voice_follows_text
        << "\nvoice_language=" << voice_language << '\n';
    out << "# ---- controls\nquick_cast_all=" << quick_cast_all << '\n';
    for (int s = 0; s < kSkillSlots; ++s) out << "quick_cast_" << s << '=' << quick_cast[std::size_t(s)] << '\n';
    out << "auto_attack=" << auto_attack << "\nchamps_only_toggle=" << champs_only_toggle
        << "\nboth_buttons_center=" << both_buttons_center << "\nshow_attack_range=" << show_attack_range << "\nattack_move_cursor=" << attack_move_cursor << "\ncompanion=" << companion << '\n';
    for (int i = 0; i < kActions; ++i) out << "key_" << kInfo[i].id << '=' << SDL_GetScancodeName(keys[std::size_t(i)]) << '\n';
    out << "# ---- window\nwindow=" << int(window) << "\nconfine_mouse=" << confine_mouse << "\nvsync=" << vsync << '\n';
    out << "# ---- graphics\ndev_mode=" << dev_mode << "\nhitbox_opacity=" << hitbox_opacity << "\nsmooth_fog=" << smooth_fog
        << "\ndamage_numbers=" << damage_numbers << "\ncc_timers=" << cc_timers << "\nshow_paths=" << show_paths
        << "\nshow_rewards=" << show_rewards << "\nshow_names=" << show_names << "\ncolourblind=" << colourblind
        << "\nminimap=" << minimap << "\nshow_chat=" << show_chat << "\ntree_models=" << tree_models << '\n';
    out << "# ---- skins / resource packs\nresource_pack=" << resource_pack << "\nshow_other_skins=" << show_other_skins
        << "\nforce_own_skins=" << force_own_skins << "\nserver_skins_only=" << server_skins_only << '\n';
    out << "# ---- sound (0-100)\nvol_master=" << vol_master << "\nvol_music=" << vol_music << "\nvol_effects=" << vol_effects
        << "\nvol_ui=" << vol_ui << "\nvol_ambient=" << vol_ambient << "\nvol_voice=" << vol_voice << '\n';
    out << "# ---- friends, servers, setups, champions, builds, custom champions\n";
    for (const auto& f : friends) out << "friend=" << f << '\n';
    for (const auto& s : servers) out << "server=" << s.name << '|' << s.address << '\n';
    for (const auto& s : setups) {
        out << "setup=" << s.name << '|' << int(s.loadout.keystone) << '|' << int(s.loadout.tree);
        for (auto n : s.loadout.nodes) out << '|' << int(n);
        for (auto r : s.loadout.runes) out << '|' << int(r);
        out << '\n';
    }
    for (const auto& c : champs)
        out << "champ=" << c.champ << '|' << c.setup << '|' << c.q << '|' << c.f << '|' << c.g << '|' << c.favourite << '|' << c.skin << '\n';
    for (const auto& bd : builds) {
        out << "build=" << bd.name << '|' << bd.champ;
        for (auto i : bd.items) out << '|' << i;
        out << '\n';
    }
    for (const auto& c : custom_champions) out << "custom_champion=" << c << '\n';
}

}  // namespace em
