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
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        g_table[line.substr(0, eq)] = line.substr(eq + 1);
    }
}

const std::string& tr(std::string_view english) {
    const std::string key(english);
    if (auto it = g_table.find(key); it != g_table.end()) return it->second;
    auto [it, _] = g_misses.try_emplace(key, key);
    return it->second;
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
