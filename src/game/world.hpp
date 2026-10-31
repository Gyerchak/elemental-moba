#pragma once
// [X-04][X-05][X-06] The match simulation: fixed tick (default 128 Hz [U-102]), integer math,
// plain arrays of units / effects (own minimal ECS [X-83]). Server-authoritative [X-01]:
// players and bots only send Cmds, the World decides everything.
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/wall_rules.hpp"
#include "core/core.hpp"
#include "core/channel_rules.hpp"
#include "game/content.hpp"
#include "game/loadout/loadout.hpp"
#include "game/mobs/rules.hpp"
#include "game/reference/lol_dota.hpp"  // [U-230] reference only, not used
#include "game/mechanics/vision.hpp"
#include "map/default_map.hpp"

namespace em {

inline constexpr std::int32_t kSub = 256;  // [X-04] positions: 1 game unit = 256 sub-units (Q24.8)
inline constexpr int kTeams = 2;
inline constexpr std::int32_t kShopRange = 1'700;  // [X-100][U-400] game units around the fountain (was 1 100)

enum class UnitKind : std::uint8_t { Champion, Minion, Turret, Inhibitor, Nexus, Monster, Boss };
enum class MinionType : std::uint8_t { Melee, Ranged, Siege, Super };

enum class CmdKind : std::uint8_t {
    Move, Attack, AttackMove, Cast, Stop, Buy, Sell, LevelUp, Stance, UseItem, Portal, Recall,
    // practice tool only [U-100]
    CheatGold, CheatLevel, CheatNoCooldowns,
    SetAutoAttack,  // slot 0 off / 1 on [U-158]
    Emote,          // slot = EmoteId [U-246]
    Ping,           // slot = PingKind, x / y = where [U-246]
    CancelCast,     // [U-265] Z: cancel the cast / channel (S does not)
    SourceForm,     // [U-293] waterbenders: slot 0 = take water from sources as water, 1 = as ice
    SetCompanion,   // [U-407] slot = CompanionKind
    Steer,          // [U-402] slot = skill, x / y = cursor (sent while the key is held)
    DrawPoint,      // [U-402] slot = skill, x / y = a point of the drawn path; item = 1 → the first point (clears)
    AttackWall,     // [U-393] x / y = a wall cell to hit with autoattacks (champions whose autos have hits_walls)
};

// [U-407] travel companions (see struct Companion)
enum class CompanionKind : std::uint8_t { SkyBison, PolarBearDog, MonkeyCat, Shirshu, Badgermole, Dragon, Count };
inline constexpr const char* kCompanionNames[] = {"Sky Bison", "Polar Bear-Dog", "Monkey-Cat", "Shirshu", "Badgermole", "Dragon"};

// [U-246] emotes (Ctrl+1…6) and pings (like LoL + the bait ping)
enum class EmoteId : std::uint8_t { Laugh, Taunt, Dance, Joke, Wave, Cheer, Count };
enum class PingKind : std::uint8_t { Generic, Danger, Missing, OnMyWay, AssistMe, Bait, Count };
struct PingEvent {
    Team team = Team::Neutral;
    PingKind kind = PingKind::Generic;
    std::int32_t x = 0, y = 0;
    std::uint32_t from = 0;
    std::int64_t ms = 0;
};
struct Cmd {
    CmdKind kind = CmdKind::Stop;
    std::uint8_t slot = 0;       // Slot for Cast / LevelUp, item slot for Sell / UseItem
    std::int16_t item = -1;      // Buy
    std::uint32_t target = 0;    // unit id
    std::int32_t x = 0, y = 0;   // game units
    std::int32_t sx = -1, sy = -1;  // [U-340] the chosen source (water / rock) for skills that start from one; −1 = none
};

enum class OrderKind : std::uint8_t { None, Move, Attack, AttackMove, Cast, Portal, Recall, AttackWall };
struct Order {
    OrderKind kind = OrderKind::None;
    std::uint32_t target = 0;
    std::int32_t x = 0, y = 0;  // game units
    std::uint8_t slot = 0;
    std::int32_t sx = -1, sy = -1;  // [U-340] chosen source
};

struct AbilityRt {
    std::uint8_t rank = 0;
    std::int64_t cd_end_ms = 0;
    MulticastState mc;           // [U-106]
    std::uint32_t boulder = 0;   // [U-27] raised boulder waiting for its push
    std::uint8_t charges_used = 0;       // [U-433] charges spent (0 = all ready) …
    std::int64_t charge_next_ms = 0;     // … the next one comes back then
};

// [X-94] Collision layers. Every check uses the whole body border, not the centre [U-130].
enum CollideBits : std::uint8_t {
    kCollTerrain = 1,    // deep water, elevation step > 1 level [U-93]
    kCollTree = 2,       // living trees
    kCollWall = 4,       // map walls + temporary bent walls (stone / ice) [U-24]
    kCollStructure = 8,  // turrets, inhibitors, Nation Shrines: fixed walls, not bendable [X-95]
    kCollUnit = 16,      // unit vs unit [X-96]; [U-367] ON for everybody: two units collide when BOTH have it.
                         // Spirits, items / effects that let you pass through units clear it.
};
inline constexpr std::uint8_t kCollDefault = kCollTerrain | kCollTree | kCollWall | kCollStructure | kCollUnit;

struct BotMem {
    std::uint8_t lane = 1;
    std::uint8_t mode = 0;  // 0 lane, 1 retreat, 2 fight
    std::int64_t next_think_ms = 0;
    std::int64_t next_path_ms = 0;
};

// [U-254][U-262] WHAT dealt a hit: autoattack, a skill (slot), an item, a burn (damage over time) …
enum class DmgFrom : std::uint8_t { Other, Auto, Skill, Item, Passive, Burn, Keystone, Combo, Chain, Turret, Minion, Monster };
struct DmgTag {
    DmgFrom from = DmgFrom::Other;
    std::uint8_t slot = 0;     // Skill: 0 P … 7 G
    std::int16_t item = -1;    // Item: item id
};
// one hit a champion took — kept for the last 30 s for the death recap [U-262]
// [U-311] what a unit is — skill rules (which units a skill hits / what stops it) and other rules use this
enum class UnitCategory : std::uint8_t { Champion, Minion, SiegeMinion, SuperMinion, Monster, EpicMonster, Turret, Inhibitor, Shrine, Count };
[[nodiscard]] constexpr std::uint16_t cat_bit(UnitCategory c) { return std::uint16_t(1u << int(c)); }
inline constexpr std::uint16_t kAllCategories = 0x1ff;
inline constexpr std::uint16_t kMinionCategories = cat_bit(UnitCategory::Minion) | cat_bit(UnitCategory::SiegeMinion) | cat_bit(UnitCategory::SuperMinion);

// [U-302] named monsters (the order = the network name id; net::kMobNames is the same list)
inline constexpr const char* kMobNameList[] = {"", "Spirit Wolf", "Wandering Spirit", "Cursed Spirit", "Angry Beast", "Bandit", "Face Stealer", "Bandit Archer", "Platypus Bear", "Knowledge Seeker", "Lion Turtle", "Sun Dragons", "Dark Spirit", "Swamp Monster"};
struct TakenHit {
    std::int64_t ms = 0;
    std::uint32_t src = 0;
    DmgTag tag;
    std::int32_t amount = 0;  // HP lost (after defence and shields)
    std::int32_t shielded = 0;  // absorbed by shields
    std::uint8_t sub = 0;
    bool physical = false;
    std::uint8_t src_kind = 255, src_extra = 0;  // [U-302] what dealt it (UnitKind; minion type / monster name id / champion)
    std::uint8_t cc = 255;      // [U-343] a crowd-control entry (StatusKind), amount 0 …
    std::int32_t cc_ms = 0;     // … and how long it held
};
struct Unit {
    std::uint32_t id = 0;
    UnitKind kind = UnitKind::Minion;
    Team team = Team::Neutral;
    bool alive = true;
    std::int8_t champ = -1;  // index into champions()
    MinionType mtype = MinionType::Melee;
    std::uint8_t lane = 0;
    std::int8_t tier = 0;    // turrets: 1 outer … 3 base, 4 nexus turret
    std::string name;

