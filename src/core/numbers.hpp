#pragma once
// [U-482] ONE way to write any number in an editable file (own champions, game versions, …):
//     570      set it                 2.8     decimals are fine (scaled like the key's unit)
//     =570     set it                 +40     add          -40   subtract
//     +30%     30 % more              -20%    20 % less    150%  150 % of the current value
//     *1.5     multiply (x1.5 too)
// "current value" = what the key had before this line (the base champion's / the official number).
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

namespace em {

struct NumExpr {
    enum Op : std::uint8_t { Set, Add, Pct, OfPct, Mul } op = Set;
    double v = 0;
    [[nodiscard]] bool relative() const { return op != Set; }
};

[[nodiscard]] inline std::optional<NumExpr> parse_num_expr(std::string_view s) {
    while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
    if (s.empty()) return std::nullopt;
    NumExpr e;
    bool sign = false;
    double sgn = 1;
    if (s.front() == '*' || s.front() == 'x') e.op = NumExpr::Mul, s.remove_prefix(1);
    else if (s.front() == '=') s.remove_prefix(1);
    else if (s.front() == '+' || s.front() == '-') sign = true, sgn = s.front() == '-' ? -1 : 1, s.remove_prefix(1);
    const bool pct = !s.empty() && s.back() == '%';
    if (pct) s.remove_suffix(1);
    double x = 0;
    const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), x);
    if (ec != std::errc{} || p != s.data() + s.size() || !std::isfinite(x)) return std::nullopt;
    e.v = sgn * x;
    if (e.op == NumExpr::Mul) {
        if (pct) return std::nullopt;
    } else if (pct) {
        e.op = sign ? NumExpr::Pct : NumExpr::OfPct;
    } else if (sign) {
        e.op = NumExpr::Add;
    }
    return e;
}

// the new value: `old` in the field's own unit, `scale` = field units per written unit (hp: 100 hundredths per point)
[[nodiscard]] inline std::int64_t apply_num_expr(const NumExpr& e, std::int64_t old, double scale) {
    double r = 0;
    switch (e.op) {
        case NumExpr::Set: r = e.v * scale; break;
        case NumExpr::Add: r = double(old) + e.v * scale; break;
        case NumExpr::Pct: r = double(old) * (100.0 + e.v) / 100.0; break;
        case NumExpr::OfPct: r = double(old) * e.v / 100.0; break;
        case NumExpr::Mul: r = double(old) * e.v; break;
    }
    return std::llround(std::clamp(r, -4e18, 4e18));
}

}  // namespace em
