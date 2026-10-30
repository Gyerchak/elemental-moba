#pragma once
// [U-414] Own emotes (like own skins, but separate): custom_emotes/<name>/emote.png (≤ 256 × 256, ≤ 96 KB) and an
// optional custom_emotes/<name>/sound.wav (≤ 256 KB). The first 4 folders (A–Z) are in the emote wheel after the
// official 6 emotes. They are sent to the server when you lock in; a server passes them on only when its host allows
// own emotes, and each player chooses if they show other players' own emotes (Options → Game).
#include <cstdint>
#include <string>
#include <vector>

namespace em {

struct OwnEmote {
    std::string name;
    std::vector<std::uint8_t> png, wav;
};
[[nodiscard]] std::vector<OwnEmote> load_own_emotes();

}  // namespace em
