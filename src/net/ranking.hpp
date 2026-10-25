#pragma once
// [U-276] Server ranking: MMR (hidden Elo), LP (shown) and tiers, saved per server in server_data/ratings.txt.
// Claude idea numbers:
//   MMR: Elo, start 1200, K = 32 (K = 48 for the first 10 games). Each player against the OTHER team's average
//        (bots count as 1200). Practice matches and servers with ranked = false change nothing.
//   LP:  win +20, loss −18, ±1 per 25 MMR the player is above / below where their LP says they are (cap 10…30).
//   Tiers by LP: Iron 0, Bronze 400, Silver 800, Gold 1200, Platinum 1600, Diamond 2000, Master 2400+;
//        4 divisions of 100 LP inside a tier (IV … I).
// A player is their nickname + the client's secret (a different secret = a guest: not rated).
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace em::net {

struct Rating {
    std::int32_t mmr = 1200, lp = 0, wins = 0, losses = 0;
    std::uint64_t secret = 0;
};

class Ratings {
public:
    void load(const std::string& path);
    void save() const;
    // the rating of this player (a new one is created); nullptr = name taken by another secret (guest)
    Rating* get(const std::string& name, std::uint64_t secret);
    struct Result {
        std::string name;
        std::uint64_t secret = 0;
        int team = 0;
        bool bot = false;
        std::uint32_t unit = 0;           // the champion it played (for the post-game table)
        int lp_delta = 0, mmr_after = 0;  // filled by apply()
    };
    // winner: 0 / 1. Updates every non-bot player and fills lp_delta / mmr_after.
    void apply(std::vector<Result>& players, int winner);
    [[nodiscard]] std::vector<std::pair<std::string, Rating>> top(std::size_t n) const;

private:
    std::string path_;
    std::map<std::string, Rating> r_;
};

[[nodiscard]] std::string tier_name(std::int32_t lp);  // "Gold II  37 LP"

}  // namespace em::net
