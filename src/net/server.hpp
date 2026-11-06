#pragma once
// [X-01][U-102][U-103][U-211] Authoritative match server over enet. Runs the World at a fixed tick
// (default 128 Hz) and sends only numbers and results; the client draws everything.
// Used three ways: practice (loopback + bots [U-100]), host match (host plays too), dedicated server.
//
//   server.cpp        port, sending, the fixed-tick loop, disconnects
//   server_lobby.cpp  hello, lobby, draft + bans, chat channels, skins, server-list ping
//   server_match.cpp  match start, snapshots, terrain diffs, final statistics, back to the lobby
#include "net/ranking.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

#include "game/world.hpp"
#include "net/protocol.hpp"

struct _ENetHost;
struct _ENetPeer;

namespace em::net {

struct ServerConfig {
    // ---- identity (server list [U-212])
    std::string name = "elemental-moba server";
    std::string motd = "Welcome!";
    std::string password;                 // [U-374] empty = none
    std::string bind_ip;                  // [U-537] the address it listens on (empty = all network cards of this PC)
    std::string public_ip;                // [U-537] the address others use from outside (only shown and copied: it belongs to the router)
    bool allow_spectators = true;         // [X-20] players may join to watch
    bool record_replays = true;           // [X-19] replays/<date>_<map>.emr (small: commands only)
    std::string replay_file;              // [X-19] this server plays that replay to whoever joins
    std::string version = std::string(kVersion);  // [U-95] the game version this server runs (official or versions/<name>)
    std::uint16_t port = kDefaultPort;
    // ---- simulation
    std::uint32_t tick_hz = 128;          // [U-102][U-103] default 128 Hz
    int snap_div = 2;                     // snapshot every N ticks [X-93]
    bool soft_cap = false;                // [X-45]
    std::int64_t start_ms = 0;            // sky clock offset
    std::int64_t first_wave_ms = 65'000;
    // ---- who may play
    bool practice = false;                // [U-100] cheats on, bots fill
    bool loopback_only = false;
    std::uint64_t host_secret = 0;        // the hosting client proves it is the host
    bool allow_duplicate_names = false;   // [U-102]
    bool registration = false;            // [U-103] unique nicknames + login (placeholder)
    std::uint8_t allowed_platforms = kAllPlatforms;  // [U-213] default: every system
    // ---- draft [U-181]
    bool bans = false;                    // ban phase: OFF in the draft modes for now
    int pick_seconds = 0;                 // [U-276] lobby timer (0 = off: the host starts). When it ends: everybody picked →
                                          // the match starts; somebody did not → the lobby is disbanded (they are sent away)
    // ---- ranking [U-276]
    bool ranked = true;                   // MMR + LP after every match (not in practice)
    bool matchmaking = true;              // balance the teams by MMR when the match starts
    // ---- content
    bool force_server_textures = false;   // [U-102]
    bool allow_custom_champions = true;   // [U-210][U-277] own champions; their files are sent to every player who joins
    bool players_are_bots = false;        // test / autoplay: bots drive the human slots too
    std::uint16_t features = 0;           // [U-478] switched-on Feature bits (the map's default; the lobby owner changes them)
    std::uint8_t ruleset = 0;             // [U-416][U-454] RuleSet: 0 all systems, 1 only LoL-like, 2 only Dota-like
    std::uint8_t mode = 0;                // [U-454] GameMode: 0 Classic, 1 Blind Pick, 2 All Random
    int map = 0;                          // [U-420] index in maps(); the lobby owner may change it before the match
    bool queue_mode = false;              // [U-423] matchmaking: players queue for a map + mode, the server makes random teams
    int queue_min = 2;                    // [U-423] players in the same queue that start a match (bots fill the teams)
    bool allow_own_emotes = true;         // [U-414] players' own emotes are sent to everyone (separate from skins)
};

class Server {
public:
    explicit Server(ServerConfig cfg);
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool start(std::string* error = nullptr);  // binds the port
    void run(std::stop_token stop);            // blocking loop
    [[nodiscard]] std::uint16_t port() const { return cfg_.port; }

