// [U-323][U-324] Auto-buy (build creator) and auto-levelling. See auto_play.hpp.
#include "client/auto_play.hpp"

#include <algorithm>

#include "client/net_client.hpp"
#include "client/settings.hpp"
#include "core/items.hpp"
#include "game/content.hpp"

namespace em {

namespace {

const SavedSetup* my_setup(const NetClient& net, const Settings& set, const ChampionDef*& champ) {
    const auto it = net.roster.find(net.my_unit);
    if (it == net.roster.end()) return nullptr;
    champ = &champions()[std::size_t(std::clamp<int>(it->second.champ, 0, int(champions().size()) - 1))];
    for (const ChampPrefs& p : set.champs)  // (prefs() would create an entry; this is read-only)
        if (p.champ == champ->name) return p.setup >= 0 && p.setup < int(set.setups.size()) ? &set.setups[std::size_t(p.setup)] : nullptr;
    return nullptr;
}

// the skill in a slot (Q … Y) — the same rules as GameView::skill_of
const SkillDef* skill_in(const NetClient& net, const ChampionDef& d, int slot) {
    const RosterEntry& e = net.roster.at(net.my_unit);
    if (slot >= 1 && slot <= 4)  // [U-296] awakened non-benders
        for (const LocalAnnounce& la : net.announces)
            if (la.a.kind == std::uint8_t(AnnounceKind::Awakened) && la.a.a == net.my_unit)
                if (const SkillDef* s = awakened_skill(d, Element(std::min<std::uint32_t>(la.a.b, 4)), slot)) return s;
    switch (Slot(slot)) {
        case Slot::Q: return e.q >= 0 && e.q < int(q_options().size()) ? &q_options()[std::size_t(e.q)] : nullptr;
        case Slot::W: return &d.w;
        case Slot::E: return &d.e;
        case Slot::R: return &d.r;
        case Slot::T: return &d.t;
        case Slot::Y: return &d.y;
        default: return nullptr;
    }
}

bool carries(const Bag& bag, int item) {
    return std::ranges::find(bag, std::int16_t(item)) != bag.end();
}

// [U-323] what to buy now for `item`: the item itself if I can pay the rest after my parts, else the
// first affordable missing part (recursively, build paths [U-261]). −1 = nothing affordable.
int next_purchase(const Bag& bag, std::array<bool, kItemSlots>& claimed, int item, int gold, bool top) {
    if (!top) {  // a part I already carry is done
        for (std::size_t s = 0; s < bag.size(); ++s)
            if (!claimed[s] && bag[s] == item) return claimed[s] = true, -1;
    }
    if (plan_buy(bag, item).price <= gold) return item;
    for (std::string_view part : kItems[std::size_t(item)].from) {
        if (part.empty() || item_index(part) < 0) continue;
        const int r = next_purchase(bag, claimed, item_index(part), gold, false);
        if (r >= 0) return r;
    }
    return -1;
}

}  // namespace

void run_auto_play(NetClient& net, const Settings& set, double now_s) {
    if (net.state != NetClient::State::Match || !net.have_snapshot || (!set.auto_buy && !set.auto_level)) return;
    const ChampionDef* champ = nullptr;
    const SavedSetup* setup = my_setup(net, set, champ);
    if (!setup || !champ) return;
    const net::NetMe& me = net.cur.me;
    static double last_level = -1, last_buy = -1;
    static std::int64_t match_start = -1;
    static std::size_t step = 0;
    if (match_start != net.start_ms) match_start = net.start_ms, step = 0, last_level = last_buy = -1;  // a new match

    // ---- [U-324] auto-levelling: one rank per call (the server answers with the next snapshot)
    if (set.auto_level && me.skill_points > 0 && now_s - last_level >= 0.25) {
        int level = 1;
        for (const net::NetUnit& u : net.cur.units)
            if (u.id == net.my_unit) level = u.level;
        auto can = [&](int slot) {
            const SkillDef* d = skill_in(net, *champ, slot);
            return d && d->kind != SkillKind::Passive && me.rank[slot] < skill_max_rank(*d, level);
        };
        int pick = -1;
        if (setup->level_mode == 1) {  // an exact order: the step = ranks spent so far
            int spent = 0;
            for (int s = int(Slot::Q); s <= int(Slot::Y); ++s) spent += me.rank[s];
            if (spent < 18 && setup->level_exact[std::size_t(spent)] >= 1 && can(setup->level_exact[std::size_t(spent)]))
                pick = setup->level_exact[std::size_t(spent)];
        }
        if (pick < 0)  // the ultimate whenever it is allowed …
            for (int s = int(Slot::Q); s <= int(Slot::Y) && pick < 0; ++s) {
                const SkillDef* d = skill_in(net, *champ, s);
                if (d && d->ultimate && can(s)) pick = s;
            }
        if (pick < 0)  // … then the priority list (a skill is filled up before the next one)
            for (std::int8_t s : setup->level_prio)
                if (s >= 1 && s <= 6 && can(s)) {
                    pick = s;
                    break;
                }
        if (pick >= 0) {
            net.send_cmd({.kind = CmdKind::LevelUp, .slot = std::uint8_t(pick)});
            last_level = now_s;
        }
    }

    // ---- [U-323] auto-buy: only while I stand in the shop
    if (set.auto_buy && me.can_shop && now_s - last_buy >= 0.5) {
        Bag bag{};
        for (int i = 0; i < kItemSlots; ++i) bag[std::size_t(i)] = me.items[i];
        while (step < setup->build.size() && (setup->build[step].item < 0 || carries(bag, setup->build[step].item))) ++step;
        if (step >= setup->build.size()) return;
        const int target = setup->build[step].item;
        std::array<bool, kItemSlots> claimed{};
        const int buy = next_purchase(bag, claimed, target, me.gold, true);
        if (buy < 0) return;
        const ItemDef& d = kItems[std::size_t(buy)];
        const BuyPlan plan = plan_buy(bag, buy);
        auto open = [&](std::size_t s) { return bag[s] < 0 || plan.used[s]; };  // the same slot rules as World::buy [U-322]
        bool room = false;
        if (d.group == ItemGroup::Boots) room = open(kBootsSlot);
        else if (d.group == ItemGroup::Special) room = open(kSpecialSlot);
        else
            for (std::size_t s = 1; s <= 8 && !room; ++s) room = open(s);
        if (!room) return;
        net.send_cmd({.kind = CmdKind::Buy, .item = std::int16_t(buy)});
        last_buy = now_s;
    }
}

}  // namespace em
