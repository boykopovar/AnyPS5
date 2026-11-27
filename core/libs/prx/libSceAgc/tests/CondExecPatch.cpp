#include "prx/libSceAgc/Patch/include/CondExec.hpp"
#include "prx/libSceAgc/DcbFlow/include/Sync.hpp"
#include "prx/libSceAgc/Acb/include/Sync.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;
constexpr int InvalidPacket = static_cast<int>(0x8a6c000cu);

using CondExecWriter = std::uint32_t* (APS5_VABI *)(CommandBuffer*, const volatile std::uint32_t*, std::uint32_t);
using PatchFunction = int (APS5_VABI *)(std::uint32_t*, const volatile std::uint32_t*);

struct Variant {
    CondExecWriter write;
    PatchFunction setEnd;
    PatchFunction setCommandAddress;
};

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

const volatile std::uint32_t* at(std::uintptr_t address) {
    return reinterpret_cast<const volatile std::uint32_t*>(address);
}

struct Storage {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    std::uint32_t condition = 1;
    std::uint32_t* packet = nullptr;

    explicit Storage(CondExecWriter write) {
        words.fill(Sentinel);
        packet = write(&buffer, &condition, 3);
        check(packet == words.data() && packet[0] == 0xc0032200u && packet[4] == 3, "unexpected COND_EXEC packet");
    }
};

struct PageMapping {
    std::uint8_t* base = nullptr;
    std::size_t size = 0;
    std::size_t pageSize = 0;

    explicit PageMapping(std::size_t pageCount) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        size = pageSize * pageCount;
        base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        check(base != nullptr, "VirtualAlloc failed");
#else
        const long rawSize = sysconf(_SC_PAGESIZE);
        check(rawSize > 0, "sysconf(_SC_PAGESIZE) failed");
        pageSize = static_cast<std::size_t>(rawSize);
        size = pageSize * pageCount;
        void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        check(mapped != MAP_FAILED, "mmap failed");
        base = static_cast<std::uint8_t*>(mapped);
#endif
    }

    ~PageMapping() {
        if (base == nullptr) {
            return;
        }
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    void protectReadOnly(std::size_t pageIndex) {
        std::uint8_t* page = base + pageIndex * pageSize;
#ifdef _WIN32
        DWORD previous = 0;
        check(VirtualProtect(page, pageSize, PAGE_READONLY, &previous) != 0, "VirtualProtect PAGE_READONLY failed");
#else
        check(mprotect(page, pageSize, PROT_READ) == 0, "mprotect PROT_READ failed");
#endif
    }

    void protectNoAccess(std::size_t pageIndex) {
        std::uint8_t* page = base + pageIndex * pageSize;
#ifdef _WIN32
        DWORD previous = 0;
        check(VirtualProtect(page, pageSize, PAGE_NOACCESS, &previous) != 0, "VirtualProtect PAGE_NOACCESS failed");
#else
        check(mprotect(page, pageSize, PROT_NONE) == 0, "mprotect PROT_NONE failed");
#endif
    }
};

std::uintptr_t endAfter(const std::uint32_t* packet, std::uintptr_t numDwords) {
    return reinterpret_cast<std::uintptr_t>(packet + 5) + numDwords * sizeof(std::uint32_t);
}

template <typename TAction>
void expectInvalidPacket(const Variant& variant, TAction action) {
    Storage storage(variant.write);
    const auto before = storage.words;
    errno = 71;
    const int status = action(storage.packet);
    check(errno == 71, "invalid packet patch modified errno");
    check(status == InvalidPacket, "expected invalid packet error code");
    check(storage.words == before, "invalid packet patch modified storage");
}

