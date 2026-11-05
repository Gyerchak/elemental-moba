// Languages. See locale.hpp.
#include "client/locale.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace em::loc {

namespace {
std::string g_code = "en";
std::unordered_map<std::string, std::string> g_table;
std::unordered_map<std::string, std::string> g_misses;  // keeps English strings alive for the returned refs
}  // namespace

void load(const std::string& code) {
    g_code = code.empty() ? "en" : code;
    g_table.clear();
    std::ifstream in(std::filesystem::path("lang") / (g_code + ".txt"));
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        // [U-508] a "=" inside the English key is written \\= ("C \\= all=C = wszystko")
        std::size_t eq = line.find('=');
        while (eq != std::string::npos && eq > 0 && line[eq - 1] == '\\') eq = line.find('=', eq + 1);
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        for (std::size_t p = key.find("\\="); p != std::string::npos; p = key.find("\\=", p)) key.erase(p, 1);
        g_table[key] = line.substr(eq + 1);
    }
}

const std::string& tr(std::string_view english) {
    const std::string key(english);
    if (auto it = g_table.find(key); it != g_table.end()) return it->second;
    auto [it, _] = g_misses.try_emplace(key, key);
    return it->second;
}

std::string tr_numbers(std::string_view english) {
    const std::string whole(english);
    if (auto it = g_table.find(whole); it != g_table.end()) return it->second;
    std::string key;
    std::vector<std::string> nums;
    for (std::size_t i = 0; i < english.size();) {
        if (english[i] >= '0' && english[i] <= '9') {
            std::size_t j = i;
            while (j < english.size() && ((english[j] >= '0' && english[j] <= '9') || english[j] == '.')) ++j;
            nums.emplace_back(english.substr(i, j - i));
            key += "{}";
            i = j;
        } else {
            key += english[i++];
        }
    }
    auto it = g_table.find(key);
    if (it == g_table.end() || nums.empty()) return whole;
    std::string out;
    std::size_t n = 0;
    const std::string& t = it->second;
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (t[i] == '{' && i + 1 < t.size() && t[i + 1] == '}' && n < nums.size()) out += nums[n++], ++i;
        else out += t[i];
    }
    return out;
}

// [U-508] the server sends match events as finished English text with names inside. The longest patterns come first.
const std::string& tr_feed(std::string_view line) {
    static constexpr std::string_view kPatterns[] = {
        "{} slew {} — assists: {} (shutdown {} g)", "{} slew {} — assists: {}", "{} slew {} (shutdown {} g)", "{} slew {}",
        "{} awakens as a {}bender! (Stance key: switch the whole set between the elements)",
        "The Lion Turtle blesses the {} (stack {})", "The Face Stealer took a face from {}!", "Group bending: {} grows (x{})",
        "{} disconnected (bot takes over)", "{} cheats death (Spirit Flask)", "{} enters the Avatar State!", "{} redirected the lightning!",
        "{} denied a minion", "{} sends the {} back", "{} reconnected", "{} calls the {}", "{} died",
    };
    static std::unordered_map<std::string, std::string> cache;
    static std::string cached_for;
    if (cached_for != g_code) cache.clear(), cached_for = g_code;
    const std::string whole(line);
    if (auto it = cache.find(whole); it != cache.end()) return it->second;
    std::string out = whole;
    if (auto it = g_table.find(whole); it != g_table.end()) {
        out = it->second;
    } else if (!g_table.empty()) {
        for (std::string_view pat : kPatterns) {
            const auto key = g_table.find(std::string(pat));
            if (key == g_table.end()) continue;
            std::vector<std::string> args;  // every {} = what stands between the fixed words
            std::size_t at = 0, from = 0;
            bool ok = true;
            for (std::size_t brace = pat.find("{}"); ok && brace != std::string_view::npos; brace = pat.find("{}", from)) {
                const std::string_view before = pat.substr(from, brace - from);
                if (!args.empty() || !before.empty()) {
                    const std::size_t hit = line.find(before, at);
                    if (hit == std::string_view::npos || (args.empty() && hit != 0)) { ok = false; break; }
                    if (!args.empty()) args.back() = std::string(line.substr(at, hit - at));
                    at = hit + before.size();
                }
                args.emplace_back();
                from = brace + 2;
            }
            if (!ok || args.empty()) continue;
            const std::string_view tail = pat.substr(from);
            if (line.size() < at + tail.size() || line.substr(line.size() - tail.size()) != tail) continue;
            args.back() = std::string(line.substr(at, line.size() - tail.size() - at));
            std::string built;
            std::size_t n = 0;
            const std::string& t = key->second;
            for (std::size_t i = 0; i < t.size(); ++i) {
                if (t[i] == '{' && i + 1 < t.size() && t[i + 1] == '}' && n < args.size()) built += tr(args[n++]), ++i;
                else built += t[i];
            }
            out = std::move(built);
            break;
        }
    }
    return cache.try_emplace(whole, std::move(out)).first->second;
}

std::vector<std::string> languages() {
    std::vector<std::string> out{"en"};
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator("lang", ec))
        if (e.path().extension() == ".txt") {
            const std::string c = e.path().stem().string();
            if (std::ranges::find(out, c) == out.end()) out.push_back(c);
        }
    return out;
}

const std::string& current() { return g_code; }

}  // namespace em::loc
