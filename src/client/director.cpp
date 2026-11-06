// [U-549] The director: camera, what is shown, time, sequences of camera keys and video recording. In the replay tool all of it; in the
// practice tool the camera and what is shown, for a while, with one button back to normal. F9 opens it, F10 records.
// The same controls in both, because the camera is the same code. Only this computer's picture changes: nothing here touches the game.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

#include "client/game_view.hpp"
#include "client/locale.hpp"

namespace em {

using namespace gfx;
using loc::tr;
using loc::trf;
namespace fs = std::filesystem;

namespace {
constexpr const char* kSeqFile = "replays/director_sequence.txt";
// a smooth way through the keys (Catmull-Rom)
[[nodiscard]] float through(float p0, float p1, float p2, float p3, float t) {
    const float t2 = t * t, t3 = t2 * t;
    return 0.5f * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (3 * p1 - p0 - 3 * p2 + p3) * t3);
}
[[nodiscard]] std::string mmss(double ms) {
    const int s = int(ms / 1000);
    return std::format("{:02}:{:02}", s / 60, s % 60);
}
}  // namespace

bool GameView::Director::shows(UnitKind k) const {
    switch (k) {
        case UnitKind::Champion: return champions;
        case UnitKind::Minion: return minions;
        case UnitKind::Turret: case UnitKind::Inhibitor: case UnitKind::Nexus: return structures;
        default: return monsters;
    }
}

// The camera for this frame: a sequence, a followed unit, and the angle.
void GameView::director_camera(NetClient& net, std::int64_t now_ms) {
    Director& d = dir_;
    if (d.seq_play && d.keys.size() >= 2) {
        std::ranges::sort(d.keys, {}, &Director::Key::t);
        const double t = double(now_ms);
        if (t >= d.keys.back().t) {
            if (d.seq_loop) d.target_ms = d.keys.front().t;
            else d.seq_play = false;
        }
        std::size_t i = 0;
        while (i + 2 < d.keys.size() && d.keys[i + 1].t <= t) ++i;
        const auto& k1 = d.keys[i];
        const auto& k2 = d.keys[i + 1];
        const auto& k0 = d.keys[i > 0 ? i - 1 : i];
        const auto& k3 = d.keys[std::min(i + 2, d.keys.size() - 1)];
        const float q = float(std::clamp((t - k1.t) / std::max(1.0, k2.t - k1.t), 0.0, 1.0));
        view_.cx = through(k0.cx, k1.cx, k2.cx, k3.cx, q), view_.cy = through(k0.cy, k1.cy, k2.cy, k3.cy, q);
        view_.zoom = std::clamp(through(k0.zoom, k1.zoom, k2.zoom, k3.zoom, q), 0.05f, 4.f);
        d.on = true;
        d.gy = std::clamp(through(k0.gy, k1.gy, k2.gy, k3.gy, q), 0.3f, 1.f), d.hz = std::clamp(through(k0.hz, k1.hz, k2.hz, k3.hz, q), 0.f, 1.5f);
        if (net.spectator && k1.speed != net.replay_speed && d.target_ms < 0) net.replay_speed = k1.speed, net.send_spectate();
    } else if (d.follow) {
        for (const DrawUnit& u : draw_)
            if (u.u.id == d.follow) view_.cx = u.x, view_.cy = u.y;
    }
    if (d.on) {
        view_.gy = d.gy, view_.hz = d.hz;
        if (d.turn >= 0) view_.flip = d.turn == 1;
    }
    // ---- going to another moment of a replay: it runs fast until it is there; backwards it starts again from the beginning
    if (net.spectator && d.target_ms >= 0) {
        if (double(now_ms) > d.target_ms + 1500) {
            replay_restart = true;
        } else if (double(now_ms) < d.target_ms - 200) {
            if (!d.seeking) d.speed_before = net.replay_speed, d.seeking = true;
            if (net.replay_speed != 3200) net.replay_speed = 3200, net.send_spectate();
        } else {
            d.target_ms = -1;
            if (d.seeking) net.replay_speed = std::max(25, d.speed_before), net.send_spectate();
            d.seeking = false;
        }
    }
}

void GameView::director_reset() {
    const auto keys = dir_.keys;
    const bool open = dir_.open;
    dir_ = Director{};
    dir_.keys = keys, dir_.open = open;
    view_.zoom = 0.85f;
    cam_started_ = false;
}

