#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int32_t APS5_VABI sceAt9EncClearContext(void* pAt9EncWorkArea, int32_t* pInternalResult);
int32_t APS5_VABI sceAt9EncCreateEncoder(void* pAt9EncWorkArea, void* pAt9EncParam, int32_t memSize, int32_t* pInternalResult);
int32_t APS5_VABI sceAt9EncEncode(void* pAt9EncWorkArea, const void* pInputBuf, void* pOutputBuf, uint32_t inputBufSize, uint32_t outputBufSize, uint32_t* pOutputDataByte, uint32_t* pUsedBufSize, int32_t* pInternalResult);
int32_t APS5_VABI sceAt9EncFlush(void* pAt9EncWorkArea, void* pOutputBuf, uint32_t outputBufSize, uint32_t* pOutputDataByte, int32_t* pInternalResult);
int32_t APS5_VABI sceAt9EncQueryMemSize(void* pAt9EncParam, int32_t* pMemSize, int32_t* pInternalResult);
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
    int32_t memSize = 0;
    int32_t internalResult = 0;

    Require(Throws([&] { return sceAt9EncClearContext(nullptr, &internalResult); }));
    Require(Throws([&] { return sceAt9EncCreateEncoder(nullptr, nullptr, 0, &internalResult); }));
    Require(Throws([&] { return sceAt9EncEncode(nullptr, nullptr, nullptr, 0, 0, nullptr, nullptr, &internalResult); }));
    Require(Throws([&] { return sceAt9EncFlush(nullptr, nullptr, 0, nullptr, &internalResult); }));
    Require(Throws([&] { return sceAt9EncQueryMemSize(nullptr, &memSize, &internalResult); }));

    Require(memSize == 0);
    Require(internalResult == 0);
}
