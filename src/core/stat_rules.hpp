#pragma once
// ======================================================================================
//  STACKING RULES [U-219][U-242][U-248] — how every number combines. The game code follows these;
//  the shop / stat panels show the same text. Change a rule here and in the function that applies it.
//
//  Defence / elemental defence (like LoL):  damage × 100 / (100 + effective defence)
//      effective defence = ((defence − flat reduction) × (1 − % reduction)) × (1 − % penetration) − flat penetration,
//      never below 0 from penetration (reduction may go below 0 = more damage). Order = LoL [U-248].
//  Haste (elemental / non-elemental):        cooldown × 100 / (100 + haste)        — additive, never reaches 0
//  Cast speed:                               cast time × 100 / (100 + cast speed)  — additive
//  Attack speed:                             base AS × (1 + growth % × (level − 1) + bonus %)  — % additive
//  Move speed:                               (base + flat bonuses) × (1 + bonus %)  then × slows / hastes
//                                            (strongest slow only; hastes add up; swimming halves)
//  Health / attack / elemental power:        base + base growth × (level − 1) + flat bonuses — additive
//  Tenacity:                                 additive, cap 80 %
//  Element resist / slow resist:             additive, cap 60 % / 80 %
//  Model size:                               additive %, never below 50 %
//  Item % stats (e.g. +5 % health):          % of the BASE value, additive with each other
//  Sky events (comet, full moon, eclipse):   applied FIRST — environment modifiers [U-237]
// ======================================================================================
#include <cstdint>
#include <string_view>

namespace em {

// How several sources of the same effect combine [U-242].
enum class StackRule : std::uint8_t {
    Additive,             // a + b
    Multiplicative,       // a × b
    MultiplicativePct,    // (1 + a%) × (1 + b%) − 1
    DiminishingReturns,   // 1 − (1 − a)(1 − b)   (each source covers part of what is left)
    Strongest,            // only the biggest one counts
    None,                 // does not stack: one source only
};
// What happens when the same effect is applied again [U-242].
enum class ReapplyRule : std::uint8_t {
    Refresh,              // the timer starts again
    KeepFirst,            // the first one stays (newer ones are ignored)
    PreferNewest,         // the newest one replaces the older
    StackPerSource,       // every source (e.g. every different item) stacks; the same source refreshes
    StackAlways,          // every application stacks
};

[[nodiscard]] constexpr double combine(StackRule r, double a, double b) {
    switch (r) {
        case StackRule::Additive: return a + b;
        case StackRule::Multiplicative: return a * b;
        case StackRule::MultiplicativePct: return (1 + a) * (1 + b) - 1;
        case StackRule::DiminishingReturns: return 1 - (1 - a) * (1 - b);
        case StackRule::Strongest: return a > b ? a : b;
        case StackRule::None: return a;
    }
    return a;
}
static_assert(combine(StackRule::DiminishingReturns, 0.5, 0.5) == 0.75);
static_assert(combine(StackRule::MultiplicativePct, 0.1, 0.1) > 0.209 && combine(StackRule::MultiplicativePct, 0.1, 0.1) < 0.211);

// [U-248] LoL order: flat reduction, % reduction, % penetration, flat penetration (lethality).
[[nodiscard]] constexpr double effective_defence(double def, double flat_red, double pct_red, double pct_pen, double flat_pen) {
    double d = def - flat_red;
    if (d > 0) d *= 1 - pct_red;
    if (d > 0) d *= 1 - pct_pen;
    if (d > 0) {
        d -= flat_pen;
        if (d < 0) d = 0;
    }
    return d;
}
static_assert(effective_defence(100, 10, 0.3, 0.4, 20) > 17.7 && effective_defence(100, 10, 0.3, 0.4, 20) < 17.9);

// One line per stat for the shop / stat panels.
[[nodiscard]] constexpr std::string_view stacking_text(std::string_view stat) {
    if (stat == "defence" || stat == "elemental defence") return "adds up; damage × 100 / (100 + defence)";
    if (stat == "haste") return "adds up; cooldown × 100 / (100 + haste) — never 0";
    if (stat == "cast speed") return "adds up; cast time × 100 / (100 + cast speed)";
    if (stat == "attack speed") return "% adds up on top of the base attack speed";
    if (stat == "move speed") return "flat adds up, then × (1 + bonus %)";
    if (stat == "move speed %") return "% adds up, applied after flat move speed";
    if (stat == "tenacity") return "adds up, max 80 %";
    if (stat == "element resist") return "adds up per element, max 60 %";
    if (stat == "slow resist") return "adds up, max 80 % (ice and other slows apart)";
    if (stat == "model size") return "adds up, never below 50 %";
    if (stat == "penetration") return "% first, then flat";
    return "adds up";
}

}  // namespace em
