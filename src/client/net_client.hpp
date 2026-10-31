#pragma once
// [X-01][X-02][U-173] Client side of the protocol. A dedicated network thread services enet every
// millisecond (acks + packets are not delayed by the frame rate); the game thread reads the decoded
// state in poll(). Pure data — rendering lives in game_view / hud.
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/grid.hpp"
#include "game/world.hpp"
#include "net/protocol.hpp"

struct _ENetHost;
struct _ENetPeer;

namespace em {

struct Snapshot {
    std::uint64_t tick = 0;
    std::int64_t now_ms = 0;
    std::int32_t kills[2] = {};
    std::vector<net::NetUnit> units;
    std::vector<net::NetFx> fx;
    std::vector<net::NetTether> tethers;  // [U-456]
    std::vector<net::NetLight> lights;
    std::vector<net::NetPath> paths;  // [U-203]
    std::vector<net::NetPing> pings;  // [U-246]
    std::vector<net::NetCloud> clouds;  // [U-241]
    std::vector<net::NetWallHp> walls;  // [U-288] damaged wall pieces
    std::vector<net::NetCamp> camps;    // [U-365] jungle camps + respawn timers
    std::vector<net::NetCompanion> companions;  // [U-407]
    std::vector<net::NetWallSeg> segs;          // [U-443] wall segments (drawn rotated)
    std::vector<net::NetFalcon> falcons;  // [U-235]
    std::vector<net::NetGhost> ghosts;
    std::vector<net::NetScore> scores;
    net::NetMe me{};
    double arrived = 0;  // local seconds
};

struct RosterEntry {
    std::uint32_t id = 0;
    std::uint8_t team = 0;
    std::int8_t champ = 0, q = 0, f = -1, g = -1;
    std::string name;
};

struct LocalWave {
    net::NetWave w;
    double born = 0;  // local seconds
};
struct LocalHit {
    net::NetHit h;
    double born = 0;
};
struct LocalReward {
    net::NetReward r;
    double born = 0;
};
struct TextLine {
    std::string text;
    double born = 0;
};
struct LocalAnnounce {  // [U-263]
    net::NetAnnounce a;
    double born = 0;
};
struct DeathRecapView {  // [U-262]
    std::uint32_t killer = 0;
    std::vector<net::NetRecapHit> hits;
    double born = -1;
};
struct ChatLine {  // [U-182]
    net::Chat channel = net::Chat::Lobby;
    std::string from, text;
    double born = 0;
};

class NetClient {
public:
    enum class State { Idle, Connecting, Lobby, Match, Rejected, Disconnected };

    NetClient();
    ~NetClient();
    bool connect(const std::string& host, std::uint16_t port, const std::string& name, std::uint64_t secret,
                 std::string version, bool allow_skins = true);
    void disconnect();
    void poll(double now_s);  // call every frame: decodes what the network thread received

    void send_pick(int team, int champ, int q, int f, int g, bool ready, const Loadout& lo);
    void send_ban(int champ);
    void send_start();
    void send_cmd(const Cmd& c);
    void send_chat(net::Chat ch, const std::string& text, const std::string& to = {});
    void send_skin(const std::vector<std::uint8_t>& bmp);

    State state = State::Idle;
    std::string reject_reason, server_version, client_version, server_name, server_motd;
    std::uint8_t my_id = 0;
    bool is_host = false, practice = false;
    std::uint32_t tick_hz = 128;
    std::vector<net::LobbySlot> lobby;
    bool lobby_in_match = false;
    net::DraftPhase phase = net::DraftPhase::Picking;
    bool bans_enabled = false;
    int lobby_timer_s = 0;        // [U-276] seconds left on the pick timer when the lobby came (0 = no timer)
    double lobby_timer_at = 0;

