// [U-414] own emotes from custom_emotes/ (see own_emotes.hpp)
#include "client/own_emotes.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "game/world.hpp"  // (protocol.hpp needs the game types first)
#include "net/protocol.hpp"

namespace em {

namespace {
std::vector<std::uint8_t> read_all(const std::filesystem::path& p, std::uint32_t max) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(p, ec) || std::filesystem::file_size(p, ec) > max) return {};
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
}  // namespace

std::vector<OwnEmote> load_own_emotes() {
    std::vector<OwnEmote> out;
    std::error_code ec;
    std::vector<std::filesystem::path> dirs;
    for (const auto& e : std::filesystem::directory_iterator("custom_emotes", ec))
        if (e.is_directory()) dirs.push_back(e.path());
    std::ranges::sort(dirs);
    for (const auto& d : dirs) {
        if (int(out.size()) >= net::kOwnEmotes) break;
        OwnEmote e{d.filename().string(), read_all(d / "emote.png", net::kOwnEmotePngMax), read_all(d / "sound.wav", net::kOwnEmoteWavMax)};
        if (e.png.size() > 8 && e.png[0] == 0x89 && e.png[1] == 'P') out.push_back(std::move(e));
    }
    return out;
}

}  // namespace em
