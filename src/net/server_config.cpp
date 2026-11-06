// [U-511] See server_config.hpp.
#include "net/server_config.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace em::net {

namespace {
// One table for reading and writing: a new setting is one line here.
template <class F>
void each_field(ServerConfig& c, F&& f) {
    f("name", c.name);
    f("motd", c.motd);
    f("password", c.password);
    f("version", c.version);
    f("port", c.port);
    f("tick_hz", c.tick_hz);
    f("snap_div", c.snap_div);
    f("soft_cap", c.soft_cap);
    f("start_ms", c.start_ms);
    f("first_wave_ms", c.first_wave_ms);
    f("practice", c.practice);
    f("allow_duplicate_names", c.allow_duplicate_names);
    f("registration", c.registration);
    f("allowed_platforms", c.allowed_platforms);
    f("bans", c.bans);
    f("pick_seconds", c.pick_seconds);
    f("ranked", c.ranked);
    f("matchmaking", c.matchmaking);
    f("force_server_textures", c.force_server_textures);
    f("allow_custom_champions", c.allow_custom_champions);
    f("features", c.features);
    f("ruleset", c.ruleset);
    f("mode", c.mode);
    f("map", c.map);
    f("queue_mode", c.queue_mode);
    f("queue_min", c.queue_min);
    f("allow_own_emotes", c.allow_own_emotes);
    f("allow_spectators", c.allow_spectators);
    f("record_replays", c.record_replays);
}
void read_into(std::string& dst, const std::string& v) { dst = v; }
void read_into(bool& dst, const std::string& v) { dst = v == "1" || v == "yes" || v == "true"; }
template <class N>
void read_into(N& dst, const std::string& v) { dst = static_cast<N>(std::atoll(v.c_str())); }
std::string written(const std::string& v) { return v; }
std::string written(bool v) { return v ? "1" : "0"; }
template <class N>
std::string written(N v) { return std::to_string(static_cast<long long>(v)); }
}  // namespace

bool load_server_cfg(ServerConfig& c, std::string* language, const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "language") {
            if (language) *language = v;
            continue;
        }
        each_field(c, [&](const char* key, auto& field) {
            if (k == key) read_into(field, v);
        });
    }
    c.port = std::uint16_t(std::clamp<int>(c.port, 1, 65'535));
    c.tick_hz = std::clamp<std::uint32_t>(c.tick_hz, 16, 256);
    c.snap_div = std::clamp(c.snap_div, 1, 16);
    c.allowed_platforms = c.allowed_platforms ? c.allowed_platforms : kAllPlatforms;
    return true;
}

bool save_server_cfg(const ServerConfig& c, const std::string& language, const std::string& path) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp);
        if (!out) return false;
        out << "# Elemental MOBA server settings. Used by em_server and by elemental_moba --server.\n";
        if (!language.empty()) out << "language=" << language << "\n";
        ServerConfig& m = const_cast<ServerConfig&>(c);  // (each_field only reads here)
        each_field(m, [&](const char* key, auto& field) { out << key << "=" << written(field) << "\n"; });
        if (!out) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

}  // namespace em::net
