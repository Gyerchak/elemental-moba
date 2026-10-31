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

// [U-486] SPRITE ANIMATION KEYS — what a model sheet (sheet.txt) may contain. A model needs only "idle"; a missing key
// falls back step by step (anim_fallback) until one exists. sheet.txt can also remap keys: `use <key> <anim>`.
//   idle idle2 walk run dash | attack attack2 attack3 crit | cast cast_<slot> cast_<slot><stage 1-3>  (slot: p q w e r t y)
//   death recall channel | emote_<laugh|taunt|dance|joke|wave|cheer> | stunned frozen airborne flying swim silenced
//   blinded bloodbent burning slowed | form_<any key> = the same while transformed (e.g. form_walk, form_cast_w2)
inline constexpr std::string_view kAnimKeys[] = {
    "idle", "idle2", "walk", "run", "dash", "attack", "attack2", "attack3", "crit", "cast", "death", "recall", "channel",
    "stunned", "frozen", "airborne", "flying", "swim", "silenced", "blinded", "bloodbent", "burning", "slowed"};
[[nodiscard]] inline std::string anim_fallback(std::string_view k) {
    if (k.starts_with("form_")) return std::string(k.substr(5));                     // form_walk → walk
    if (!k.empty() && k.back() >= '0' && k.back() <= '9') {                          // cast_w2 → cast_w, attack3 → attack
        while (!k.empty() && k.back() >= '0' && k.back() <= '9') k.remove_suffix(1);
        return std::string(k);
    }
    if (k.starts_with("cast_")) return "cast";
    if (k.starts_with("emote_")) return "emote";
    if (k == "crit") return "attack";
    if (k == "dash") return "run";
    if (k == "run") return "walk";
    if (k == "recall") return "channel";
    if (k == "idle") return "";
    return "idle";  // walk, attack, cast, emote, channel, statuses, death …
}

inline constexpr std::string_view kEmoteNames[] = {"laugh", "taunt", "dance", "joke", "wave", "cheer"};
inline constexpr std::string_view kPingNames[] = {"ping", "danger", "enemy missing", "on my way", "assist me", "bait"};

}  // namespace em
