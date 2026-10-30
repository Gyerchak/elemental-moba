#pragma once
// [U-323][U-324] Auto-buy and auto-levelling: follow the build and the skill order of the setup chosen for my
// champion (Settings: auto_buy / auto_level, both off by default). Call once per frame during a match.
namespace em {
class NetClient;
struct Settings;
void run_auto_play(NetClient& net, const Settings& set, double now_s);
}  // namespace em
