#pragma once

#include "SceTypes.hpp"

struct AudiodecCpuMemory {
    std::uint32_t ui_size;
    void* p_memory;
    std::uint32_t ui_memory_size;
};

extern "C" {

int APS5_VABI sceAudiodecCpuClearContext(const AudiodecCpuMemory* memory, std::uint32_t type);
int APS5_VABI sceAudiodecCpuDecode(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type);
int APS5_VABI sceAudiodecCpuInitDecoder(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type);
int APS5_VABI sceAudiodecCpuInternalClearContext(const AudiodecCpuMemory* memory, std::uint32_t type);
int APS5_VABI sceAudiodecCpuInternalDecode(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type);
int APS5_VABI sceAudiodecCpuInternalInitDecoder(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type);
int APS5_VABI sceAudiodecCpuInternalQueryMemSize(const AudiodecCtrl* ctrl, AudiodecCpuMemory* memory, std::uint32_t type);
int APS5_VABI sceAudiodecCpuQueryMemSize(const AudiodecCtrl* ctrl, AudiodecCpuMemory* memory, std::uint32_t type);

}
