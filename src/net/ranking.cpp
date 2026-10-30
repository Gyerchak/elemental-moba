// [U-276] See ranking.hpp.
#include "net/ranking.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

namespace em::net {

void Ratings::load(const std::string& path) {
    path_ = path;
    r_.clear();
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {  // name|mmr|lp|wins|losses|secret
        std::vector<std::string> f;
        std::stringstream ss(line);
        for (std::string x; std::getline(ss, x, '|');) f.push_back(x);
        if (f.size() != 6 || f[0].empty() || f[0][0] == '#') continue;
        Rating r;
        r.mmr = std::atoi(f[1].c_str());
        r.lp = std::atoi(f[2].c_str());
        r.wins = std::atoi(f[3].c_str());
        r.losses = std::atoi(f[4].c_str());
        r.secret = std::strtoull(f[5].c_str(), nullptr, 10);
        r_[f[0]] = r;
    }
}

void Ratings::save() const {
    if (path_.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path_).parent_path(), ec);
    {
        std::ofstream out(path_ + ".tmp");  // [U-383] temp file + rename: a crash never loses the ranking
        out << "# [U-276] name|mmr|lp|wins|losses|secret\n";
        for (const auto& [n, r] : r_) out << n << '|' << r.mmr << '|' << r.lp << '|' << r.wins << '|' << r.losses << '|' << r.secret << '\n';
    }
    std::filesystem::rename(path_ + ".tmp", path_, ec);
}

Rating* Ratings::get(const std::string& name, std::uint64_t secret) {
    auto [it, fresh] = r_.try_emplace(name);
    if (fresh) it->second.secret = secret;
    return it->second.secret == secret ? &it->second : nullptr;
}

void Ratings::apply(std::vector<Result>& ps, int winner) {
    double sum[2] = {0, 0};
    int n[2] = {0, 0};
    for (const Result& p : ps) {
        const Rating* r = p.bot ? nullptr : get(p.name, p.secret);
        sum[p.team & 1] += r ? r->mmr : 1200;
        ++n[p.team & 1];
    }
    const double avg[2] = {n[0] ? sum[0] / n[0] : 1200, n[1] ? sum[1] / n[1] : 1200};
    for (Result& p : ps) {
        if (p.bot) continue;
        Rating* r = get(p.name, p.secret);
        if (!r) continue;  // guest
        const int t = p.team & 1;
        const bool won = t == winner;
        const double expected = 1.0 / (1.0 + std::pow(10.0, (avg[1 - t] - r->mmr) / 400.0));
        const double k = r->wins + r->losses < 10 ? 48 : 32;
        r->mmr += int(std::lround(k * ((won ? 1.0 : 0.0) - expected)));
        const int lp_mmr = 800 + r->lp / 2;  // where the LP says this player is (Claude idea scale)
        const int adj = std::clamp((r->mmr - lp_mmr) / 25, -10, 10);
        const int d = won ? std::clamp(20 + adj, 10, 30) : -std::clamp(18 - adj, 8, 28);
        r->lp = std::max(0, r->lp + d);
        (won ? r->wins : r->losses)++;
        p.lp_delta = d;
        p.mmr_after = r->mmr;
    }
    save();
}

std::vector<std::pair<std::string, Rating>> Ratings::top(std::size_t n) const {
    std::vector<std::pair<std::string, Rating>> v(r_.begin(), r_.end());
    std::ranges::sort(v, [](const auto& a, const auto& b) { return a.second.lp != b.second.lp ? a.second.lp > b.second.lp : a.second.mmr > b.second.mmr; });
    if (v.size() > n) v.resize(n);
    return v;
}

std::string tier_name(std::int32_t lp) {
    static const char* kT[] = {"Iron", "Bronze", "Silver", "Gold", "Platinum", "Diamond", "Master"};
    static const char* kD[] = {"IV", "III", "II", "I"};
    const int t = std::min(6, lp / 400);
    if (t == 6) return std::format("Master  {} LP", lp - 2'400);
    return std::format("{} {}  {} LP", kT[t], kD[(lp % 400) / 100], lp % 100);
}

}  // namespace em::net