    // match
    std::uint32_t my_unit = 0;
    std::uint8_t my_team = 0;
    std::int64_t start_ms = 0;
    std::unordered_map<std::uint32_t, RosterEntry> roster;
    Grid grid;       // what the server says
    Grid seen;       // what my team last saw: bending under the fog stays hidden [U-142][X-92]
    bool grid_dirty = true;
    Snapshot prev, cur;
    // [U-338] unit positions of the last snapshots (arrival time, id → x / y) for smooth, delayed interpolation
    struct PosFrame {
        double arrived = 0;
        std::unordered_map<std::uint32_t, std::pair<std::int32_t, std::int32_t>> pos;
        std::unordered_map<std::uint32_t, std::pair<std::int32_t, std::int32_t>> fx, comp;  // [U-389] effects, companions
    };
    std::deque<PosFrame> pos_hist;
    bool have_snapshot = false;
    std::vector<LocalWave> waves;
    std::vector<TextLine> feed;
    std::vector<ChatLine> chat;        // all channels, newest last [U-182]
    std::vector<LocalHit> hits;        // [X-103]
    std::vector<LocalReward> rewards;  // [U-159]
    std::unordered_map<std::uint32_t, std::vector<std::uint8_t>> skins;  // unit id → BMP [U-200]
    std::vector<net::FinalRow> final_stats;  // [U-179]
    std::vector<LocalAnnounce> announces;    // [U-263] newest last (the whole match: the announcements window)
    struct WallAnim {  // [U-341]
        net::NetWallBreak b;
        double born = 0;
    };
    std::vector<WallAnim> wall_anims;
    std::int64_t last_break_ms = -1;
    DeathRecapView recap;                    // [U-262] the last death
    bool reload_own_champions = false;      // [U-415] set on disconnect after a server sent its own champions
    int custom_from_server = -1;
    std::uint8_t ruleset = 0;               // [U-416] the server's rule set (RuleSet)
    std::string_view map;                   // [U-420] the server's map id
    bool queue_mode = false;                // [U-423] the server matches players by queue (map + mode)
    std::string address;                    // [U-422] the server address I joined (for invites)
    std::uint16_t port = 0;
    void send_lobby_config(int map_index, int rules, int game_mode, int feats = -1);  // feats −1 = keep  // [U-422][U-454] owner only
    std::uint8_t mode = 0;                  // [U-454] the lobby's game mode
    std::uint16_t features = 0;             // [U-478] the lobby's switched-on Feature bits
    void send_transfer_owner(std::uint8_t slot_id);
    void send_queue(bool on, int map_index, int rules);  // [U-423]
    void use_map(int index);
    // [U-414] other players' own emotes: unit → 4 × (PNG, WAV)
    std::unordered_map<std::uint32_t, std::array<std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>, 4>> own_emotes;
    void send_own_emote(std::uint8_t idx, const std::vector<std::uint8_t>& png, const std::vector<std::uint8_t>& wav);
    bool own_emotes_allowed = false;        // [U-414] the server passes players' own emotes on             // [U-277] own champions taken from the server (−1 = none sent)
    bool back_in_lobby = false;              // server returned to the lobby after the match
    std::string notice;           // last "why nothing happened" from the server
    int notice_seq = 0;
    int winner = -1;
    double ping_ms = 0;

private:
    void handle(const std::uint8_t* data, std::size_t n, double now_s);
    void send_raw(const net::Writer& w, bool reliable);
    void net_loop(std::stop_token st);

    // network thread ↔ game thread
    struct Packet {
        std::vector<std::uint8_t> data;
        int event = 0;  // 0 data, 1 connected, 2 disconnected
    };
    std::mutex mu_;
    std::deque<Packet> inbox_;
    std::deque<std::pair<std::vector<std::uint8_t>, bool>> outbox_;
    std::atomic<std::uint32_t> rtt_{0};
    std::jthread thread_;

    _ENetHost* host_ = nullptr;
    _ENetPeer* peer_ = nullptr;
    std::string name_;
    std::uint64_t secret_ = 0;
    bool allow_skins_ = true;
};

// [U-212] Minecraft-like server list entry ping (own short connection, no join).
struct ServerInfo {
    bool ok = false, done = false;
    std::string name, motd, version, error;
    int players = 0, max_players = 0;
    bool in_match = false;
    std::uint8_t platforms = 0;
    double ping_ms = 0;
    struct Ranked {  // [U-276]
        std::string name;
        int lp = 0, wins = 0, losses = 0;
    };
    std::vector<Ranked> ranking;
};
[[nodiscard]] ServerInfo ping_server(const std::string& host, std::uint16_t port, int timeout_ms = 800);

}  // namespace em
