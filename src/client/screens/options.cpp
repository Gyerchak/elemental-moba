// [U-99][U-163][U-165][U-201][U-207][U-208][U-239] Options — also live inside a match (Esc menu).
#include <format>

#include "client/app.hpp"
#include "client/locale.hpp"

namespace em {

using namespace gfx;

void App::options(bool in_game) {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren_, &w, &h);
    const float x = float(w) / 2 - 320;
    float y = in_game ? 90 : 30;
    if (!in_game) {
        text(ren_, x, y, tr("Options"), {255, 200, 120}, 3);
        y += 40;
    }
    const char* pages[] = {"Game", "Window", "Graphics", "Sound", "Controls", "Quick cast", "Language"};
    for (int p = 0; p < 7; ++p)
        if (button(ren_, ui_, x + float(p) * 92, y, 88, 28, tr(pages[p]), options_page_ == p)) options_page_ = p, bind_wait_ = -1;
    y += 40;
    bool changed = false, window_changed = false;
    auto toggle = [&](std::string_view label, bool& v) {
        if (button(ren_, ui_, x, y, 640, 26, std::format("{}: {}", label, v ? tr("ON") : tr("OFF")), v)) v = !v, changed = true;
        y += 30;
    };
    auto slide = [&](int id, std::string_view label, int& v, int lo, int hi) {
        if (slider(ren_, ui_, id, x, y, 640, v, lo, hi, label)) changed = true;
        y += 30;
    };
    switch (options_page_) {
        case 0: {  // game
            if (!in_game && text_field(ren_, ui_, 1, x, y + 12, 300, set_.name, tr("Nickname"))) changed = true;
            if (!in_game) y += 54;
            toggle(tr("Autoattack on its own (key H)"), set_.auto_attack);
            toggle(tr("Left + right click together = camera on me"), set_.both_buttons_center);
            toggle(tr("Champions-only key switches on / off (off = hold)"), set_.champs_only_toggle);
            toggle(tr("Attack-move: the enemy nearest the cursor (off = nearest to me)"), set_.attack_move_cursor);  // [U-303]
            toggle(tr("Show gold / XP gains on kills"), set_.show_rewards);
            toggle(tr("Show nicknames"), set_.show_names);
            toggle(tr("Chat"), set_.show_chat);
            toggle(tr("Minimap"), set_.minimap);
            toggle(tr("Show other players' own skins"), set_.show_other_skins);
            toggle(tr("Force my selected skins"), set_.force_own_skins);
            toggle(tr("Only server-native skins"), set_.server_skins_only);
            break;
        }
        case 1: {  // window [U-207][U-233]
            const char* modes[] = {"Fullscreen (exclusive)", "Fullscreen (borderless)", "Window"};
            for (int m = 0; m < 3; ++m)
                if (button(ren_, ui_, x + float(m) * 214, y, 208, 28, tr(modes[m]), int(set_.window) == m))
                    set_.window = WindowMode(m), changed = window_changed = true;
            y += 36;
            const bool before = set_.confine_mouse;
            toggle(tr("Keep the mouse inside the game window"), set_.confine_mouse);
            if (before != set_.confine_mouse) window_changed = true;
            const bool vs = set_.vsync;
            toggle(tr("V-sync"), set_.vsync);
            if (vs != set_.vsync) SDL_SetRenderVSync(ren_, set_.vsync ? 1 : 0);
            text(ren_, x, y + 6, std::format("{}: {}", tr("Renderer"), renderer_name_), {170, 170, 190});
            y += 26;
            break;
        }
        case 2: {  // graphics [U-176][U-198][U-202][U-203][U-204]
            toggle(tr("Developer view (top-down, full hitboxes, no textures)"), set_.dev_mode);
            slide(10, tr("Hitbox shadow opacity %"), set_.hitbox_opacity, 0, 100);
            toggle(tr("Smooth fog of war"), set_.smooth_fog);
            toggle(tr("Damage numbers"), set_.damage_numbers);
            toggle(tr("Crowd-control timers above units"), set_.cc_timers);
            toggle(tr("Path lines (mine and allies')"), set_.show_paths);
            toggle(tr("Tree and bush models"), set_.tree_models);
            toggle(tr("Colourblind team colours"), set_.colourblind);
            // resource packs [U-97][U-197]
            text(ren_, x, y + 6, tr("Resource pack:"), {200, 200, 210});
            float px = x + 160;
            for (const std::string& p : res_.packs())
                if (button(ren_, ui_, px, y, 150, 26, p, set_.resource_pack == p)) {
                    set_.resource_pack = p;
                    res_.init(ren_, p);
                    changed = true;
                    px += 156;
                } else {
                    px += 156;
                }
            y += 32;
            break;
        }
        case 3: {  // sound [U-201]
            slide(20, tr("Master volume"), set_.vol_master, 0, 100);
            slide(21, tr("Music"), set_.vol_music, 0, 100);
            slide(22, tr("Effects (skills, hits)"), set_.vol_effects, 0, 100);
            slide(23, tr("Voices"), set_.vol_voice, 0, 100);
            slide(24, tr("Ambient"), set_.vol_ambient, 0, 100);
            slide(25, tr("Interface"), set_.vol_ui, 0, 100);
            text(ren_, x, y + 6, tr("Skill sounds are heard only within hearing range, like the sound waves."), {160, 160, 180});
            y += 26;
            if (changed) audio_.set_volumes(set_);
            break;
        }
        case 4: {  // controls [U-165]
            text(ren_, x, y, bind_wait_ >= 0 ? tr("Press the new key (Esc = keep the old one)") : tr("Click an action, then press a key."),
                 {255, 220, 140}, 1.5f);
            y += 22;
            for (int i = 0; i < kActions; ++i) {
                const float cx2 = x + float(i % 2) * 324, cy2 = y + float(i / 2) * 24;
                const std::string label = std::format("{:<26} {}", tr(action_name(Action(i))),
                                                      bind_wait_ == i ? "..." : SDL_GetScancodeName(set_.keys[std::size_t(i)]));
                if (button(ren_, ui_, cx2, cy2, 318, 21, label, bind_wait_ == i)) bind_wait_ = i;
            }
            y += float((kActions + 1) / 2) * 24 + 6;
            if (button(ren_, ui_, x, y, 260, 26, tr("Reset all keys to default"))) set_.default_keys(), changed = true;
            y += 32;
            for (const char* line : {"Ctrl + skill key = learn / rank up.  Alt + skill / item key = cast on yourself.",
                                     "Ctrl + Space = switch stance.  F1…F12 = camera on teammates.  Ctrl+1…6 = emotes.",
                                     "Ctrl + left click and drag = ping wheel (danger, missing, on my way, assist, bait).",
                                     "Practice: Ctrl+Shift+N no cooldowns, Ctrl+Shift+G gold, Ctrl+Shift+L level up."}) {
                text(ren_, x, y, tr(line), {160, 160, 180});
                y += 14;
            }
            break;
        }
        case 5: {  // quick cast [U-163]
            toggle(tr("Quick cast ALL abilities"), set_.quick_cast_all);
            static const char* names[] = {"P", "Q", "W", "E", "R", "T", "Y (ultimate)", "G (special)", "H (special)"};  // [U-289][U-291]
            for (int i = 1; i < kSkillSlots; ++i) toggle(std::format("{} {}", tr("Quick cast"), names[i]), set_.quick_cast[std::size_t(i)]);
            text(ren_, x, y + 4, tr("Quick cast = the skill fires at the cursor when the key is pressed (no click)."), {170, 170, 190});
            y += 24;
            break;
        }
        case 6: {  // language [U-239]
            text(ren_, x, y + 6, tr("Text language:"), {200, 200, 210});
            float lx = x + 200;
            for (const std::string& code : loc::languages()) {
                if (button(ren_, ui_, lx, y, 90, 26, code, set_.language == code)) {
                    set_.language = code;
                    loc::load(code);
                    changed = true;
                }
                lx += 96;
            }
            y += 34;
            toggle(tr("Voices in the same language as the text"), set_.voice_follows_text);
            if (!set_.voice_follows_text) {
                text(ren_, x, y + 6, tr("Voice language:"), {200, 200, 210});
                float vx = x + 200;
                for (const std::string& code : loc::languages()) {
                    if (button(ren_, ui_, vx, y, 90, 26, code, set_.voice_language == code)) set_.voice_language = code, changed = true;
                    vx += 96;
                }
                y += 34;
            }
            text(ren_, x, y + 6, tr("Add a language: copy lang/pl.txt to lang/<code>.txt and translate the right side."), {160, 160, 180});
            y += 26;
            break;
        }
        default: break;
    }
    if (changed) set_.save();
    if (window_changed) apply_window();
    if (!in_game && button(ren_, ui_, x, float(h) - 56, 200, 36, tr("Back"))) {
        set_.save();
        go(back_);
    }
}

}  // namespace em
