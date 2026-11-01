#include "prx/libSceAgc/DcbFlow/include/Control.hpp"
#include "prx/libSceAgc/Acb/include/Control.hpp"
#include "prx/libSceAgc/Patch/include/Jump.hpp"
#include "prx/libSceAgc/Cb/include/Branch.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

static void Require(bool condition) {
    if (!condition) std::abort();
}

template<typename TAction>
static void Reject(TAction action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected);
}

int main() {
    std::array<std::uint32_t, 32> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    alignas(4) std::array<std::uint32_t, 8> targetBuffer{0xc0001000u, 0u};

    auto* jump = sceAgcDcbJump(&buffer, 1, 0, targetBuffer.data(), 8);
    Require(jump == words.data());
    Require(jump[0] == 0xc0023f00u);
    Require(jump[1] == (static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(targetBuffer.data())) & ~3u));
    Require(jump[2] == static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(targetBuffer.data()) >> 32u));
    Require(sceAgcDcbJumpGetSize() == 16);

    AgcDriver::Pm4::Validate({jump, 4}, 0);
    Reject([&] { AgcDriver::Pm4::Validate({jump, 3}, 0); });
    Reject([&] { AgcDriver::Pm4::Validate({jump, 5}, 0); });

    auto* savedCursor = buffer.cursor_up;
    for (std::uintptr_t offset = 1; offset < 4; ++offset) {
        const auto* unaligned = reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const char*>(targetBuffer.data()) + offset);
        Reject([&] { sceAgcDcbJump(&buffer, 1, 0, unaligned, 8); });
        Require(buffer.cursor_up == savedCursor);
        Reject([&] { sceAgcDcbJump(&buffer, 0, 0, unaligned, 8); });
        Require(buffer.cursor_up == savedCursor);
    }

    Reject([&] { sceAgcDcbJump(&buffer, 2, 0, targetBuffer.data(), 8); });
    Reject([&] { sceAgcDcbJump(&buffer, 0, 4, targetBuffer.data(), 8); });
    Reject([&] { sceAgcDcbJump(&buffer, 0, 0, targetBuffer.data(), 0x100000u); });
    Require(buffer.cursor_up == savedCursor);

    auto* acbJump = sceAgcAcbJump(&buffer, 0, targetBuffer.data(), 8);
    Require(acbJump == savedCursor);
    Require(acbJump[0] == 0xc0023f00u);
    Require(sceAgcAcbJumpGetSize() == 16);

    savedCursor = buffer.cursor_up;
    for (std::uintptr_t offset = 1; offset < 4; ++offset) {
        const auto* unaligned = reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const char*>(targetBuffer.data()) + offset);
        Reject([&] { sceAgcAcbJump(&buffer, 0, unaligned, 8); });
        Require(buffer.cursor_up == savedCursor);
    }

    Reject([&] { sceAgcAcbJump(&buffer, 4, targetBuffer.data(), 8); });
    Reject([&] { sceAgcAcbJump(&buffer, 0, targetBuffer.data(), 0x100000u); });
    Require(buffer.cursor_up == savedCursor);

    Require(sceAgcJumpPatchSetTarget(jump, targetBuffer.data(), 8) == 0);
    Require(sceAgcJumpPatchSetTarget_0300(jump, 1, targetBuffer.data(), 8) == 0);

    for (std::uintptr_t offset = 1; offset < 4; ++offset) {
        const auto* unaligned = reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const char*>(targetBuffer.data()) + offset);
        Reject([&] { sceAgcJumpPatchSetTarget(jump, unaligned, 8); });
        Reject([&] { sceAgcJumpPatchSetTarget_0300(jump, 0, unaligned, 8); });
    }

    Reject([&] { sceAgcJumpPatchSetTarget(jump, nullptr, 8); });
    Reject([&] { sceAgcJumpPatchSetTarget_0300(jump, 0, nullptr, 8); });
    Reject([&] { sceAgcJumpPatchSetTarget(jump, targetBuffer.data(), 0x100000u); });
    Reject([&] { sceAgcJumpPatchSetTarget_0300(jump, 0, targetBuffer.data(), 0x100000u); });
    Reject([&] { sceAgcJumpPatchSetTarget_0300(jump, 4, targetBuffer.data(), 8); });

    for (std::uintptr_t offset = 1; offset < 4; ++offset) {
        const auto* unaligned = reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const char*>(targetBuffer.data()) + offset);
        Reject([&] { sceAgcCbBranch(&buffer, 1, 0, nullptr, 0, 0, 0, unaligned, 8, 0, nullptr, 0); });
        Require(buffer.cursor_up == savedCursor);
        Reject([&] { sceAgcCbBranch(&buffer, 1, 0, nullptr, 0, 0, 0, targetBuffer.data(), 8, 0, unaligned, 8); });
        Require(buffer.cursor_up == savedCursor);
    }

    Reject([&] { sceAgcCbBranch(&buffer, 1, 0, nullptr, 0, 0, 0, nullptr, 8, 0, nullptr, 0); });
    Reject([&] { sceAgcCbBranch(&buffer, 1, 0, nullptr, 0, 0, 0, targetBuffer.data(), 8, 0, nullptr, 8); });
    Require(buffer.cursor_up == savedCursor);

    return 0;
}