void testSetEnd(const Variant& variant) {
    const auto setEnd = variant.setEnd;
    for (const std::uintptr_t numDwords : {0u, 1u, 7u, 0x3fffu}) {
        Storage storage(variant.write);
        storage.packet[4] |= 0xabcd0000u;
        const auto before = storage.words;
        errno = 71;
        check(setEnd(storage.packet, at(endAfter(storage.packet, numDwords))) == 0, "SetEnd failed");
        check(errno == 71, "SetEnd modified errno");
        check(storage.packet[4] == (0xabcd0000u | static_cast<std::uint32_t>(numDwords)), "SetEnd wrote the wrong dword count");
        check(std::equal(before.begin(), before.begin() + 4, storage.words.begin()), "SetEnd modified other packet words");
        check(std::equal(before.begin() + 5, before.end(), storage.words.begin() + 5), "SetEnd modified following words");
    }

    {
        Storage storage(variant.write);
        storage.packet[4] |= 0xabcd0000u;
        check(setEnd(storage.packet, at(endAfter(storage.packet, 0x4000u))) == 0, "SetEnd wrap 0x4000 failed");
        check(storage.packet[4] == 0xabcd0000u, "SetEnd did not mask count to 14 bits");
    }
    {
        Storage storage(variant.write);
        storage.packet[4] |= 0xabcd0000u;
        check(setEnd(storage.packet, storage.packet + 4) == 0, "SetEnd packet+4 failed");
        check(storage.packet[4] == 0xabcd3fffu, "SetEnd did not mask negative delta to 0x3fff");
    }
    {
        Storage storage(variant.write);
        storage.packet[4] |= 0xabcd0000u;
        check(setEnd(storage.packet, at(endAfter(storage.packet, 2) + 2)) == 0, "SetEnd unaligned failed");
        check(storage.packet[4] == 0xabcd0002u, "SetEnd did not shift unaligned byte delta");
    }

    expectInvalidPacket(variant, [=](std::uint32_t* packet) { return setEnd(nullptr, at(endAfter(packet, 2))); });
    expectInvalidPacket(variant, [=](std::uint32_t* packet) { return setEnd(packet + 1, at(endAfter(packet, 2))); });
    {
        Storage storage(variant.write);
        storage.packet[0] = (storage.packet[0] & ~0xff00u) | 0x2100u;
        const auto before = storage.words;
        errno = 71;
        check(setEnd(storage.packet, at(endAfter(storage.packet, 2))) == InvalidPacket, "SetEnd accepted wrong opcode");
        check(errno == 71, "SetEnd wrong opcode modified errno");
        check(storage.words == before, "SetEnd modified wrong-opcode packet");
    }
}

void testSetCommandAddress(const Variant& variant) {
    const auto setCommandAddress = variant.setCommandAddress;
    for (const std::uintptr_t address : {std::uintptr_t{0x0000123456789abcu}, std::uintptr_t{4u}, std::uintptr_t{0x0000fffffffffffcu}}) {
        Storage storage(variant.write);
        storage.packet[1] |= 3u;
        const auto before = storage.words;
        errno = 71;
        check(setCommandAddress(storage.packet, at(address)) == 0, "SetCommandAddress failed");
        check(errno == 71, "SetCommandAddress modified errno");
        check(storage.packet[1] == (static_cast<std::uint32_t>(address) | 3u) && storage.packet[2] == static_cast<std::uint32_t>(address >> 32u), "SetCommandAddress wrote the wrong address");
        check(storage.packet[0] == before[0] && storage.packet[3] == before[3] && storage.packet[4] == before[4], "SetCommandAddress modified other packet words");
        check(std::equal(before.begin() + 5, before.end(), storage.words.begin() + 5), "SetCommandAddress modified following words");
    }

    {
        Storage storage(variant.write);
        storage.packet[1] = 0x1234567bu;
        storage.packet[2] = 0xdeadbeefu;
        check(setCommandAddress(storage.packet, nullptr) == 0, "SetCommandAddress null address failed");
        check(storage.packet[1] == 3u && storage.packet[2] == 0u, "SetCommandAddress did not clear null address");
    }
    {
        Storage storage(variant.write);
        storage.packet[1] |= 2u;
        check(setCommandAddress(storage.packet, at(0x1002u)) == 0, "SetCommandAddress unaligned failed");
        check(storage.packet[1] == 0x1002u && storage.packet[2] == 0u, "SetCommandAddress did not mask low two address bits");
    }

    expectInvalidPacket(variant, [=](std::uint32_t*) { return setCommandAddress(nullptr, at(0x1000u)); });
    expectInvalidPacket(variant, [=](std::uint32_t* packet) { return setCommandAddress(packet + 1, at(0x1000u)); });
    {
        Storage storage(variant.write);
        storage.packet[0] = (storage.packet[0] & ~0xff00u) | 0x2100u;
        const auto before = storage.words;
        errno = 71;
        check(setCommandAddress(storage.packet, at(0x1000u)) == InvalidPacket, "SetCommandAddress accepted wrong opcode");
        check(errno == 71, "SetCommandAddress wrong opcode modified errno");
        check(storage.words == before, "SetCommandAddress modified wrong-opcode packet");
    }
}

