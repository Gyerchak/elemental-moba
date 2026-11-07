// [X-23] See voice.hpp.
#include "client/voice.hpp"

#include <algorithm>

namespace em {

namespace {
constexpr int kRate = 16'000, kFrame = 640;  // 40 ms
constexpr SDL_AudioSpec kSpec{SDL_AUDIO_S16, 1, kRate};
constexpr int kIndexTable[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
constexpr int kStepTable[89] = {7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
}  // namespace

std::string Voice::encode(const std::int16_t* pcm, int n) {
    std::string out;
    out.reserve(std::size_t(n / 2 + 4));
    int pred = n > 0 ? pcm[0] : 0, index = 20;
    out.push_back(char(pred & 255)), out.push_back(char((pred >> 8) & 255)), out.push_back(char(index));
    std::uint8_t nib = 0;
    bool high = false;
    for (int i = 0; i < n; ++i) {
        const int step = kStepTable[index];
        int diff = pcm[i] - pred, code = 0;
        if (diff < 0) code = 8, diff = -diff;
        int delta = step >> 3;
        if (diff >= step) code |= 4, diff -= step, delta += step;
        if (diff >= step / 2) code |= 2, diff -= step / 2, delta += step / 2;
        if (diff >= step / 4) code |= 1, delta += step / 4;
        pred = std::clamp(pred + (code & 8 ? -delta : delta), -32'768, 32'767);
        index = std::clamp(index + kIndexTable[code], 0, 88);
        if (high) out.push_back(char(nib | (code << 4)));
        else nib = std::uint8_t(code);
        high = !high;
    }
    if (high) out.push_back(char(nib));
    return out;
}

std::vector<std::int16_t> Voice::decode(const std::string& p) {
    std::vector<std::int16_t> out;
    if (p.size() < 4) return out;
    int pred = std::int16_t(std::uint8_t(p[0]) | std::uint8_t(p[1]) << 8), index = std::clamp<int>(std::uint8_t(p[2]), 0, 88);
    out.reserve((p.size() - 3) * 2);
    for (std::size_t i = 3; i < p.size(); ++i)
        for (int half = 0; half < 2; ++half) {
            const int code = half ? std::uint8_t(p[i]) >> 4 : std::uint8_t(p[i]) & 15;
            const int step = kStepTable[index];
            int delta = step >> 3;
            if (code & 4) delta += step;
            if (code & 2) delta += step / 2;
            if (code & 1) delta += step / 4;
            pred = std::clamp(pred + (code & 8 ? -delta : delta), -32'768, 32'767);
            index = std::clamp(index + kIndexTable[code], 0, 88);
            out.push_back(std::int16_t(pred));
        }
    return out;
}

Voice::~Voice() {
    if (mic_) SDL_DestroyAudioStream(mic_);
    for (auto& [id, s] : speakers_) SDL_DestroyAudioStream(s);
    if (out_) SDL_CloseAudioDevice(out_);
}

std::vector<std::string> Voice::update(bool talk) {
    std::vector<std::string> packets;
    if (talk && !mic_ && !mic_failed_) {  // the first press: only now is the microphone opened
        mic_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &kSpec, nullptr, nullptr);
        if (!mic_) mic_failed_ = true;
    }
    if (mic_ && talk != talking_) {
        if (talk) SDL_ClearAudioStream(mic_), SDL_ResumeAudioStreamDevice(mic_);
        else SDL_PauseAudioStreamDevice(mic_);
    }
    talking_ = talk && mic_;
    if (!mic_) return packets;
    if (talking_ || !pending_.empty()) {
        std::int16_t buf[kFrame];
        int got = 0;
        while (talking_ && (got = SDL_GetAudioStreamData(mic_, buf, sizeof buf)) > 0) pending_.insert(pending_.end(), buf, buf + got / 2);
        while (int(pending_.size()) >= kFrame) {
            packets.push_back(encode(pending_.data(), kFrame));
            pending_.erase(pending_.begin(), pending_.begin() + kFrame);
        }
        if (!talking_) pending_.clear();
    }
    return packets;
}

void Voice::play(std::uint8_t speaker, const std::string& packet) {
    const std::vector<std::int16_t> pcm = decode(packet);
    if (pcm.empty()) return;
    if (!out_) out_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!out_) return;
    auto it = speakers_.find(speaker);
    if (it == speakers_.end()) {
        SDL_AudioStream* s = SDL_CreateAudioStream(&kSpec, nullptr);
        if (!s || !SDL_BindAudioStream(out_, s)) return;
        SDL_SetAudioStreamGain(s, gain_);
        it = speakers_.emplace(speaker, s).first;
    }
    SDL_PutAudioStreamData(it->second, pcm.data(), int(pcm.size() * sizeof(std::int16_t)));
    heard_[speaker] = double(SDL_GetTicks()) / 1000.0;
}

void Voice::set_gain(float g) {
    gain_ = g;
    for (auto& [id, s] : speakers_) SDL_SetAudioStreamGain(s, g);
}

double Voice::last_heard(std::uint8_t speaker) const {
    const auto it = heard_.find(speaker);
    return it == heard_.end() ? -100.0 : it->second;
}

}  // namespace em
