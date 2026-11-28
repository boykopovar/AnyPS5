#include "prx/libScePlayReady2/libScePlayReady2.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x80070057u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

class GuardedPage {
public:
    unsigned char* data = nullptr;
    std::size_t size = 0;
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr, "VirtualAlloc failed");
        DWORD previous = 0;
        Require(VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) != 0, "VirtualProtect failed");
#else
        const auto pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0, "sysconf failed");
        size = static_cast<std::size_t>(pageSize);
        auto* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED, "mmap failed");
        data = static_cast<unsigned char*>(mapping);
        Require(mprotect(data + size, size, PROT_NONE) == 0, "mprotect failed");
#endif
    }
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;
};

using Block = std::array<unsigned char, 0x40>;

template <typename TValue>
void Put(Block& block, std::size_t offset, TValue value) {
    std::memcpy(block.data() + offset, &value, sizeof(value));
}

std::uint32_t FlagsOf(const Block& block) {
    std::uint32_t flags = 0;
    std::memcpy(&flags, block.data() + 8, sizeof(flags));
    return flags;
}

Block ValidBlock() {
    Block block{};
    Put<std::uint32_t>(block, 0x0, 0x50524459);
    Put<std::uint16_t>(block, 0x4, 3);
    Put<std::uint16_t>(block, 0x6, 0x40);
    Put<std::uint32_t>(block, 0x8, 0x11111111);
    for (std::size_t offset = 0x18; offset <= 0x30; offset += 8) Put<std::uint64_t>(block, offset, 0x1000 + offset);
    return block;
}

void CheckSetInitParamFlags() {
    Require(scePlayReadyApiSetInitParamFlags(nullptr, 1) == ErrorInvalidArgument, "A null block must be rejected");

    for (const std::uint32_t flags : {0u, 1u, 0x80000000u, 0xFFFFFFFFu}) {
        Block block = ValidBlock();
        const Block before = block;
        Require(scePlayReadyApiSetInitParamFlags(block.data(), flags) == 0, "A valid block must be accepted");
        Require(FlagsOf(block) == flags, "The flags must be stored at offset 8");
        for (std::size_t i = 0; i < block.size(); ++i)
            if (i < 8 || i >= 12) Require(block[i] == before[i], "Only offset 8 to 11 may change");
    }

    std::array<Block, 14> invalid{};
    invalid.fill(ValidBlock());
    Put<std::uint32_t>(invalid[0], 0x0, 0x50524458);
    Put<std::uint32_t>(invalid[1], 0x0, 0);
    Put<std::uint16_t>(invalid[2], 0x4, 1);
    Put<std::uint16_t>(invalid[3], 0x4, 2);
    Put<std::uint16_t>(invalid[4], 0x6, 0x3F);
    Put<std::uint16_t>(invalid[5], 0x6, 0x138);
    Put<std::uint32_t>(invalid[6], 0xC, 1);
    Put<std::uint64_t>(invalid[7], 0x10, 1);
    Put<std::uint64_t>(invalid[8], 0x18, 0);
    Put<std::uint64_t>(invalid[9], 0x20, 0);
    Put<std::uint64_t>(invalid[10], 0x28, 0);
    Put<std::uint64_t>(invalid[11], 0x30, 0);
    Put<std::uint64_t>(invalid[12], 0x10, 0xFFFFFFFF00000000ull);
    Put<std::uint32_t>(invalid[13], 0xC, 0x80000000u);
    for (Block& block : invalid) {
        Require(scePlayReadyApiSetInitParamFlags(block.data(), 7) == ErrorInvalidArgument,
                "An invalid block must be rejected");
        Require(FlagsOf(block) == 0x11111111, "A rejected block must not be written");
    }
}

void CheckFieldsReadInOrder() {
    GuardedPage page;
    const Block valid = ValidBlock();
    for (const std::size_t readable : {std::size_t{4}, std::size_t{8}, std::size_t{0x10}, std::size_t{0x18}}) {
        Block bad = valid;
        if (readable == 4) Put<std::uint32_t>(bad, 0x0, 0);
        if (readable == 8) Put<std::uint16_t>(bad, 0x6, 0);
        if (readable == 0x10) Put<std::uint32_t>(bad, 0xC, 1);
        if (readable == 0x18) Put<std::uint64_t>(bad, 0x10, 1);
        unsigned char* block = page.data + page.size - readable;
        std::memcpy(block, bad.data(), readable);
        Require(scePlayReadyApiSetInitParamFlags(block, 7) == ErrorInvalidArgument,
                "An earlier field must be answered before a later unreadable one");
    }
}

void CheckBufferInitWithData() {
    std::array<unsigned char, 0x18> buffer{};
    const std::array<unsigned char, 4> data{};
    Require(scePlayReadyBufferInitWithData(nullptr, data.data(), data.size()) == ErrorInvalidArgument,
            "BufferInitWithData must reject a null buffer");
    Require(scePlayReadyBufferInitWithData(nullptr, nullptr, 0) == ErrorInvalidArgument,
            "BufferInitWithData must reject a null buffer with no data");
    Require(scePlayReadyBufferInitWithData(buffer.data(), nullptr, 1) == ErrorInvalidArgument,
            "BufferInitWithData must reject a size without data");
    Require(scePlayReadyBufferInitWithData(buffer.data(), data.data(), 0) == ErrorInvalidArgument,
            "BufferInitWithData must reject data without a size");
    Require(buffer == std::array<unsigned char, 0x18>{}, "BufferInitWithData must not write when it rejects");
}

} // namespace

int main() {
    for (const int expectedErrno : {0, 7, 42}) {
        errno = expectedErrno;
        CheckSetInitParamFlags();
        CheckFieldsReadInOrder();
        CheckBufferInitWithData();
        Require(errno == expectedErrno, "These calls must not change errno");
    }
}
