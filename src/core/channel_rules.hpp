#pragma once
// [U-337] What breaks a channel (recall, portal) or a cast that is winding up. Bit flags: every skill
// (SkillDef::interrupt, 0 = the default) and every channel kind sets its own mix. Change the defaults here.
#include <cstdint>

#include "core/status.hpp"

namespace em {

enum InterruptBit : std::uint32_t {
    kIntStun = 1u << 0, kIntAirborne = 1u << 1, kIntFrozen = 1u << 2, kIntBloodbound = 1u << 3,
    kIntSilence = 1u << 4, kIntMute = 1u << 5,        // mute breaks only BENDING casts (not physical skills)
    kIntRoot = 1u << 6, kIntSlow = 1u << 7, kIntBlind = 1u << 8,
    kIntDamageHp = 1u << 9,      // damage that reaches HP
    kIntDamageShield = 1u << 10, // damage fully eaten by a shield — like LoL this does NOT interrupt (off everywhere)
    kIntKnock = 1u << 11,        // pushed / pulled (any displacement)
    kIntLiftedSelf = 1u << 12, kIntLiftedAlly = 1u << 13, kIntLiftedEnemy = 1u << 14,  // lifted into the air by …
    kIntMove = 1u << 15,         // my own move / attack order
};

inline constexpr std::uint32_t kIntHardCC = kIntStun | kIntAirborne | kIntFrozen | kIntBloodbound | kIntSilence | kIntMute |
                                            kIntKnock | kIntLiftedEnemy;
inline constexpr std::uint32_t kIntDefaultCast = kIntHardCC;                                                    // skills
inline constexpr std::uint32_t kIntRecall = kIntHardCC | kIntRoot | kIntDamageHp | kIntMove | kIntLiftedAlly;  // recall
inline constexpr std::uint32_t kIntPortal = kIntRecall;                                                         // spirit portal
inline constexpr std::int64_t kRecallMs = 6'000;  // recall and the spirit portal take this long [U-350][U-369]

// the interrupt event of a crowd-control status (0 = none)
[[nodiscard]] constexpr std::uint32_t interrupt_of(StatusKind k) {
    switch (k) {
        case StatusKind::Stun: return kIntStun;
        case StatusKind::Airborne: return kIntAirborne;
        case StatusKind::Frozen: return kIntFrozen;
        case StatusKind::Bloodbound: return kIntBloodbound;
        case StatusKind::Silence: return kIntSilence;
        case StatusKind::Mute: return kIntMute;
        case StatusKind::Root: return kIntRoot;
        case StatusKind::Slow: return kIntSlow;
        case StatusKind::Blind: return kIntBlind;
        default: return 0;
    }
}

}  // namespace em