    std::int32_t x = 0, y = 0;  // sub-units
    std::int32_t home_x = 0, home_y = 0;  // spawn / leash point (sub)
    Dir face;
    std::int32_t walk_phase = 0;
    bool moving = false;
    std::int64_t last_move_ms = -1'000'000;
    const BodyDef* body = &kHumanoidBody;

    Stats stats;       // final (level + items + passives)
    std::int32_t hp = 0, res = 0;  // hundredths
    std::int32_t res_max = 0, res_regen = 0;
    std::int32_t level = 1, skill_points = 1;
    std::int64_t xp = 0;
    std::int32_t gold = 500;
    std::int32_t sight = 1'200;    // game units
    std::int32_t attack_range = 40;
    std::int32_t as100 = 65;       // attacks / s × 100 (base)
    std::int32_t bounty = 300;
    StatusSet st;

    Order order;
    std::vector<std::pair<std::int16_t, std::int16_t>> path;  // cells
    std::size_t path_i = 0;
    std::int64_t repath_ms = 0;
    std::int64_t stuck_ms = 0;
    bool path_pending = false;  // [U-287] walking straight; the real path search waits for the next tick

    std::int64_t attack_ready_ms = 0;
    std::int64_t windup_end_ms = 0;
    std::uint32_t windup_target = 0;

    std::array<AbilityRt, std::size_t(Slot::Count)> ab{};
    std::array<const SkillDef*, std::size_t(Slot::Count)> skill{};
    std::int64_t cast_end_ms = 0;
    Order cast;           // the cast being wound up
    bool casting = false;
    std::int64_t channel_end_ms = 0;  // portal / recall
    std::uint32_t channel_interrupt = kIntRecall;  // [U-337] what breaks the running channel

