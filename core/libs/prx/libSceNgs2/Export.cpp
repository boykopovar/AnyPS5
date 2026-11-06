#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNgs2CalcWaveformBlock(const Ngs2WaveformFormat* format, uint32_t sample_pos, uint32_t num_samples, Ngs2WaveformBlock* block) {
 (void)format;
 (void)sample_pos;
 (void)num_samples;
 (void)block;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}



int APS5_VABI sceNgs2GeomCalcListener(const Ngs2GeomListenerParam* param, Ngs2GeomListenerWork* out_work, uint32_t flags) {
 (void)param;
 (void)out_work;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNgs2GeomResetListenerParam(Ngs2GeomListenerParam* out_listener_param) {
 (void)out_listener_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNgs2GeomResetSourceParam(Ngs2GeomSourceParam* out_source_param) {
 (void)out_source_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNgs2ParseWaveformData(const void* data, size_t data_size, Ngs2WaveformInfo* info) {
 (void)data;
 (void)data_size;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
