#ifndef CORE_LIBS_PRX_LIBSCENGS2_PITCHSHIFT_HPP
#define CORE_LIBS_PRX_LIBSCENGS2_PITCHSHIFT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

struct Ngs2PitchShiftChannel {
    std::array<float, 7> resampleHistory{};
    std::array<float, 960> overlapHistory{};
    std::array<float, 15> coefficients{};
    std::array<float, 142> filterHistory{};
    std::array<float, 3008> input{};
    std::array<float, 1088> output{};
    std::uint32_t filterCursor = 0;
    std::uint32_t copyRemaining = 0;
    std::uint32_t inputCount = 0;
    std::uint32_t outputCount = 0;
    std::uint32_t inputTime = 0;
    std::uint32_t outputTime = 0;
    bool started = false;

    void SetPitch(float ratio);
    void Process(const float* source, float* dest, float ratio);
};

struct Ngs2PitchShift {
    std::vector<Ngs2PitchShiftChannel> channels;
    std::uint32_t bypassMask = ~0u;
    float ratio = 1.0f;
    bool reset = true;
    bool changed = false;
    std::uint32_t initializedChannels = 0;
};

enum class Ngs2VoiceTransition { None, Pause, Resume, Stop, Kill };

struct Ngs2Voice;
bool Ngs2HasPitchShift(const Ngs2Voice& voice);
void Ngs2ResetPitchShift(Ngs2Voice& voice);
void Ngs2RestartPitchShift(Ngs2Voice& voice);
void Ngs2ProcessPitchShift(Ngs2Voice& voice, std::size_t module, std::uint32_t grain);
void Ngs2ApplyVoiceGain(Ngs2Voice& voice, std::uint32_t grain);

#endif