void GameView::draw_director(NetClient& net, std::int64_t now_ms, Ui& ui) {
    Director& d = dir_;
    if (!d.open || d.rec) return;
    const float pw = 380, px = view_.sw - pw - 8, py = 40, ph = view_.sh - 110;
    fill_rect(r_, px, py, pw, ph, {16, 18, 28, 235});
    rect(r_, px, py, pw, ph, {255, 190, 110, 255});
    text(r_, px + 10, py + 8, net.spectator ? tr("Replay director") : tr("Practice camera"), {255, 220, 150}, 2);
    if (button(r_, ui, px + pw - 34, py + 6, 26, 24, "x")) d.open = false;
    const char* const tabs[] = {"Camera", "Show", "Time", "Sequence"};
    const int ntabs = net.spectator ? 4 : 2;
    const float tw = (pw - 16) / float(ntabs);
    for (int i = 0; i < ntabs; ++i)
        if (button(r_, ui, px + 8 + tw * float(i), py + 36, tw - 3, 26, tr(tabs[i]), d.tab == i)) d.tab = i;
    d.tab = std::min(d.tab, ntabs - 1);
    const float x = px + 10, w = pw - 20;
    float y = py + 70;
    auto toggle = [&](const char* label, bool& v) {
        if (button(r_, ui, x, y, w, 24, std::format("{}: {}", tr(label), v ? tr("ON") : tr("OFF")), v)) v = !v;
        y += 27;
    };
    if (d.tab == 0) {
        int zoom = int(std::lround(view_.zoom * 100)), gy = int(std::lround(d.gy * 100)), hz = int(std::lround(d.hz * 100));
        if (slider(r_, ui, 7001, x, y, w, zoom, 5, 300, tr("Zoom %"))) view_.zoom = float(zoom) / 100, d.on = true;
        y += 26;
        if (slider(r_, ui, 7002, x, y, w, gy, 30, 100, tr("Angle of the camera"))) d.gy = float(gy) / 100, d.on = true;
        y += 26;
        if (slider(r_, ui, 7003, x, y, w, hz, 0, 150, tr("Height of things"))) d.hz = float(hz) / 100, d.on = true;
        y += 26;
        int cx = int(view_.cx), cy = int(view_.cy);
        if (slider(r_, ui, 7004, x, y, w, cx, 0, kMapUnits, tr("Camera, left - right"))) view_.cx = float(cx), d.follow = 0;
        y += 26;
        if (slider(r_, ui, 7005, x, y, w, cy, 0, kMapUnits, tr("Camera, up - down"))) view_.cy = float(cy), d.follow = 0;
        y += 30;
        const float b3 = w / 3;
        if (button(r_, ui, x, y, b3 - 4, 26, tr("Game view"))) d.on = true, d.gy = 0.78f, d.hz = 0.62f;
        if (button(r_, ui, x + b3, y, b3 - 4, 26, tr("From above"))) d.on = true, d.gy = 1.f, d.hz = 0.f;
        if (button(r_, ui, x + 2 * b3, y, b3 - 4, 26, tr("Low view"))) d.on = true, d.gy = 0.45f, d.hz = 0.9f;
        y += 30;
        if (button(r_, ui, x, y, w, 26, std::format("{}: {}", tr("Turn the map"), d.turn < 0 ? tr("as usual") : d.turn == 1 ? tr("turned") : tr("not turned")), d.turn >= 0)) d.turn = d.turn == 1 ? -1 : d.turn + 1, d.on = true;
        y += 34;
        text(r_, x, y, tr("Follow"), {255, 220, 150});
        y += 18;
        if (button(r_, ui, x, y, w, 24, tr("Nobody: a free camera"), d.follow == 0)) d.follow = 0;
        y += 27;
        const float fw = w / 2;
        int n = 0;
        for (const DrawUnit& u : draw_) {
            if (UnitKind(u.u.kind) != UnitKind::Champion || u.u.champ < 0 || y > py + ph - 66) continue;
            const float bx = x + fw * float(n % 2);
            if (button(r_, ui, bx, y, fw - 4, 24, fit_text(std::string(champions()[std::size_t(u.u.champ)].name), fw - 12), d.follow == u.u.id)) d.follow = u.u.id;
            if (++n % 2 == 0) y += 27;
        }
    } else if (d.tab == 1) {
        toggle("Panels on the screen", d.hud);
        toggle("Health bars", d.bars);
        toggle("Names", d.names);
        toggle("Fog of war", d.fog);
        toggle("Champions", d.champions);
        toggle("Minions", d.minions);
        toggle("Monsters", d.monsters);
        toggle("Structures", d.structures);
        toggle("Skills and hits", d.effects);
        toggle("Clouds, puddles, mud", d.clouds);
        toggle("Lights", d.lights);
        toggle("Particles", d.particles);
        toggle("Screen effects", d.post);
    } else if (d.tab == 2) {
        const float bw = (w - 8) / 6;
        const int speeds[] = {25, 50, 100, 200, 400, 800};
        if (button(r_, ui, x, y, w, 30, net.replay_speed == 0 ? tr("Play") : tr("Pause"), net.replay_speed == 0)) {
            if (net.replay_speed) d.speed_before = net.replay_speed, net.replay_speed = 0;
            else net.replay_speed = std::max(25, d.speed_before);
            net.send_spectate();
        }
        y += 36;
        for (int i = 0; i < 6; ++i)
            if (button(r_, ui, x + bw * float(i), y, bw - 3, 26, std::format("{:g}x", speeds[i] / 100.0), net.replay_speed == speeds[i])) net.replay_speed = speeds[i], net.send_spectate();
        y += 34;
        const double total = std::max(double(replay_total_ms), double(now_ms) + 1);
        text(r_, x, y, std::format("{} / {}", mmss(double(now_ms)), mmss(total)), {230, 230, 240}, 1.5f);
        y += 24;
        fill_rect(r_, x, y + 8, w, 6, {70, 70, 90, 255});
        fill_rect(r_, x, y + 8, w * float(double(now_ms) / total), 6, {255, 190, 90, 255});
        for (const auto& k : d.keys) fill_rect(r_, x + w * float(k.t / total) - 1, y + 2, 3, 18, {255, 240, 170, 255});
        if (ui.clicked && hover(ui, x, y, w, 22)) d.target_ms = total * double(std::clamp((ui.mx - x) / w, 0.f, 1.f)), ui.clicked = false;
        y += 30;
        const int jumps[] = {-60, -10, 10, 60};
        const float jw = w / 4;
        for (int i = 0; i < 4; ++i)
            if (button(r_, ui, x + jw * float(i), y, jw - 3, 26, std::format("{:+} s", jumps[i]))) d.target_ms = std::max(0.0, double(now_ms) + jumps[i] * 1000.0);
        y += 34;
        if (button(r_, ui, x, y, w, 26, trf("Vision of: {} (O)", tr(net.my_team == 0 ? "Fire" : "Earth")))) net.spec_team = 1 - net.spec_team, net.my_team = net.spec_team, net.send_spectate(), net.grid_dirty = true;
        y += 30;
    } else {
        text(r_, x, y, fit_text(tr("Move the camera, press Add a key, go on. Playing goes smoothly through the keys."), w), {170, 170, 190});
        y += 22;
        const float b2 = w / 2;
        if (button(r_, ui, x, y, b2 - 3, 26, tr("Add a key here"))) {
            d.keys.push_back({double(now_ms), view_.cx, view_.cy, view_.zoom, d.on ? d.gy : view_.gy, d.on ? d.hz : view_.hz, std::max(25, net.replay_speed)});
            std::ranges::sort(d.keys, {}, &Director::Key::t);
        }
        if (button(r_, ui, x + b2, y, b2 - 3, 26, d.seq_play ? tr("Stop") : tr("Play the sequence"), d.seq_play, d.keys.size() >= 2) && d.keys.size() >= 2) {
            d.seq_play = !d.seq_play;
            if (d.seq_play) d.target_ms = d.keys.front().t;
        }
        y += 30;
        if (button(r_, ui, x, y, b2 - 3, 24, std::format("{}: {}", tr("Loop"), d.seq_loop ? tr("ON") : tr("OFF")), d.seq_loop)) d.seq_loop = !d.seq_loop;
        if (button(r_, ui, x + b2, y, b2 - 3, 24, tr("Clear"), false, !d.keys.empty())) d.keys.clear(), d.seq_play = false;
        y += 28;
        if (button(r_, ui, x, y, b2 - 3, 24, tr("Save the sequence"), false, !d.keys.empty())) {
            std::error_code ec;
            fs::create_directories("replays", ec);
            std::ofstream out(kSeqFile);
            for (const auto& k : d.keys) out << k.t << ' ' << k.cx << ' ' << k.cy << ' ' << k.zoom << ' ' << k.gy << ' ' << k.hz << ' ' << k.speed << '\n';
            hint(out ? trf("saved: {}", kSeqFile) : tr("could not save it"));
        }
        if (button(r_, ui, x + b2, y, b2 - 3, 24, tr("Load the sequence"))) {
            std::ifstream in(kSeqFile);
            Director::Key k;
            d.keys.clear();
            while (in >> k.t >> k.cx >> k.cy >> k.zoom >> k.gy >> k.hz >> k.speed) d.keys.push_back(k);
        }
        y += 30;
        std::size_t remove = d.keys.size();
        for (std::size_t i = 0; i < d.keys.size() && y < py + ph - 66; ++i) {
            const auto& k = d.keys[i];
            if (button(r_, ui, x, y, w - 30, 24, fit_text(std::format("{}   {} %   {:g}x", mmss(k.t), int(k.zoom * 100), k.speed / 100.0), w - 40))) d.target_ms = k.t, view_.cx = k.cx, view_.cy = k.cy, view_.zoom = k.zoom;
            if (button(r_, ui, x + w - 26, y, 24, 24, "x")) remove = i;
            y += 27;
        }
        if (remove < d.keys.size()) d.keys.erase(d.keys.begin() + std::ptrdiff_t(remove));
    }
    // ---- always there
    const float by = py + ph - 62;
    if (button(r_, ui, x, by, w, 26, tr("Reset to normal"))) director_reset();
    if (button(r_, ui, x, by + 30, w, 26, tr("Record a video (F10 stops)"))) director_record(net);
    if (hover(ui, px, py, pw, ph)) {
        mouse_on_hud_ = true;
        ui.clicked = ui.rclicked = false;
    }
}

