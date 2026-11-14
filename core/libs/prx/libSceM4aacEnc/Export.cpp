#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceM4aacEnc/include/InventoryExports.hpp"
#include "prx/libSceM4aacEnc/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int32_t ErrorEncoderState = static_cast<int32_t>(0x80A90001u);
constexpr int32_t ErrorInvalidPointer = static_cast<int32_t>(0x80A90002u);
constexpr int32_t ErrorChannels = static_cast<int32_t>(0x80A90003u);
constexpr int32_t ErrorSamplingRate = static_cast<int32_t>(0x80A90004u);
constexpr int32_t ErrorBitRate = static_cast<int32_t>(0x80A90005u);
constexpr int32_t ErrorInputFormat = static_cast<int32_t>(0x80A90006u);
constexpr int32_t ErrorOutputFormat = static_cast<int32_t>(0x80A90007u);
constexpr int32_t ErrorInputSize = static_cast<int32_t>(0x80A90008u);

constexpr int32_t NoEncoder = -1;
constexpr int32_t SupportedSamplingRate = 48000;
constexpr int32_t MinimumBitRate = 28000;
constexpr uint32_t MaximumBitRatePerChannel = 160000;
constexpr uint32_t MaximumInputBytes = 0x2000;
constexpr uint32_t OutputFormatMask = 0x00010001u;
constexpr uint32_t OutputFormatHighBit = 0x10000u;

int32_t CheckEncoderParameters(int32_t channels, int32_t samplingRate, int32_t bitRate, int32_t inputFormat,
    int32_t outputFormat, int32_t* pInternalError) {
    if (pInternalError == nullptr) {
        return ErrorInvalidPointer;
    }
    const int savedErrno = errno;
    const int32_t zero = 0;
    if (!GuestCopyOut_nid_no_patch(pInternalError, &zero, sizeof(zero))) {
        errno = savedErrno;
        return ErrorInvalidPointer;
    }
    if (channels != 1 && channels != 2) {
        return ErrorChannels;
    }
    if (samplingRate != SupportedSamplingRate) {
        return ErrorSamplingRate;
    }
    if (bitRate < MinimumBitRate || static_cast<uint32_t>(channels) * MaximumBitRatePerChannel < static_cast<uint32_t>(bitRate)) {
        return ErrorBitRate;
    }
    if (static_cast<uint32_t>(inputFormat) > 1) {
        return ErrorInputFormat;
    }
    const auto format = static_cast<uint32_t>(outputFormat);
    if ((format & ~OutputFormatMask) != 0 || (channels == 1 && format >= OutputFormatHighBit)) {
        return ErrorOutputFormat;
    }
    return 0;
}

int32_t CheckEncode(void* pInput, uint32_t inputByte, void* pOutput, uint32_t* pOutputByte, int32_t* pInternalError) {
    if (pInternalError == nullptr) {
        return ErrorInvalidPointer;
    }
    const int savedErrno = errno;
    const int32_t zero = 0;
    if (!GuestCopyOut_nid_no_patch(pInternalError, &zero, sizeof(zero))) {
        errno = savedErrno;
        return ErrorInvalidPointer;
    }
    if (pInput == nullptr || pOutput == nullptr || pOutputByte == nullptr) {
        return ErrorInvalidPointer;
    }
    uint32_t prevOutputByte = 0;
    if (!GuestCopyIn_nid_no_patch(&prevOutputByte, pOutputByte, sizeof(prevOutputByte)) ||
        !GuestCopyOut_nid_no_patch(pOutputByte, &prevOutputByte, sizeof(prevOutputByte))) {
        errno = savedErrno;
        return ErrorInvalidPointer;
    }
    if (inputByte > MaximumInputBytes) {
        return ErrorInputSize;
    }
    return 0;
}

int32_t CheckFlush(void* pOutput, uint32_t* pOutputByte, int32_t* pInternalError) {
    if (pInternalError == nullptr) {
        return ErrorInvalidPointer;
    }
    const int savedErrno = errno;
    const int32_t zero = 0;
    if (!GuestCopyOut_nid_no_patch(pInternalError, &zero, sizeof(zero))) {
        errno = savedErrno;
        return ErrorInvalidPointer;
    }
    if (pOutput == nullptr || pOutputByte == nullptr) {
        return ErrorInvalidPointer;
    }
    uint32_t prevOutputByte = 0;
    if (!GuestCopyIn_nid_no_patch(&prevOutputByte, pOutputByte, sizeof(prevOutputByte)) ||
        !GuestCopyOut_nid_no_patch(pOutputByte, &prevOutputByte, sizeof(prevOutputByte))) {
        errno = savedErrno;
        return ErrorInvalidPointer;
    }
    return 0;
}

}

