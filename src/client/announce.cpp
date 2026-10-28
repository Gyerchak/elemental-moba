// [U-263] Announcements: a banner in the middle-top + the announcer voice, and an "events" window with the whole
// match history. [U-253] Sky events (eclipse, comet, full moon, alignment) are announced too.
// [U-262] Death recap like LoL: a short line when I die, a button to open the full window with graphs.
#include <algorithm>
#include <format>
#include <map>

#include "client/game_view.hpp"
#include "client/locale.hpp"
#include "client/modifiers.hpp"
#include "game/world.hpp"

namespace em {

using namespace gfx;

namespace {
struct AnnLook {
    const char* id;     // voice file: sounds/<lang>/announce/<id>.wav
    const char* title;
};
// index = AnnounceKind
constexpr AnnLook kAnn[] = {
    {"first_blood", "FIRST BLOOD"},  {"enemy_slain", "AN ENEMY HAS BEEN SLAIN"}, {"double_kill", "DOUBLE KILL"},
    {"triple_kill", "TRIPLE KILL"},  {"quadra_kill", "QUADRA KILL"},             {"penta_kill", "PENTA KILL"},
    {"shutdown", "SHUTDOWN"},        {"killing_spree", "KILLING SPREE"},         {"rampage", "RAMPAGE"},
    {"unstoppable", "UNSTOPPABLE"},  {"dominating", "DOMINATING"},               {"godlike", "GODLIKE"},
    {"legendary", "LEGENDARY"},      {"ace", "ACE"},                             {"executed", "EXECUTED"},
    {"turret_destroyed", "TURRET DESTROYED"}, {"inhibitor_destroyed", "INHIBITOR DESTROYED"},
    {"shrine_destroyed", "NATION SHRINE DESTROYED"}, {"objective_taken", "OBJECTIVE TAKEN"},
    {"minions_spawned", "MINIONS HAVE SPAWNED"},
    {"awakened", "A NON-BENDER AWAKENS"},  // [U-296]
};
static_assert(std::size(kAnn) == std::size_t(AnnounceKind::Count));

std::string mmss2(std::int64_t ms) { return std::format("{:02}:{:02}", std::max<std::int64_t>(0, ms) / 60'000, std::max<std::int64_t>(0, ms) / 1'000 % 60); }
}  // namespace

std::string GameView::unit_label(NetClient& net, std::uint32_t id) const {
    if (auto r = net.roster.find(id); r != net.roster.end())
        return std::format("{} ({})", r->second.name, champions()[std::size_t(std::max<int>(0, r->second.champ))].name);
    for (const DrawUnit& d : draw_)
        if (d.u.id == id) {
            switch (UnitKind(d.u.kind)) {
                case UnitKind::Minion: return tr("minion");
                case UnitKind::Turret: return tr("turret");
                case UnitKind::Inhibitor: return tr("inhibitor");
                case UnitKind::Nexus: return tr("Nation Shrine");
                default: return tr(net::kMobNames[std::min<int>(d.u.name_id, int(std::size(net::kMobNames)) - 1)]);
            }
        }
    return id ? tr("unknown") : tr("nobody");
}

void GameView::announce_text(NetClient& net, const net::NetAnnounce& a, std::string& title, std::string& line, std::string& voice) const {
    const AnnounceKind k = AnnounceKind(std::min<int>(a.kind, int(AnnounceKind::Count) - 1));
    const bool ours = a.team == net.my_team;
    title = tr(kAnn[int(k)].title);
    voice = kAnn[int(k)].id;
    switch (k) {
        case AnnounceKind::Kill:
            if (a.a == net.my_unit) title = tr("YOU HAVE SLAIN AN ENEMY"), voice = "you_slain";
            else if (a.b == net.my_unit) title = tr("YOU HAVE BEEN SLAIN"), voice = "you_died";
            else if (!ours) title = tr("AN ALLY HAS BEEN SLAIN"), voice = "ally_slain";
            break;
        case AnnounceKind::TurretDestroyed:
        case AnnounceKind::InhibitorDestroyed:
            title = std::format("{} — {}", title, ours ? tr("by your team") : tr("your team lost it"));
            voice += ours ? "_theirs" : "_ours";
            break;
        case AnnounceKind::BossSlain:
            title = std::format("{} — {}", title, ours ? tr("your team") : tr("the enemy team"));
            voice += ours ? "_ours" : "_theirs";
            break;
        case AnnounceKind::MinionsSpawned:
        case AnnounceKind::Ace:
            break;
        case AnnounceKind::Awakened:  // b = the element, not a unit
            line = std::format("{}  →  {}", unit_label(net, a.a), tr(element_name(Element(std::min<std::uint32_t>(a.b, 4)))));
            return;
        default: break;
    }
    // [U-363] the killer by what it was, also when it is not on my screen (a jungle monster in the fog)
    std::string killer = unit_label(net, a.a);
    if (a.a_kind != 255 && !net.roster.contains(a.a)) {
        net::NetRecapHit h{};
        h.src = a.a, h.src_kind = a.a_kind, h.src_extra = a.a_extra;
        killer = recap_who(net, h);
    }
    if (a.a && a.b) line = std::format("{}  →  {}", killer, unit_label(net, a.b));
    else if (a.b) line = unit_label(net, a.b);
    else line.clear();
}

void GameView::play_voice(const std::string& id) {
    const std::string lang = set_.voice_follows_text ? set_.language : set_.voice_language;
    const Sound* s = res_.sound(lang + "/announce/" + id);
    if (!s) s = res_.sound("en/announce/" + id);  // fallback: English voice
    audio_.play(s, SoundCat::Voice);
}

void GameView::draw_announcements(NetClient& net, double now_s) {
    const float W = view_.sw;
    // [U-253] sky events become announcements when they begin (client-side: the clock is shared)
    std::vector<std::string> now_names;
    for (const ModView& m : sky_mods_)
        if (m.tag == "ECL" || m.tag == "CMT" || m.tag == "MON" || m.tag == "ALN") now_names.push_back(m.tag);
    for (const std::string& t : now_names)
        if (std::ranges::find(sky_prev_, t) == sky_prev_.end() && sky_seen_once_) {
            const ModView* m = nullptr;
            for (const ModView& mm : sky_mods_)
                if (mm.tag == t) m = &mm;
            if (!m) continue;
            sky_banners_.push_back({m->name, m->detail, m->colour, now_s});
            play_voice(t == "ECL" ? "eclipse_begins" : t == "CMT" ? "comet_arrives" : t == "MON" ? "full_moon_rises" : "alignment_begins");
        }
    sky_prev_ = std::move(now_names);
    sky_seen_once_ = true;

    // one banner at a time: newest unseen announcement first in line, each 2.6 s
    if (announce_next_ > net.announces.size()) announce_next_ = 0;  // a new match
    if (now_s - announce_t_ > 2.6 && announce_next_ < net.announces.size()) {
        const auto& la = net.announces[announce_next_++];
        if (now_s - la.born < 8) {
            std::string title, line, voice;
            announce_text(net, la.a, title, line, voice);
            banner_title_ = title;
            banner_line_ = line;
            banner_ours_ = la.a.team == net.my_team;
            announce_t_ = now_s;
            play_voice(voice);
        }
    }
    auto banner = [&](const std::string& title, const std::string& line, Rgba col, double age) {
        if (age > 2.6) return;
        const std::uint8_t al = std::uint8_t(std::clamp(255.0 * std::min(1.0, (2.6 - age) * 3), 0.0, 255.0));
        const float tw = std::max(text_w(title, 2.5f), text_w(line, 1.5f)) + 40;
        const float y = 120;
        fill_rect(r_, (W - tw) / 2, y, tw, 60, {0, 0, 0, std::uint8_t(al * 3 / 4)});
        rect(r_, (W - tw) / 2, y, tw, 60, with_alpha(col, al));
        text(r_, (W - text_w(title, 2.5f)) / 2, y + 10, title, with_alpha(col, al), 2.5f);
        text(r_, (W - text_w(line, 1.5f)) / 2, y + 40, line, {235, 235, 235, al}, 1.5f);
    };
    banner(banner_title_, banner_line_, banner_ours_ ? Rgba{120, 190, 255} : Rgba{255, 110, 100}, now_s - announce_t_);
    // sky banners sit lower so both can show
    std::erase_if(sky_banners_, [&](const SkyBanner& b) { return now_s - b.born > 6; });
    if (!sky_banners_.empty()) {
        const SkyBanner& b = sky_banners_.front();
        const double age = now_s - b.born;
        const std::uint8_t al = std::uint8_t(std::clamp(255.0 * std::min(1.0, (6 - age) * 2), 0.0, 255.0));
        const std::string t = tr("THE SKY CHANGES") + ": " + b.name;
        const float tw = std::max(text_w(t, 2.f), text_w(b.detail, 1.2f)) + 40;
        fill_rect(r_, (W - tw) / 2, 190, tw, 48, {0, 0, 0, std::uint8_t(al * 3 / 4)});
        rect(r_, (W - tw) / 2, 190, tw, 48, with_alpha(b.colour, al));
        text(r_, (W - text_w(t, 2.f)) / 2, 198, t, with_alpha(b.colour, al), 2.f);
        text(r_, (W - text_w(b.detail, 1.2f)) / 2, 222, b.detail, {235, 235, 235, al}, 1.2f);
    }

    // [U-263] the events window: button under the sky box, the whole match's announcements
    if (!ui_) return;
    Ui& ui = *ui_;
    if (button(r_, ui, W - 92, 4, 86, 16, show_events_ ? tr("events ▴") : tr("events ▾"), show_events_)) show_events_ = !show_events_;
    if (show_events_) {  // drawn over the kill feed while open
        const float x = W - 470, w = 462;
        float y = 86;
        const std::size_t n = std::min<std::size_t>(net.announces.size(), 24);
        fill_rect(r_, x, y, w, float(n) * 14 + 22, {0, 0, 0, 215});
        rect(r_, x, y, w, float(n) * 14 + 22, {120, 120, 150});
        text(r_, x + 6, y + 5, tr("Events this match (newest first)"), {255, 230, 150});
        y += 18;
        for (std::size_t i = 0; i < n; ++i) {
            const LocalAnnounce& la = net.announces[net.announces.size() - 1 - i];
            std::string title, line, voice;
            announce_text(net, la.a, title, line, voice);
            const std::int64_t at = net.cur.now_ms - std::int64_t((now_s - la.born) * 1000);
            text(r_, x + 6, y + 2, fit_text(std::format("{}  {}  {}", mmss2(at), title, line), w - 12),
                 la.a.team == net.my_team ? Rgba{150, 200, 255} : Rgba{255, 150, 140});
            y += 14;
        }
        mouse_on_hud_ |= hover(ui, x, 86, w, y - 86);
    }
}

// ---------------------------------------------------------------- death recap [U-262]
// the skill behind a recap hit (the attacker's champion + slot + its picks), or nullptr
const SkillDef* GameView::recap_skill(NetClient& net, const net::NetRecapHit& h) const {
    if (DmgFrom(h.from) != DmgFrom::Skill) return nullptr;
    const auto r = net.roster.find(h.src);
    if (r == net.roster.end()) return nullptr;
    const ChampionDef& c = champions()[std::size_t(std::max<int>(0, r->second.champ))];
    switch (h.slot) {
        case 0: return &c.p;
        case 1: return r->second.q >= 0 && r->second.q < int(q_options().size()) ? &q_options()[std::size_t(r->second.q)] : nullptr;
        case 2: return &c.w;
        case 3: return &c.e;
        case 4: return &c.r;
        case 5: return &c.t;
        case 6: return &c.y;
        case 7: return r->second.f >= 0 ? &specials()[std::size_t(r->second.f)] : nullptr;
        case 8: return r->second.g >= 0 ? &specials()[std::size_t(r->second.g)] : nullptr;
        default: return nullptr;
    }
}

// [U-335] the icon of a recap row: skill icon, item icon, else the icon of the source kind (icons/sources/<kind>.png)
SDL_Texture* GameView::recap_icon(NetClient& net, const net::NetRecapHit& h) {
    if (const SkillDef* d = recap_skill(net, h))
        if (SDL_Texture* t = res_.skill_icon(d->name)) return t;
    if (DmgFrom(h.from) == DmgFrom::Item && h.item >= 0 && h.item < int(kItems.size())) return res_.item_icon(kItems[std::size_t(h.item)].name);
    static const char* kSrc[] = {"other", "autoattack", "skill", "item", "passive", "burn", "keystone", "combo", "chain", "turret", "minion", "monster"};
    const int f = std::min<int>(h.from, int(std::size(kSrc)) - 1);
    if (DmgFrom(h.from) == DmgFrom::Auto && UnitKind(h.src_kind) != UnitKind::Champion)  // minions / monsters / turrets hitting
        return res_.texture(std::format("icons/sources/{}.png", UnitKind(h.src_kind) == UnitKind::Turret ? "turret" : UnitKind(h.src_kind) == UnitKind::Minion ? "minion" : "monster"));
    return res_.texture(std::format("icons/sources/{}.png", kSrc[f]));
}

std::string GameView::recap_source(NetClient& net, const net::NetRecapHit& h) const {
    switch (DmgFrom(h.from)) {
        case DmgFrom::Auto: return tr("Autoattack");
        case DmgFrom::Burn: return tr("Burn (damage over time)");
        case DmgFrom::Keystone: return tr("Keystone");
        case DmgFrom::Combo: return tr("Element combo");
        case DmgFrom::Chain: return tr("Lightning through water");
        case DmgFrom::Turret: return tr("Turret shot");
        case DmgFrom::Minion: return tr("Minion");
        case DmgFrom::Monster: return tr("Monster");
        case DmgFrom::Item: return h.item >= 0 && h.item < int(kItems.size()) ? std::string(kItems[std::size_t(h.item)].name) : tr("Item");
        case DmgFrom::Passive: return tr("Passive");
        case DmgFrom::Skill: {
            static const char* kK[] = {"P", "Q", "W", "E", "R", "T", "Y", "G", "H"};
            const SkillDef* d = recap_skill(net, h);
            return std::format("{} [{}]", d ? std::string(d->name) : tr("Skill"), kK[std::min<int>(h.slot, 8)]);
        }
        default: return tr("Other");
    }
}

// [U-302] who dealt a recap hit: a known champion by name, else what it was when it hit (never "unknown")
std::string GameView::recap_who(NetClient& net, const net::NetRecapHit& h) const {
    if (net.roster.contains(h.src)) return unit_label(net, h.src);
    switch (UnitKind(h.src_kind)) {
        case UnitKind::Minion: {
            static const char* kM[] = {"melee", "ranged", "siege", "super"};
            return std::format("{} ({})", tr("minion"), tr(kM[std::min<int>(h.src_extra, 3)]));
        }
        case UnitKind::Turret: return tr("turret");
        case UnitKind::Inhibitor: return tr("inhibitor");
        case UnitKind::Nexus: return tr("Nation Shrine");
        case UnitKind::Monster:
        case UnitKind::Boss: return tr(net::kMobNames[std::min<int>(h.src_extra, int(std::size(net::kMobNames)) - 1)]);
        case UnitKind::Champion: return h.src_extra < champions().size() ? std::string(champions()[h.src_extra].name) : tr("champion");
        default: return DmgFrom(h.from) == DmgFrom::Burn ? tr("fire") : tr("the world");
    }
}

void GameView::draw_death_recap(NetClient& net, double now_s) {
    (void)now_s;
    const DeathRecapView& rc = net.recap;
    if (rc.born < 0 || rc.hits.empty() || !ui_) return;
    Ui& ui = *ui_;
    const float W = view_.sw, H = view_.sh;
    // totals by damage type
    std::int64_t total = 0, phys = 0, dot = 0, shielded = 0;
    std::map<int, std::int64_t> by_el;  // element → damage
    for (const auto& h : rc.hits) {
        total += h.amount;
        shielded += h.shielded;
        if (h.physical) phys += h.amount;
        else by_el[int(base_element(SubElement(h.sub)))] += h.amount;
        if (DmgFrom(h.from) == DmgFrom::Burn) dot += h.amount;
    }
    const float span_s = float(rc.hits.front().before_ms) / 1000.f;
    std::string killer_name = unit_label(net, rc.killer);  // [U-302] from the hits when the killer is not known on screen
    bool killer_hit = false;
    for (const auto& h : rc.hits)
        if (h.src == rc.killer) killer_name = recap_who(net, h), killer_hit = true;
    if (!killer_hit && !net.roster.contains(rc.killer)) killer_name = recap_who(net, rc.hits.back());  // [U-363] never "unknown": the last hit's source
    // short line under "respawn in"
    const std::string line = std::format("{} {} — {} {} {:.1f} s ({} {}, {} {}, {} {})", tr("Killed by"), killer_name, total,
                                         tr("damage in"), span_s, phys, tr("physical"), total - phys, tr("elemental"), dot, tr("over time"));
    text(r_, (W - text_w(line, 1.5f)) / 2, H / 2 - 18, line, {255, 220, 200}, 1.5f);
    if (button(r_, ui, W / 2 - 90, H / 2 + 2, 180, 22, show_recap_ ? tr("Hide death recap") : tr("Death recap ▸"), show_recap_)) show_recap_ = !show_recap_;
    if (!show_recap_) return;

    // full window: one row per (source unit, what) with a bar; then a timeline graph
    struct Row {
        std::uint32_t src;
        std::string who, what;
        net::NetRecapHit first{};
        std::int64_t dmg = 0, shield = 0;
        int hits = 0;
        bool physical = false;
        std::uint8_t sub = 0;
    };
    std::vector<Row> rows;
    std::vector<std::string> ccs;  // [U-343] crowd control lines
    for (const auto& h : rc.hits) {
        if (h.cc != 255) {
            if (ccs.size() < 6) ccs.push_back(std::format("{} {:.1f} s — {}", tr(h.cc == 254 ? "displaced" : status_name(h.cc)), h.cc_cs / 100.0, recap_who(net, h)));
            continue;
        }
        const std::string what = recap_source(net, h);
        auto it = std::ranges::find_if(rows, [&](const Row& r) { return r.src == h.src && r.what == what; });
        if (it == rows.end()) rows.push_back({h.src, recap_who(net, h), what, h, 0, 0, 0, h.physical != 0, h.sub}), it = rows.end() - 1;
        it->dmg += h.amount;
        it->shield += h.shielded;
        ++it->hits;
    }
    std::ranges::sort(rows, std::greater{}, &Row::dmg);
    const float w = 640, x = (W - w) / 2;
    float y = H / 2 + 30;
    const float rows_h = float(std::min<std::size_t>(rows.size(), 12)) * 20;
    const float cc_h = ccs.empty() ? 0.f : 18.f + float(ccs.size()) * 14.f;
    const float h_total = 40 + rows_h + 110 + cc_h;
    fill_rect(r_, x, y, w, h_total, {6, 6, 12, 240});
    rect(r_, x, y, w, h_total, {200, 120, 100});
    text(r_, x + 10, y + 8, std::format("{} — {} {}  ({} {})", tr("Death recap"), total, tr("damage taken"), shielded, tr("absorbed by shields")),
         {255, 230, 200}, 1.4f);
    y += 32;
    auto colour_of = [](bool physical, std::uint8_t sub) {
        if (physical) return Rgba{255, 150, 60};
        switch (base_element(SubElement(sub))) {
            case Element::Water: return Rgba{80, 170, 255};
            case Element::Earth: return Rgba{205, 165, 95};
            case Element::Fire: return Rgba{255, 90, 50};
            case Element::Air: return Rgba{220, 230, 240};
            default: return Rgba{200, 200, 200};
        }
    };
    const std::int64_t top = rows.empty() ? 1 : std::max<std::int64_t>(1, rows.front().dmg);
    for (std::size_t i = 0; i < rows.size() && i < 12; ++i) {
        const Row& r = rows[i];
        text(r_, x + 10, y + 5, fit_text(r.who, 170), {230, 230, 240});
        if (SDL_Texture* ic = recap_icon(net, r.first)) {  // [U-335]
            const SDL_FRect d{x + 182, y + 1, 18, 18};
            SDL_RenderTexture(r_, ic, nullptr, &d);
        }
        text(r_, x + 204, y + 5, fit_text(r.what, 152), {210, 210, 220});
        const float bw = 200.f * float(r.dmg) / float(top);
        fill_rect(r_, x + 362, y + 3, bw, 13, colour_of(r.physical, r.sub));
        text(r_, x + 368 + bw, y + 5, std::format("{}  ({}×){}", r.dmg, r.hits, r.shield ? std::format(" +{} {}", r.shield, tr("shield")) : ""), {255, 255, 255});
        y += 20;
    }
    if (!ccs.empty()) {  // [U-343]
        y += 4;
        text(r_, x + 10, y, tr("Crowd control on you:"), {255, 220, 150});
        y += 14;
        for (const std::string& l : ccs) {
            text(r_, x + 20, y, fit_text(l, w - 30), {230, 210, 255});
            y += 14;
        }
    }
    // timeline: damage per 0.5 s before the death, stacked by type (physical / element colours)
    y += 8;
    text(r_, x + 10, y, tr("Timeline (left = start of the fight, right = death)"), {200, 200, 210});
    y += 14;
    const int buckets = std::clamp(int(span_s * 2) + 1, 4, 60);
    const float gw = w - 20, bwid = gw / float(buckets), gh = 70;
    std::vector<std::array<std::int64_t, 6>> b(std::size_t(buckets), std::array<std::int64_t, 6>{});  // physical, water, earth, fire, air, other
    std::int64_t bmax = 1;
    for (const auto& h : rc.hits) {
        const float from_start = span_s - float(h.before_ms) / 1000.f;
        const int bi = std::clamp(int(from_start * 2), 0, buckets - 1);
        const int ty = h.physical ? 0 : 1 + std::min(4, int(base_element(SubElement(h.sub))));
        b[std::size_t(bi)][std::size_t(ty)] += h.amount;
    }
    for (const auto& col : b) {
        std::int64_t s = 0;
        for (auto v : col) s += v;
        bmax = std::max(bmax, s);
    }
    fill_rect(r_, x + 10, y, gw, gh, {20, 20, 30, 255});
    const Rgba kCols[] = {{255, 150, 60}, {80, 170, 255}, {205, 165, 95}, {255, 90, 50}, {220, 230, 240}, {200, 200, 200}};
    for (int i = 0; i < buckets; ++i) {
        float yy = y + gh;
        for (int t = 0; t < 6; ++t) {
            const float hh = gh * float(b[std::size_t(i)][std::size_t(t)]) / float(bmax);
            if (hh <= 0) continue;
            fill_rect(r_, x + 10 + float(i) * bwid + 1, yy - hh, bwid - 2, hh, kCols[t]);
            yy -= hh;
        }
    }
    mouse_on_hud_ |= hover(ui, x, H / 2 + 30, w, h_total);
}

}  // namespace em
