#pragma once
// [U-535] What the item maker and the loadout maker share: a list to choose from, and the game version they write into.
//   versions/<name>/version.txt keeps every other line as it was; a maker owns the lines that start with its prefix.
#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "client/draw.hpp"
#include "client/locale.hpp"
#include "client/tool_shell.hpp"
#include "game/versions.hpp"

namespace em::makers {

struct Pick { std::string label, value, note; };

class Picker {
public:
    void open(tool::Window& win, std::string title, std::vector<Pick> items, std::function<void(const std::string&)> done) {
        title_ = std::move(title), items_ = std::move(items), done_ = std::move(done);
        search_.clear(), scroll_ = 0;
        win.ui.focus = 99;
    }
    [[nodiscard]] bool active() const { return done_ != nullptr; }
    void draw(tool::Window& win) {
        using namespace gfx;
        SDL_Renderer* r = win.ren();
        Ui& ui = win.ui;
        const float pw = std::min(760.f, win.w - 80), ph = win.h - 120, x = (win.w - pw) / 2, y = 60;
        fill_rect(r, 0, 0, win.w, win.h, {0, 0, 0, 170});
        fill_rect(r, x, y, pw, ph, {30, 32, 44, 255});
        rect(r, x, y, pw, ph, {255, 190, 110, 255});
        text(r, x + 14, y + 10, fit_text(title_, pw - 28, 2), {255, 220, 150}, 2);
        text_field(r, ui, 99, x + 14, y + 52, pw - 140, search_, loc::tr("Search"));
        if (button(r, ui, x + pw - 110, y + 50, 96, 30, loc::tr("Close"))) {
            done_ = {};
            ui.focus = -1;
            return;
        }
        std::string low = search_;
        for (char& c : low) c = char(std::tolower(static_cast<unsigned char>(c)));
        const float top = y + 92, bottom = y + ph - 10;
        const SDL_Rect clip{int(x), int(top), int(pw), int(bottom - top)};
        SDL_SetRenderClipRect(r, &clip);
        scroll_ = std::max(0.f, scroll_ - ui.wheel * 40);
        float ry = top - scroll_;
        std::function<void(const std::string&)> chosen;
        std::string value;
        for (const Pick& p : items_) {
            std::string l = p.label + " " + p.note;
            for (char& c : l) c = char(std::tolower(static_cast<unsigned char>(c)));
            if (!low.empty() && l.find(low) == std::string::npos) continue;
            if (ry > top - 30 && ry < bottom) {
                if (button(r, ui, x + 14, ry, 300, 26, fit_text(p.label, 288)) && hover(ui, x, top, pw, bottom - top)) chosen = done_, value = p.value;
                text(r, x + 324, ry + 8, fit_text(p.note, pw - 340), {180, 180, 195});
            }
            ry += 30;
        }
        SDL_SetRenderClipRect(r, nullptr);
        if (chosen) {
            done_ = {};
            ui.focus = -1;
            chosen(value);
        }
    }

private:
    std::string title_, search_;
    std::vector<Pick> items_;
    std::function<void(const std::string&)> done_;
    float scroll_ = 0;
};

// The lines of one version file that belong to a maker, in order; every other line stays untouched.
struct VersionDoc {
    struct Line { std::string key, value; };
    std::string name;          // "" = none chosen yet
    std::string prefix;        // "item." or "setup."
    std::vector<std::string> others;
    std::vector<Line> lines;

    [[nodiscard]] std::filesystem::path file() const { return std::filesystem::path("versions") / name / "version.txt"; }
    void load(const std::string& version) {
        name = version;
        others.clear(), lines.clear();
        std::ifstream in(file());
        for (std::string l; std::getline(in, l);) {
            std::string t = l;
            if (const auto h = t.find('#'); h != std::string::npos) t.resize(h);
            const auto eq = t.find('=');
            const auto cut = [](std::string s) {
                while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
                std::size_t i = 0;
                while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
                return s.substr(i);
            };
            if (eq != std::string::npos && cut(t.substr(0, eq)).starts_with(prefix)) lines.push_back({cut(t.substr(0, eq)), cut(t.substr(eq + 1))});
            else others.push_back(l);
        }
    }
    void set(const std::string& key, const std::string& value) {
        for (Line& l : lines)
            if (l.key == key) {
                l.value = value;
                return;
            }
        lines.push_back({key, value});
    }
    [[nodiscard]] bool save(std::string* error) const {
        std::error_code ec;
        std::filesystem::create_directories(file().parent_path(), ec);
        const std::string tmp = file().string() + ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary);
            bool has_description = false;
            for (const std::string& l : others) {
                out << l << '\n';
                has_description |= l.starts_with("description");
            }
            if (!has_description) out << "description = Made with the makers\n";
            for (const Line& l : lines) out << l.key << " = " << l.value << '\n';
            if (!out) {
                if (error) *error = loc::trf("could not write {}", file().string());
                return false;
            }
        }
        std::filesystem::rename(tmp, file(), ec);
        if (ec && error) *error = ec.message();
        return !ec;
    }
};

[[nodiscard]] inline std::string version_folder_name(std::string s) {
    for (char& c : s)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) c = '-';
    return s.empty() ? std::string("my-version") : s;
}

}  // namespace em::makers