extern "C" {

int32_t APS5_VABI sceM4aacEncClearContext(int32_t m4aacEncHandle) {
    (void)m4aacEncHandle;
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncClearContextEx(int32_t* context) {
    if (context == nullptr) {
        return ErrorInvalidPointer;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncCreateEncoder(int32_t channels, int32_t samplingRate, int32_t bitRate, int32_t inputFormat, int32_t outputFormat, int32_t *pInternalError) {
    const int32_t status = CheckEncoderParameters(channels, samplingRate, bitRate, inputFormat, outputFormat, pInternalError);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncCreateEncoderEx(int32_t* context, int32_t channels, int32_t samplingRate, int32_t bitRate, int32_t inputFormat, int32_t outputFormat, int32_t* pInternalError) {
    if (context == nullptr) {
        return ErrorInvalidPointer;
    }
    int32_t localContext[2]{};
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(localContext, context, sizeof(localContext))) {
        errno = savedErrno;
        return ErrorInvalidPointer;
    }
    if (localContext[1] != NoEncoder) {
        return ErrorEncoderState;
    }
    const int32_t status = CheckEncoderParameters(channels, samplingRate, bitRate, inputFormat, outputFormat, pInternalError);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncDeleteEncoder(int32_t m4aacEncHandle, int32_t *pInternalError) {
    (void)m4aacEncHandle;
    if (pInternalError == nullptr) {
        return ErrorInvalidPointer;
    }
    const int savedErrno = errno;
    const int32_t zero = 0;
    if (!GuestCopyOut_nid_no_patch(pInternalError, &zero, sizeof(zero))) {
        errno = savedErrno;
        return ErrorInvalidPointer;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncDeleteEncoderEx(int32_t* context, int32_t* pInternalError) {
    if (context == nullptr) {
        return ErrorInvalidPointer;
    }
    int32_t localContext[2]{};
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(localContext, context, sizeof(localContext))) {
        errno = savedErrno;
        return ErrorInvalidPointer;
    }
    if (localContext[1] == NoEncoder) {
        return ErrorEncoderState;
    }
    if (pInternalError == nullptr) {
        return ErrorInvalidPointer;
    }
    const int32_t zero = 0;
    if (!GuestCopyOut_nid_no_patch(pInternalError, &zero, sizeof(zero))) {
        errno = savedErrno;
        return ErrorInvalidPointer;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncEncode(int32_t m4aacEncHandle, void *pInput, uint32_t inputByte, void *pOutput, uint32_t *pOutputByte, int32_t *pInternalError) {
    (void)m4aacEncHandle;
    const int32_t status = CheckEncode(pInput, inputByte, pOutput, pOutputByte, pInternalError);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncEncodeEx(int32_t* context, void* pInput, uint32_t inputByte, void* pOutput, uint32_t* pOutputByte, int32_t* pInternalError) {
    if (context == nullptr) {
        return ErrorInvalidPointer;
    }
    const int32_t status = CheckEncode(pInput, inputByte, pOutput, pOutputByte, pInternalError);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncEncodeWithPriority(int32_t m4aacEncHandle, void* pInput, uint32_t inputByte, void* pOutput, uint32_t* pOutputByte, int32_t* pInternalError, uint32_t priority) {
    (void)m4aacEncHandle;
    (void)priority;
    const int32_t status = CheckEncode(pInput, inputByte, pOutput, pOutputByte, pInternalError);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncEncodeWithPriorityEx(int32_t* context, void* pInput, uint32_t inputByte, void* pOutput, uint32_t* pOutputByte, int32_t* pInternalError, uint32_t priority) {
    (void)priority;
    if (context == nullptr) {
        return ErrorInvalidPointer;
    }
    const int32_t status = CheckEncode(pInput, inputByte, pOutput, pOutputByte, pInternalError);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncFlush(int32_t m4aacEncHandle, void *pOutput, uint32_t *pOutputByte, int32_t *pInternalError) {
    (void)m4aacEncHandle;
    const int32_t status = CheckFlush(pOutput, pOutputByte, pInternalError);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncFlushEx(int32_t* context, void* pOutput, uint32_t* pOutputByte, int32_t* pInternalError) {
    if (context == nullptr) {
        return ErrorInvalidPointer;
    }
    const int32_t status = CheckFlush(pOutput, pOutputByte, pInternalError);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncFlushWithPriority(int32_t m4aacEncHandle, void* pOutput, uint32_t* pOutputByte, int32_t* pInternalError, uint32_t priority) {
    (void)m4aacEncHandle;
    (void)priority;
    const int32_t status = CheckFlush(pOutput, pOutputByte, pInternalError);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncFlushWithPriorityEx(int32_t* context, void* pOutput, uint32_t* pOutputByte, int32_t* pInternalError, uint32_t priority) {
    (void)priority;
    if (context == nullptr) {
        return ErrorInvalidPointer;
    }
    const int32_t status = CheckFlush(pOutput, pOutputByte, pInternalError);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncGetContext(int32_t* context) {
    if (context == nullptr) {
        return ErrorInvalidPointer;
    }
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceM4aacEncInitialize(int32_t* context) {
    if (context == nullptr) {
        return ErrorInvalidPointer;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceM4aacEncTerminate(int32_t* context) {
    (void)context;
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int32_t APS5_VABI libSceM4aacEncUnknown_dyjd6gEaAXw_nid_no_patch(const int32_t* context, int32_t* value) {
    if (context == nullptr) {
        return ErrorInvalidPointer;
    }
    int32_t localContext = 0;
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&localContext, context, sizeof(localContext))) {
        errno = savedErrno;
        return ErrorInvalidPointer;
    }
    if (value != nullptr) {
        if (!GuestCopyOut_nid_no_patch(value, &localContext, sizeof(localContext))) {
            errno = savedErrno;
            return ErrorInvalidPointer;
        }
    }
    return 0;
}
APS5_EXPORT("dyjd6gEaAXw", libSceM4aacEncUnknown_dyjd6gEaAXw_nid_no_patch);

}
