#include "prx/libSceVdecwrap/libSceVdecwrap.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

static void Put32(unsigned char* block, std::size_t offset, std::uint32_t value) {
    std::memcpy(block + offset, &value, sizeof(value));
}

static void Put64(unsigned char* block, std::size_t offset, std::uint64_t value) {
    std::memcpy(block + offset, &value, sizeof(value));
}

constexpr int StructSize = static_cast<int>(0x810c0002u);
constexpr int CodecType = static_cast<int>(0x810c0003u);
constexpr int ReservedField = static_cast<int>(0x810c0004u);
constexpr int InputMode = static_cast<int>(0x810c0005u);
constexpr int NullArgument = static_cast<int>(0x810c0007u);
constexpr int DirectMemory = static_cast<int>(0x810c0008u);
constexpr int InputBuffer = static_cast<int>(0x810c0009u);
constexpr int MapAddress = static_cast<int>(0x810c000au);
constexpr int ResetParam = static_cast<int>(0x810c0014u);

static void CheckConfigCalls(std::uint64_t infoSize, int (*call)(const void*, void*, void*)) {
    std::array<unsigned char, 0x40> config{};
    std::array<unsigned char, 0x40> info{};
    unsigned char decoder[8]{};
    Put64(config.data(), 0, 0x28);
    Put64(info.data(), 0, infoSize);
    Put64(config.data(), 0, 0x27);
    Put32(config.data(), 8, 0x7fffffffu);
    Require(call(config.data(), info.data(), decoder) == StructSize, "A configuration size other than 0x28 must be rejected first");
    Put64(config.data(), 0, 0x28);
    Put64(info.data(), 0, infoSize + 8);
    Require(call(config.data(), info.data(), decoder) == StructSize, "A wrong output size must be rejected");
    Put64(info.data(), 0, infoSize);
    for (const std::uint32_t codec : {0u, 3u, 0x1456u, 0x1458u, 0x1a4du, 0x1a4fu, 0xffffffffu}) {
        Put32(config.data(), 8, codec);
        Put32(config.data(), 0x24, 1);
        Require(call(config.data(), info.data(), decoder) == CodecType, "An unknown codec type must be rejected before the reserved field");
    }
    for (const std::uint32_t codec : {1u, 2u, 0x1457u, 0x1a4eu}) {
        Put32(config.data(), 8, codec);
        for (const std::uint32_t reserved : {1u, 0x80000000u, 0xffffffffu}) {
            Put32(config.data(), 0x24, reserved);
            Require(call(config.data(), info.data(), decoder) == ReservedField, "A known codec type with a non-zero reserved field must be rejected");
        }
    }
}

