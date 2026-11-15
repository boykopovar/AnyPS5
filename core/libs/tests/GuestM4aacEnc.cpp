#include "prx/libSceM4aacEnc/libSceM4aacEnc.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int32_t ErrorEncoderState = static_cast<int32_t>(0x80A90001u);
constexpr int32_t ErrorInvalidPointer = static_cast<int32_t>(0x80A90002u);
constexpr int32_t ErrorChannels = static_cast<int32_t>(0x80A90003u);
constexpr int32_t ErrorSamplingRate = static_cast<int32_t>(0x80A90004u);
constexpr int32_t ErrorBitRate = static_cast<int32_t>(0x80A90005u);
constexpr int32_t ErrorInputFormat = static_cast<int32_t>(0x80A90006u);
constexpr int32_t ErrorOutputFormat = static_cast<int32_t>(0x80A90007u);
constexpr int32_t ErrorInputSize = static_cast<int32_t>(0x80A90008u);
constexpr int32_t Stale = 0x5A5A5A5A;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "M4AACENC: %s\n", message);
    std::abort();
}

void CheckCreate() {
    int32_t error = Stale;
    Require(sceM4aacEncCreateEncoder(2, 48000, 128000, 0, 0, nullptr) == ErrorInvalidPointer, "Create: null error pointer");
    for (const int32_t channels : {0, 3, -1, 6}) {
        error = Stale;
        Require(sceM4aacEncCreateEncoder(channels, 48000, 128000, 0, 0, &error) == ErrorChannels, "Create: channels");
        Require(error == 0, "Create: error cleared before the checks");
    }
    for (const int32_t rate : {44100, 0, 96000, 47999}) {
        Require(sceM4aacEncCreateEncoder(2, rate, 128000, 0, 0, &error) == ErrorSamplingRate, "Create: sampling rate");
    }
    Require(sceM4aacEncCreateEncoder(2, 48000, 27999, 0, 0, &error) == ErrorBitRate, "Create: bit rate below 28000");
    Require(sceM4aacEncCreateEncoder(2, 48000, -1, 0, 0, &error) == ErrorBitRate, "Create: negative bit rate");
    Require(sceM4aacEncCreateEncoder(1, 48000, 160001, 0, 0, &error) == ErrorBitRate, "Create: mono above 160000");
    Require(sceM4aacEncCreateEncoder(2, 48000, 320001, 0, 0, &error) == ErrorBitRate, "Create: stereo above 320000");
    for (const int32_t format : {2, -1, 0x100}) {
        Require(sceM4aacEncCreateEncoder(2, 48000, 128000, format, 0, &error) == ErrorInputFormat, "Create: input format");
    }
    for (const int32_t format : {2, 4, 0x20000, -1, 0x10002}) {
        Require(sceM4aacEncCreateEncoder(2, 48000, 128000, 1, format, &error) == ErrorOutputFormat, "Create: output format");
    }
    Require(sceM4aacEncCreateEncoder(1, 48000, 64000, 0, 0x10000, &error) == ErrorOutputFormat, "Create: mono with 0x10000");
    Require(sceM4aacEncCreateEncoder(1, 48000, 64000, 0, 0x10001, &error) == ErrorOutputFormat, "Create: mono with 0x10001");

    int32_t context[2] = {7, -1};
    Require(sceM4aacEncCreateEncoderEx(nullptr, 2, 48000, 128000, 0, 0, &error) == ErrorInvalidPointer, "CreateEx: null context");
    Require(sceM4aacEncCreateEncoderEx(context, 2, 48000, 128000, 0, 0, nullptr) == ErrorInvalidPointer, "CreateEx: null error");
    Require(sceM4aacEncCreateEncoderEx(context, 3, 48000, 128000, 0, 0, &error) == ErrorChannels, "CreateEx: channels");
    Require(sceM4aacEncCreateEncoderEx(context, 2, 48000, 128000, 0, 0x10002, &error) == ErrorOutputFormat, "CreateEx: format");
    Require(context[0] == 7 && context[1] == -1, "CreateEx: a failed check leaves the context");
    context[1] = 0;
    error = Stale;
    Require(sceM4aacEncCreateEncoderEx(context, 3, 48000, 128000, 0, 0, &error) == ErrorEncoderState, "CreateEx: encoder present");
    Require(error == Stale, "CreateEx: the encoder check comes before the error pointer is written");
}

