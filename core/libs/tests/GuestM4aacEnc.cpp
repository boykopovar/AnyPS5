#include "prx/libSceM4aacEnc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

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
