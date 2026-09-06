#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int32_t sceAudiodecClearContext(int32_t handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceAudiodecCreateDecoder(AudiodecCtrl* ctrl, uint32_t codec_type) {
 (void)ctrl;
 (void)codec_type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceAudiodecDecode(int32_t handle, AudiodecCtrl* ctrl) {
 (void)handle;
 (void)ctrl;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceAudiodecDeleteDecoder(int32_t handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceAudiodecInitLibrary(uint32_t codec_type) {
 (void)codec_type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceAudiodecTermLibrary(uint32_t codec_type) {
 (void)codec_type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