    std::array<std::int16_t, kItemSlots> items = [] {
        std::array<std::int16_t, kItemSlots> a{};
        a.fill(-1);
        return a;
    }();
    std::array<std::int64_t, kItemSlots> item_cd_ms{};
    std::int64_t respawn_ms = 0;
    std::int32_t kills = 0, deaths = 0, assists = 0, cs = 0;
    std::int8_t q_pick = 0, f_pick = -1, g_pick = -1;
    bool stance_b = false;  // [U-72]
    bool no_cooldowns = false;  // practice cheat
    std::int64_t wind_sense_until = 0;
    std::int32_t tmp_size = 0;
    std::int64_t tmp_size_until = 0;
    std::int64_t lotus_until = 0;
    std::int64_t flask_until = 0;  // [U-81] immortal heal window
    std::int32_t flask_revive = 0; // revive heals left
    std::int64_t flask_next_ms = 0;
    std::int64_t untargetable_until = 0;
    std::int64_t stolen_until = 0;  // [X-55] Face Stealer took a skill
    std::uint8_t stolen_slot = 0;
    std::int64_t blocked_ms = 0;    // [U-13] minion stuck time
    std::uint32_t last_hitter = 0;
    std::array<std::pair<std::uint32_t, std::int64_t>, 6> assist_by{};
    std::uint32_t aggro = 0;
    std::int64_t next_scan_ms = 0;
    std::int32_t lane_wp = 0;
    int player = -1;  // network player slot, −1 = none
    bool bot = false;
    BotMem botm;
    std::uint8_t collide = kCollDefault;  // [X-94]
    bool aggressive = false;              // [X-97] jungle monsters that attack on sight
    bool auto_attack = true;              // [X-99] idle champions hit enemies in range
    bool auto_acquired = false;
    std::int64_t heat_next_ms = 0, seismic_next_ms = 0;
    std::int64_t last_hurt_ms = -100'000;  // monsters leash [X-107]
    std::uint8_t shield_sub = std::uint8_t(SubElement::Physical);  // kind of my shield, shown on the HP bar [U-155]
    std::int64_t aggro_until = 0;          // minion "call for help" ends [X-108]
    std::int32_t fly_height = 120;         // [U-224] height while flying (Sky Ascension sets it)
    std::uint8_t lifted_by = 0;            // [U-249] kAirSelf / kAirAlly / kAirEnemy while in the air
    std::uint32_t last_victim = 0;         // who I hit last … [U-194] minion calls for help
    std::int64_t last_victim_ms = -100'000;
    std::int32_t redirect_x = 0, redirect_y = 0;  // [U-185] where redirected lightning goes (cursor at cast)
    // ---- loadout [U-177] and its keystone state
    Loadout loadout;
    std::uint32_t surge_target = 0;
    std::int8_t surge_count = 0;
    std::int64_t surge_ms = 0, stoneheart_ready_ms = 0, chirush_until = 0;
    // ---- statistics for the post-game screen [U-179]
    std::int64_t stat_dmg_champs = 0, stat_dmg_taken = 0, stat_healing = 0, stat_shielding = 0, stat_gold = 0;
    std::int64_t stat_keystone = 0;  // what the keystone did (see KeystoneDef::impact)
    std::array<std::int64_t, kItemSlots> item_impact{};  // [U-209] what each item's effect did this game
    std::vector<TakenHit> taken;   // [U-262] champions: hits taken in the last 30 s
    std::int64_t burn_acc = 0;     // [U-254] burn damage gathered since the last burn hit (hundredths)
    std::uint32_t burn_src = 0;
    std::int64_t burn_flush_ms = 0;
    std::int32_t spree = 0, multi = 0;  // [U-263] kills without dying / kills in a row (10 s each)
    std::int32_t death_streak = 0;      // [U-413] deaths without a kill (lowers the bounty)
    CompanionKind companion = CompanionKind::SkyBison;  // [U-407] chosen travel animal
    bool riding = false;                // [U-407] carried home by the companion: no moving, casting or attacking
    std::int64_t proc_ready_ms = 0;     // [U-416] the passive's own cooldown (procs)
    std::array<std::int64_t, 2> sensed_until{};  // [U-431] sensed by team 0 / 1 (seismic, heat, sound) until then
    std::uint8_t steer_slot = 0;        // [U-402] steering this skill's projectiles …
    std::int32_t steer_x = 0, steer_y = 0;
    std::int64_t steer_until = 0;       // … while the key is held (refreshed by every Steer command)
    std::vector<std::pair<std::int32_t, std::int32_t>> draw_pts;  // [U-402] the drawn wall path (max 40 points)
    std::uint8_t draw_slot = 0;
    bool rage_boost = false;           // [U-268] the skill being cast was paid with rage
    std::int64_t avatar_until = 0;     // [U-279] Avatar State
    std::uint8_t cycle_mask = 0;       // [U-279] Elemental Cycle: elements used (bit per element)
    bool cycle_ready = false;          //         all four used → the next skill +40 %
    bool cycle_boost = false;          //         the skill being cast now gets it
    bool source_ice = false;           // [U-293] waterbender: water taken from sources becomes ice (else water)
    std::int32_t water = 0, water_max = 0;  // [U-339] carried water of champions whose resource is NOT water (the Avatar)
    std::int64_t magic_until = 0;      // [U-264] non-bender magic stance
    std::uint8_t awakened = 255;       // [U-296] a non-bender's awakened Element (255 = not awakened)
    std::uint8_t stance_el = 255;      // [U-448] the magic stance in use: an Element, or 4 = the own physical set
    std::array<const SkillDef*, 5> base_set{};  // [U-448] P Q W E R as before the awakening (the physical stance)
    std::uint8_t jbuff = 0;            // [U-366] JungleBuff the champion carries …
    std::int64_t jbuff_until = 0;      // … until then
    std::int8_t mob = -1;              // [U-308] index in kMonsters (game/mobs/monsters.hpp), −1 = not a monster
    std::uint32_t turret_last = 0;     // [U-309] turrets: the champion of the last shot …
    std::int32_t turret_streak = 0;    //         … and how many shots in a row
    std::int64_t dried_ms = -1;        // [U-344] the moment a fire hit dried me (no burn from that hit)
    std::int64_t stat_dmg_from_champs = 0;  // [U-296] damage taken from champions (decides the random element)
    std::uint8_t magic_sub = 0;        //         its SubElement
    std::int64_t last_kill_ms = -100'000;
    std::uint8_t emote = 255;  // [U-246] EmoteId playing now (255 = none)
    std::int64_t emote_until = 0;
    std::string fail;          // last refused order, shown to the player [X-103]
    std::int64_t fail_ms = -1;

