#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceVoiceConnectIPortToOPort(uint32_t input_port_id, uint32_t output_port_id) {
 (void)input_port_id;
 (void)output_port_id;
 return 0;
}

int sceVoiceCreatePort(uint32_t* port_id, const VoicePortParam* param) {
 (void)port_id;
 (void)param;
 return 0;
}

int sceVoiceDeletePort(uint32_t port_id) {
 (void)port_id;
 return 0;
}

int sceVoiceDisconnectIPortFromOPort(uint32_t input_port_id, uint32_t output_port_id) {
 (void)input_port_id;
 (void)output_port_id;
 return 0;
}

int sceVoiceEnd_nid_postfix(void) {
 return 0;
}

int sceVoiceGetBitRate(uint32_t port_id, uint32_t* bitrate) {
 (void)port_id;
 (void)bitrate;
 return 0;
}

int sceVoiceGetPortAttr(uint32_t port_id, int32_t attr, void* value, int32_t size) {
 (void)port_id;
 (void)attr;
 (void)value;
 (void)size;
 return 0;
}

int sceVoiceGetPortInfo(uint32_t port_id, VoicePortInfo* info) {
 (void)port_id;
 (void)info;
 return 0;
}

int sceVoiceGetVolume(uint32_t port_id, float* volume) {
 (void)port_id;
 (void)volume;
 return 0;
}

int sceVoiceInit(VoiceInitParam* param, int32_t version) {
 (void)param;
 (void)version;
 return 0;
}

int sceVoiceReadFromOPort(uint32_t output_port_id, void* data, uint32_t* size) {
 (void)output_port_id;
 (void)data;
 (void)size;
 return 0;
}

int sceVoiceSetThreadsParams(void* params) {
 (void)params;
 return 0;
}

int sceVoiceSetVolume(uint32_t port_id, float volume) {
 (void)port_id;
 (void)volume;
 return 0;
}

int sceVoiceStart(const VoiceStartParam* param) {
 (void)param;
 return 0;
}

int sceVoiceStop(void) {
 return 0;
}

int sceVoiceWriteToIPort(uint32_t input_port_id, const void* data, uint32_t* size, int16_t frame_gaps) {
 (void)input_port_id;
 (void)data;
 (void)size;
 (void)frame_gaps;
 return 0;
}

}
