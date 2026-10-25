#pragma once
// [U-70..U-72, U-104..U-106, U-109..U-114, U-116, U-123] Ability slots, scaling and the data every ability has.
#include <algorithm>
#include <array>
#include <cstdint>

#include "core/elements.hpp"
#include "core/stats.hpp"

namespace em {

// [U-104] P = champion passive. [U-70] QWER + T ultimate. [U-71] F, G special.
// [U-289] Y = the sixth skill. F and G are the two SPECIAL skills (their keys are G and H since U-289;
// the names stay F / G in the code).
enum class Slot : std::uint8_t { P, Q, W, E, R, T, Y, F, G, Count };
inline constexpr int kSkillSlots = int(Slot::Count);  // 9: arrays per skill slot use this, never a number

// [U-37] [U-109] PointClick (targeted) must stay close to 0 — see kit_ok().
enum class Delivery : std::uint8_t { Projectile, NonProjectile, Skillshot, PointClick, MultiHit };

// [U-53]
enum class CastMove : std::uint8_t { Free, Rooted, Slowed };

// [U-110] Any ability number can scale with any of these.
// Result is in the unit of the field (damage points, game units, ms).
// Ratios read stats in hundredths, so 60 % of 100.00 EP = 60 points.
struct Scaling {
    std::int32_t base = 0;
    std::int32_t per_rank = 0;       // each ability rank above 1, no cap [U-111]
    std::int32_t per_level = 0;      // each champion level above 1, no cap [U-111]
    std::int32_t per_stack = 0;      // only skills with stacks
    std::int16_t ep_pct = 0;         // % of total elemental power
    std::int16_t ad_pct = 0;         // % of total attack
    std::int16_t bonus_ad_pct = 0;   // % of bonus attack
};

struct ScaleCtx {
    const Stats& stats;
    std::uint32_t rank = 1;    // ability rank, no cap [U-111]
    std::uint32_t level = 1;   // champion level, no cap [U-111]
    std::uint32_t stacks = 0;
    int power_pct = 100;  // [U-123] sky_boost(): full moon, comet, alignment, eclipse
    int size_pct = 100;   // [U-123] sky events make skills bigger too (range, length)
    int width_pct = 100, radius_pct = 100, speed_pct = 100;  // [U-264]
    int dur_pct = 100, dot_pct = 100;                         // [U-264] skill duration / damage-over-time duration
};

[[nodiscard]] constexpr std::int64_t scale(const Scaling& s, const ScaleCtx& c) {
    const auto& st = c.stats;
    return std::int64_t(s.base) + std::int64_t(s.per_rank) * (std::max(c.rank, 1u) - 1) +
           std::int64_t(s.per_level) * (std::max(c.level, 1u) - 1) + std::int64_t(s.per_stack) * c.stacks +
           std::int64_t(st.elemental_power.total()) * s.ep_pct / 10'000 +
           std::int64_t(st.attack.total()) * s.ad_pct / 10'000 +
           std::int64_t(st.attack.bonus) * s.bonus_ad_pct / 10'000;
}

// [U-105] Every skill can carry its own passive (always-on effect while learned).
struct PassiveDef {
    bool enabled = false;
    std::uint16_t effect_id = 0;  // [X-13] points into data-driven effect table
};

// [U-106] Multicast / recast (Taliyah-like): one cooldown, several casts in a window.
struct MulticastDef {
    std::uint8_t max_casts = 1;           // 1 = normal skill
    std::uint16_t recast_window_ms = 0;   // time to use the next cast
    std::uint8_t falloff_pct = 0;         // [X-41] damage drop per extra hit on same target
    bool cooldown_on_first_cast = false;  // false = CD starts after last cast / window end
};

struct AbilityDef {
    Slot slot = Slot::Q;
    SubElement sub = SubElement::Physical;  // Physical → non-elemental haste [U-57]
    Delivery delivery = Delivery::Skillshot;
    bool has_active = true;  // false = passive-only skill (P is usually passive-only)
    PassiveDef passive;      // [U-105]
    MulticastDef multicast;  // [U-106]
    CastMove cast_move = CastMove::Rooted;  // [U-53]
    std::uint16_t move_speed_while_casting_pct = 100;
    // [U-110][U-112] every number scales; sizes/speeds can grow with rank/level.
    Scaling damage;
    Scaling range, width, length, radius, speed;  // [U-54]
    Scaling cast_time_ms{.base = 250};            // [U-42]
    Scaling cooldown_ms{.base = 8'000};
    Scaling cost;
    // [U-116] Light the skill gives off (seen by EVERY team). Fire skills get
    // kFireLightRadius when 0 [X-61]. light_ms = how long it stays after the skill ends.
    Scaling light_radius, light_ms;
    bool changes_water_state = false;   // [U-28] → state_change_nerf on every number
    bool melee_ranged_aspect = false;   // [U-29] weak ranged version for melee mages
};

// Final numbers of one cast, after scaling, items and effects.
struct ResolvedAbility {
    std::int64_t damage, range, width, length, radius, speed, cast_time_ms, cooldown_ms, cost;
    std::int64_t light_radius, light_ms;  // [U-116]
    int dur_pct = 100, dot_pct = 100;     // [U-264] the caster multiplies the skill's durations by these
};

// [X-61] Default light of any fire-family skill (fire, blue fire, lightning, combustion): 6 cells.
inline constexpr std::int64_t kFireLightRadius = 192;

[[nodiscard]] constexpr ResolvedAbility resolve(const AbilityDef& a, const ScaleCtx& c) {
    const Stats& st = c.stats;
    auto pos = [](std::int64_t v) { return std::max<std::int64_t>(v, 0); };
    // [X-47] Haste-style: 100.00 points = half time, never reaches 0.
    auto hasted = [](std::int64_t v, std::int32_t haste) { return v * 10'000 / (10'000 + std::max(haste, 0)); };
    // [U-28] 20 % worse in every aspect (cast time / cooldown not touched [X-48]).
    auto nerf = [&](std::int64_t v) { return a.changes_water_state ? v * (100 - kStateChangeNerfPct) / 100 : v; };
    auto big = [&](std::int64_t v, int pct) { return nerf(v * std::max(pct, 0) / 100); };  // [U-123][U-264]

