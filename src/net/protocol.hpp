#pragma once
// [X-01][X-02][X-24] Network protocol. enet UDP: channel 0 reliable (lobby, cells, feed, cmds),
// channel 1 unreliable (snapshots). Little-endian byte packing, no external serializer.
// Server-authoritative: clients send only intents (Cmd), the server sends what the team may see.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/items.hpp"
#include "game/loadout/loadout.hpp"

namespace em::net {

inline constexpr std::uint16_t kProtocol = 19;  // 19: animation info (stage, form, dash) [U-486]  // 18: heal / shield / buff power, polymorph, setup per version [U-483][U-484]  // 17: healing received [U-481]  // 16: switchable Dota features, version mods [U-478]  // 15: LoL AP / magic pen, Dota attributes + statuses [U-474..U-476]  // 14: crits, skill powers, sweet spots + shapes [U-461..U-471]  // 13: cast stages, chains, skill visuals, proc + recast timers [U-456..U-459]  // 12: maps, lobby owner / queue, charges [U-420..U-433]  // 11: rule sets, own emotes [U-414][U-416]  // 7: ten item slots [U-322]; 8: cast sources [U-340]; 9: camps, jungle buffs [U-364..U-366]; 10: bounties, companions [U-407][U-413]
inline constexpr std::string_view kVersion = "official-0.1";  // [U-95][X-14]
inline constexpr std::uint16_t kDefaultPort = 7777;
inline constexpr int kMaxPlayers = 32;  // [U-09]

enum class Msg : std::uint8_t {
    // client → server
    Hello = 1, Pick, StartMatch, Command, Chat, Leave,
    StatusQuery,   // [U-212] server list ping: answered with StatusReply, no join
    Ban,           // [U-181] ban one champion in the ban phase
    SkinUpload,    // [U-200] my skin image for this match (Minecraft-like)
    EmoteUpload,   // [U-414] one of my own emotes (picture + optional sound), separate from skins
    LobbyConfig,   // [U-422] the owner sets the map + rule set (u8 map, u8 ruleset)
    TransferOwner, // [U-422] the owner hands the lobby to another player (u8 slot id)
    Queue,         // [U-423] queue mode: u8 on, u8 map, u8 ruleset
    // server → client
    Welcome = 64, Reject, Lobby, MatchStart, Cells, Snapshot, Feed, GameOver, ChatLine, Notice,
    StatusReply,   // [U-212]
    SkinData,      // [U-200] another player's skin
    FinalStats,    // [U-179] post-game statistics
    Announce,      // [U-263] kill / multikill / objective announcement
    DeathRecap,    // [U-262] what killed you (only to the dead player)
    CustomChampions,  // [U-277] the server's own champion files (sent on joining)
    EmoteData,     // [U-414] another player's own emote (only when the server allows own emotes)
};

// [U-414] own emotes: up to 4 per player, a PNG ≤ 96 KB (≤ 256 × 256) + an optional WAV ≤ 256 KB; emote slots 6…9
inline constexpr int kOwnEmotes = 4, kOwnEmoteSlot0 = 6;
inline constexpr std::uint32_t kOwnEmotePngMax = 96 * 1024, kOwnEmoteWavMax = 256 * 1024;

// [U-213] platforms; a server may allow only some (default all)
enum Platform : std::uint8_t { kLinux = 1, kWindows = 2, kMacOS = 4, kAllPlatforms = 7 };
[[nodiscard]] constexpr Platform this_platform() {
#if defined(_WIN32)
    return kWindows;
#elif defined(__APPLE__)
    return kMacOS;
#else
    return kLinux;
#endif
}
[[nodiscard]] constexpr const char* platform_name(std::uint8_t p) {
    return p == kWindows ? "Windows" : p == kMacOS ? "macOS" : p == kLinux ? "Linux" : "?";
}

// [U-182] chat channels
enum class Chat : std::uint8_t {
    Lobby,     // everyone in the lobby / champion select of this server ("server chat")
    Team,      // my team (champion select + in game)
    All,       // everyone in the match
    Whisper,   // one player by name — friends chat
    System,    // server messages
};
[[nodiscard]] constexpr const char* chat_name(Chat c) {
    switch (c) {
        case Chat::Lobby: return "lobby";
        case Chat::Team: return "team";
        case Chat::All: return "all";
        case Chat::Whisper: return "friend";
        case Chat::System: return "server";
    }
    return "?";
}

// [U-181] draft phases
enum class DraftPhase : std::uint8_t { Picking, Banning };

class Writer {
public:
    template <class T>
    void put(T v) {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
        buf.insert(buf.end(), p, p + sizeof(T));
    }
    void str(std::string_view s) {
        put<std::uint16_t>(std::uint16_t(std::min<std::size_t>(s.size(), 4'000)));
        buf.insert(buf.end(), s.begin(), s.begin() + std::min<std::size_t>(s.size(), 4'000));
    }
    // [U-425] a long text (own champion files: up to 16 000 bytes, the .champ limit) — never cut
    void long_str(std::string_view s) {
        const std::size_t n = std::min<std::size_t>(s.size(), 16'000);
        put<std::uint32_t>(std::uint32_t(n));
        buf.insert(buf.end(), s.begin(), s.begin() + std::ptrdiff_t(n));
    }
    std::vector<std::uint8_t> buf;
};

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> d) : d_(d) {}
    template <class T>
    T get() {
        T v{};
        if (pos_ + sizeof(T) > d_.size()) {
            bad_ = true;
            return v;
        }
        std::memcpy(&v, d_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return v;
    }
    std::string long_str() {  // [U-425]
        const std::uint32_t n = get<std::uint32_t>();
        if (n > 16'000 || pos_ + n > d_.size()) {
            bad_ = true;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(d_.data() + pos_), n);
        pos_ += n;
        return s;
    }
    std::string str() {
        const std::uint16_t n = get<std::uint16_t>();
        if (pos_ + n > d_.size()) {
            bad_ = true;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(d_.data() + pos_), n);
        pos_ += n;
        return s;
    }
    [[nodiscard]] bool bad() const { return bad_; }
    [[nodiscard]] bool done() const { return pos_ >= d_.size(); }

private:
    std::span<const std::uint8_t> d_;
    std::size_t pos_ = 0;
    bool bad_ = false;
};

// Lobby slot (also used for practice bots).
struct LobbySlot {
    std::uint8_t id = 0;
    std::string name;
    std::int8_t team = 0;    // 0 Fire Nation, 1 Earth Nation [U-06]
    std::int8_t champ = -1;  // −1 = not picked
    std::int8_t q = 0, f = -1, g = -1;
    bool ready = false;
    bool bot = false;
    bool host = false;
    Loadout loadout;           // [U-177]
    std::int8_t ban = -1;      // [U-181]
    std::uint8_t platform = 0; // [U-213]
    bool has_skin = false;     // [U-200]
    std::int32_t lp = -1;      // [U-276] ranking LP (−1 = not rated: bot / guest / unranked server)
    bool queued = false;       // [U-423] queue mode: in the queue for q_map / q_rules …
    std::uint8_t q_map = 0, q_rules = 0;
    bool plays = true;         // … and taking part in the match being prepared (false = waiting in the lobby)
};

// [U-179] one row of the post-game statistics
struct FinalRow {
    std::uint32_t unit;
    std::uint8_t team;
    std::int8_t champ, keystone, pad;
    std::uint16_t level, kills, deaths, assists, cs, pad2;
    std::int32_t gold, dmg_champs, dmg_taken, healing, shielding, keystone_impact;
    std::int16_t items[kItemSlots];
    std::int16_t lp_delta, pad3;  // [U-276] LP won / lost (0 = not rated)
    std::int32_t lp_after;        //         LP now (−1 = not rated)
};

// [U-341] a wall piece that just fell (the client animates it once)
struct NetWallBreak {
    std::int16_t x, y, dx, dy;
    std::uint8_t kind, pad[3];
    std::int64_t ms;
};
// [U-288] a damaged wall piece (only damaged ones are sent): HP left in %
struct NetWallHp {
    std::int16_t x, y;
    std::uint8_t pct, pad;
};
// [U-443] one wall segment as it is: centre, direction, thickness, height, kind, HP % — drawn rotated (any angle)
struct NetWallSeg {
    std::int32_t x, y;
    std::int16_t dx, dy;           // along (Q14)
    std::int16_t len, thick;
    std::uint8_t level, kind, pct, pad;
};
// [U-241] a cloud everyone sees (it blocks vision for both teams)
struct NetCloud {
    std::int32_t x, y;
    std::int16_t radius;
    std::uint8_t kind, pad;
};

// [U-235] my team's falcons and the ghosts they show (a falcon report being replayed)
struct NetFalcon {
    std::int32_t x, y;
    std::uint8_t state, pad[3];
};
struct NetGhost {
    std::uint32_t id;
    std::int8_t champ;
    std::uint8_t team, kind, pad;
    std::int32_t x, y;
};

// [U-246] a ping of my team
struct NetPing {
    std::uint8_t kind, pad[3];
    std::uint32_t from;
    std::int32_t x, y;
    std::int64_t ms;
};

// [U-203] path of an allied champion (cells), for the map / minimap
struct NetPath {
    std::uint32_t unit;
    std::uint8_t n, pad[3];
    std::int16_t pts[8][2];
};

// Unit as the client sees it. Positions in game units.
struct NetUnit {
    std::uint32_t id;
    std::uint8_t kind, team;
    std::int8_t champ;
    std::uint8_t mtype, body, alive;
    std::int32_t x, y;
    std::int16_t fx, fy;
    std::uint16_t walk, size_pct, sight;
    std::int32_t hp, hp_max, res, res_max;
    std::uint16_t level;
    std::uint32_t status;  // bit per StatusKind
    std::uint8_t casting;  // 0 none, else slot + 1
    std::uint8_t name_id;  // monsters: index into kMobNames
    std::int32_t shield;   // shield HP points [U-155]
    std::uint8_t shield_sub, riding;  // [U-407] riding = on the companion's back
    std::int16_t atk, def, edef, range;  // for the info panel [U-161]
    std::uint8_t cc_kind, emote;         // strongest crowd control now [U-193] (255 = none); emote [U-246]
    std::uint16_t cc_left_cs;            // … time left in 1/100 s
    std::uint8_t cast_stage, anim_flags, pad9[2];  // [U-486] multicast cast now (0 = first); bit 1 transformed, 2 dashing, 4 hasted
};
inline constexpr const char* kMobNames[] = {"", "Spirit Wolf", "Wandering Spirit", "Cursed Spirit", "Angry Beast", "Bandit", "Face Stealer", "Bandit Archer", "Platypus Bear", "Knowledge Seeker", "Lion Turtle", "Sun Dragons", "Dark Spirit", "Swamp Monster"};

struct NetFx {
    std::uint32_t id, seed;
    std::uint8_t kind, team, sub, flow;
    std::int32_t x, y;
    std::int16_t radius, width, length, dx, dy;
    std::uint8_t feeds, combo, armed, ally;
    // [U-459] the skill's own look (SkillDef::vis), drawn on the exact hitbox above: 0 = the default for its shape
    std::uint32_t vcol;
    std::uint8_t vfill, vglow, vtrail, vflash, vpart, vpad;
    std::uint16_t vmark_ms;
    std::uint8_t geo, szone, ssize, spct;  // [U-461][U-462] shape rule, sweet zone / size, sweet % (0 = none)
    std::int16_t inner, pad2;
};

// [U-456] a chain between a caster and an enemy: pulled to the anchor if still inside the circle when it ends
struct NetTether {
    std::int32_t sx, sy, dx, dy, ax, ay;
    std::int16_t radius;
    std::uint8_t team, sub;
    std::int32_t left_ms, total_ms;
};

struct NetLight {
    std::int32_t x, y;
    std::int16_t radius;
    std::uint8_t strength, pad;
};

struct NetWave {
    std::uint8_t kind, pad;
    std::int16_t dx, dy;
    std::int32_t x, y;
    std::int64_t ms;
};

struct NetAnnounce {  // [U-263]
    std::uint8_t kind, team, a_kind, a_extra;  // [U-363] the killer's UnitKind (255 = none) + minion type / monster name / champion
    std::uint32_t a, b;  // killer / victim (or structure)
};
struct NetRecapHit {  // [U-262]
    std::int32_t before_ms;  // how long before the death
    std::uint32_t src;
    std::uint8_t from, slot, sub, physical;  // DmgFrom, skill slot, sub-element
    std::int16_t item;
    std::uint8_t src_kind, src_extra;  // [U-302] UnitKind (255 = none) + minion type / monster name id / champion
    std::int32_t amount, shielded;
    std::uint8_t cc, pad2;             // [U-343] crowd control (StatusKind, 255 = a damage entry)
    std::uint16_t cc_cs;               //         its time in 1/100 s
};
struct NetHit {  // [X-103]
    std::uint32_t src, dst;
    std::int32_t amount;
    std::uint8_t is_auto, sub;
    std::int16_t pad;
    std::int64_t ms;
};

struct NetScore {
    std::uint32_t id;
    std::uint16_t kills, deaths, assists, cs, level;
    std::uint8_t alive, pad;
    std::int32_t respawn_ms;
    std::int16_t items[kItemSlots];
    // [U-161] skills for the tooltip: ranks, full cooldown, damage; cooldown LEFT only for allies (−1 = hidden)
    std::uint8_t rank[kSkillSlots];
    std::int32_t cd_total[kSkillSlots], cd_left[kSkillSlots], dmg[kSkillSlots];
    std::int16_t attack, ep, def, edef, ms, as100x, range, size;
    std::int16_t bounty, pad_b;  // [U-413] gold for killing this champion (above 300 = shutdown)
};

struct NetReward {  // [U-159]
    std::int32_t x, y, gold, xp;
    std::int64_t ms;
};

// Private state of the player's own champion.
struct NetMe {
    std::uint32_t unit;
    std::int32_t gold, skill_points;
    std::int64_t xp, xp_next, xp_this;
    std::uint8_t rank[kSkillSlots];
    std::int32_t cd_left_ms[kSkillSlots];
    std::int32_t cd_total_ms[kSkillSlots];
    std::uint8_t recast[kSkillSlots];       // multicast casts used [U-106]
    std::int16_t items[kItemSlots];
    std::int32_t item_cd_ms[kItemSlots];
    std::int32_t cast_left_ms, cast_total_ms, channel_left_ms, respawn_ms, wind_sense_ms, stolen_ms;
    std::uint8_t stolen_slot, stance_b, no_cd, can_shop;
    std::int32_t attack_range;
    std::int32_t move_speed;
    std::int32_t attack, ep, def, edef, as100x, haste, tenacity, model_size;
    // resolved skill sizes → the indicator shows the exact edges [U-131]
    std::int32_t s_range[kSkillSlots], s_width[kSkillSlots], s_radius[kSkillSlots], s_cost[kSkillSlots];
    std::int32_t res, res_max;
    std::uint8_t auto_attack, pad3[3];
    std::int32_t s_dmg[kSkillSlots], s_cast_ms[kSkillSlots], s_speed[kSkillSlots], s_length[kSkillSlots];  // resolved numbers for the skill tooltip [U-168]
    std::int32_t ep_raw, attack_raw, bonus_attack_raw, cast_speed, power_pct, size_pct;  // for the Alt formulas
    std::int32_t item_impact[kItemSlots];  // [U-209]
    Loadout loadout;              // [U-238] my runes / tree / keystone …
    std::int32_t keystone_impact; // … and what the keystone did so far
    std::uint8_t nstatus, pad4[3];
    struct St {
        std::uint8_t kind, pad;
        std::int16_t value;
        std::int32_t left_ms;
    } status[20];  // my buffs / debuffs [U-167]; kind ≥ kModWireBase = a ModKind [U-255]
    std::uint8_t jbuff, pad5[3];  // [U-366] my jungle buff (JungleBuff) …
    std::int32_t jbuff_ms;        // … time left
    std::int32_t water, water_max;  // [U-339] the Avatar's carried water
    std::int16_t lion_stacks[2];    // [U-364] Lion Turtle stacks per team
    std::uint8_t charges[kSkillSlots];  // [U-433] charges ready per skill (255 = the skill has no charges)
    std::int32_t charge_left_ms[kSkillSlots], charge_total_ms[kSkillSlots];  // [U-444] the next charge: time left / full
    std::int32_t recast_left_ms[kSkillSlots], recast_total_ms[kSkillSlots];  // [U-458] a multicast's open window: time left / full
    // [U-463][U-464][U-468] 100 = 1 %; uses bit 1 = can crit, 2 = has arcane skills, 4 = has martial (physical) skills
    std::int32_t crit_chance, crit_damage, arcane_power, martial_power, skill_amp, vamp;
    std::uint8_t uses, pad6[3];
    // [U-475][U-476] LoL ability power / magic pen; Dota attributes, magic resistance, evasion, HP regen
    std::int32_t ap, mpen, mpen_pct, str, agi, intel, mres, eva, hp_regen;
    // [U-478] switchable features: buyback, glyph (my team), courier, neutral item, talents
    std::int32_t buyback_cost, buyback_left_ms, glyph_left_ms, glyph_active_ms, courier_left_ms;
    std::uint8_t courier_n, talents[4], level;
    std::int8_t neutral;
    std::uint8_t pad8;
    std::int32_t heal_amp;  // [U-481] healing received %, 100 = 1 %
    std::int32_t self_heal, heal_shield_power, buff_power;  // [U-484]
};

// [U-407] a travel companion on its way (or carrying its rider home)
struct NetCompanion {
    std::uint32_t owner;
    std::int32_t x, y;
    std::uint8_t kind, carrying, pad[2];
};

// [U-365] every jungle camp: where, what, alive or the respawn time (public, like LoL timers)
struct NetCamp {
    std::int16_t x, y;            // home cell
    std::uint8_t name_id, epic;   // kMobNames index; 1 = epic monster
    std::uint16_t respawn_s;      // 0 = alive
};

}  // namespace em::net
