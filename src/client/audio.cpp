// Software mixer. See audio.hpp.
#include "client/audio.hpp"

#include <algorithm>
#include <cmath>

namespace em {

Audio::~Audio() {
    if (stream_) SDL_DestroyAudioStream(stream_);
}

bool Audio::init() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 44'100};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &Audio::callback, this);
    if (!stream_) return false;
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

void Audio::set_volumes(const Settings& s) {
    std::scoped_lock lk(mu_);
    master_ = float(s.vol_master) / 100.f;
    vol_[std::size_t(SoundCat::Effects)] = float(s.vol_effects) / 100.f;
    vol_[std::size_t(SoundCat::Music)] = float(s.vol_music) / 100.f;
    vol_[std::size_t(SoundCat::Ui)] = float(s.vol_ui) / 100.f;
    vol_[std::size_t(SoundCat::Ambient)] = float(s.vol_ambient) / 100.f;
    vol_[std::size_t(SoundCat::Voice)] = float(s.vol_voice) / 100.f;
}

void Audio::play(const Sound* s, SoundCat cat, float gain, float pan) {
    if (!s || !stream_ || s->samples.empty() || gain <= 0.001f) return;
    pan = std::clamp(pan, -1.f, 1.f);
    std::scoped_lock lk(mu_);
    if (voices_.size() > 48) voices_.erase(voices_.begin());  // never more than 48 at once
    voices_.push_back({s, 0, gain * std::min(1.f, 1 - pan), gain * std::min(1.f, 1 + pan), cat, false});
}

void Audio::play_music(const Sound* s) {
    std::scoped_lock lk(mu_);
    std::erase_if(voices_, [](const Voice& v) { return v.cat == SoundCat::Music; });
    if (s && !s->samples.empty()) voices_.push_back({s, 0, 0.55f, 0.55f, SoundCat::Music, true});  // [U-445] under the effects
}

void Audio::play_at(const Sound* s, float dx, float dy, float hear_range, SoundCat cat) {
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d > hear_range) return;  // [U-201] out of hearing range: silent (the same rule as the sound waves)
    const float gain = 1.f - 0.85f * d / hear_range;
    play(s, cat, gain, dx / std::max(400.f, hear_range * 0.6f));
}

void SDLCALL Audio::callback(void* self, SDL_AudioStream* stream, int additional, int) {
    auto* a = static_cast<Audio*>(self);
    const int frames = additional / int(sizeof(float) * 2);
    if (frames <= 0) return;
    a->scratch_.assign(std::size_t(frames) * 2, 0.f);
    a->mix(a->scratch_.data(), frames);
    SDL_PutAudioStreamData(stream, a->scratch_.data(), frames * int(sizeof(float) * 2));
}

// [U-445] the mix: effects first, the music ducks under busy moments (up to −6 dB), a 1 s crossfade where a looping
// track starts again (no click), and a soft limiter instead of hard clipping (no distortion when many sounds play)
void Audio::mix(float* out, int frames) {
    std::scoped_lock lk(mu_);
    int busy = 0;
    for (const Voice& v : voices_) busy += v.cat != SoundCat::Music && v.cat != SoundCat::Ambient;
    const float target_duck = 1.f - 0.5f * std::min(1.f, float(busy) / 8.f);
    duck_ += (target_duck - duck_) * 0.2f;  // smooth (per audio block)
    constexpr std::size_t kXfade = 44'100;  // 1 s
    for (Voice& v : voices_) {
        const float g = master_ * vol_[std::size_t(v.cat)] * (v.cat == SoundCat::Music ? duck_ : 1.f);
        const auto& smp = v.s->samples;
        const std::size_t n = smp.size();
        const bool xf = v.loop && n > 4 * kXfade;
        for (int i = 0; i < frames; ++i) {
            if (v.pos >= n) {
                if (!v.loop) break;
                v.pos = xf ? kXfade : 0;  // the start was already heard inside the crossfade
            }
            float x = smp[v.pos];
            if (xf && v.pos >= n - kXfade) {  // the end fades out while the beginning fades in
                const float t = float(v.pos - (n - kXfade)) / float(kXfade);
                x = x * (1.f - t) + smp[v.pos - (n - kXfade)] * t;
            }
            ++v.pos;
            out[2 * i] += x * g * v.gl;
            out[2 * i + 1] += x * g * v.gr;
        }
    }
    std::erase_if(voices_, [](const Voice& v) { return !v.loop && v.pos >= v.s->samples.size(); });
    for (int i = 0; i < frames * 2; ++i) {  // soft limiter: linear up to 0.6, then a smooth knee towards ±1
        const float a = std::abs(out[i]);
        if (a > 0.6f) out[i] = std::copysign(0.6f + 0.4f * std::tanh((a - 0.6f) / 0.4f), out[i]);
    }
}

}  // namespace em
