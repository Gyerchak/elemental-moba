// Own champions. See custom_champions.hpp.
#include "client/custom_champions.hpp"

#include <algorithm>
#include <charconv>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>

#include "game/champions/custom.hpp"
#include "game/content.hpp"

namespace em {

namespace fs = std::filesystem;

namespace {
std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}
std::string parse(const fs::path& path, ChampionDef& out) {
    std::error_code ec;
    if (!fs::exists(path, ec)) return "cannot read the file";
    return parse_champion_text(read_file(path), out);
}
}  // namespace

std::vector<CustomChampionFile> scan_custom_champions() {
    std::vector<CustomChampionFile> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator("custom_champions", ec)) {
        if (e.path().extension() != ".champ") continue;
        CustomChampionFile f;
        f.file = e.path().filename().string();
        ChampionDef d;
        f.error = parse(e.path(), d);
        f.ok = f.error.empty();
        if (f.ok) f.name = std::string(d.name);
        out.push_back(std::move(f));
    }
    std::ranges::sort(out, {}, &CustomChampionFile::file);
    return out;
}

int load_custom_champions(const std::vector<std::string>& enabled) {
    int n = 0;
    for (const std::string& file : enabled) {
        ChampionDef d;
        if (!parse(fs::path("custom_champions") / file, d).empty()) continue;  // any error → ignored
        if (register_champion(d) >= 0) ++n;
    }
    return n;
}

}  // namespace em