    [[nodiscard]] std::int32_t ux() const { return x / kSub; }
    [[nodiscard]] std::int32_t uy() const { return y / kSub; }
    [[nodiscard]] UnitCategory category() const;  // [U-311]
    [[nodiscard]] bool is_structure() const {
        return kind == UnitKind::Turret || kind == UnitKind::Inhibitor || kind == UnitKind::Nexus;
    }
    [[nodiscard]] bool flying() const { return st.has(StatusKind::Flying); }
};

enum class FxKind : std::uint8_t { Bolt, Wave, Burst, Field, Cone, Tornado, Lantern, Auto, TowerShot, Boulder, Heal, Steam };

struct Effect {
    std::uint32_t id = 0;
    FxKind kind = FxKind::Bolt;
    Team team = Team::Neutral;
    std::uint32_t owner = 0;
    std::uint32_t target = 0;  // homing (autos, tower shots)
    const SkillDef* def = nullptr;
    SubElement sub = SubElement::Physical;
    Flow flow = Flow::Plain;
    std::int32_t x = 0, y = 0;    // sub
    std::int32_t vx = 0, vy = 0;  // sub per tick
    std::int32_t travel_left = 0; // sub
    std::int32_t travel_total = 0; // sub, at launch (sweet spots [U-222])
    Dir dir;
    std::int32_t radius = 0, width = 0, length = 0;  // units
    std::int32_t damage = 0;      // points
    bool physical = false;
    StatusKind cc = StatusKind::Count;
    std::int32_t cc_ms = 0, cc_val = 0, knock = 0;
    std::int64_t born_ms = 0, arm_ms = 0, end_ms = 0, next_tick_ms = 0;
    std::int32_t tick_ms = 500;
    bool pierce = false, group = false, ally = false;
    std::int8_t feeds = 0;          // [U-135]
    Combo combo = Combo::None;      // [U-134]
    std::uint32_t seed = 0;
    std::array<std::uint32_t, 24> hit{};
    std::uint8_t nhit = 0;
    std::uint32_t last_wall_cell = ~0u;  // [U-288] one hit per wall cell per effect
    std::int32_t light = 0;
    std::int64_t light_until = 0;
    bool dead = false;
    Stats src;                      // caster stats (pen, CC amplify)
    std::int32_t power_pct = 100;
    std::int32_t base_height = 0;   // [U-216] ground height where it was cast

    [[nodiscard]] std::int32_t ux() const { return x / kSub; }
    [[nodiscard]] std::int32_t uy() const { return y / kSub; }
};

struct LightOut {
    std::int32_t x = 0, y = 0, radius = 0;
    std::uint8_t strength = 100;
};
struct TimedLight {  // [U-256] afterglow of fire skills, burning trees
    LightOut l;
    std::int64_t until = 0;
};
struct WaveOut {
    SenseWave w;
    std::int64_t ms = 0;
    Team team = Team::Neutral;
    std::uint32_t only_for = 0;  // 0 = whole team
};
struct CellChange {
    std::int16_t x = 0, y = 0;
    std::uint32_t raw = 0;
    std::uint8_t pub = 0, pad[3]{};  // [U-288] bit 1 = a change by time: every map shows it at once; [U-436] bit 2 / 4 = only
                                     // the Fire / Earth team's map (its own bending: the creator's team sees it without vision)
};
struct ElevLayer {  // [U-440] one raise of the ground on one cell: they ADD UP and each ends on its own
    std::int16_t x = 0, y = 0;
    std::int8_t amount = 0;
    std::int64_t until_ms = 0;
};
struct WallCell {  // [U-288] a player-made wall cell
    std::int32_t hp = 0, max = 0;
    std::uint32_t seg = 0;  // [U-380] its wall segment (shared HP), 0 = a single cell
};
struct WallSeg {  // [U-380] one 100-unit piece of a skill wall: all its cells share the HP and fall together
    std::int32_t hp = 0, max = 0;
    std::vector<std::uint32_t> cells;
    std::int64_t until_ms = 0;
    // [U-443] its shape for drawing: centre (units), direction along it, thickness, height, kind, who made it
    std::int32_t x = 0, y = 0;
    Dir along{kOne, 0};
    std::int16_t thick = 66;
    std::uint8_t level = 2, kind = 0;
    Team team = Team::Neutral;
};
struct WallBreak {  // [U-341] a wall piece fell: broken by a hit (pushed in dx / dy) or crumbled by time (0 / 0)
    std::int16_t x = 0, y = 0;
    std::int16_t dx = 0, dy = 0;  // direction × 1000
    std::uint8_t kind = 0;        // Wall
    std::int64_t ms = 0;
};
struct DeathRecap {
    std::uint32_t victim = 0, killer = 0;
    std::int64_t ms = 0;
    std::vector<TakenHit> hits;  // the fight that killed (gap < 10 s, at most 30 s)
};
// [U-263] announcements (the client turns them into text + announcer voice)
enum class AnnounceKind : std::uint8_t {
    FirstBlood, Kill, DoubleKill, TripleKill, QuadraKill, PentaKill, Shutdown,
    KillingSpree, Rampage, Unstoppable, Dominating, Godlike, Legendary,  // 3, 4, 5, 6, 7, 8+ kills without dying
    Ace, Executed, TurretDestroyed, InhibitorDestroyed, ShrineDestroyed, BossSlain, MinionsSpawned,
    Awakened,  // [U-296] a = the champion, b = its new Element
    Count
};
struct Announce {
    AnnounceKind kind = AnnounceKind::Kill;
    std::uint32_t a = 0, b = 0;  // killer / victim (or the structure)
    Team team = Team::Neutral;    // the team that scored
    std::int64_t ms = 0;
    std::uint8_t a_kind = 255, a_extra = 0;  // [U-363] what the killer was (shown even when it is not on screen)
};
// [X-103] every damage instance, for floating numbers / swing marks on the client
struct HitEvent {
    std::uint32_t src = 0, dst = 0;
    std::int32_t amount = 0;  // HP points after defence and shields
    std::int64_t ms = 0;
    std::uint8_t is_auto = 0, sub = 0;
    std::uint8_t flags = 0;  // kHitSweet, kHitPhysical, kHitDot [U-222][U-223][U-254]
};
inline constexpr std::uint8_t kHitSweet = 1, kHitPhysical = 2, kHitDot = 4;
// [U-159][X-109] gold / XP gained on a kill, shown as small text where it died
struct RewardEvent {
    std::uint32_t to = 0;
    std::int32_t x = 0, y = 0, gold = 0, xp = 0;
    std::int64_t ms = 0;
};
// [U-235] falcons: fly to a spot, watch, fly back; on landing the team sees the recording as ghosts
struct GhostUnit {
    std::uint32_t id = 0;
    std::int8_t champ = -1;
    std::uint8_t team = 0, kind = 0;
    std::int32_t x = 0, y = 0;
};
struct GhostFrame {
    std::int64_t t = 0;  // ms since the watch began
    std::vector<GhostUnit> units;
};
struct FalconRun {
    std::uint32_t owner = 0;
    Team team = Team::Neutral;
    std::int32_t x = 0, y = 0;    // game units
    std::int32_t tx = 0, ty = 0;  // the spot to watch
    std::uint8_t state = 0;       // 0 flying out, 1 watching, 2 flying back, 3 landed (replaying)
    std::int64_t watch_from = 0, replay_from = 0;
    std::vector<GhostFrame> frames;
};
inline constexpr std::int32_t kFalconSpeed = 900;      // units / s
inline constexpr std::int64_t kFalconWatchMs = 8'000;
inline constexpr std::int64_t kFalconFrameMs = 250;

struct FeedLine {
    std::int64_t ms = 0;
    std::string text;
};

struct MatchConfig {
    std::uint32_t tick_hz = 128;                 // [U-102]
    bool soft_cap = false;                       // [X-45]
    std::int64_t first_wave_ms = 65'000;         // [X-08]
    std::int64_t wave_period_ms = 30'000;
    std::int64_t start_ms = 0;                   // practice: start later in the day/moon cycle
    bool practice = false;                       // [U-100] cheats allowed
    std::uint32_t seed = 1;
    bool classic_recall = false;                 // [U-407] true = the old 6 s recall channel instead of a companion
    // [U-416][U-454] ONE unified game: every system is on (vamp, healing reduction, denies, the elemental systems).
    // `ruleset` only narrows it down when wanted: only the LoL-like code (no denies) or only the Dota-like code.
    std::uint8_t ruleset = 0;                    // RuleSet
    std::uint8_t mode = 0;                       // [U-454] GameMode (how champions are picked) — a different thing
    std::uint8_t map = 0;                        // [U-420] index in maps() (game/match/maps.hpp)
};
enum class RuleSet : std::uint8_t { AllSystems, LeagueLike, DotaLike, Count };
inline constexpr const char* kRuleSetNames[] = {"All systems", "LoL-like only", "Dota-like only"};
[[nodiscard]] constexpr bool classic_rules(std::uint8_t) { return true; }  // [U-454] vamp + healing reduction: always on
[[nodiscard]] constexpr bool denies_allowed(std::uint8_t r) { return r != std::uint8_t(RuleSet::LeagueLike); }  // LoL has no denies
// [U-454] game modes: how the champions are chosen (maps and systems are separate choices)
enum class GameMode : std::uint8_t { Classic, BlindPick, AllRandom, Count };
inline constexpr const char* kGameModeNames[] = {"Classic", "Blind Pick", "All Random"};
inline constexpr const char* kGameModeDescs[] = {"pick your champion, see the others' picks", "nobody sees the enemy picks until the match",
                                                 "everyone gets a random champion"};

// [U-407] Travel companions: recall calls the chosen animal from the base; it runs to the champion and carries them
// home. Cannot be interrupted or cancelled; the rider can be hit and killed on the way.
inline constexpr std::int32_t kCompanionSpeed = 1'500;  // units / s, both ways [U-429]
struct Companion {
    std::uint32_t owner = 0;
    CompanionKind kind = CompanionKind::SkyBison;
    std::int32_t x = 0, y = 0;  // game units
    bool carrying = false;      // false: on the way to the champion; true: carrying them home
};

// The terrain timers: walls collapse [U-24], trees regrow [U-14], lava cools [X-56], fog clears [X-57].
enum class CellTimerKind : std::uint8_t { Restore, LavaToStone, Regrow };
struct CellTimer {
    std::int16_t x = 0, y = 0;
    std::uint32_t raw = 0;
    std::int64_t at_ms = 0;
    CellTimerKind kind = CellTimerKind::Restore;
    bool iced = false;  // [U-356] lava: already cooled by ice once (rest time halved)
};

// Prepares the default map for a match (camps cleared, spirit island) — same on server and client.
void prepare_match_grid(Grid& g);

class World {
public:
    explicit World(MatchConfig cfg = {});

