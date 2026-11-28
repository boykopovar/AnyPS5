#include "prx/libSceAt9Enc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace {

constexpr int32_t invalidArgument = static_cast<int32_t>(0x81210001u);
constexpr int32_t invalidParameter = static_cast<int32_t>(0x81210000u);

using Parameters = std::array<uint32_t, 13>;

Parameters ValidParameters(uint32_t mode, uint32_t bitrate) {
    Parameters parameters{};
    parameters[0] = 0x34;
    parameters[1] = mode;
    parameters[2] = 0x5dc0;
    parameters[3] = bitrate;
    parameters[4] = 2;
    parameters[5] = 4;
    return parameters;
}

int32_t ExpectedMemorySize(uint32_t mode) {
    const uint32_t coefficient = mode == 1 ? 1 : mode == 2 ? 2 : mode == 4 ? 2 : mode == 6 ? 4 : 5;
    const uint32_t internalMemory = 0xf0 + coefficient * 0x220 + mode * (0xd18 + 0x1820) + 4;
    return static_cast<int32_t>(0x10060 + ((4 * mode) << 9) + internalMemory);
}

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

    const std::array<std::pair<uint32_t, uint32_t>, 5> validModes{{{1, 0x24}, {2, 0xc0}, {4, 0x48}, {6, 0xf0}, {8, 0x150}}};
    for (const auto [mode, bitrate] : validModes) {
        auto parameters = ValidParameters(mode, bitrate);
        memSize = -1;
        internalResult = -1;
        errno = 71;
        Require(sceAt9EncQueryMemSize(parameters.data(), &memSize, &internalResult) == 0);
        Require(memSize == ExpectedMemorySize(mode));
        Require(internalResult == 0);
        Require(errno == 71);
    }

    auto invalid = ValidParameters(1, 0x24);
    invalid[0] = 0x33;
    memSize = -1;
    internalResult = -1;
    Require(sceAt9EncQueryMemSize(invalid.data(), &memSize, &internalResult) == invalidParameter);
    Require(memSize == 0 && internalResult == -999);

    const std::array<std::pair<std::size_t, uint32_t>, 7> invalidFields{{
        {1, 3}, {2, 0}, {3, 0x25}, {4, 3}, {5, 2}, {6, 2}, {11, 3},
    }};
    for (const auto [field, value] : invalidFields) {
        invalid = ValidParameters(1, 0x24);
        invalid[field] = value;
        memSize = -1;
        internalResult = -1;
        Require(sceAt9EncQueryMemSize(invalid.data(), &memSize, &internalResult) == invalidParameter);
        Require(memSize == 0 && internalResult == -999);
    }
}
