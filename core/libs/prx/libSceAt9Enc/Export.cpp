#include "prx/libSceAt9Enc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int32_t SCE_AT9ENC_ERROR_INVALID_ARGUMENT = static_cast<int32_t>(0x81210001u);

bool IsMisaligned(const void* pointer) {
    return (reinterpret_cast<std::uintptr_t>(pointer) & 7) != 0;
}

} // namespace

extern "C" {

int32_t APS5_VABI sceAt9EncClearContext(void *pAt9EncWorkArea, int32_t *pInternalResult) {
    if (pAt9EncWorkArea == nullptr || pInternalResult == nullptr || IsMisaligned(pAt9EncWorkArea)) {
        return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceAt9EncCreateEncoder(void *pAt9EncWorkArea, void *pAt9EncParam, int32_t memSize, int32_t *pInternalResult) {
    if (IsMisaligned(pAt9EncWorkArea) || pAt9EncWorkArea == nullptr || pAt9EncParam == nullptr || pInternalResult == nullptr) {
        return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceAt9EncEncode(void *pAt9EncWorkArea, const void *pInputBuf, void *pOutputBuf, uint32_t inputBufSize, uint32_t outputBufSize, uint32_t *pOutputDataByte, uint32_t *pUsedBufSize, int32_t *pInternalResult) {
    if (IsMisaligned(pAt9EncWorkArea) || pAt9EncWorkArea == nullptr || pInputBuf == nullptr || pOutputBuf == nullptr ||
        pOutputDataByte == nullptr || pUsedBufSize == nullptr) {
        return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceAt9EncFlush(void *pAt9EncWorkArea, void *pOutputBuf, uint32_t outputBufSize, uint32_t *pOutputDataByte, int32_t *pInternalResult) {
    if (IsMisaligned(pAt9EncWorkArea) || pAt9EncWorkArea == nullptr || pOutputBuf == nullptr || pOutputDataByte == nullptr ||
        pInternalResult == nullptr) {
        return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceAt9EncQueryMemSize(void *pAt9EncParam, int32_t *pMemSize, int32_t *pInternalResult) {
    if (pAt9EncParam == nullptr || pMemSize == nullptr || pInternalResult == nullptr) return SCE_AT9ENC_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
