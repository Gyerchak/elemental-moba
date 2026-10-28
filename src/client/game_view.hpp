#pragma once
// [U-03][X-17][X-25] In-match view: map, fog + night darkness + light, units with body hitboxes,
// fluid skills with exact edges, skill indicators, sense waves, HUD, shop, scoreboard, minimap.
#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

#include "client/draw.hpp"
#include "client/modifiers.hpp"
#include "client/sprites.hpp"
#include "client/item_browser.hpp"
#include "client/net_client.hpp"
#include "client/anim.hpp"
#include "client/audio.hpp"
#include "client/resources.hpp"
#include "client/settings.hpp"
#include "game/mechanics/vision.hpp"

namespace em {


class GameView {
public:
    GameView(SDL_Renderer* r, Settings& s, Resources& res, Audio& audio);
    ~GameView();
    void reset();
    void handle_event(const SDL_Event& e, NetClient& net);
    void frame(NetClient& net, double now_s, gfx::Ui& ui);
    bool leave = false;
    bool wants_menu = false;  // Esc pressed: the app opens the in-game menu [U-207]
    bool screenshot_ready = false;
    [[nodiscard]] bool wants_text() const { return chat_open_; }
    void draw_icon(int champ, float x, float y, float size, bool dim);  // champion icon (lobby, HUD, portraits)

private:
    struct DrawUnit {
        net::NetUnit u;
        float x, y;
    };
    void rebuild_textures(NetClient& net, std::int64_t now_ms, bool force);
    // world_draw.cpp
    void draw_world(NetClient& net, double now_s);
    void draw_terrain_models(NetClient& net);
    void draw_clouds(NetClient& net);
    void draw_wall_damage(NetClient& net);  // [U-288]
    void draw_ghosts(NetClient& net);
    void draw_units(NetClient& net, double now_s);
    void draw_paths(NetClient& net);
    void draw_pings(NetClient& net);
    void draw_waves(NetClient& net, double now_s);
    void draw_hits(NetClient& net, double now_s);
    [[nodiscard]] AnimSlot anim_of(const net::NetUnit& u) const;
    void play_sounds(NetClient& net);  // [U-201]
    void draw_hud(NetClient& net, double now_s, gfx::Ui& ui);
    void draw_indicator(NetClient& net);
    void cast_key(NetClient& net, int slot);
    void on_action(Action a, bool ctrl, NetClient& net);
    void self_cast(NetClient& net, int slot);  // [U-243]
    [[nodiscard]] bool champs_only() const;
    void draw_health_bar(float x, float y, float w, float h, std::int32_t hp, std::int32_t hp_max, std::int32_t shield,
                         std::uint8_t shield_sub, gfx::Rgba colour, bool ticks);
    void draw_portraits(NetClient& net, gfx::Ui& ui);
    void draw_tooltip(NetClient& net, gfx::Ui& ui);
    void draw_rewards(NetClient& net, double now_s);
    void center_on(float x, float y);
    void draw_bottom_panel(NetClient& net, double now_s, gfx::Ui& ui);
    void draw_skill_tooltip(NetClient& net, int slot, float x, float y, bool alt, bool plus);
    void draw_buffs(NetClient& net, float x, float y, double since);
    // announce.cpp [U-262][U-263]
    void draw_announcements(NetClient& net, double now_s);
    void draw_death_recap(NetClient& net, double now_s);
    void announce_text(NetClient& net, const net::NetAnnounce& a, std::string& title, std::string& line, std::string& voice) const;
    [[nodiscard]] std::string unit_label(NetClient& net, std::uint32_t id) const;
    [[nodiscard]] std::string recap_source(NetClient& net, const net::NetRecapHit& h) const;
    [[nodiscard]] std::string recap_who(NetClient& net, const net::NetRecapHit& h) const;  // [U-302]
    [[nodiscard]] const SkillDef* recap_skill(NetClient& net, const net::NetRecapHit& h) const;
    SDL_Texture* recap_icon(NetClient& net, const net::NetRecapHit& h);  // [U-335]
    void play_voice(const std::string& id);
    bool icon_button(gfx::Ui& ui, float x, float y, float size, std::string_view label, std::string_view key, bool active);
    void cast_at_cursor(NetClient& net, int slot);
    [[nodiscard]] const SkillDef* skill_of(const NetClient& net, int slot) const;
    [[nodiscard]] const DrawUnit* unit_at(float wx, float wy, bool enemies_only, bool champions_only = false) const;
    [[nodiscard]] gfx::Rgba team_colour(int team) const;

