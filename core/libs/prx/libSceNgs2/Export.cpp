#include <cstdint>
#include <cstdio>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"


namespace {
int SoftFailNp(const char* func) {
    static std::mutex mutex;
    static std::map<std::string, unsigned long long> calls;
    std::lock_guard lock(mutex);
    const auto count = ++calls[func];
    if (count <= 3 || count % 100000 == 0) {
        std::fprintf(stderr, "[NGS2-STUB] %s -> 0 (audio engine not implemented, call #%llu)\n", func, count);
        std::fflush(stderr);
    }
    return 0;
}
}  // namespace

extern "C" {

int APS5_VABI sceNgs2CalcWaveformBlock(const Ngs2WaveformFormat* format, uint32_t sample_pos, uint32_t num_samples, Ngs2WaveformBlock* block) {
 (void)format;
 (void)sample_pos;
 (void)num_samples;
 (void)block;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2GeomApply(const Ngs2GeomListenerWork* listener, const Ngs2GeomSourceParam* source, Ngs2GeomAttribute* out_attrib, uint32_t flags) {
 (void)listener;
 (void)source;
 (void)out_attrib;
 (void)flags;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2GeomCalcListener(const Ngs2GeomListenerParam* param, Ngs2GeomListenerWork* out_work, uint32_t flags) {
 (void)param;
 (void)out_work;
 (void)flags;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2GeomResetListenerParam(Ngs2GeomListenerParam* out_listener_param) {
 (void)out_listener_param;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2GeomResetSourceParam(Ngs2GeomSourceParam* out_source_param) {
 (void)out_source_param;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2PanGetVolumeMatrix(Ngs2PanWork* work, const Ngs2PanParam* params, uint32_t num_params, uint32_t matrix_format, float* out_volume_matrix) {
 (void)work;
 (void)params;
 (void)num_params;
 (void)matrix_format;
 (void)out_volume_matrix;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2PanInit(Ngs2PanWork* work, const float* speaker_angles, float unit_angle, uint32_t num_speakers) {
 (void)work;
 (void)speaker_angles;
 (void)unit_angle;
 (void)num_speakers;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2ParseWaveformData(const void* data, size_t data_size, Ngs2WaveformInfo* info) {
 (void)data;
 (void)data_size;
 (void)info;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2RackCreate(uintptr_t system_handle, uint32_t rack_id, const Ngs2RackOption* option, const Ngs2ContextBufferInfo* buffer_info, uintptr_t* handle) {
 (void)system_handle;
 (void)rack_id;
 (void)option;
 (void)buffer_info;
 (void)handle;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2RackCreateWithAllocator(uintptr_t system_handle, uint32_t rack_id, const Ngs2RackOption* option, const Ngs2BufferAllocator* allocator, uintptr_t* handle) {
 (void)system_handle;
 (void)rack_id;
 (void)option;
 (void)allocator;
 (void)handle;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2RackDestroy(uintptr_t rack_handle, Ngs2ContextBufferInfo* buffer_info) {
 (void)rack_handle;
 (void)buffer_info;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2RackGetVoiceHandle(uintptr_t rack_handle, uint32_t voice_id, uintptr_t* handle) {
 (void)rack_handle;
 (void)voice_id;
 (void)handle;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2RackLock(uintptr_t rack_handle) {
 (void)rack_handle;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2RackQueryBufferSize(uint32_t rack_id, const Ngs2RackOption* option, Ngs2ContextBufferInfo* buffer_info) {
 (void)rack_id;
 (void)option;
 buffer_info->host_buffer_size = 0x10000;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2RackUnlock(uintptr_t rack_handle) {
 (void)rack_handle;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2SystemCreate(const Ngs2SystemOption* option, const Ngs2ContextBufferInfo* buffer_info, uintptr_t* handle) {
 (void)option;
 (void)buffer_info;
 (void)handle;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2SystemCreateWithAllocator(const Ngs2SystemOption* option, const Ngs2BufferAllocator* allocator, uintptr_t* handle) {
 (void)option;
 (void)allocator;
 (void)handle;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2SystemDestroy(uintptr_t system_handle, Ngs2ContextBufferInfo* buffer_info) {
 (void)system_handle;
 (void)buffer_info;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2SystemGetInfo(uintptr_t system_handle, Ngs2SystemInfo* info, size_t info_size) {
 (void)system_handle;
 (void)info;
 (void)info_size;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2SystemQueryBufferSize(const Ngs2SystemOption* option, Ngs2ContextBufferInfo* buffer_info) {
 (void)option;
 buffer_info->host_buffer_size = 0x10000;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2SystemRender(uintptr_t system_handle, const Ngs2RenderBufferInfo* buffer_info, uint32_t num_buffer_info) {
    (void)system_handle;
    if (buffer_info != nullptr) {
        for (uint32_t i = 0; i < num_buffer_info; ++i) {
            if (buffer_info[i].buffer != nullptr && buffer_info[i].buffer_size != 0) std::memset(buffer_info[i].buffer, 0, buffer_info[i].buffer_size);
        }
    }
    SoftFailNp(__func__);
    std::this_thread::sleep_for(std::chrono::microseconds(5333));
    return 0;
}
int APS5_VABI sceNgs2SystemSetGrainSamples(uintptr_t system_handle, uint32_t num_samples) {
 (void)system_handle;
 (void)num_samples;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2VoiceControl(uintptr_t voice_handle, const Ngs2VoiceParamHeader* param_list) {
 (void)voice_handle;
 (void)param_list;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2VoiceGetState(uintptr_t voice_handle, Ngs2VoiceState* state, size_t state_size) {
 (void)voice_handle;
 (void)state;
 (void)state_size;
 return SoftFailNp(__func__);
 return 0;
}
int APS5_VABI sceNgs2VoiceGetStateFlags(uintptr_t voice_handle, uint32_t* state_flags) {
 (void)voice_handle;
 (void)state_flags;
 return SoftFailNp(__func__);
 return 0;
}
}
