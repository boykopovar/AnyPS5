#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceAudioOut2ContextAdvance(AudioOut2ContextHandle ctx) {
 (void)ctx;
 return 0;
}

int sceAudioOut2ContextCreate(const AudioOut2ContextParam* params, void* buffer, size_t buffer_size, AudioOut2ContextHandle* ctx) {
 (void)params;
 (void)buffer;
 (void)buffer_size;
 (void)ctx;
 return 0;
}

int sceAudioOut2ContextDestroy(AudioOut2ContextHandle ctx) {
 (void)ctx;
 return 0;
}

int sceAudioOut2ContextGetQueueLevel(AudioOut2ContextHandle ctx, uint32_t* queue_level, uint32_t* available_queues) {
 (void)ctx;
 (void)queue_level;
 (void)available_queues;
 return 0;
}

int sceAudioOut2ContextPush(AudioOut2ContextHandle ctx, uint32_t blocking) {
 (void)ctx;
 (void)blocking;
 return 0;
}

int sceAudioOut2ContextQueryMemory(const AudioOut2ContextParam* params, size_t* memory_size) {
 (void)params;
 (void)memory_size;
 return 0;
}

int sceAudioOut2ContextResetParam(AudioOut2ContextParam* params) {
 (void)params;
 return 0;
}

int sceAudioOut2ContextSetAttributes(AudioOut2ContextHandle ctx, const AudioOut2Attribute* attributes, uint32_t num) {
 (void)ctx;
 (void)attributes;
 (void)num;
 return 0;
}

int sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(AudioOut2SpeakerArrayHandle handle, uint32_t ambisonics_channel, float* coefficients, uint32_t num_coefficients) {
 (void)handle;
 (void)ambisonics_channel;
 (void)coefficients;
 (void)num_coefficients;
 return 0;
}

int sceAudioOut2GetSpeakerArrayCoefficients(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, uint32_t num_coefficients, uint8_t height_aware, float downmix_spread_radius) {
 (void)handle;
 (void)pos;
 (void)spread;
 (void)coefficients;
 (void)num_coefficients;
 (void)height_aware;
 (void)downmix_spread_radius;
 return 0;
}

size_t sceAudioOut2GetSpeakerArrayMemorySize(uint32_t num_speakers, uint8_t is_3d, uint8_t is_ambisonics) {
 (void)num_speakers;
 (void)is_3d;
 (void)is_ambisonics;
 return 0;
}

int sceAudioOut2GetSpeakerInfo(AudioOut2SpeakerInfo* info, uint32_t flags) {
 (void)info;
 (void)flags;
 return 0;
}

int sceAudioOut2GetSystemState(AudioOut2SystemState* state) {
 (void)state;
 return 0;
}

int sceAudioOut2Initialize(void) {
 return 0;
}

int sceAudioOut2MasteringGetState(AudioOut2MasteringStatesHeader* state, uint32_t output, AudioOut2UserHandle user) {
 (void)state;
 (void)output;
 (void)user;
 return 0;
}

int sceAudioOut2MasteringInit(uint32_t flags) {
 (void)flags;
 return 0;
}

int sceAudioOut2MasteringSetParam(const AudioOut2MasteringParamsHeader* param, uint32_t output, uint32_t flags) {
 (void)param;
 (void)output;
 (void)flags;
 return 0;
}

int sceAudioOut2MasteringTerm(void) {
 return 0;
}

int sceAudioOut2PortCreate(AudioOut2ContextHandle ctx, const AudioOut2PortParam* params, AudioOut2PortHandle* port) {
 (void)ctx;
 (void)params;
 (void)port;
 return 0;
}

int sceAudioOut2PortDestroy(AudioOut2PortHandle port) {
 (void)port;
 return 0;
}

int sceAudioOut2PortGetState(AudioOut2PortHandle port, AudioOut2PortState* state) {
 (void)port;
 (void)state;
 return 0;
}

int sceAudioOut2PortSetAttributes(AudioOut2PortHandle port, const AudioOut2Attribute* attributes, uint32_t num) {
 (void)port;
 (void)attributes;
 (void)num;
 return 0;
}

int sceAudioOut2SetSystemDebugState(const AudioOut2SystemDebugStateParam* param) {
 (void)param;
 return 0;
}

int sceAudioOut2SpeakerArrayCreate(AudioOut2SpeakerArrayHandle* handle, const void* vbap_params, const void* ambi_params) {
 (void)handle;
 (void)vbap_params;
 (void)ambi_params;
 return 0;
}

int sceAudioOut2SpeakerArrayDestroy(AudioOut2SpeakerArrayHandle handle) {
 (void)handle;
 return 0;
}

int sceAudioOut2UserCreate(uint32_t user_id, AudioOut2UserHandle* handle) {
 (void)user_id;
 (void)handle;
 return 0;
}

int sceAudioOut2UserDestroy(AudioOut2UserHandle handle) {
 (void)handle;
 return 0;
}

int sceAudioOutClose(int handle) {
 (void)handle;
 return 0;
}

int sceAudioOutGetPortState(int handle, AudioOutPortState* state) {
 (void)handle;
 (void)state;
 return 0;
}

int sceAudioOutInit(void) {
 return 0;
}

int sceAudioOutOpen(int user_id, int type, int index, uint32_t len, uint32_t freq, uint32_t param) {
 (void)user_id;
 (void)type;
 (void)index;
 (void)len;
 (void)freq;
 (void)param;
 return 0;
}

int sceAudioOutOutput(int handle, const void* ptr) {
 (void)handle;
 (void)ptr;
 return 0;
}

int sceAudioOutOutputs(AudioOutOutputParam* param, uint32_t num) {
 (void)param;
 (void)num;
 return 0;
}

int sceAudioOutSetVolume(int handle, uint32_t flag, int* vol) {
 (void)handle;
 (void)flag;
 (void)vol;
 return 0;
}

}