void testInvalidGuestPointers(const Variant& variant) {
    auto* const unmapped = reinterpret_cast<std::uint32_t*>(std::uintptr_t{0x10});
    errno = 71;
    check(variant.setEnd(unmapped, at(0x1000u)) == InvalidPacket, "SetEnd accepted unmapped cmd pointer");
    check(errno == 71, "SetEnd unmapped cmd modified errno");
    errno = 71;
    check(variant.setCommandAddress(unmapped, at(0x1000u)) == InvalidPacket, "SetCommandAddress accepted unmapped cmd pointer");
    check(errno == 71, "SetCommandAddress unmapped cmd modified errno");

    for (const std::uintptr_t offset : {std::uintptr_t{1u}, std::uintptr_t{2u}}) {
        expectInvalidPacket(variant, [=](std::uint32_t* packet) {
            auto* const misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(packet) + offset);
            return variant.setEnd(misaligned, at(endAfter(packet, 2)));
        });
        expectInvalidPacket(variant, [=](std::uint32_t* packet) {
            auto* const misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(packet) + offset);
            return variant.setCommandAddress(misaligned, at(0x1000u));
        });
    }

    Storage templateStorage(variant.write);
    std::array<std::uint32_t, 5> packetBytes{};
    std::memcpy(packetBytes.data(), templateStorage.packet, sizeof(packetBytes));

    {
        PageMapping readOnlyMapping(1);
        std::memcpy(readOnlyMapping.base, packetBytes.data(), sizeof(packetBytes));
        readOnlyMapping.protectReadOnly(0);
        auto* const roCmd = reinterpret_cast<std::uint32_t*>(readOnlyMapping.base);

        errno = 71;
        check(variant.setEnd(roCmd, at(endAfter(roCmd, 2))) == InvalidPacket, "SetEnd wrote into read-only cmd page");
        check(errno == 71, "SetEnd read-only cmd modified errno");
        check(std::memcmp(readOnlyMapping.base, packetBytes.data(), sizeof(packetBytes)) == 0, "SetEnd mutated read-only cmd page");

        errno = 71;
        check(variant.setCommandAddress(roCmd, at(0x1000u)) == InvalidPacket, "SetCommandAddress wrote into read-only cmd page");
        check(errno == 71, "SetCommandAddress read-only cmd modified errno");
        check(std::memcmp(readOnlyMapping.base, packetBytes.data(), sizeof(packetBytes)) == 0, "SetCommandAddress mutated read-only cmd page");
    }

    {
        PageMapping guardedMapping(2);
        std::uint8_t* const end = guardedMapping.base + guardedMapping.pageSize;
        std::uint8_t* const straddlingBytes = end - 16;
        std::memcpy(straddlingBytes, packetBytes.data(), 16);
        guardedMapping.protectNoAccess(1);
        auto* const straddlingCmd = reinterpret_cast<std::uint32_t*>(straddlingBytes);

        errno = 71;
        check(variant.setEnd(straddlingCmd, at(endAfter(straddlingCmd, 2))) == InvalidPacket, "SetEnd accepted guard-straddling cmd pointer");
        check(errno == 71, "SetEnd guard-straddling cmd modified errno");
        check(std::memcmp(straddlingBytes, packetBytes.data(), 16) == 0, "SetEnd mutated guard-straddling mapped bytes");

        errno = 71;
        check(variant.setCommandAddress(straddlingCmd, at(0x1000u)) == InvalidPacket, "SetCommandAddress accepted guard-straddling cmd pointer");
        check(errno == 71, "SetCommandAddress guard-straddling cmd modified errno");
        check(std::memcmp(straddlingBytes, packetBytes.data(), 16) == 0, "SetCommandAddress mutated guard-straddling mapped bytes");
    }

    for (const std::size_t tailOffset : {std::size_t{16u}, std::size_t{8u}}) {
        PageMapping straddlingRoMapping(2);
        std::uint8_t* const end = straddlingRoMapping.base + straddlingRoMapping.pageSize;
        std::uint8_t* const straddlingBytes = end - tailOffset;
        std::memcpy(straddlingBytes, packetBytes.data(), sizeof(packetBytes));
        straddlingRoMapping.protectReadOnly(1);
        auto* const straddlingCmd = reinterpret_cast<std::uint32_t*>(straddlingBytes);

        errno = 71;
        check(variant.setEnd(straddlingCmd, at(endAfter(straddlingCmd, 2))) == InvalidPacket, "SetEnd accepted RW-to-RO straddling cmd pointer");
        check(errno == 71, "SetEnd RW-to-RO straddling cmd modified errno");
        check(std::memcmp(straddlingBytes, packetBytes.data(), sizeof(packetBytes)) == 0, "SetEnd mutated RW-to-RO straddling bytes");

        errno = 71;
        check(variant.setCommandAddress(straddlingCmd, at(0x123456789abcu)) == InvalidPacket, "SetCommandAddress accepted RW-to-RO straddling cmd pointer");
        check(errno == 71, "SetCommandAddress RW-to-RO straddling cmd modified errno");
        check(std::memcmp(straddlingBytes, packetBytes.data(), sizeof(packetBytes)) == 0, "SetCommandAddress mutated RW-to-RO straddling bytes");
    }
}

}

int main() {
    try {
        for (const Variant& variant : {Variant{sceAgcDcbCondExec, sceAgcCondExecPatchSetEnd, sceAgcCondExecPatchSetCommandAddress},
                                       Variant{sceAgcAcbCondExec, sceAgcAsyncCondExecPatchSetEnd, sceAgcAsyncCondExecPatchSetCommandAddress}}) {
            testSetEnd(variant);
            testSetCommandAddress(variant);
            testInvalidGuestPointers(variant);
        }
        std::puts("AGC COND_EXEC patch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
