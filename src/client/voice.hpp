#pragma once
// [X-23] Voice chat with my team: push to talk. The microphone is opened only the first time the key is held, and it records only while it is held.
// 16 kHz mono, IMA ADPCM (4 : 1, about 64 kbit/s), 40 ms a packet. No codec library is needed. The server only passes it on to my team.
#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace em {

class Voice {
public:
    ~Voice();
    // Call every frame: `talk` = the key is held. Returns packets to send.
    [[nodiscard]] std::vector<std::string> update(bool talk);
    void play(std::uint8_t speaker, const std::string& packet);
    void set_gain(float g);
    [[nodiscard]] bool talking() const { return talking_; }
    [[nodiscard]] bool mic_failed() const { return mic_failed_; }
    [[nodiscard]] double last_heard(std::uint8_t speaker) const;
    // the codec, public for the checks
    [[nodiscard]] static std::string encode(const std::int16_t* pcm, int n);
    [[nodiscard]] static std::vector<std::int16_t> decode(const std::string& packet);

private:
    SDL_AudioStream* mic_ = nullptr;
    SDL_AudioDeviceID out_ = 0;
    std::map<std::uint8_t, SDL_AudioStream*> speakers_;
    std::map<std::uint8_t, double> heard_;
    std::vector<std::int16_t> pending_;
    bool talking_ = false, mic_failed_ = false;
    float gain_ = 1.f;
};

}  // namespace em