    // status for the host / dedicated-server screen
    std::atomic<int> players{0};
    std::atomic<bool> in_match{false};
    std::atomic<std::int64_t> match_ms{0};
    std::atomic<double> tick_load{0};  // fraction of the tick budget used
    struct ReplayChampion {
        std::uint8_t team = 0, bot = 0;
        std::int32_t champ = 0, q = -1, f = -1, g = -1, slot_id = -1;
        std::string name;
        Loadout loadout;
    };
    struct ReplayEvent {
        std::uint64_t tick = 0;
        std::uint8_t type = 0;  // 0 command, 1 a bot takes over, 2 the player is back, 3 checksum
        std::uint32_t unit = 0;
        Cmd cmd{};
        std::uint64_t value = 0;
    };

private:
    struct Player {
        _ENetPeer* peer = nullptr;
        LobbySlot slot;
        std::uint32_t unit = 0;
        std::int64_t waves_sent_ms = -1;
        std::int64_t hits_sent_ms = -1;
        std::int64_t rewards_sent_ms = -1;
        std::int64_t fail_sent_ms = -1;
        bool connected = false;
        bool joined = false;        // said Hello (a server-list ping never does)
        bool spectator = false;     // [X-20] only watches
        std::uint8_t spec_team = 0; // … whose vision it sees
        bool allow_skins = true;    // [U-98] wants other players' skins
        std::uint64_t secret = 0;   // [U-276] identifies the player for the ranking
        std::uint64_t lights_sent_tick = ~0ull;  // [U-280]
        std::vector<std::uint8_t> skin;  // [U-200] BMP bytes
        std::array<std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>, 4> emotes;  // [U-414] PNG + WAV
    };
    // server.cpp
    void send(_ENetPeer* p, const Writer& w, bool reliable);
    void broadcast(const Writer& w, bool reliable);
    void drop(int pid);
    // [U-277] own champions: (file name, text) in roster order; loaded at start, sent to every joining player
    std::vector<std::pair<std::string, std::string>> custom_files_;
    void send_custom_maps(Player& p);  // [U-518]
    // [X-19][X-20] server_replay.cpp
    struct ReplayData {
        MatchConfig config{};
        std::uint32_t rng = 1;
        std::vector<ReplayChampion> champions;
        std::vector<ReplayEvent> events;
    } replay_;
    bool replaying_ = false, replay_saved_ = false, replay_off_ = false;
    std::size_t replay_next_ = 0;
    int replay_speed_pct_ = 100;  // 0 = paused
    [[nodiscard]] std::uint64_t world_checksum() const;
    void replay_note_champion(const ReplayChampion& c);
    void replay_note(std::uint8_t type, std::uint32_t unit, const Cmd& c = {}, std::uint64_t value = 0);
    void save_replay();
    bool load_replay(std::string* error);
    void start_replay_match();
    void replay_step();
    void load_custom_champions();
    void send_custom_champions(Player& p);
    // server_lobby.cpp
    void receive(int pid, Reader& r);
    void on_hello(int pid, Reader& r);
    void on_pick(Player& p, Reader& r);
    void on_ban(Player& p, Reader& r);
    void on_chat(Player& p, Reader& r);
    void on_skin(Player& p, Reader& r);
    void on_status_query(Player& p);
    void send_lobby();
    void send_chat(Chat ch, std::string_view from, std::string_view text, const Player* team_of, const Player* to);
    [[nodiscard]] bool champion_banned(int champ) const;
    // server_match.cpp
    void start_match();
    void send_match_start(Player& p);
    void send_skins();
    void send_own_emotes();  // [U-414]
    void on_emote(Player& p, Reader& r);
    void send_snapshot(Player& p);
    void send_all_cells(Player& p);
    void finish_match();  // final statistics, then back to the lobby

    ServerConfig cfg_;
    _ENetHost* host_ = nullptr;
    std::vector<Player> players_;
    std::unique_ptr<World> world_;
    std::size_t feed_sent_ = 0;
    DraftPhase phase_ = DraftPhase::Picking;
    std::int64_t over_ms_ = -1;  // match time when it ended
    std::int64_t over_ticks_ = 0;
    Ratings ratings_;                       // [U-276]
    std::int64_t lobby_deadline_ms_ = -1;
    int owner_ = -1;                        // [U-422] the lobby owner (player index): map, rule set, start, hand over
    void pick_owner();                      // after the owner left
    void try_pop_queue();                   // [U-423]   // [U-276] steady-clock ms when the pick timer ends (−1 = not running)
    void lobby_timer();
    [[nodiscard]] static std::int64_t wall_ms();
};

}  // namespace em::net