int main() {
    unsigned char decoder[0x40]{};
    unsigned char block[0x60]{};
    std::array<unsigned char, 0x40> config{};
    Put64(config.data(), 0, 0x28);
    Put32(config.data(), 8, 1);

    errno = 7;
    Require(sceVdecwrapCreateDecoder(nullptr, block, decoder) == NullArgument, "CreateDecoder: null configuration");
    Require(sceVdecwrapCreateDecoder(config.data(), nullptr, decoder) == NullArgument, "CreateDecoder: null resource");
    Require(sceVdecwrapCreateDecoder(config.data(), block, nullptr) == NullArgument, "CreateDecoder: null decoder");
    Require(sceVdecwrapQueryDecoderMemoryInfo(nullptr, block) == NullArgument, "QueryDecoderMemoryInfo: null configuration");
    Require(sceVdecwrapQueryDecoderMemoryInfo(config.data(), nullptr) == NullArgument, "QueryDecoderMemoryInfo: null info");
    Require(sceVdecwrapQueryFrameBufferInfo(nullptr, block) == NullArgument, "QueryFrameBufferInfo: null configuration");
    Require(sceVdecwrapQueryFrameBufferInfo(config.data(), nullptr) == NullArgument, "QueryFrameBufferInfo: null info");
    Require(sceVdecwrapDeleteDecoder(nullptr) == NullArgument, "DeleteDecoder: null decoder");
    Require(sceVdecwrapFlushDecodeOutput(nullptr, block) == NullArgument, "FlushDecodeOutput: null decoder");
    Require(sceVdecwrapFlushDecodeOutput(decoder, nullptr) == NullArgument, "FlushDecodeOutput: null output");
    Require(sceVdecwrapSyncDecode(nullptr, block) == NullArgument, "SyncDecode: null decoder");
    Require(sceVdecwrapSyncDecode(decoder, nullptr) == NullArgument, "SyncDecode: null second argument");
    Require(sceVdecwrapGetDecodeOutput(nullptr, block) == NullArgument, "GetDecodeOutput: null decoder");
    Require(sceVdecwrapGetDecodeOutput(decoder, nullptr) == NullArgument, "GetDecodeOutput: null output");
    Require(sceVdecwrapMapDirectMemory(nullptr, block) == NullArgument, "MapDirectMemory: null decoder");
    Require(sceVdecwrapMapDirectMemory(decoder, nullptr) == NullArgument, "MapDirectMemory: null memory");
    Require(sceVdecwrapResetDecoder(nullptr, block) == NullArgument, "ResetDecoder: null decoder");
    Require(sceVdecwrapResetDecoder(decoder, nullptr) == NullArgument, "ResetDecoder: null parameter");
    Require(sceVdecwrapSetDecodeInput(nullptr, block, block) == NullArgument, "SetDecodeInput: null decoder");
    Require(sceVdecwrapSetDecodeInput(decoder, nullptr, block) == NullArgument, "SetDecodeInput: null input");
    Require(sceVdecwrapSetDecodeInput(decoder, block, nullptr) == NullArgument, "SetDecodeInput: null output");
    Require(errno == 7, "The argument checks must not change errno");

    Require(sceVdecwrapMapMemory(nullptr, 0, nullptr) == MapAddress, "MapMemory: a null third argument is checked first");
    Require(sceVdecwrapMapMemory(decoder, 0x1000, nullptr) == MapAddress, "MapMemory: null third argument");
    Require(sceVdecwrapMapMemory(nullptr, 0x1000, block) == NullArgument, "MapMemory: null decoder");
    Require(sceVdecwrapMapMemory(decoder, 0, block) == NullArgument, "MapMemory: zero second argument");

    CheckConfigCalls(0x38, [](const void* c, void* i, void* d) { return sceVdecwrapCreateDecoder(c, i, d); });
    CheckConfigCalls(0x38, [](const void* c, void* i, void*) { return sceVdecwrapQueryDecoderMemoryInfo(c, i); });
    CheckConfigCalls(0x18, [](const void* c, void* i, void*) { return sceVdecwrapQueryFrameBufferInfo(c, i); });

    unsigned char output[0x40]{};
    for (const std::uint64_t size : {0ull, 0x37ull, 0x39ull, 0x18ull}) {
        Put64(output, 0, size);
        Require(sceVdecwrapGetDecodeOutput(decoder, output) == StructSize, "GetDecodeOutput: an output size other than 0x38 must be rejected");
    }

    unsigned char memory[0x20]{};
    Put64(memory, 0, 0x28);
    Require(sceVdecwrapMapDirectMemory(decoder, memory) == StructSize, "MapDirectMemory: a size other than 0x20 must be rejected");
    Put64(memory, 0, 0x20);
    Put64(memory, 0x10, 0x1000);
    Require(sceVdecwrapMapDirectMemory(decoder, memory) == DirectMemory, "MapDirectMemory: a zero first field must be rejected");
    Put64(memory, 0x8, 0x1000);
    Put64(memory, 0x10, 0);
    Require(sceVdecwrapMapDirectMemory(decoder, memory) == DirectMemory, "MapDirectMemory: a zero second field must be rejected");

    unsigned char param[0x10]{};
    Put64(param, 0, 0x18);
    Put32(param, 8, 1);
    Require(sceVdecwrapResetDecoder(decoder, param) == StructSize, "ResetDecoder: a size other than 0x10 must be rejected first");
    Put64(param, 0, 0x10);
    Put32(param, 0xc, 0);
    Require(sceVdecwrapResetDecoder(decoder, param) == ResetParam, "ResetDecoder: a non-zero field at 8 must be rejected");
    Put32(param, 8, 0);
    for (const std::uint32_t mode : {3u, 4u, 0xffffffffu}) {
        Put32(param, 0xc, mode);
        Require(sceVdecwrapResetDecoder(decoder, param) == ResetParam, "ResetDecoder: a mode of 3 or more must be rejected");
    }

    unsigned char input[0x48]{};
    unsigned char inputOutput[0x10]{};
    Put64(input, 0, 0x40);
    Put64(inputOutput, 0, 0x10);
    Require(sceVdecwrapSetDecodeInput(decoder, input, inputOutput) == StructSize, "SetDecodeInput: an input size other than 0x48 must be rejected");
    Put64(input, 0, 0x48);
    Put64(inputOutput, 0, 0x18);
    Require(sceVdecwrapSetDecodeInput(decoder, input, inputOutput) == StructSize, "SetDecodeInput: an output size other than 0x10 must be rejected");
    Put64(inputOutput, 0, 0x10);
    Put32(input, 0x28, 3);
    Put64(input, 0x10, 0x1000);
    Require(sceVdecwrapSetDecodeInput(decoder, input, inputOutput) == InputBuffer, "SetDecodeInput: a zero field at 8 must be rejected before the mode");
    Put64(input, 0x8, 0x1000);
    Put64(input, 0x10, 0);
    Require(sceVdecwrapSetDecodeInput(decoder, input, inputOutput) == InputBuffer, "SetDecodeInput: a zero field at 0x10 must be rejected");
    Put64(input, 0x10, 0x1000);
    for (const std::uint32_t mode : {3u, 7u, 0xffffffffu}) {
        Put32(input, 0x28, mode);
        Require(sceVdecwrapSetDecodeInput(decoder, input, inputOutput) == InputMode, "SetDecodeInput: a mode of 3 or more must be rejected");
    }
    Require(errno == 7, "The field checks must not change errno");
}
