#include "prx/libSceAc3Enc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_AC3ENC_ERROR_INVALID_HANDLE = static_cast<int>(0x80E00001);
constexpr int SCE_AC3ENC_ERROR_NOT_ENCODING = static_cast<int>(0x80E00009);

}

extern "C" {

int APS5_VABI sceAc3EncCreateEncoder() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAc3EncDeleteEncoder() {
    return SCE_AC3ENC_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceAc3EncEncode() {
    return SCE_AC3ENC_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceAc3EncEncodeAsync() {
    return SCE_AC3ENC_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceAc3EncFlush() {
    return SCE_AC3ENC_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceAc3EncIsEncoding() {
    return 0;
}

int APS5_VABI sceAc3EncWaitEncode() {
    return SCE_AC3ENC_ERROR_NOT_ENCODING;
}

}
