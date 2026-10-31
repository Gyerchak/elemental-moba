#pragma once
// [X-27] One status-effect system for every CC and buff [U-36][U-59].
// Durations go through cc_duration_ms() → tenacity / CC amplify [U-127][X-68].
// [U-125] The eclipse fire mute is NOT a status: the sim asks sky_boost().muted every tick.
#include <array>
#include <cstdint>

#include "core/stats.hpp"

namespace em {

enum class StatusKind : std::uint8_t {
    Stun,      // no move, no cast, no attack
    Root,      // no move
    Slow,      // value = % move speed lost
    Silence,   // no abilities
    Mute,      // [U-474] CHI BLOCK (own): bending blocked — bending skills AND bending autoattacks of the elements in `value`
               // (bit 1 << Element: 1 water, 2 earth, 4 fire, 8 air; 0 = every element). Non-bending skills still work.
    Airborne,  // knock-up: like stun, ignores tenacity [X-68]
    Frozen,    // ice prison: like stun; NO HEAT → invisible to heat sense [U-143]
    Blind,     // value = % sight lost (sandbending) [U-137][X-75]
    Burn,      // value = damage per second (hundredths of HP)
    Shield,    // value = shield HP left (hundredths)
    Haste,     // value = % move speed gained
    Flying,    // above ground: no seismic waves [U-142], passes ground walls
    Bloodbound,// [U-78] moved by the bloodbender, ignores tenacity
    Channel,   // channelling (portal X-54, group bending)
    Swimming,  // [U-188] in deep water: half speed, no spells (except water mobility skills)
    WaterRide, // [U-189] riding a water column: raised above melee / ground skills, casts freely
    Redirect,  // [U-185] lightning redirection window is open
    Wet,       // [U-241] soaked: lightning +25 %; fire or waterbending dries it
    Grievous,  // [U-416] healing reduced by `value` % (classic rule sets only)
    Fear,      // [U-472] runs away from the one who scared it (slowly, by `value` % slow); cannot act
    Disarm,    // [U-474] own: no weapon autoattacks, no non-magical (weapon) skills
    Subdue,    // [U-474] own: both kinds — no bending (all elements, skills + autoattacks) AND disarmed
    Count
};

[[nodiscard]] constexpr bool is_cc(StatusKind k) {
    switch (k) {
        case StatusKind::Stun: case StatusKind::Root: case StatusKind::Slow: case StatusKind::Silence:
        case StatusKind::Mute: case StatusKind::Airborne: case StatusKind::Frozen: case StatusKind::Blind:
        case StatusKind::Bloodbound: case StatusKind::Fear: case StatusKind::Disarm: case StatusKind::Subdue:
            return true;
        default:
            return false;
    }
}
[[nodiscard]] constexpr bool ignores_tenacity(StatusKind k) {
    return k == StatusKind::Airborne || k == StatusKind::Bloodbound;  // [X-68]
}

struct Status {
    StatusKind kind = StatusKind::Count;
    std::int64_t until_ms = 0;
    std::int32_t value = 0;
    std::uint32_t source = 0;  // unit id
};

class StatusSet {
public:
    static constexpr int kMax = 16;

    // [U-127] CC durations scaled by attacker CC amplify and target tenacity.
    // Same kind again: keep the longer time and the stronger value.
    constexpr void apply(StatusKind k, std::int64_t now_ms, std::int64_t base_ms, std::int32_t value,
                         std::uint32_t source, const Stats& attacker, const Stats& target) {
        const std::int64_t ms = is_cc(k) ? cc_duration_ms(base_ms, attacker, target, ignores_tenacity(k)) : base_ms;
        add(k, now_ms + ms, value, source);
    }
    constexpr void add(StatusKind k, std::int64_t until_ms, std::int32_t value, std::uint32_t source) {
        for (int i = 0; i < n_; ++i)
            if (s_[i].kind == k && (k != StatusKind::Burn || s_[i].source == source)) {
                s_[i].until_ms = std::max(s_[i].until_ms, until_ms);
                s_[i].value = k == StatusKind::Shield ? s_[i].value + value : std::max(s_[i].value, value);
                return;
            }
        if (n_ < kMax) s_[n_++] = {k, until_ms, value, source};
    }
    constexpr void expire(std::int64_t now_ms) {
        int w = 0;
        for (int i = 0; i < n_; ++i)
            if (s_[i].until_ms > now_ms && !(s_[i].kind == StatusKind::Shield && s_[i].value <= 0)) s_[w++] = s_[i];
        n_ = std::uint8_t(w);
    }
    constexpr void remove(StatusKind k) {
        int w = 0;
        for (int i = 0; i < n_; ++i)
            if (s_[i].kind != k) s_[w++] = s_[i];
        n_ = std::uint8_t(w);
    }
    constexpr void clear() { n_ = 0; }

