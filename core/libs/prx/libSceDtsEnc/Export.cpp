#include "prx/libSceDtsEnc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_DTSENC_ERROR_INVALID_HANDLE = static_cast<int>(0x80FA0001);
constexpr int SCE_DTSENC_ERROR_NOT_ENCODING = static_cast<int>(0x80FA0009);

}

extern "C" {

int APS5_VABI sceDtsEncCreateEncoder() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDtsEncDeleteEncoder() {
    return SCE_DTSENC_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceDtsEncEncode() {
    return SCE_DTSENC_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceDtsEncEncodeAsync() {
    return SCE_DTSENC_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceDtsEncFlush() {
    return SCE_DTSENC_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceDtsEncIsEncoding() {
    return 0;
}

int APS5_VABI sceDtsEncWaitEncode() {
    return SCE_DTSENC_ERROR_NOT_ENCODING;
}

}
