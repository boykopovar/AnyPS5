#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceVoiceConnectIPortToOPort(uint32_t input_port_id, uint32_t output_port_id) {
 (void)input_port_id;
 (void)output_port_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceCreatePort(uint32_t* port_id, const VoicePortParam* param) {
 (void)port_id;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceDeletePort(uint32_t port_id) {
 (void)port_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceDisconnectIPortFromOPort(uint32_t input_port_id, uint32_t output_port_id) {
 (void)input_port_id;
 (void)output_port_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceEnd_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceGetBitRate(uint32_t port_id, uint32_t* bitrate) {
 (void)port_id;
 (void)bitrate;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceGetPortAttr(uint32_t port_id, int32_t attr, void* value, int32_t size) {
 (void)port_id;
 (void)attr;
 (void)value;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceGetPortInfo(uint32_t port_id, VoicePortInfo* info) {
 (void)port_id;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceGetVolume(uint32_t port_id, float* volume) {
 (void)port_id;
 (void)volume;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceInit(VoiceInitParam* param, int32_t version) {
 (void)param;
 (void)version;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceReadFromOPort(uint32_t output_port_id, void* data, uint32_t* size) {
 (void)output_port_id;
 (void)data;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceSetThreadsParams(void* params) {
 (void)params;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceSetVolume(uint32_t port_id, float volume) {
 (void)port_id;
 (void)volume;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceStart(const VoiceStartParam* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceStop(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceWriteToIPort(uint32_t input_port_id, const void* data, uint32_t* size, int16_t frame_gaps) {
 (void)input_port_id;
 (void)data;
 (void)size;
 (void)frame_gaps;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
