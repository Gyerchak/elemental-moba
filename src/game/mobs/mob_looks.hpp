#pragma once
// [U-541] The colour of what each monster, minion and structure throws, and of its glow and its death. One table, by model id.
#include <cstdint>
#include <string_view>

namespace em {

[[nodiscard]] constexpr std::uint32_t mob_colour(std::string_view id) {
    struct Row { std::string_view id; std::uint32_t colour; };
    constexpr Row kRows[] = {
        {"wandering_spirit", 0x9fe8ff}, {"spirit_wolf", 0xa0d0ff}, {"cursed_spirit", 0xb060ff}, {"dark_spirit", 0x8a40e0}, {"face_stealer", 0x6060a0},
        {"angry_beast", 0xc06030}, {"platypus_bear", 0xe0a040}, {"knowledge_seeker", 0xfff0b0}, {"swamp_monster", 0x70a040}, {"lion_turtle", 0xd8c890},
        {"sun_dragons", 0xff6a20}, {"bandit", 0xb09070}, {"bandit_archer", 0xb09070}, {"minion_siege", 0xa89880},
    };
    for (const Row& r : kRows)
        if (r.id == id) return r.colour;
    return 0;
}
[[nodiscard]] constexpr bool mob_glows(std::string_view id) {  // spirits and the great ones have a light of their own
    return id == "wandering_spirit" || id == "spirit_wolf" || id == "cursed_spirit" || id == "dark_spirit" || id == "face_stealer" || id == "knowledge_seeker" ||
           id == "lion_turtle" || id == "sun_dragons";
}

}  // namespace em
