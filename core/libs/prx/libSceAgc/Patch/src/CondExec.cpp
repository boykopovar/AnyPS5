#include "prx/libSceAgc/Patch/include/CondExec.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

namespace {

constexpr int InvalidPacket = static_cast<int>(0x8a6c000cu);

bool ReadCondExecPacket(const std::uint32_t* cmd, std::array<std::uint32_t, 5>& words) {
    if (cmd == nullptr || (reinterpret_cast<std::uintptr_t>(cmd) & 3u) != 0) {
        return false;
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(words.data(), cmd, sizeof(words));
    errno = savedErrno;
    return copied && ((words[0] >> 8u) & 0xffu) == 0x22u;
}

bool WriteCondExecWords(std::uint32_t* cmd, std::size_t wordOffset, const std::uint32_t* words, std::size_t wordCount) {
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(cmd, sizeof(std::uint32_t) * 5);
    const bool copied = writable && GuestCopyOut_nid_no_patch(cmd + wordOffset, words, wordCount * sizeof(std::uint32_t));
    errno = savedErrno;
    return copied;
}

int SetCommandAddress(std::uint32_t* cmd, const volatile std::uint32_t* command) {
    std::array<std::uint32_t, 5> words{};
    if (!ReadCondExecPacket(cmd, words)) {
        return InvalidPacket;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(command);
    const std::array<std::uint32_t, 2> patched{
        (words[1] & 3u) | (static_cast<std::uint32_t>(address) & ~3u),
        static_cast<std::uint32_t>(address >> 32u),
    };
    return WriteCondExecWords(cmd, 1, patched.data(), patched.size()) ? 0 : InvalidPacket;
}

int SetEnd(std::uint32_t* cmd, const volatile std::uint32_t* buffer) {
    std::array<std::uint32_t, 5> words{};
    if (!ReadCondExecPacket(cmd, words)) {
        return InvalidPacket;
    }
    const std::uint32_t byteDelta = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(buffer) - reinterpret_cast<std::uintptr_t>(cmd));
    const std::uint32_t numDwords = ((byteDelta >> 2u) - 5u) & 0x3fffu;
    const std::uint32_t patched = (words[4] & ~0x3fffu) | numDwords;
    return WriteCondExecWords(cmd, 4, &patched, 1) ? 0 : InvalidPacket;
}

}

extern "C" {

int APS5_VABI sceAgcCondExecPatchSetCommandAddress(uint32_t* cmd, const volatile uint32_t* command) {
    return SetCommandAddress(cmd, command);
}

int APS5_VABI sceAgcCondExecPatchSetEnd(uint32_t* cmd, const volatile uint32_t* buffer) {
    return SetEnd(cmd, buffer);
}

int APS5_VABI sceAgcAsyncCondExecPatchSetCommandAddress(std::uint32_t* cmd, const volatile std::uint32_t* command) {
    return SetCommandAddress(cmd, command);
}

int APS5_VABI sceAgcAsyncCondExecPatchSetEnd(std::uint32_t* cmd, const volatile std::uint32_t* buffer) {
    return SetEnd(cmd, buffer);
}

}