    [[nodiscard]] constexpr bool has(StatusKind k) const {
        for (int i = 0; i < n_; ++i)
            if (s_[i].kind == k) return true;
        return false;
    }
    [[nodiscard]] constexpr std::int32_t value(StatusKind k) const {
        std::int32_t v = 0;
        for (int i = 0; i < n_; ++i)
            if (s_[i].kind == k) v = std::max(v, s_[i].value);
        return v;
    }
    [[nodiscard]] constexpr bool hard_cc() const {
        return has(StatusKind::Stun) || has(StatusKind::Airborne) || has(StatusKind::Frozen) ||
               has(StatusKind::Bloodbound) || has(StatusKind::Fear);
    }
    [[nodiscard]] constexpr bool can_move() const { return !hard_cc() && !has(StatusKind::Root); }
    // [U-474] only the HARD crowd control here. Which skills / autoattacks are blocked by silence (LoL: lol.cpp) or by
    // chi block / disarm / subdue (own: bending_cc.cpp) depends on the skill → World::skill_block_reason / auto_allowed.
    [[nodiscard]] constexpr bool can_cast() const { return !hard_cc(); }
    [[nodiscard]] constexpr bool can_attack() const { return !hard_cc(); }
    // Move speed % after slows / hastes. Slows don't stack: strongest wins.
    [[nodiscard]] constexpr std::int32_t move_speed_pct() const {
        const std::int32_t pct = std::max(10, 100 - value(StatusKind::Slow) + value(StatusKind::Haste));
        return has(StatusKind::Swimming) ? pct / 2 : pct;  // [U-188] swimming is slow
    }
    // Shield soaks damage first. Returns damage left after the shield.
    constexpr std::int32_t soak(std::int32_t dmg) {
        for (int i = 0; i < n_ && dmg > 0; ++i)
            if (s_[i].kind == StatusKind::Shield) {
                const std::int32_t used = std::min(dmg, s_[i].value);
                s_[i].value -= used;
                dmg -= used;
            }
        return dmg;
    }
    [[nodiscard]] constexpr std::uint32_t source_of(StatusKind k) const {  // [U-472] who applied it (the latest)
        for (int i = 0; i < n_; ++i)
            if (s_[i].kind == k) return s_[i].source;
        return 0;
    }
    [[nodiscard]] constexpr int size() const { return n_; }
    [[nodiscard]] constexpr const Status& operator[](int i) const { return s_[i]; }

private:
    std::array<Status, kMax> s_{};
    std::uint8_t n_ = 0;
};

static_assert([] {
    StatusSet s;
    Stats a, t;
    t.tenacity = 5'000;  // 50 %
    s.apply(StatusKind::Stun, 0, 2'000, 0, 1, a, t);
    s.apply(StatusKind::Airborne, 0, 1'000, 0, 1, a, t);  // ignores tenacity
    s.apply(StatusKind::Slow, 0, 3'000, 30, 1, a, t);
    s.apply(StatusKind::Slow, 0, 1'000, 50, 2, a, t);
    if (s.can_move() || s.can_cast() || s.move_speed_pct() != 50) return false;
    s.expire(999);
    if (!s.has(StatusKind::Airborne) || !s.has(StatusKind::Stun)) return false;
    s.expire(1'000);  // stun 2 000 × 50 % = 1 000 ms, airborne full 1 000 ms
    return s.can_move() && s.has(StatusKind::Slow) && !s.has(StatusKind::Airborne);
}());
static_assert([] {
    StatusSet s;
    s.add(StatusKind::Shield, 5'000, 100, 1);
    const std::int32_t left = s.soak(150);
    s.expire(0);
    return left == 50 && !s.has(StatusKind::Shield);
}());
static_assert([] {
    StatusSet s;
    s.add(StatusKind::Mute, 1'000, 0, 0);  // [U-474] chi block is not hard CC: the World decides per skill / autoattack
    return s.can_cast() && s.can_attack() && s.can_move();
}());

}  // namespace em