// A video through ffmpeg, if this computer has it (the picture goes in raw, 30 frames a second).
void GameView::director_record(NetClient&) {
    Director& d = dir_;
    if (d.rec) {
#if defined(_WIN32)
        _pclose(d.rec);
#else
        pclose(d.rec);
#endif
        d.rec = nullptr;
        hint(trf("saved: {}", d.rec_file));
        return;
    }
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(r_, &w, &h);
    w &= ~1, h &= ~1;
    std::error_code ec;
    fs::create_directories("recordings", ec);
    const auto t = std::chrono::system_clock::now().time_since_epoch();
    d.rec_file = std::format("recordings/replay_{}.mp4", std::chrono::duration_cast<std::chrono::seconds>(t).count());
    const std::string cmd = std::format("ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgba -s {}x{} -r 30 -i - -c:v libx264 -preset veryfast -crf 18 -pix_fmt yuv420p \"{}\"", w, h, d.rec_file);
#if defined(_WIN32)
    d.rec = _popen(cmd.c_str(), "wb");
#else
    d.rec = popen(cmd.c_str(), "w");
#endif
    if (!d.rec) return hint(tr("A video needs the ffmpeg program on this computer")), void();
    d.rec_w = w, d.rec_h = h, d.rec_next = 0;
    hint(tr("Recording: F10 stops it"));
}

void GameView::record_frame(double now_s) {
    Director& d = dir_;
    if (!d.rec || now_s < d.rec_next) return;
    d.rec_next = d.rec_next == 0 ? now_s + 1.0 / 30 : d.rec_next + 1.0 / 30;
    SDL_Surface* s = SDL_RenderReadPixels(r_, nullptr);
    if (!s) return;
    if (SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32)) {
        for (int y = 0; y < d.rec_h; ++y) std::fwrite(static_cast<const char*>(c->pixels) + std::ptrdiff_t(y) * c->pitch, 1, std::size_t(d.rec_w) * 4, d.rec);
        SDL_DestroySurface(c);
    }
    SDL_DestroySurface(s);
}

}  // namespace em
