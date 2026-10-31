// [U-177][U-178][U-318][U-319][U-323][U-324] The setup editor: one saved setup = everything prepared before a game.
// Tabs: Runes (classic marks / seals / glyphs / quintessences) · Masteries (30 points, three trees) ·
// Specialisation & Keystone (+ the rune shards) · Build (step list) · Levelling (skill order).
// A summary panel on the right always shows what the whole setup gives. Readable from 1280 × 720.
#include <algorithm>
#include <format>

#include "client/app.hpp"
#include "client/locale.hpp"

namespace em {

using namespace gfx;

namespace {

// [U-483] any stat: its label = the StatKey name with spaces
std::string stat_label(StatKey k) {
    std::string s = k < StatKey::Count ? kStatKeyNames[int(k)] : "?";
    for (char& c : s) c = c == '_' ? ' ' : c;
    return s;
}
constexpr const char* kKeyNames[] = {"P", "Q", "W", "E", "R", "T", "Y"};
constexpr Rgba kGold{255, 220, 140}, kGrey{150, 150, 165}, kText{220, 220, 230};
constexpr Rgba kKindColour[] = {{230, 120, 70, 255}, {220, 190, 80, 255}, {110, 190, 230, 255}, {150, 220, 120, 255}, {200, 140, 230, 255}};

std::string fmt_value(StatKey k, std::int32_t v, bool pct = false) {
    const std::string num = v % 100 == 0 ? std::format("{:+d}", v / 100) : std::format("{:+.1f}", v / 100.0);
    return num + (pct || stat_is_pct(k) ? "%" : "");
}
std::string fmt_mod(const StatMod& m, int mult = 1) {
    return std::format("{} {}{}", fmt_value(m.key, m.value * mult, m.pct), m.pct ? "of " : "", tr(stat_label(m.key)));
}

// greedy word wrap at `max_w` pixels
std::vector<std::string> wrap_lines(const std::string& s, float max_w) {
    std::vector<std::string> out;
    std::string cur, word;
    auto flush_word = [&] {
        if (word.empty()) return;
        if (!cur.empty() && text_w(cur + " " + word) > max_w) out.push_back(cur), cur.clear();
        cur += (cur.empty() ? "" : " ") + word;
        word.clear();
    };
    for (char c : s) {
        if (c == ' ') flush_word();
        else word += c;
    }
    flush_word();
    if (!cur.empty()) out.push_back(cur);
    return out;
}

struct Ctx {
    SDL_Renderer* r;
    Ui& ui;
    std::vector<std::string> tip;  // the hover tooltip, drawn last
    bool changed = false;
    Resources* res = nullptr;  // [U-432] icons of runes / masteries (icons/runes/<name>.png, icons/masteries/<name>.png)
    bool left(float x, float y, float w, float h) const { return ui.clicked && hover(ui, x, y, w, h); }
    // a square icon; returns how much room it took (0 when the pack has no picture yet)
    float icon(const char* folder, std::string_view name, float x, float y, float s, bool dim) const {
        SDL_Texture* t = res ? res->texture(std::format("icons/{}/{}.png", folder, Resources::slug(name))) : nullptr;
        if (!t) return 0;
        const SDL_FRect d{x, y, s, s};
        if (dim) SDL_SetTextureColorMod(t, 110, 110, 110);
        SDL_RenderTexture(r, t, nullptr, &d);
        if (dim) SDL_SetTextureColorMod(t, 255, 255, 255);
        return s + 4;
    }
    bool right(float x, float y, float w, float h) const { return ui.rclicked && hover(ui, x, y, w, h); }
};

float draw_wrapped(Ctx& c, float x, float y, float w, const std::string& s, Rgba col) {
    float dy = 0;
    for (const std::string& l : wrap_lines(s, w)) text(c.r, x, y + dy, l, col), dy += 14;
    return dy;
}

// ---------------------------------------------------------------- tab 0: classic runes [U-318]
float tab_runes(Ctx& c, float x, float y, float w, Loadout& lo) {
    const float y0 = y;
    for (int sec = 0; sec < kClassicSections; ++sec) {
        int used = 0;
        for (auto n : lo.classic[std::size_t(sec)]) used += n;
        text(c.r, x, y + 2, std::format("{}   {}/{}", tr(kClassicSectionNames[sec]), used, kClassicSlots), kGold);
        // the nine slots of the page, coloured by the kind that fills them
        float sx = x + 270;
        int slot = 0;
        for (int k = 0; k < kClassicKinds; ++k)
            for (int n = 0; n < lo.classic[std::size_t(sec)][std::size_t(k)]; ++n, ++slot) fill_rect(c.r, sx + float(slot) * 18, y, 15, 15, kKindColour[k]);
        for (; slot < kClassicSlots; ++slot) rect(c.r, sx + float(slot) * 18, y, 15, 15, {90, 90, 110});
        y += 22;
        const float cw = (w - 4 * 6) / 5;
        for (int k = 0; k < kClassicKinds; ++k) {
            const float cx = x + float(k) * (cw + 6);
            const RuneDef& d = classic_runes()[std::size_t(sec)][std::size_t(k)];
            if (d.name.empty()) continue;  // [U-483] removed by the game version
            std::uint8_t& n = lo.classic[std::size_t(sec)][std::size_t(k)];
            const bool hv = hover(c.ui, cx, y, cw, 46);
            fill_rect(c.r, cx, y, cw, 46, n ? Rgba{70, 50, 30, 235} : hv ? Rgba{60, 60, 80, 235} : Rgba{34, 34, 46, 225});
            rect(c.r, cx, y, cw, 46, n ? kKindColour[k] : Rgba{100, 100, 130});
            fill_rect(c.r, cx, y, 4, 46, kKindColour[k]);
            const float ic = c.icon("runes", d.name, cx + 7, y + 3, 40, n == 0);  // [U-432]
            text(c.r, cx + 9 + ic, y + 5, fit_text(tr(d.name), cw - 44 - ic), {235, 235, 240});
            text(c.r, cx + 9 + ic, y + 24, fit_text(fmt_mod(d.mod), cw - 14 - ic), {170, 210, 170});
            if (n) text(c.r, cx + cw - 30, y + 5, std::format("x{}", n), {255, 220, 120});
            if (c.left(cx, y, cw, 46) && used < kClassicSlots) ++n, c.changed = true, c.ui.clicked = false;
            if (c.right(cx, y, cw, 46) && n > 0) --n, c.changed = true, c.ui.rclicked = false;
            if (hv) c.tip = {std::string(tr(d.name)), std::format("{} {}", tr("Each piece:"), fmt_mod(d.mod)),
                             std::format("{} {}", tr("On the page:"), fmt_mod(d.mod, n)), tr("click +1 · right click −1")};
        }
        y += 56;
    }
    // quintessences: three slots, six kinds, strong
    text(c.r, x, y + 2, std::format("{}   {}", tr("Quintessences (few, strong)"), tr("click again = empty")), kGold);
    y += 22;
    for (int q = 0; q < kQuintSlots; ++q) {
        text(c.r, x, y + 6, std::format("Q{}", q + 1), kGrey);
        int nq = 0;
        for (const RuneDef& rd : quints()) nq += !rd.name.empty();
        const float bw = (w - 34 - float(std::max(nq, 1) - 1) * 4) / float(std::max(nq, 1));
        for (int k = 0, at = 0; k < kQuintKinds; ++k) {
            if (quints()[std::size_t(k)].name.empty()) continue;
            const float bx = x + 34 + float(at++) * (bw + 4);
            std::string nm = std::string(tr(quints()[std::size_t(k)].name));
            const auto of = nm.find(" of ");
            if (of != std::string::npos) nm = nm.substr(of + 4);
            if (button(c.r, c.ui, bx, y, bw, 24, fit_text(nm, bw - 8), lo.quints[std::size_t(q)] == k)) lo.quints[std::size_t(q)] = std::int8_t(lo.quints[std::size_t(q)] == k ? -1 : k), c.changed = true;
            if (hover(c.ui, bx, y, bw, 24)) c.tip = {std::string(tr(quints()[std::size_t(k)].name)), fmt_mod(quints()[std::size_t(k)].mod)};
        }
        y += 28;
    }
    return y - y0;
}

// ---------------------------------------------------------------- tab 1: masteries [U-319]
float tab_masteries(Ctx& c, float x, float y, float w, Loadout& lo, int& tree) {
    const float y0 = y;
    const int total = mastery_points(lo);
    const float tw = (w - 12) / 3;
    for (int t = 0; t < kMasteryTrees; ++t)
        if (button(c.r, c.ui, x + float(t) * (tw + 6), y, tw, 28,
                   std::format("{}  {}", tr(mastery_trees()[std::size_t(t)].name), mastery_points_in_tree(lo, t)), tree == t))
            tree = t;
    y += 34;
    text(c.r, x, y, std::format("{}: {} / {}   ({}, {})", tr("Points spent"), total, kMasteryPoints, tr(mastery_trees()[std::size_t(tree)].theme),
                               tr("click +1 · right click −1")), total >= kMasteryPoints ? Rgba{255, 200, 100} : kText);
    y += 20;
    const float lw = 96, nw = (w - lw - 2 * 8) / 3, nh = 46, gap = 16;
    const MasteryTree& tr_ = mastery_trees()[std::size_t(tree)];
    for (int tier = 0; tier < kMasteryTiers; ++tier) {
        const int below = mastery_points_below(lo, tree, tier), need = tier * kMasteryTierStep;
        const bool open = below >= need;
        text(c.r, x, y + 8, std::format("{} {}", tr("Tier"), tier + 1), open ? kGold : Rgba{110, 110, 120});
        text(c.r, x, y + 22, open ? tr("open") : std::format("{} {} {}", tr("needs"), need, tr("pts")), open ? Rgba{140, 200, 140} : Rgba{200, 110, 110});
        for (int col = 0; col < kMasteryPerTier; ++col) {
            const MasteryDef& d = tr_.tiers[std::size_t(tier)][std::size_t(col)];
            if (d.max_rank == 0) continue;
            const float nx = x + lw + float(col) * (nw + 8);
            std::uint8_t& rank = lo.masteries[std::size_t(mastery_index(tree, tier, col))];
            if (tier + 1 < kMasteryTiers) {  // the "tree": connectors down to the next tier
                for (int c2 = 0; c2 < kMasteryPerTier; ++c2)
                    if (tr_.tiers[std::size_t(tier + 1)][std::size_t(c2)].max_rank)
                        line(c.r, nx + nw / 2, y + nh, x + lw + float(c2) * (nw + 8) + nw / 2, y + nh + gap, rank ? Rgba{200, 150, 70, 200} : Rgba{70, 70, 85, 160});
            }
            const bool hv = hover(c.ui, nx, y, nw, nh);
            Rgba bg = !open ? Rgba{24, 24, 28, 230} : rank ? Rgba{75, 52, 28, 235} : hv ? Rgba{62, 62, 84, 235} : Rgba{40, 40, 56, 230};
            fill_rect(c.r, nx, y, nw, nh, bg);
            rect(c.r, nx, y, nw, nh, !open ? Rgba{55, 55, 62} : rank ? Rgba{255, 190, 90} : Rgba{110, 110, 140});
            const float ic = c.icon("masteries", d.name, nx + 4, y + 4, 38, !open || rank == 0);  // [U-432]
            text(c.r, nx + 6 + ic, y + 5, fit_text(tr(d.name), nw - 56 - ic), open ? Rgba{235, 235, 240} : Rgba{100, 100, 108});
            text(c.r, nx + 6 + ic, y + 26, fit_text(fmt_mod(d.mod), nw - 60 - ic), open ? Rgba{170, 210, 170} : Rgba{90, 100, 90});
            for (int p = 0; p < d.max_rank; ++p) {  // pips + the number
                const float px = nx + nw - 8 - float(d.max_rank - p) * 9;
                fill_rect(c.r, px, y + 27, 7, 7, p < rank ? Rgba{255, 210, 90} : Rgba{50, 50, 60});
            }
            text(c.r, nx + nw - 44, y + 5, std::format("{}/{}", rank, d.max_rank), rank ? Rgba{255, 220, 120} : kGrey);
            if (c.left(nx, y, nw, nh) && open) {
                Loadout t = lo;
                ++t.masteries[std::size_t(mastery_index(tree, tier, col))];
                const Loadout v = validated(t);
                if (v == t) lo = v, c.changed = true;
                c.ui.clicked = false;
            }
            if (c.right(nx, y, nw, nh)) {
                Loadout t = lo;
                if (t.masteries[std::size_t(mastery_index(tree, tier, col))] > 0) {
                    --t.masteries[std::size_t(mastery_index(tree, tier, col))];
                    if (validated(t) == t) lo = t, c.changed = true;  // (a point that holds up a later tier stays)
                }
                c.ui.rclicked = false;
            }
            if (hv) {
                c.tip = {std::format("{}  ({}/{})", tr(d.name), rank, d.max_rank), std::format("{} {}", tr("Each rank:"), fmt_mod(d.mod)),
                         std::format("{} {}", tr("Now:"), fmt_mod(d.mod, rank))};
                if (!open) c.tip.push_back(std::format("{} {} {} ({} {})", tr("Locked: needs"), need, tr("points in the tiers above"), tr("you have"), below));
                else if (tier + 1 < kMasteryTiers && rank > 0) c.tip.push_back(tr("A point here may hold up a higher tier."));
            }
        }
        y += nh + gap;
    }
    return y - y0;
}

// ---------------------------------------------------------------- tab 2: specialisation + keystone + shards [U-177]
float tab_spec(Ctx& c, float x, float y, float w, Loadout& lo) {
    const float y0 = y;
    text(c.r, x, y, tr("Keystone — pick 1 (a special effect)"), kGold);
    y += 18;
    int nk = 0;  // [U-483] only what is there (a version may remove / add)
    for (const KeystoneDef& kd : keystones()) nk += !kd.name.empty();
    const float kw = (w - float(std::max(nk, 1) - 1) * 6) / float(std::max(nk, 1));
    for (int k = 0, at = 0; k < int(keystones().size()); ++k) {
        if (keystones()[std::size_t(k)].name.empty()) continue;
        const float kx = x + float(at++) * (kw + 6);
        if (button(c.r, c.ui, kx, y, kw, 26, fit_text(tr(keystones()[std::size_t(k)].name), kw - 8), lo.keystone == k)) lo.keystone = std::int8_t(lo.keystone == k ? -1 : k), c.changed = true;
        if (hover(c.ui, kx, y, kw, 26)) c.tip = {std::string(tr(keystones()[std::size_t(k)].name)), std::string(tr(keystones()[std::size_t(k)].desc))};
    }
    y += 32;
    y += draw_wrapped(c, x, y, w, lo.keystone >= 0 ? std::string(tr(keystones()[std::size_t(lo.keystone)].desc)) : std::string(tr("No keystone picked.")), {255, 230, 150});
    y += 10;
    text(c.r, x, y, tr("Specialisation — 1 tree, then 1 node per tier"), kGold);
    y += 18;
    int nt = 0;
    for (const SpecTree& st : spec_trees()) nt += !st.name.empty();
    const float tw = (w - float(std::max(nt, 1) - 1) * 6) / float(std::max(nt, 1));
    for (int t = 0, at = 0; t < int(spec_trees().size()); ++t) {
        if (spec_trees()[std::size_t(t)].name.empty()) continue;
        const float tx = x + float(at++) * (tw + 6);
        if (button(c.r, c.ui, tx, y, tw, 26, fit_text(std::format("{} · {}", tr(spec_trees()[std::size_t(t)].name), tr(spec_trees()[std::size_t(t)].theme)), tw - 8), lo.tree == t))
            lo.tree = std::int8_t(lo.tree == t ? -1 : t), lo.nodes = {-1, -1, -1}, c.changed = true;
    }
    y += 34;
    const float nw = (w - 80 - 8) / 2;
    for (int tier = 0; tier < 3; ++tier) {
        text(c.r, x, y + 9, std::format("{} {}", tr("Tier"), tier + 1), lo.tree >= 0 ? kGold : Rgba{110, 110, 120});
        for (int n = 0; n < 2; ++n) {
            const float nx = x + 80 + float(n) * (nw + 8);
            if (lo.tree < 0) {  // greyed: no tree yet
                fill_rect(c.r, nx, y, nw, 30, {24, 24, 28, 230});
                rect(c.r, nx, y, nw, 30, {55, 55, 62});
                continue;
            }
            const SpecNode& node = spec_trees()[std::size_t(lo.tree)].tiers[std::size_t(tier)][std::size_t(n)];
            if (node.name.empty()) continue;  // [U-483]
            if (button(c.r, c.ui, nx, y, nw, 30, fit_text(std::format("{}  {}", tr(node.name), fmt_mod(node.mod)), nw - 8), lo.nodes[std::size_t(tier)] == n))
                lo.nodes[std::size_t(tier)] = std::int8_t(lo.nodes[std::size_t(tier)] == n ? -1 : n), c.changed = true;
            if (hover(c.ui, nx, y, nw, 30)) c.tip = {std::string(tr(node.name)), fmt_mod(node.mod)};
        }
        if (tier < 2) line(c.r, x + 80 + nw / 2, y + 30, x + 80 + nw / 2, y + 38, {120, 120, 150});
        y += 38;
    }
    y += 4;
    text(c.r, x, y, tr("Rune shards — 1 per slot (the old runes)"), kGold);
    y += 18;
    for (int slot = 0; slot < 3; ++slot) {
        text(c.r, x, y + 6, tr(kRuneSlotNames[slot]), kGrey);
        int ns = 0;
        for (const RuneDef& rd : shards()[std::size_t(slot)]) ns += !rd.name.empty();
        const float bw = (w - 80 - float(std::max(ns, 1) - 1) * 6) / float(std::max(ns, 1));
        for (int r = 0, at = 0; r < kMaxShards; ++r) {
            if (shards()[std::size_t(slot)][std::size_t(r)].name.empty()) continue;
            const float bx = x + 80 + float(at++) * (bw + 6);
            if (button(c.r, c.ui, bx, y, bw, 24, fit_text(tr(shards()[std::size_t(slot)][std::size_t(r)].name), bw - 8), lo.runes[std::size_t(slot)] == r))
                lo.runes[std::size_t(slot)] = std::int8_t(lo.runes[std::size_t(slot)] == r ? -1 : r), c.changed = true;
        }
        y += 28;
    }
    return y - y0;
}

}  // namespace

// ---------------------------------------------------------------- tab 3 + 4 are App members (they use the item browser / shop_)
void App::setup_editor(float x, float y, float w, float h, SavedSetup& s) {
    Ctx c{ren_, ui_, {}, false, &res_};
    int sw = 0, sh = 0;
    SDL_GetRenderOutputSize(ren_, &sw, &sh);
    // ---- tabs
    static const char* kTabs[] = {"Runes", "Masteries", "Specialisation & Keystone", "Build", "Levelling"};
    const float tab_w[] = {100, 130, 250, 100, 120};
    float tx = x;
    for (int t = 0; t < 5; ++t) {
        if (button(ren_, ui_, tx, y, tab_w[t], 28, tr(kTabs[t]), setup_tab_ == t)) setup_tab_ = t, build_adding_ = false;
        tx += tab_w[t] + 6;
    }
    y += 38;
    h -= 38;
    const bool wide = build_adding_ && setup_tab_ == 3;  // the item browser takes the whole width
    const float panel_w = 270, cw = wide ? w : w - panel_w - 12;
    Loadout& lo = s.loadout;
    const Loadout before = lo;
    SDL_Rect clip{int(x - 4), int(y - 2), int(cw + 8), int(h + 2)};
    if (!wide) SDL_SetRenderClipRect(ren_, &clip);
    switch (setup_tab_) {
        case 0: tab_runes(c, x, y, cw, lo); break;
        case 1: tab_masteries(c, x, y, cw, lo, mastery_tree_); break;
        case 2: tab_spec(c, x, y, cw, lo); break;
        case 3: {  // build creator [U-323]
            if (build_adding_) {
                if (button(ren_, ui_, x, y, 160, 26, tr("Done"))) build_adding_ = false;
                text(ren_, x + 170, y + 8, std::format("{} {}/{}", tr("Steps:"), s.build.size(), kMaxBuildSteps), kText);
                const int added = shop_.draw(ren_, ui_, x, y + 32, cw, h - 32, tr("Add as the next step"), [&](int) { return int(s.build.size()) < kMaxBuildSteps; });
                if (added >= 0 && int(s.build.size()) < kMaxBuildSteps) s.build.push_back({std::int16_t(added), ""}), build_sel_ = int(s.build.size()) - 1, c.changed = true;
                break;
            }
            text(ren_, x, y + 2, tr("Step by step, in buying order. Auto-buy buys it when you stand in the shop."), kGrey);
            y += 20;
            if (button(ren_, ui_, x, y, 190, 26, tr("Add an item"), false, int(s.build.size()) < kMaxBuildSteps)) build_adding_ = true;
            int total = 0;
            for (const BuildStep& st : s.build) total += kItems[std::size_t(st.item)].cost;
            text(ren_, x + 200, y + 8, std::format("{}: {}   ·   {}: {}", tr("Steps"), s.build.size(), tr("Total gold"), total), {255, 215, 80});
            y += 32;
            build_sel_ = std::min(build_sel_, int(s.build.size()) - 1);
            int del = -1, swap_with = -1;
            for (int i = 0; i < int(s.build.size()); ++i, y += 31) {
                BuildStep& st = s.build[std::size_t(i)];
                const ItemDef& d = kItems[std::size_t(st.item)];
                const bool sel = build_sel_ == i;
                fill_rect(ren_, x, y, cw, 29, sel ? Rgba{70, 50, 30, 235} : Rgba{34, 34, 46, 225});
                rect(ren_, x, y, cw, 29, sel ? Rgba{255, 170, 80} : Rgba{90, 90, 120});
                draw_item_icon(ren_, st.item, x + 30, y + 2, 25);
                text(ren_, x + 8, y + 10, std::format("{}", i + 1), kGold);
                text(ren_, x + 62, y + 3, fit_text(std::string(d.name), cw - 62 - 120), {235, 235, 240});
                text(ren_, x + 62, y + 16, fit_text(st.text.empty() ? std::string(tr("(no text)")) : st.text, cw - 62 - 120), st.text.empty() ? Rgba{100, 100, 110} : Rgba{170, 200, 230});
                text(ren_, x + cw - 112, y + 10, std::format("{}", d.cost), {255, 215, 80});
                if (hover(ui_, x, y, cw - 84, 29) && ui_.clicked) build_sel_ = i, ui_.clicked = false;
                if (button(ren_, ui_, x + cw - 80, y + 3, 24, 23, "▲", false, i > 0)) swap_with = i - 1, build_sel_ = i - 1;
                if (button(ren_, ui_, x + cw - 54, y + 3, 24, 23, "▼", false, i + 1 < int(s.build.size()))) swap_with = i, build_sel_ = i + 1;
                if (button(ren_, ui_, x + cw - 28, y + 3, 24, 23, "✕")) del = i;
                if (hover(ui_, x + 30, y, cw - 120, 29)) c.tip = item_lines(d), c.tip.insert(c.tip.begin(), std::string(d.name));
            }
            if (swap_with >= 0) std::swap(s.build[std::size_t(swap_with)], s.build[std::size_t(swap_with) + 1]), c.changed = true;
            if (del >= 0) s.build.erase(s.build.begin() + del), c.changed = true, build_sel_ = std::min(build_sel_, int(s.build.size()) - 1);
            if (build_sel_ >= 0 && build_sel_ < int(s.build.size())) {
                y += 14;
                std::string& t = s.build[std::size_t(build_sel_)].text;
                const std::string old = t;
                text_field(ren_, ui_, 120, x, y, std::min(cw, 640.f), t, std::format("{} {}", tr("Text for step"), build_sel_ + 1));
                if (t != old) c.changed = true;
            }
            break;
        }
        default: {  // auto-levelling [U-324]
            text(ren_, x, y + 2, tr("The skill points are spent by this order when auto-levelling is on (Options → Game)."), kGrey);
            y += 22;
            if (button(ren_, ui_, x, y, 200, 26, tr("Priority list"), s.level_mode == 0)) s.level_mode = 0, c.changed = true;
            if (button(ren_, ui_, x + 206, y, 200, 26, tr("Exact 18 steps"), s.level_mode == 1)) s.level_mode = 1, c.changed = true;
            y += 36;
            if (s.level_mode == 0) {
                y += draw_wrapped(c, x, y, cw, tr("The ultimate is always taken when it is allowed. After that the first skill of this list that can be raised is raised."), kText);
                y += 6;
                int up = -1, down = -1;
                for (int i = 0; i < 6; ++i, y += 30) {
                    text(ren_, x, y + 8, std::format("{}.", i + 1), kGold);
                    fill_rect(ren_, x + 30, y, 90, 26, {45, 40, 60, 235});
                    rect(ren_, x + 30, y, 90, 26, {140, 120, 190});
                    text(ren_, x + 50, y + 8, std::format("{}{}", kKeyNames[s.level_prio[std::size_t(i)]], s.level_prio[std::size_t(i)] == 6 ? "  (ult)" : ""), {235, 235, 240});
                    if (button(ren_, ui_, x + 130, y, 26, 26, "▲", false, i > 0)) up = i;
                    if (button(ren_, ui_, x + 160, y, 26, 26, "▼", false, i < 5)) down = i;
                }
                if (up >= 0) std::swap(s.level_prio[std::size_t(up)], s.level_prio[std::size_t(up) - 1]), c.changed = true;
                if (down >= 0) std::swap(s.level_prio[std::size_t(down)], s.level_prio[std::size_t(down) + 1]), c.changed = true;
                if (button(ren_, ui_, x, y + 4, 200, 26, tr("Reset: Q W E R T Y"))) s.level_prio = {1, 2, 3, 4, 5, 6}, c.changed = true;
            } else {
                y += draw_wrapped(c, x, y, cw, tr("One skill per level-up. Click = next skill, right click = previous, empty = use the priority list for that step. A step the game does not allow yet is taken from the priority list."), kText);
                y += 6;
                const float bw = (cw - 5 * 6) / 6;
                for (int i = 0; i < 18; ++i) {
                    const float bx = x + float(i % 6) * (bw + 6), by = y + float(i / 6) * 34;
                    std::int8_t& v = s.level_exact[std::size_t(i)];
                    const bool hv = hover(ui_, bx, by, bw, 28);
                    fill_rect(ren_, bx, by, bw, 28, v >= 0 ? Rgba{70, 50, 30, 235} : hv ? Rgba{60, 60, 80, 235} : Rgba{34, 34, 46, 225});
                    rect(ren_, bx, by, bw, 28, v >= 0 ? Rgba{255, 170, 80} : Rgba{100, 100, 130});
                    text(ren_, bx + 6, by + 10, std::format("{}", i + 1), kGrey);
                    text(ren_, bx + bw / 2 - 4, by + 10, v >= 0 ? kKeyNames[v] : "-", {235, 235, 240});
                    if (c.left(bx, by, bw, 28)) v = std::int8_t(v >= 6 ? -1 : v + 1), c.changed = true, ui_.clicked = false;
                    if (c.right(bx, by, bw, 28)) v = std::int8_t(v <= -1 ? 6 : v - 1), c.changed = true, ui_.rclicked = false;
                    if (v == 0) v = -1;  // (P is not a choice)
                }
                y += 3 * 34 + 8;
                int count[7] = {};
                for (auto v : s.level_exact)
                    if (v >= 1) ++count[v];
                std::string sum = tr("Ranks after 18 steps:");
                for (int k = 1; k <= 6; ++k) sum += std::format("  {} {}", kKeyNames[k], count[k]);
                text(ren_, x, y, sum, kText);
                y += 20;
                if (button(ren_, ui_, x, y, 200, 26, tr("Clear all steps"))) s.level_exact.fill(-1), c.changed = true;
            }
            break;
        }
    }
    if (!wide) SDL_SetRenderClipRect(ren_, nullptr);
    if (!(before == lo)) c.changed = true;

    // ---- summary: what the whole setup gives
    if (!wide) {
        const float px = x + w - panel_w, py = y - 38;
        fill_rect(ren_, px, py, panel_w, h + 38, {16, 16, 24, 235});
        rect(ren_, px, py, panel_w, h + 38, {90, 90, 120});
        float ly = py + 8;
        text(ren_, px + 10, ly, tr("Gives in total"), kGold, 1.5f);
        ly += 22;
        std::array<std::int32_t, std::size_t(StatKey::Count) * 2> sum{};  // [U-483] flat + "% of" per stat
        std::array<int, int(ModSource::Count)> src{};
        for_each_mod(lo, [&](ModSource so, const StatMod& m) {
            if (m.key < StatKey::Count) sum[std::size_t(m.key) * 2 + (m.pct ? 1 : 0)] += m.value;
            ++src[std::size_t(so)];
        });
        bool any = false;
        for (std::size_t k = 0; k < sum.size(); ++k)
            if (sum[k]) {
                any = true;
                const StatMod m{StatKey(k / 2), sum[k], (k & 1) != 0};
                text(ren_, px + 10, ly, fit_text(fmt_mod(m), panel_w - 20), {170, 230, 170});
                ly += 14;
            }
        if (!any) text(ren_, px + 10, ly, tr("nothing yet"), kGrey), ly += 14;
        ly += 8;
        line(ren_, px + 10, ly, px + panel_w - 10, ly, {90, 90, 120});
        ly += 8;
        int marks = 0;
        for (const auto& sec : lo.classic)
            for (auto n : sec) marks += n;
        int quints = 0;
        for (auto q : lo.quints) quints += q >= 0;
        auto row = [&](const std::string& s2, Rgba col = kText) {
            text(ren_, px + 10, ly, fit_text(s2, panel_w - 20), col);
            ly += 15;
        };
        row(std::format("{}: {}/{} + {}/{} {}", tr("Classic runes"), marks, kClassicSections * kClassicSlots, quints, kQuintSlots, tr("quints")));
        row(std::format("{}: {}/{} {}", tr("Masteries"), mastery_points(lo), kMasteryPoints, tr("points")));
        row(std::format("{}: {}", tr("Tree"), lo.tree >= 0 ? std::string(spec_trees()[std::size_t(lo.tree)].name) : std::string(tr("none"))));
        row(std::format("{}: {}", tr("Keystone"), lo.keystone >= 0 ? std::string(keystones()[std::size_t(lo.keystone)].name) : std::string(tr("none"))), {255, 230, 150});
        int total = 0;
        for (const BuildStep& st : s.build) total += kItems[std::size_t(st.item)].cost;
        row(std::format("{}: {} {} · {} g", tr("Build"), s.build.size(), tr("steps"), total));
        std::string lv = s.level_mode == 0 ? std::string(tr("priority")) + " " : std::string(tr("exact order"));
        if (s.level_mode == 0)
            for (auto p : s.level_prio) lv += kKeyNames[p];
        row(std::format("{}: {}", tr("Levelling"), lv));
        row(std::format("{} {} · {} {}", tr("Auto-buy"), set_.auto_buy ? tr("ON") : tr("OFF"), tr("auto-level"), set_.auto_level ? tr("ON") : tr("OFF")), kGrey);
        row(tr("(switch them in Options → Game)"), kGrey);
    }

    // ---- tooltip last, next to the mouse, kept on the screen
    if (!c.tip.empty()) {
        float tw = 0;
        for (const std::string& l : c.tip) tw = std::max(tw, text_w(l));
        tw = std::min(tw + 16, 420.f);
        const float th = float(c.tip.size()) * 14 + 10;
        const float tx2 = std::min(ui_.mx + 16, float(sw) - tw - 4), ty2 = std::min(ui_.my + 16, float(sh) - th - 4);
        fill_rect(ren_, tx2, ty2, tw, th, {12, 12, 20, 245});
        rect(ren_, tx2, ty2, tw, th, {255, 190, 100});
        for (std::size_t i = 0; i < c.tip.size(); ++i) text(ren_, tx2 + 8, ty2 + 6 + float(i) * 14, fit_text(c.tip[i], tw - 12), i == 0 ? kGold : kText);
    }
    if (c.changed) set_.save();
}

}  // namespace em