    // Setup
    std::uint32_t add_champion(Team team, int champ, int q_pick, int f_pick, int g_pick, bool bot, int player,
                               std::string name, Loadout loadout = {});
    void command(std::uint32_t unit_id, const Cmd& c);
    void step();  // one fixed tick

    // Queries
    [[nodiscard]] Unit* find(std::uint32_t id);
    [[nodiscard]] const Unit* find(std::uint32_t id) const;
    [[nodiscard]] bool visible_to(Team t, const Unit& u) const;
    [[nodiscard]] bool visible_to(Team t, const Effect& e) const;
    [[nodiscard]] Shape hitbox(const Unit& u) const;
    [[nodiscard]] std::int32_t size_pct(const Unit& u) const;
    [[nodiscard]] Shape shape_of(const Effect& e) const;
    [[nodiscard]] bool targetable(const Unit& u) const;
    [[nodiscard]] bool vulnerable(const Unit& u) const;  // structure order rules
    [[nodiscard]] std::int64_t edge_gap(const Unit& a, const Unit& b) const;
    [[nodiscard]] ResolvedAbility resolved(const Unit& u, Slot s) const;
    [[nodiscard]] bool in_base(const Unit& u) const;
    [[nodiscard]] std::pair<std::int32_t, std::int32_t> fountain(Team t) const;  // units
    [[nodiscard]] std::int32_t attack_period_ms(const Unit& u) const;
    [[nodiscard]] bool can_cast_now(const Unit& u, Slot s, std::string* why = nullptr, std::int32_t sx = -1, std::int32_t sy = -1) const;
    // [U-426] a skill that only acts on something in the world (lava, water, a lifted stone…) is refused when there is
    // nothing — it is never "burned" on nothing (no cooldown, no cost). Returns the reason, or "" when it may be cast.
    [[nodiscard]] bool sensed_by(Team t, const Unit& o) const { return t != Team::Neutral && o.sensed_until[std::size_t(t) & 1] > now_ms; }
    [[nodiscard]] bool can_target(const Unit& caster, const SkillDef& d, const Unit& t) const;  // [U-431] seen, or sensed if allowed
    [[nodiscard]] std::string_view nothing_to_act_on(const Unit& u, const SkillDef& d, std::int32_t sx, std::int32_t sy) const;
    [[nodiscard]] PassiveId special_passive(const Unit& u) const;  // seismic / heat sense from F/G
    [[nodiscard]] bool has_passive(const Unit& u, PassiveId p) const;
    // ---- water and elevation [U-188..U-191]
    [[nodiscard]] bool on_deep_water(const Unit& u) const;  // centre on unfrozen deep water
    [[nodiscard]] bool on_any_water(const Unit& u) const;   // deep or shallow water
    [[nodiscard]] bool on_ground_water(const Unit& u) const;  // [U-292] shallow water or a puddle (not frozen, not deep)
    // [U-293] cold passing through steam: the steam around (x, y) falls as water (or ice); those there get wet
    void condense_steam(std::int32_t x, std::int32_t y, std::int32_t radius, const Unit* by, bool as_ice);
    // [U-293] a waterbender casting next to a puddle / ice patch bends from it: +15 %, the source is used up, and the
    // skill takes the bender's chosen form (water / ice). Returns the bonus %; may change `sub`.
    int draw_from_water_source(const Unit& u, SubElement& sub);
    // [U-294] a big water source near the unit (river / sea / pole ice / ice wall, or a puddle / ice patch → used up
    // when `consume`). Waterbenders carry only a little water: ice walls need this.
    // returns 0 none, 1 a lasting source (river / sea / pole / ice wall), 2 a puddle / ice patch (index in *patch)
    [[nodiscard]] int big_water_near(const Unit& u, std::size_t* patch = nullptr) const;
    void use_water_source(const Unit& u);  // a puddle / ice patch used for an ice wall is gone
    // [U-339][U-340] water as numbers
    enum class WaterSource : std::uint8_t { None, Lasting, Patch, IceWall };  // river / sea / glacier, puddle / ice patch, ice wall
    [[nodiscard]] WaterSource water_source_at(std::int32_t x, std::int32_t y, std::size_t* patch = nullptr) const;
    // a water skill has its water: a chosen source in reach, or one near the caster (then it costs no carried water)
    [[nodiscard]] bool water_sourced(const Unit& u, const SkillDef& d, std::int32_t sx, std::int32_t sy) const;
    void take_from_source(const Unit& u, std::int32_t sx, std::int32_t sy);
    // [U-398] water moved away from a lasting source: shallow water → wet ground, deep → shallow, for ms
    void drain_water(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t ms);
    // [U-378] a rock to bend (lift a rock, then throw it): any stone wall cell — a map rock, a bent wall, a thrown rock
    [[nodiscard]] bool rock_source_at(std::int32_t x, std::int32_t y) const;  // a puddle is used up, an ice wall melts a bit
    [[nodiscard]] static bool water_skill(const SkillDef& d) { return base_element(d.sub) == Element::Water; }
    [[nodiscard]] bool elevated(const Unit& u) const;       // flying or riding water → melee / ground can't hit
    [[nodiscard]] bool can_hit_elevated(const Effect& e) const;
    // [U-216] heights (game units): terrain level × 40 + airborne 60 / water ride 80 / flying 120
    [[nodiscard]] std::int32_t unit_height(const Unit& u) const;
    [[nodiscard]] std::int32_t ground_height(std::int32_t ux, std::int32_t uy) const;
    [[nodiscard]] std::int32_t effect_reach(const Effect& e) const;      // highest point the hitbox reaches
    [[nodiscard]] bool reaches(const Effect& e, const Unit& target) const;

