#include "prx/libSceVdecCore.native/libSceVdecCore.native.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int InvalidArgument = static_cast<int>(0x80C00001u);
constexpr int UnsupportedCodec = static_cast<int>(0x80C00002u);
constexpr int InvalidParam = static_cast<int>(0x80C00007u);
constexpr int MemoryLimit = static_cast<int>(0x80C00015u);

using CreateFn = int (APS5_VABI *)(std::int32_t, const void*, const void*, void**);

void TestCreateDecoder(CreateFn fn) {
    void* sentinelOut = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xA5A5A5A5A5A5A5A5ull));
    std::array<std::uint8_t, 64> config{};
    std::array<std::uint8_t, 64> memory{};

    Require(fn(0, config.data(), memory.data(), nullptr) == InvalidArgument);

    void* outDecoder = sentinelOut;
    Require(fn(0, nullptr, memory.data(), &outDecoder) == InvalidArgument);
    Require(outDecoder == sentinelOut);

    errno = 71;
    outDecoder = sentinelOut;
    Require(fn(0, reinterpret_cast<const void*>(0x10), memory.data(), &outDecoder) == InvalidArgument);
    Require(errno == 71 && outDecoder == sentinelOut);

    for (const std::uint32_t badCodec : {std::uint32_t{12u}, std::uint32_t{0xffffffffu}}) {
        std::memcpy(config.data(), &badCodec, sizeof(badCodec));
        outDecoder = sentinelOut;
        Require(fn(0, config.data(), memory.data(), &outDecoder) == UnsupportedCodec);
        Require(outDecoder == sentinelOut);
    }

    const std::uint32_t validCodec0 = 0;
    std::memcpy(config.data(), &validCodec0, sizeof(validCodec0));
    outDecoder = sentinelOut;
    Require(fn(0, config.data(), nullptr, &outDecoder) == InvalidArgument);
    Require(outDecoder == sentinelOut);

    errno = 71;
    outDecoder = sentinelOut;
    Require(fn(0, config.data(), reinterpret_cast<const void*>(0x10), &outDecoder) == InvalidArgument);
    Require(errno == 71 && outDecoder == sentinelOut);

    const std::uint64_t nonzero = 0x1000ull;
    for (const std::uint32_t codec : {0u, 4u, 8u, 11u}) {
        std::memcpy(config.data(), &codec, sizeof(codec));
        memory.fill(0);
        Require(fn(0, config.data(), memory.data(), &outDecoder) == InvalidParam);
        memory.fill(0);
        std::memcpy(memory.data() + 0x8, &nonzero, sizeof(nonzero));
        Require(fn(0, config.data(), memory.data(), &outDecoder) == InvalidParam);
        memory.fill(0);
        std::memcpy(memory.data() + 0x28, &nonzero, sizeof(nonzero));
        Require(fn(0, config.data(), memory.data(), &outDecoder) == InvalidParam);
        Require(outDecoder == sentinelOut);
    }

    for (const std::uint32_t codec : {2u, 3u, 9u}) {
        std::memcpy(config.data(), &codec, sizeof(codec));
        memory.fill(0);
        std::memcpy(memory.data() + 0x8, &nonzero, sizeof(nonzero));
        std::memcpy(memory.data() + 0x28, &nonzero, sizeof(nonzero));
        Require(fn(0, config.data(), memory.data(), &outDecoder) == InvalidParam);
        memory.fill(0);
        std::memcpy(memory.data() + 0x18, &nonzero, sizeof(nonzero));
        std::memcpy(memory.data() + 0x28, &nonzero, sizeof(nonzero));
        Require(fn(0, config.data(), memory.data(), &outDecoder) == InvalidParam);
        Require(outDecoder == sentinelOut);
    }

    const std::uint32_t codec1 = 1u;
    std::memcpy(config.data(), &codec1, sizeof(codec1));
    memory.fill(0);
    std::memcpy(memory.data() + 0x8, &nonzero, sizeof(nonzero));
    Require(fn(0, config.data(), memory.data(), &outDecoder) == InvalidParam);
    memory.fill(0);
    std::memcpy(memory.data() + 0x18, &nonzero, sizeof(nonzero));
    Require(fn(0, config.data(), memory.data(), &outDecoder) == InvalidParam);
    Require(outDecoder == sentinelOut);
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);

    TestCreateDecoder(sceVdecCoreCreateDecoderBid);
    TestCreateDecoder(sceVdecCoreCreateDecoderSvp);

    std::array<std::uint8_t, 0x300> fakeDecoder{};
    const std::uint8_t dummyTarget = 0;

    Require(sceVdecCoreMapMemory(nullptr, &dummyTarget, 64) == InvalidArgument);
    errno = 71;
    Require(sceVdecCoreMapMemory(reinterpret_cast<void*>(0x10), &dummyTarget, 64) == InvalidArgument);
    Require(errno == 71);
    fakeDecoder[0x138] = 16;
    Require(sceVdecCoreMapMemory(fakeDecoder.data(), &dummyTarget, 64) == MemoryLimit);
    fakeDecoder[0x138] = 15;
    Require(sceVdecCoreMapMemory(fakeDecoder.data(), nullptr, 64) == InvalidArgument);

    std::array<std::uint8_t, 64> pictureInfo{};
    std::array<std::uint8_t, 64> auInfo{};
    Require(sceVdecCoreSetDecodeInputSvp(nullptr, pictureInfo.data(), auInfo.data(), nullptr) == InvalidArgument);
    Require(sceVdecCoreSetDecodeInputSvp(fakeDecoder.data(), pictureInfo.data(), nullptr, nullptr) == InvalidArgument);
    errno = 71;
    Require(sceVdecCoreSetDecodeInputSvp(fakeDecoder.data(), pictureInfo.data(), reinterpret_cast<const void*>(0x10), nullptr) == InvalidArgument);
    Require(errno == 71);

    for (const std::uint64_t badAddr : {std::uint64_t{0x1001ull}, std::uint64_t{0x1020ull}, std::uint64_t{0x103full}}) {
        std::memcpy(auInfo.data() + 0x10, &badAddr, sizeof(badAddr));
        Require(sceVdecCoreSetDecodeInputSvp(fakeDecoder.data(), pictureInfo.data(), auInfo.data(), nullptr) == InvalidParam);
    }

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