    const std::int32_t haste =
        base_element(a.sub) == Element::None ? st.non_elemental_haste : st.elemental_haste;  // [U-57]
    return {
        .damage = nerf(pos(scale(a.damage, c)) * std::max(c.power_pct, 0) / 100),         // [U-123]
        .range = big(pos(scale(a.range, c) + st.skill_range / 100), c.size_pct),          // [U-113]
        .width = big(pos(scale(a.width, c) + st.skill_width / 100), c.width_pct),         // [U-54]
        .length = big(pos(scale(a.length, c) + st.skill_length / 100), c.size_pct),
        .radius = big(pos(scale(a.radius, c) + (radius_bonus(st) + st.skill_radius) / 100), c.radius_pct),
        .speed = big(pos(scale(a.speed, c) + st.skill_speed / 100), c.speed_pct),
        .cast_time_ms = hasted(pos(scale(a.cast_time_ms, c)), st.cast_speed),            // [U-53][U-114]
        .cooldown_ms = hasted(pos(scale(a.cooldown_ms, c)), haste),
        .cost = pos(scale(a.cost, c)),
        .light_radius = [&] {
            const std::int64_t r = pos(scale(a.light_radius, c));
            return r == 0 && base_element(a.sub) == Element::Fire ? kFireLightRadius : r;
        }(),
        .light_ms = pos(scale(a.light_ms, c)),
        .dur_pct = c.dur_pct,
        .dot_pct = c.dot_pct,
    };
}

// Runtime state of one multicast ability.
struct MulticastState {
    std::uint8_t casts_used = 0;
    std::int64_t window_end_ms = 0;

    // Returns true if a cast is allowed now; updates state.
    constexpr bool try_cast(const MulticastDef& d, std::int64_t now_ms) {
        if (casts_used > 0 && now_ms > window_end_ms) casts_used = 0;  // window expired
        if (casts_used >= d.max_casts) return false;
        ++casts_used;
        window_end_ms = now_ms + d.recast_window_ms;
        return true;
    }
    [[nodiscard]] constexpr bool finished(const MulticastDef& d, std::int64_t now_ms) const {
        return casts_used >= d.max_casts || (casts_used > 0 && now_ms > window_end_ms);
    }
};

using Kit = std::array<AbilityDef, std::size_t(Slot::Count)>;

// [U-109] Targeted skills close to 0. [X-43] limit = 1 per champion.
inline constexpr int kMaxTargetedPerKit = 1;

// Content check for champion data files.
[[nodiscard]] constexpr bool kit_ok(const Kit& kit, bool is_mage) {
    int targeted = 0;
    for (const AbilityDef& a : kit) {
        targeted += a.delivery == Delivery::PointClick;
        const bool deals_damage = a.damage.base != 0 || a.damage.per_rank != 0 || a.damage.ep_pct != 0 ||
                                  a.damage.ad_pct != 0 || a.damage.bonus_ad_pct != 0;
        // [U-110] non-mages: damage ALWAYS scales with attack or bonus attack.
        if (!is_mage && deals_damage && a.damage.ad_pct == 0 && a.damage.bonus_ad_pct == 0) return false;
    }
    return targeted <= kMaxTargetedPerKit;
}

static_assert([] {
    MulticastDef d{.max_casts = 3, .recast_window_ms = 1'000};
    MulticastState s;
    return s.try_cast(d, 0) && s.try_cast(d, 500) && s.try_cast(d, 900) && !s.try_cast(d, 950) &&
           s.finished(d, 950);
}());

// Rank 3, level 20, 100 EP: 80 + 2×40 + 60 % EP = 220 damage; radius 150 + 19×2 = 188.
static_assert([] {
    Stats st;
    st.elemental_power.base = 10'000;
    AbilityDef q{.damage = {.base = 80, .per_rank = 40, .ep_pct = 60}, .radius = {.base = 150, .per_level = 2}};
    const ResolvedAbility r = resolve(q, {.stats = st, .rank = 3, .level = 20});
    return r.damage == 220 && r.radius == 188 && r.cooldown_ms == 8'000 && r.light_radius == 0;
}());

// [U-123] Comet: 175 % power, 140 % size → 220 → 385 damage, radius 188 → 263.
static_assert([] {
    Stats st;
    st.elemental_power.base = 10'000;
    AbilityDef q{.damage = {.base = 80, .per_rank = 40, .ep_pct = 60}, .radius = {.base = 150, .per_level = 2}};
    const ResolvedAbility r = resolve(q, {.stats = st, .rank = 3, .level = 20, .power_pct = 175, .size_pct = 140, .radius_pct = 140});
    return r.damage == 385 && r.radius == 263;
}());

// [U-116] A fire skill with no light set still lights 6 cells.
static_assert(resolve(AbilityDef{.sub = SubElement::Fire}, {.stats = Stats{}}).light_radius == kFireLightRadius);

}  // namespace em