void CheckEncodeAndFlush() {
    uint8_t input[16]{};
    uint8_t output[16]{};
    uint32_t outputBytes = 0;
    int32_t error = Stale;
    int32_t context[2] = {7, 3};

    Require(sceM4aacEncEncode(1, input, 16, output, &outputBytes, nullptr) == ErrorInvalidPointer, "Encode: null error");
    Require(sceM4aacEncEncode(1, nullptr, 16, output, &outputBytes, &error) == ErrorInvalidPointer, "Encode: null input");
    Require(error == 0, "Encode: error cleared");
    Require(sceM4aacEncEncode(1, input, 16, nullptr, &outputBytes, &error) == ErrorInvalidPointer, "Encode: null output");
    Require(sceM4aacEncEncode(1, input, 16, output, nullptr, &error) == ErrorInvalidPointer, "Encode: null size");
    Require(sceM4aacEncEncode(1, input, 0x2001, output, &outputBytes, &error) == ErrorInputSize, "Encode: input size");
    Require(sceM4aacEncEncode(1, input, 0xFFFFFFFFu, output, &outputBytes, &error) == ErrorInputSize, "Encode: input size max");
    Require(outputBytes == 0, "Encode: a failed check leaves the output size");
    Require(sceM4aacEncEncodeWithPriority(1, nullptr, 16, output, &outputBytes, &error, 5) == ErrorInvalidPointer, "EncodeWithPriority");
    Require(sceM4aacEncEncodeWithPriority(1, input, 0x2001, output, &outputBytes, &error, 5) == ErrorInputSize, "EncodeWithPriority: size");
    Require(sceM4aacEncEncodeEx(nullptr, input, 16, output, &outputBytes, &error) == ErrorInvalidPointer, "EncodeEx: null context");
    Require(sceM4aacEncEncodeEx(context, input, 16, output, &outputBytes, nullptr) == ErrorInvalidPointer, "EncodeEx: null error");
    Require(sceM4aacEncEncodeEx(context, input, 0x2001, output, &outputBytes, &error) == ErrorInputSize, "EncodeEx: size");
    Require(sceM4aacEncEncodeWithPriorityEx(nullptr, input, 16, output, &outputBytes, &error, 5) == ErrorInvalidPointer,
        "EncodeWithPriorityEx: null context");
    Require(sceM4aacEncEncodeWithPriorityEx(context, input, 16, nullptr, &outputBytes, &error, 5) == ErrorInvalidPointer,
        "EncodeWithPriorityEx: null output");

    error = Stale;
    Require(sceM4aacEncFlush(1, output, &outputBytes, nullptr) == ErrorInvalidPointer, "Flush: null error");
    Require(sceM4aacEncFlush(1, nullptr, &outputBytes, &error) == ErrorInvalidPointer, "Flush: null output");
    Require(error == 0, "Flush: error cleared");
    Require(sceM4aacEncFlush(1, output, nullptr, &error) == ErrorInvalidPointer, "Flush: null size");
    Require(sceM4aacEncFlushWithPriority(1, nullptr, &outputBytes, &error, 5) == ErrorInvalidPointer, "FlushWithPriority");
    Require(sceM4aacEncFlushEx(nullptr, output, &outputBytes, &error) == ErrorInvalidPointer, "FlushEx: null context");
    Require(sceM4aacEncFlushEx(context, output, nullptr, &error) == ErrorInvalidPointer, "FlushEx: null size");
    Require(sceM4aacEncFlushWithPriorityEx(nullptr, output, &outputBytes, &error, 5) == ErrorInvalidPointer,
        "FlushWithPriorityEx: null context");
    Require(sceM4aacEncFlushWithPriorityEx(context, nullptr, &outputBytes, &error, 5) == ErrorInvalidPointer,
        "FlushWithPriorityEx: null output");
    Require(outputBytes == 0, "Flush: a failed check leaves the output size");
}