    MatchConfig cfg;
    Grid grid;
    std::vector<Unit> units;
    std::vector<Effect> fx;
    std::uint64_t tick = 0;
    std::int64_t now_ms = 0;
    SkyState sky;
    bool over = false;
    Team winner = Team::Neutral;
    std::array<VisionMap, kTeams> vision;
    LightMap light;
    std::vector<LightOut> lights;          // rebuilt at 16 Hz, team-neutral [U-117]
    std::uint64_t lights_tick = 0;         // [U-280] tick of the last lights rebuild (the server sends lights only then)
    std::vector<TimedLight> glows;         // [U-256] lights that stay a while (skill afterglow, burning trees)
    std::vector<LightOut> lava_lights_;    // [U-256] glowing magma, rebuilt every 2 s
    std::int64_t lava_lights_ms_ = -10'000;
    std::vector<WaveOut> waves;            // last 2 s of sense waves [U-141..U-143]
    std::vector<CellChange> cell_changes;  // for the network (cleared by the server)
    std::vector<FeedLine> feed;            // kill feed / events
    std::vector<HitEvent> hits;            // last 1 s of damage [X-103]
    std::uint8_t hit_flags_ = 0;           // flags for the next HitEvent (set around deal_damage)
    DmgTag dmg_tag_{};                     // [U-254] source of the next hit (set around deal_damage; Other = guess)
    std::vector<Announce> announces;       // [U-263] (the server sends and clears them)
    std::vector<DeathRecap> recaps;        // [U-262] (the server sends and clears them)
    bool first_blood = false;
    // [U-287] profiler (tools/sim_run --profile; the server logs slow ticks): ms per part of the last step
    static constexpr int kProfParts = 11;
    static constexpr const char* kProfNames[kProfParts] = {"match", "bots", "collision", "units", "effects", "terrain",
                                                           "lights", "vision", "senses", "falcons", "cleanup"};
    bool profile = false;
    std::array<double, kProfParts> prof_ms{};
    // [U-287] A* budget per tick, in explored cells (~0.1 µs each): a tick never gets stuck in pathfinding
    static constexpr int kPathCellsPerTick = 15'000;
    int path_budget_ = kPathCellsPerTick;
    std::vector<RewardEvent> rewards;      // last 1 s of kill rewards [X-109]
    std::vector<PingEvent> pings;          // last 4 s of pings [U-246]
    std::vector<Cloud> clouds;             // steam / smoke / sand / snow [U-241]
    std::vector<FalconRun> falcons;        // [U-235]
    void step_falcons();
    std::vector<std::uint8_t> cloud_cells; // cloud_mask() for vision
    void add_cloud(CloudKind k, std::int32_t x, std::int32_t y, std::int32_t radius, std::int64_t ms);
    void scorch(const Effect& e);          // [U-227][U-244] burn / break trees and set the ground on fire under a skill
    void cool_lava(std::int32_t x, std::int32_t y, std::int32_t radius);  // [U-225][U-241]
    void dry(Unit& u);                     // [U-241] remove wet
    void reward(Unit& champ, const Unit& dead, std::int32_t gold, std::int64_t xp);
    void refuse(Unit& u, std::string why); // tell the player why an order did nothing [X-103]
    std::array<std::int32_t, kTeams> team_kills{};

