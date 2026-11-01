#include "prx/libSceAudiodec.native/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int AudiodecErrorArgument = static_cast<int>(0x807F0002u);
constexpr int AudiodecErrorInvalidType = static_cast<int>(0x807F0001u);
constexpr int AudiodecErrorNotRegistered = static_cast<int>(0x8093000Au);

}

extern "C" {

int APS5_VABI sceAudiodecClearContextEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecCreateDecoderEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecDecode2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecDecode2Ex(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecDecode2WithPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecDecode2WithPriorityEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecDecodeEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecDecodeWithPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecDecodeWithPriorityEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecDeleteDecoderEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecGetAjmContext(const void* context, std::uint32_t* ajmiContext, std::uint32_t* ajmContext) {
    if (context == nullptr) return AudiodecErrorArgument;
    std::uint32_t ids[2];
    std::memcpy(ids, context, sizeof(ids));
    if (ajmiContext != nullptr) *ajmiContext = ids[0];
    if (ajmContext != nullptr) *ajmContext = ids[1];
    return 0;
}

int APS5_VABI sceAudiodecGetContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecInitialize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecRegisterCodec(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecTerminate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecUnregisterCodec(const void* context, std::uint32_t codecType) {
    if (context == nullptr) return AudiodecErrorArgument;
    switch (codecType) {
    case 1: case 2: case 3: case 4: case 6: case 10: case 11: case 12: break;
    default: return AudiodecErrorInvalidType;
    }
    if (codecType >= 11) {
        const auto bytes = static_cast<const unsigned char*>(context);
        const auto lock = reinterpret_cast<const unsigned char*>((reinterpret_cast<std::uintptr_t>(bytes) + 0xFu) & ~static_cast<std::uintptr_t>(7u));
        std::uint32_t flag;
        std::memcpy(&flag, lock + (codecType == 11 ? 8 : 0xC), sizeof(flag));
        if (flag == 0) return AudiodecErrorNotRegistered;
    }
    throw std::runtime_error("sceAudiodecUnregisterCodec: the Ajm service call for the accepted codec type");
}

}
