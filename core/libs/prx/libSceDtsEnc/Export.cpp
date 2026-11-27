#include "prx/libSceDtsEnc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <stdexcept>

namespace {

constexpr int SCE_DTSENC_ERROR_INVALID_HANDLE = static_cast<int>(0x80FA0001);
constexpr int SCE_DTSENC_ERROR_NOT_ENCODING = static_cast<int>(0x80FA0009);
constexpr int32_t SCE_DTSENC_ERROR_INVALID_POINTER = static_cast<int32_t>(0x80FA0002u);

std::uint32_t ReadParam(const void* param, std::size_t offset) {
    std::uint32_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, static_cast<const std::byte*>(param) + offset, sizeof(value));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument("sceDtsEncCreateEncoder: unreadable parameters");
    return value;
}

}

extern "C" {

int32_t APS5_VABI sceDtsEncCreateEncoder(int32_t* handle, const void* param, int32_t* status) {
    (void)status;
    if (handle == nullptr || param == nullptr) {
        return SCE_DTSENC_ERROR_INVALID_POINTER;
    }
    if (ReadParam(param, 0x04) != 6) {
        return static_cast<int32_t>(0x80FA0003u);
    }
    if (ReadParam(param, 0x08) != 48000) {
        return static_cast<int32_t>(0x80FA0004u);
    }
    const std::uint32_t mode = ReadParam(param, 0x0c);
    if (mode != 0 && mode != 1) {
        return static_cast<int32_t>(0x80FA0006u);
    }
    if (ReadParam(param, 0x10) != 0) {
        return static_cast<int32_t>(0x80FA0007u);
    }
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