    SDL_Renderer* r_;
    Settings& set_;
    Resources& res_;
    Audio& audio_;
    SDL_Texture* map_tex_ = nullptr;
    SDL_Texture* fog_tex_ = nullptr;
    // [U-290] the fog fades from what was shown to the newest rebuild over 0.1 s, pixel by pixel → no stepping edges
    std::vector<std::uint32_t> fog_from_px_, fog_shown_px_;
    double fog_fade_from_ = -1;
    void update_fog_fade(double now_s);
    int fog_fade_t_ = -1;
    std::vector<std::uint32_t> map_px_, fog_px_;
    // [U-257] textured ground
    static constexpr int kGroundPx = 4, kGroundTextures = 17;
    std::vector<std::uint32_t> map_raw_;  // the cell each ground block was painted from
    std::array<SDL_Surface*, kGroundTextures> ground_src_{};
    std::array<bool, kGroundTextures> ground_tried_{};
    const SDL_Surface* ground_texture(int which);
    void paint_cell(int x, int y, Cell c);
    VisionMap vis_;
    LightMap light_;
    double last_fog_ = -1;
    gfx::View view_;
    bool cam_lock_ = false;
    bool cam_started_ = false;
    int notice_seen_ = 0;
    std::uint32_t selected_ = 0;   // left-clicked unit → info panel [U-161]
    bool champs_toggle_on_ = false;
    bool auto_sent_ = false;       // autoattack setting sent to the server once per match
    bool both_down_ = false;
    std::array<std::int32_t, 256> status_max_{};
    std::vector<ModView> sky_mods_;  // [U-253] active sky effects for my champion (sky box)
    // [U-263] announcement banner + events window, [U-253] sky banners, [U-262] death recap window
    std::size_t announce_next_ = 0;
    double announce_t_ = -100;
    std::string banner_title_, banner_line_;
    bool banner_ours_ = true, show_events_ = false, show_recap_ = false, sky_seen_once_ = false;
    std::vector<std::string> sky_prev_;
    struct SkyBanner {
        std::string name, detail;
        gfx::Rgba colour;
        double born = 0;
    };
    std::vector<SkyBanner> sky_banners_;  // longest remaining time seen per status → duration animation
    int pending_ = -1;          // skill waiting for a click (normal cast)
    std::int32_t src_x_ = -1, src_y_ = -1;  // [U-340] the source picked by the first click (−1 = none yet)
    [[nodiscard]] bool source_at(const NetClient& net, float wx, float wy) const;
    void draw_camp_timers(NetClient& net);  // [U-365]
    int attack_move_ = 0;       // 0 off, 1 anything (D), 2 closest champion only (A) [X-98]
    std::string hint_;
    double hint_t_ = -10;
    void hint(std::string h);
    bool show_shop_ = false, show_score_ = false, chat_open_ = false;
    std::string chat_text_;
    float click_x_ = 0, click_y_ = 0;
    double click_t_ = -10;
    std::vector<DrawUnit> draw_;
    std::uint32_t local_tick_ = 0;
    double frame_ms_ = 0, last_frame_ = 0;
    gfx::Ui* ui_ = nullptr;
    bool mouse_on_hud_ = false;
    bool ping_wheel_ = false, swallow_click_ = false;  // [U-246]
    float ping_x_ = 0, ping_y_ = 0;
    std::vector<std::pair<std::uint32_t, std::int64_t>> pings_seen_;
    std::unordered_map<std::uint32_t, std::uint16_t> cc_max_;      // longest CC time seen per unit [U-193]
    std::unordered_map<std::uint32_t, bool> fx_seen_, alive_seen_; // for sounds [U-201]
    // [U-258] model sprites: animation state per unit (walking = its walk phase changed lately; attack = it just hit)
    Sprites sprites_;
    struct AnimMem {
        std::uint16_t walk = 0;
        double moved = -10, attacked = -10, died = -10, cast_from = -10;
        bool was_alive = true, was_casting = false;
    };
    std::unordered_map<std::uint32_t, AnimMem> anim_mem_;
    // [U-259] my events → sounds
    int snd_level_ = 0, snd_items_ = -1, snd_gold_ = 0, snd_notice_ = -1;
    bool snd_recall_ = false, snd_welcome_ = false, snd_end_ = false;
    [[nodiscard]] const DrawUnit* me_unit() const;
    std::uint32_t me_id_ = 0;
    bool source_ice_ = false;  // [U-293]
    float shop_x_ = -1, shop_y_ = -1, shop_dx_ = 0, shop_dy_ = 0;  // [U-342] where the shop window is (dragged)
    bool shop_drag_ = false;
    std::uint8_t my_team_ = 0;
    [[nodiscard]] std::string model_id(const net::NetUnit& u) const;
    std::vector<std::uint8_t> cloud_cells_;                         // [U-241]
    bool expand_stats_ = false;                                     // C [U-238]
    ItemBrowser shop_;                                              // [U-220]
    int panel_page_ = 0;                                            // 0 stats, 1 runes / loadout [U-238]
};

}  // namespace em