void CheckContextCalls() {
    int32_t error = Stale;
    int32_t context[2] = {7, -1};
    Require(sceM4aacEncDeleteEncoder(1, nullptr) == ErrorInvalidPointer, "Delete: null error");
    Require(sceM4aacEncDeleteEncoderEx(nullptr, &error) == ErrorInvalidPointer, "DeleteEx: null context");
    Require(sceM4aacEncDeleteEncoderEx(context, &error) == ErrorEncoderState, "DeleteEx: no encoder");
    Require(sceM4aacEncDeleteEncoderEx(context, nullptr) == ErrorEncoderState, "DeleteEx: no encoder before the error check");
    context[1] = 3;
    Require(sceM4aacEncDeleteEncoderEx(context, nullptr) == ErrorInvalidPointer, "DeleteEx: null error");
    Require(error == Stale, "DeleteEx: failed checks leave the error");

    Require(sceM4aacEncClearContextEx(nullptr) == ErrorInvalidPointer, "ClearContextEx");
    Require(sceM4aacEncGetContext(nullptr) == ErrorInvalidPointer, "GetContext");
    Require(sceM4aacEncInitialize(nullptr) == ErrorInvalidPointer, "Initialize");

    int32_t value = Stale;
    Require(libSceM4aacEncUnknown_dyjd6gEaAXw_nid_no_patch(nullptr, &value) == ErrorInvalidPointer, "dyjd6gEaAXw: null context");
    Require(value == Stale, "dyjd6gEaAXw: null context writes nothing");
    Require(libSceM4aacEncUnknown_dyjd6gEaAXw_nid_no_patch(context, nullptr) == 0, "dyjd6gEaAXw: null output");
    for (const int32_t id : {7, 0, -1, 0x7FFFFFFF}) {
        context[0] = id;
        Require(libSceM4aacEncUnknown_dyjd6gEaAXw_nid_no_patch(context, &value) == 0 && value == id, "dyjd6gEaAXw: value");
    }
    errno = 71;
    Require(sceM4aacEncCreateEncoder(2, 48000, 128000, 0, 0, reinterpret_cast<int32_t*>(0x10)) == ErrorInvalidPointer, "Create: unmapped error pointer");
    Require(errno == 71, "Create: errno preserved");
    errno = 71;
    Require(sceM4aacEncCreateEncoderEx(reinterpret_cast<int32_t*>(0x10), 2, 48000, 128000, 0, 0, &error) == ErrorInvalidPointer, "CreateEx: unmapped context");
    Require(errno == 71, "CreateEx: errno preserved");
    errno = 71;
    Require(sceM4aacEncDeleteEncoder(1, reinterpret_cast<int32_t*>(0x10)) == ErrorInvalidPointer, "Delete: unmapped error pointer");
    Require(errno == 71, "Delete: errno preserved");
    errno = 71;
    Require(sceM4aacEncDeleteEncoderEx(reinterpret_cast<int32_t*>(0x10), &error) == ErrorInvalidPointer, "DeleteEx: unmapped context");
    Require(errno == 71, "DeleteEx: errno preserved");
    errno = 71;
    Require(libSceM4aacEncUnknown_dyjd6gEaAXw_nid_no_patch(reinterpret_cast<const int32_t*>(0x10), &value) == ErrorInvalidPointer, "dyjd6gEaAXw: unmapped context");
    Require(errno == 71, "dyjd6gEaAXw: errno preserved on unmapped context");
    errno = 71;
    Require(libSceM4aacEncUnknown_dyjd6gEaAXw_nid_no_patch(context, reinterpret_cast<int32_t*>(0x10)) == ErrorInvalidPointer, "dyjd6gEaAXw: unmapped value");
    Require(errno == 71, "dyjd6gEaAXw: errno preserved on unmapped value");
}

}

int main() {
    CheckCreate();
    CheckEncodeAndFlush();
    CheckContextCalls();
    std::puts("M4aacEnc tests passed");
    return 0;
}
