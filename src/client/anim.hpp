#pragma once
// [U-226] Animation slots. Every unit is always in exactly one slot (plus an optional emote). A resource
// pack can give every slot its own animation and sound:
//     resourcepacks/<pack>/champions/<id>/<base | skins/<skin>>/anim/<slot>.txt     (frames, later)
//     resourcepacks/<pack>/champions/<id>/<base | skins/<skin>>/sounds/<slot>.wav
//     resourcepacks/<pack>/sounds/anim/<slot>.wav                                    (generic for everyone)
// Generic Q bender skills share one animation for everybody: slot "cast_q_<element>".
// Until real models exist the client draws a placeholder cue for every slot (world_draw.cpp).
#include <cstdint>
#include <string_view>

namespace em {

enum class AnimSlot : std::uint8_t {
    Idle, WalkSlow, Walk, Run,        // movement speed tiers
    Swim,                             // [U-188] own slot
    FlySelf, FlyAlly, FlyEnemy,       // [U-224][U-249] lifted by myself / an ally / an enemy
    Cast, Autoattack, Recall, Death,
    Stunned, Frozen, Electrocuted, Bloodbent, Blinded, Pushed, Slowed, Silenced, Burning,
    Emote,                            // laugh / taunt / dance / joke / wave / cheer [U-246]
    Count
};

[[nodiscard]] constexpr std::string_view anim_name(AnimSlot s) {
    constexpr std::string_view n[] = {"idle", "walk_slow", "walk", "run", "swim", "fly_self", "fly_ally", "fly_enemy",
                                      "cast", "autoattack", "recall", "death", "stunned", "frozen", "electrocuted",
                                      "bloodbent", "blinded", "pushed", "slowed", "silenced", "burning", "emote"};
    static_assert(std::size(n) == std::size_t(AnimSlot::Count));
    return n[std::size_t(s)];
}

inline constexpr std::string_view kEmoteNames[] = {"laugh", "taunt", "dance", "joke", "wave", "cheer"};
inline constexpr std::string_view kPingNames[] = {"ping", "danger", "enemy missing", "on my way", "assist me", "bait"};

}  // namespace em
