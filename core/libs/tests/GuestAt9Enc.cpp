#include "prx/libSceAt9Enc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int32_t invalidArgument = static_cast<int32_t>(0x81210001u);

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    alignas(16) std::array<unsigned char, 0x40> work{};
    work.fill(0x5A);
    std::array<unsigned char, 0x40> buffer{};
    buffer.fill(0x5A);
    const auto workBefore = work;
    const auto bufferBefore = buffer;
    void* aligned = work.data();
    void* misaligned = work.data() + 4;
    void* any = buffer.data();
    int32_t memSize = 0x5A5A5A5A;
    int32_t internalResult = 0x5A5A5A5A;
    uint32_t outputDataByte = 0x5A5A5A5Au;
    uint32_t usedBufSize = 0x5A5A5A5Au;

    for (int call = 0; call < 2; ++call) {
        Require(sceAt9EncClearContext(nullptr, &internalResult) == invalidArgument);
        Require(sceAt9EncClearContext(aligned, nullptr) == invalidArgument);
        Require(sceAt9EncClearContext(misaligned, &internalResult) == invalidArgument);

        Require(sceAt9EncCreateEncoder(misaligned, any, 0x40, &internalResult) == invalidArgument);
        Require(sceAt9EncCreateEncoder(nullptr, any, 0x40, &internalResult) == invalidArgument);
        Require(sceAt9EncCreateEncoder(aligned, nullptr, 0x40, &internalResult) == invalidArgument);
        Require(sceAt9EncCreateEncoder(aligned, any, 0x40, nullptr) == invalidArgument);

        Require(sceAt9EncEncode(misaligned, any, any, 1, 1, &outputDataByte, &usedBufSize, &internalResult) == invalidArgument);
        Require(sceAt9EncEncode(nullptr, any, any, 1, 1, &outputDataByte, &usedBufSize, &internalResult) == invalidArgument);
        Require(sceAt9EncEncode(aligned, nullptr, any, 1, 1, &outputDataByte, &usedBufSize, &internalResult) == invalidArgument);
        Require(sceAt9EncEncode(aligned, any, nullptr, 1, 1, &outputDataByte, &usedBufSize, &internalResult) == invalidArgument);
        Require(sceAt9EncEncode(aligned, any, any, 1, 1, nullptr, &usedBufSize, &internalResult) == invalidArgument);
        Require(sceAt9EncEncode(aligned, any, any, 1, 1, &outputDataByte, nullptr, &internalResult) == invalidArgument);

        Require(sceAt9EncFlush(misaligned, any, 1, &outputDataByte, &internalResult) == invalidArgument);
        Require(sceAt9EncFlush(nullptr, any, 1, &outputDataByte, &internalResult) == invalidArgument);
        Require(sceAt9EncFlush(aligned, nullptr, 1, &outputDataByte, &internalResult) == invalidArgument);
        Require(sceAt9EncFlush(aligned, any, 1, nullptr, &internalResult) == invalidArgument);
        Require(sceAt9EncFlush(aligned, any, 1, &outputDataByte, nullptr) == invalidArgument);

        Require(sceAt9EncQueryMemSize(nullptr, &memSize, &internalResult) == invalidArgument);
        Require(sceAt9EncQueryMemSize(any, nullptr, &internalResult) == invalidArgument);
        Require(sceAt9EncQueryMemSize(any, &memSize, nullptr) == invalidArgument);
    }

    Require(work == workBefore);
    Require(buffer == bufferBefore);
    Require(memSize == 0x5A5A5A5A);
    Require(internalResult == 0x5A5A5A5A);
    Require(outputDataByte == 0x5A5A5A5Au);
    Require(usedBufSize == 0x5A5A5A5Au);
}
