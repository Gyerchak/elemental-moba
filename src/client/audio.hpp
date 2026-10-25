#pragma once
// [U-201][X-18] Sound: a small software mixer on an SDL3 audio stream. Every sound belongs to a category
// with its own volume. In the match, sounds are placed by position: only what my champion can HEAR
// (same ranges as the visual sound waves, U-151) is played, quieter with distance, panned left / right.
#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

#include "client/resources.hpp"
#include "client/settings.hpp"

namespace em {

enum class SoundCat : std::uint8_t { Effects, Music, Ui, Ambient, Voice, Count };

class Audio {
public:
    ~Audio();
    bool init();
    void set_volumes(const Settings& s);
    void play(const Sound* s, SoundCat cat, float gain = 1.f, float pan = 0.f);  // pan −1 left … +1 right
    void play_music(const Sound* s);  // loops; nullptr = stop
    // Positional: hearing range like the sound waves; listener = my champion, screen right = +x
    void play_at(const Sound* s, float dx, float dy, float hear_range, SoundCat cat = SoundCat::Effects);

private:
    static void SDLCALL callback(void* self, SDL_AudioStream* stream, int additional, int total);
    void mix(float* out, int frames);
    struct Voice {
        const Sound* s = nullptr;
        std::size_t pos = 0;
        float gl = 1, gr = 1;
        SoundCat cat = SoundCat::Effects;
        bool loop = false;
    };
    SDL_AudioStream* stream_ = nullptr;
    std::mutex mu_;
    std::vector<Voice> voices_;
    std::array<float, std::size_t(SoundCat::Count)> vol_{1, 1, 1, 1, 1};
    float master_ = 1;
    std::vector<float> scratch_;
};

}  // namespace em
