#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int32_t APS5_VABI sceM4aacEncClearContext(int32_t m4aacEncHandle);
int APS5_VABI sceM4aacEncClearContextEx();
int32_t APS5_VABI sceM4aacEncCreateEncoder(int32_t channels, int32_t samplingRate, int32_t bitRate, int32_t inputFormat, int32_t outputFormat, int32_t* pInternalError);
int APS5_VABI sceM4aacEncCreateEncoderEx();
int32_t APS5_VABI sceM4aacEncDeleteEncoder(int32_t m4aacEncHandle, int32_t* pInternalError);
int APS5_VABI sceM4aacEncDeleteEncoderEx();
int32_t APS5_VABI sceM4aacEncEncode(int32_t m4aacEncHandle, void* pInput, uint32_t inputByte, void* pOutput, uint32_t* pOutputByte, int32_t* pInternalError);
int APS5_VABI sceM4aacEncEncodeEx();
int APS5_VABI sceM4aacEncEncodeWithPriority();
int APS5_VABI sceM4aacEncEncodeWithPriorityEx();
int32_t APS5_VABI sceM4aacEncFlush(int32_t m4aacEncHandle, void* pOutput, uint32_t* pOutputByte, int32_t* pInternalError);
int APS5_VABI sceM4aacEncFlushEx();
int APS5_VABI sceM4aacEncFlushWithPriority();
int APS5_VABI sceM4aacEncFlushWithPriorityEx();
int APS5_VABI sceM4aacEncGetContext();
int APS5_VABI sceM4aacEncInitialize();
int APS5_VABI sceM4aacEncTerminate();
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename Call>
bool Throws(Call call) {
    try {
        static_cast<void>(call());
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    uint32_t outputByte = 0;
    int32_t internalError = 0;

    Require(Throws([&] { return sceM4aacEncClearContext(1); }));
    Require(Throws([&] { return sceM4aacEncClearContextEx(); }));
    Require(Throws([&] { return sceM4aacEncCreateEncoder(2, 48000, 128000, 0, 0, &internalError); }));
    Require(Throws([&] { return sceM4aacEncCreateEncoderEx(); }));
    Require(Throws([&] { return sceM4aacEncDeleteEncoder(1, &internalError); }));
    Require(Throws([&] { return sceM4aacEncDeleteEncoderEx(); }));
    Require(Throws([&] { return sceM4aacEncEncode(1, nullptr, 0, nullptr, &outputByte, &internalError); }));
    Require(Throws([&] { return sceM4aacEncEncodeEx(); }));
    Require(Throws([&] { return sceM4aacEncEncodeWithPriority(); }));
    Require(Throws([&] { return sceM4aacEncEncodeWithPriorityEx(); }));
    Require(Throws([&] { return sceM4aacEncFlush(1, nullptr, &outputByte, &internalError); }));
    Require(Throws([&] { return sceM4aacEncFlushEx(); }));
    Require(Throws([&] { return sceM4aacEncFlushWithPriority(); }));
    Require(Throws([&] { return sceM4aacEncFlushWithPriorityEx(); }));
    Require(Throws([&] { return sceM4aacEncGetContext(); }));
    Require(Throws([&] { return sceM4aacEncInitialize(); }));
    Require(Throws([&] { return sceM4aacEncTerminate(); }));

    Require(outputByte == 0);
    Require(internalError == 0);
}
