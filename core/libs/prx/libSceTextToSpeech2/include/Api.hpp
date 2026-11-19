#pragma once

#include "SceTypes.hpp"

struct TextToSpeechInitParameter {
    std::uint32_t version;
    std::int32_t threadPriority;
    std::uint64_t cpuAffinity;
    std::uint8_t reserved[48];
};

struct TextToSpeechOpenParameter {
    std::uint32_t language;
    std::uint32_t voiceType;
    std::uint8_t reserved[56];
};

struct TextToSpeechSpeakParameter {
    const char16_t* text;
    std::uint32_t length;
    std::uint32_t mode;
    std::uint32_t volume;
    std::int32_t speed;
    std::uint8_t reserved[40];
};

extern "C" {

int APS5_VABI sceTextToSpeech2GetSystemStatus(std::int32_t* status);
std::int32_t APS5_VABI sceTextToSpeech2Initialize(const TextToSpeechInitParameter* param);
int APS5_VABI sceTextToSpeech2Cancel();
int APS5_VABI sceTextToSpeech2Close();
int APS5_VABI sceTextToSpeech2GetSpeechStatus(std::int32_t* status);
int APS5_VABI sceTextToSpeech2Open(const TextToSpeechOpenParameter* parameter);
int APS5_VABI sceTextToSpeech2Speak(const TextToSpeechSpeakParameter* parameter);
int APS5_VABI sceTextToSpeech2Terminate();
int APS5_VABI sceTextToSpeech2ConvertTextInt();
int APS5_VABI sceTextToSpeech2GetAudioInt();
int APS5_VABI sceTextToSpeech2InitializeInt();
int APS5_VABI sceTextToSpeech2OpenInt();
int APS5_VABI sceTextToSpeech2RegisterTextConversionItem();
int APS5_VABI sceTextToSpeech2SynthesizeInt();

}