    // used by sim parts
    std::uint32_t next_id = 1;
    std::unordered_map<std::uint32_t, std::size_t> index;
    std::vector<CellTimer> cell_timers;
    std::int64_t lava_melt_next_ = 0;  // [U-356]
    std::array<std::int32_t, 2> lion_stacks{};
    std::vector<Companion> companions;          // [U-407]
    void step_companions();
    void call_companion(Unit& u);  // [U-364] Lion Turtle stacks per team (permanent)
    void apply_monster_growth(Unit& m);         // [U-365] stats for its respawn count
    [[nodiscard]] bool natural_wall(int cx, int cy) const;  // [U-360] a map rock (no timer brings it down)
    void lava_melts_walls();
    void drop_rock(std::int32_t x, std::int32_t y, bool magma);  // [U-359] a thrown rock stays where it lands
    // [X-94] collision helpers
    std::vector<std::uint8_t> struct_block;  // cells under alive structures (never bendable) [X-95]
    std::vector<std::uint8_t> clearance;     // cells to the nearest blocker (Chebyshev, max 7) for A*
    std::vector<Circle> rock_bodies_;
    std::vector<Effect> fx_pending_;
    std::vector<ElevLayer> elev_layers_;     // [U-440]
    void raise_ground(int cx, int cy, int amount, std::int64_t until_ms);  // adds a layer
    void apply_elevation(int cx, int cy);    // the cell's height = its ground + every active layer
    void step_elevation();         // [U-383] effects made while the effect loop runs (added after it)        // [U-386] stone skills (boulders) this tick: they block like walls (kCollWall)
    bool clearance_dirty = true;
    void rebuild_collision_maps();
    [[nodiscard]] int body_violations(const Unit& u, std::int32_t x_sub, std::int32_t y_sub) const;
    [[nodiscard]] std::uint8_t collide_mask(const Unit& u) const;
    [[nodiscard]] bool protected_cell(int cx, int cy) const;
    std::int64_t next_wave_ms = 0;
    std::int32_t wave_no = 0;
    std::uint32_t rng = 1;

    std::uint32_t random();
    Unit& spawn_unit(Unit u);
    Effect& spawn_fx(Effect e);
    void set_cell(int cx, int cy, Cell c);
    void add_feed(std::string text);
    void recompute_stats(Unit& u);

