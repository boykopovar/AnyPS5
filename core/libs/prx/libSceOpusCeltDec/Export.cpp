#include "prx/libSceOpusCeltDec/include/Decoder.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

int APS5_VABI sceOpusCeltDecInitialize(std::uint32_t* context) {
    return OpusCeltDec::Initialize_nid_no_patch(context);
}

int APS5_VABI sceOpusCeltDecTerminate(std::uint32_t* context) {
    return OpusCeltDec::Terminate_nid_no_patch(context);
}

int APS5_VABI sceOpusCeltDecGetSize(int channels) {
    return OpusCeltDec::GetSize_nid_no_patch(channels);
}

int APS5_VABI sceOpusCeltDecCreateEx(std::uint32_t* context, void* state, int sampleRate, int channels) {
    return OpusCeltDec::Create_nid_no_patch(context, state, sampleRate, channels);
}

int APS5_VABI sceOpusCeltDecDecode(void* state, const std::uint8_t* packet, int packetBytes, std::int16_t* pcm, int capacityBytes) {
    return OpusCeltDec::Decode_nid_no_patch(state, packet, packetBytes, pcm, capacityBytes);
}

int APS5_VABI sceOpusCeltDecDestroy(void* state) {
    return OpusCeltDec::Destroy_nid_no_patch(state);
}

}