    // sim_units.cpp
    void step_units();
    void move_towards(Unit& u, std::int32_t tx, std::int32_t ty, std::int32_t speed_sub);
    void follow_path(Unit& u);
    void path_to(Unit& u, std::int32_t tx, std::int32_t ty);
    std::int32_t move_speed_sub(const Unit& u) const;
    void turn_towards(Unit& u, Dir want);
    bool walkable_at(const Unit& u, std::int32_t x_sub, std::int32_t y_sub) const;
    void place_on_walkable(Unit& u);
    void resolve_overlaps(Unit& u);  // [X-94] push out of structures / body-blocking units
    void separate_minions();         // [U-299] minions softly keep apart (no stacking, no hard blocking)
    void regen(Unit& u);
    // sim_combat.cpp
    void deal_damage(Unit* src, Unit& dst, std::int32_t points, SubElement sub, bool physical, bool is_auto,
                     const Stats* src_stats = nullptr);
    // after defence: shields, HP, statistics, hit events, death [U-254] (burns come in here directly)
    void apply_hit(Unit* src, Unit& dst, std::int32_t hund, SubElement sub, bool physical, bool is_auto);
    void announce(AnnounceKind k, std::uint32_t a, std::uint32_t b, Team t);
    Unit make_monster(std::string_view name, std::int32_t x_sub, std::int32_t y_sub);  // [U-308]
    // [U-302] index of a monster's name in kMobNameList (0 = not a named monster)
    [[nodiscard]] static std::uint8_t mob_name_id(const Unit& u);
    // [U-264] the element a skill really has now (a non-bender in a magic stance turns physical into magic)
    [[nodiscard]] SubElement skill_sub(const Unit& u, const SkillDef& d) const {
        if (u.magic_until > now_ms && d.sub == SubElement::Physical && d.kind != SkillKind::MagicStance) return SubElement(u.magic_sub);
        return d.sub;
    }
    void heal(Unit& u, std::int32_t hundredths);
    void apply_cc(const Stats& src_stats, std::uint32_t src_id, Unit& dst, StatusKind k, std::int32_t ms,
                  std::int32_t value, SubElement sub = SubElement::Physical);
    void knock(Unit& dst, std::int32_t from_x, std::int32_t from_y, std::int32_t dist_units, std::uint32_t src_id = 0);
    static constexpr std::int32_t kKnockSpeed = 1'200;  // [U-368] units / s: a displacement holds the unit dist / speed
    static constexpr std::uint8_t kCcDisplaced = 254;   // [U-368] TakenHit::cc marker for a displacement
    void interrupt(Unit& u, std::uint32_t event);  // [U-337] break the cast / channel if its rules say so
    void update_attack(Unit& u);
    void update_attack_wall(Unit& u);  // [U-393]
    void passive_proc(Unit& u, Unit* target, std::uint8_t trigger);  // [U-416] the champion passive's proc (with its cooldown)
    void fire_auto(Unit& u, Unit& target);
    void auto_extras(Unit& u, Unit& primary, std::int32_t dmg, SubElement sub, bool physical);
    void attack_on_the_move(Unit& u);
    void start_cast(Unit& u, const Order& o);
    void finish_cast(Unit& u);
    void cast_skill(Unit& u, Slot slot, std::int32_t tx, std::int32_t ty, std::uint32_t target);
    void step_effects();
    void effect_hits(Effect& e);
    void effect_interactions(Effect& e, std::size_t i);  // mechanics/interactions.cpp
    void on_kill(Unit* killer, Unit& dead);
    // sim_terrain.cpp
    // [U-288] walls with HP
    std::unordered_map<std::uint32_t, WallCell> walls;  // key = y × kGridSize + x
    bool cell_public_ = false;                           // set_cell: the change is public (by time / destruction)
    Team cell_team_ = Team::Neutral;                     // [U-436] set_cell: this team made it → its map shows it at once
    void add_wall_cell(int cx, int cy, Wall kind);
    void damage_wall(int cx, int cy, std::int32_t amount, Dir from = {});  // ≤ 0 HP → the cell falls (public)
    std::vector<WallBreak> wall_breaks;  // [U-341] the last second of falling wall pieces (for the animation)
    // hit on a wall cell by an effect: damage by element + the owner's wall stats
    void hit_wall(Effect& e, const Unit* owner, int cx, int cy);
    // casting next to a wall that feeds this skill: +15 % damage, the wall loses HP. Returns the bonus %.
    int draw_from_wall(const Unit& u, SubElement sub);
    void raise_wall(std::int32_t x, std::int32_t y, Dir along, std::int32_t length, Wall kind, std::int32_t life_ms,
                    Team by_team, bool earthbending, const WallSpec& ws = {});
    void raise_ring(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms, Team by_team,
                    Wall kind = Wall::Stone, const WallSpec& ws = {});
    // [U-380] one segment (100 long, ws.thick wide, centred at x / y); units that fit on it are lifted on top
    void raise_segment(std::int32_t x, std::int32_t y, Dir along, Wall kind, std::int32_t life_ms, Team by_team, bool earthbending,
                       const WallSpec& ws);
    std::unordered_map<std::uint32_t, WallSeg> segs;  // [U-380]
    std::uint32_t next_seg_ = 1;
    void raise_pillar(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms, Team by_team);
    void lava_pool(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms);
    void steam_cloud(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms);
    void freeze_water(std::int32_t x, std::int32_t y, std::int32_t radius, std::int32_t life_ms);  // [U-188] water → ice floor
    void burn_cell(int cx, int cy);
    void step_terrain();
    std::vector<std::pair<int, int>> earth_events;  // [U-142] where earth was bent this tick (cells)
    std::vector<Team> earth_event_team;
    // sim_match.cpp
    void setup_match();
    void spawn_structures();  // mobs/structures.cpp
    void spawn_jungle();      // mobs/jungle.cpp
    void step_match();
    void spawn_wave();
    void step_minion(Unit& u);
    [[nodiscard]] rules::MinionPriority minion_priority(const Unit& me, const Unit& enemy) const;
    void step_turret(Unit& u);
    void step_monster(Unit& u);
    std::int64_t give_xp(Unit& u, std::int64_t xp);  // returns XP really gained (sky boosts)
    void level_up(Unit& u, Slot s);
    void respawn(Unit& u);
    void buy(Unit& u, int item);
    void use_item(Unit& u, int slot, std::int32_t target_x = 0, std::int32_t target_y = 0);
    // sim_senses.cpp
    void step_vision(int only_team = -1);  // −1 = every team
    void step_senses();
    void step_lights();
    // bots.cpp
    void step_bot(Unit& u);
};

[[nodiscard]] std::int32_t isqrt32(std::int64_t v);

// Small shared helpers for effects.
[[nodiscard]] constexpr bool fire_family(SubElement s) { return base_element(s) == Element::Fire; }
[[nodiscard]] inline bool already_hit(const Effect& e, std::uint32_t id) {
    for (int i = 0; i < e.nhit; ++i)
        if (e.hit[std::size_t(i)] == id) return true;
    return false;
}
inline void mark_hit(Effect& e, std::uint32_t id) {
    if (e.nhit < e.hit.size()) e.hit[e.nhit++] = id;
}

// [X-58] mark every connected shallow-water cell from (sx, sy) (mechanics/interactions.cpp)
void flood_water(const Grid& g, int sx, int sy, std::vector<std::uint8_t>& mark, std::vector<std::pair<int, int>>& cells);

// Shared by server and client: the exact fluid shape of an effect at a tick [U-131] — the client
// draws precisely what the server tests [U-132].
[[nodiscard]] Shape fx_shape(FxKind kind, std::int32_t ux, std::int32_t uy, std::int32_t radius, std::int32_t width,
                             std::int32_t length, Dir dir, Flow flow, std::uint32_t seed, std::uint32_t tick);
// Body table for the network (index ↔ BodyDef).
[[nodiscard]] const BodyDef& body_by_index(int i);
[[nodiscard]] int body_index(const BodyDef* b);
[[nodiscard]] inline std::int64_t dist_units(const Unit& a, std::int32_t x, std::int32_t y) {
    return isqrt(dist2(a.ux(), a.uy(), x, y));
}

}  // namespace em
