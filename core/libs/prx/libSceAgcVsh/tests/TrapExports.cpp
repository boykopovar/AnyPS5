#include "prx/libSceAgc/libSceAgc.hpp"
#include "prx/libSceAgcVsh/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;

using Call = void (*)(CommandBuffer*);

struct Trap {
    const char* name;
    Call call;
};

void Check(const Trap& trap, CommandBuffer* buffer) {
    try {
        trap.call(buffer);
    } catch (const std::runtime_error& error) {
        const std::string expected = std::string(trap.name) + "() is not allowed to be called from agc vsh.";
        if (expected == error.what()) return;
        throw std::runtime_error(std::string(trap.name) + ": unexpected message: " + error.what());
    }
    throw std::runtime_error(std::string(trap.name) + " returned");
}

}

int main() {
    const std::array<Trap, 24> traps{{
        {"sceAgcAcbAcquireMem", [](CommandBuffer* buffer) { sceAgcAcbAcquireMem(buffer, 0, nullptr, 0, 0); }},
        {"sceAgcAcbAtomicGds", [](CommandBuffer* buffer) { sceAgcAcbAtomicGds(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); }},
        {"sceAgcAcbAtomicMem", [](CommandBuffer* buffer) { sceAgcAcbAtomicMem(buffer, 0, 0, nullptr, 0, 0, 0, 0); }},
        {"sceAgcAcbCopyData", [](CommandBuffer* buffer) { sceAgcAcbCopyData(buffer, 0, 0, 0, 0, 0, 0, 0, 0); }},
        {"sceAgcAcbDmaData", [](CommandBuffer* buffer) { sceAgcAcbDmaData(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0); }},
        {"sceAgcAcbJump", [](CommandBuffer* buffer) { sceAgcAcbJump(buffer, 0, nullptr, 0); }},
        {"sceAgcAcbPrimeUtcl2", [](CommandBuffer* buffer) { sceAgcAcbPrimeUtcl2(buffer, nullptr, 0); }},
        {"sceAgcAcbResetQueue", [](CommandBuffer* buffer) { sceAgcAcbResetQueue(buffer, 0, 0); }},
        {"sceAgcAcbRewind", [](CommandBuffer* buffer) { sceAgcAcbRewind(buffer, 0); }},
        {"sceAgcAcbSetFlip", [](CommandBuffer* buffer) { sceAgcAcbSetFlip(buffer, 0, 0, 0, 0); }},
        {"sceAgcAcbSetMarker", [](CommandBuffer* buffer) { sceAgcAcbSetMarker(buffer, nullptr, 0); }},
        {"sceAgcAcbWaitRegMem", [](CommandBuffer* buffer) { sceAgcAcbWaitRegMem(buffer, 0, 0, 0, nullptr, 0, 0, 0); }},
        {"sceAgcAcbWaitUntilSafeForRendering", [](CommandBuffer* buffer) { sceAgcAcbWaitUntilSafeForRendering(buffer, 0, 0); }},
        {"sceAgcAcbWriteData", [](CommandBuffer* buffer) { sceAgcAcbWriteData(buffer, 0, 0, 0, nullptr, 0, 0, 0); }},
        {"sceAgcCbMemSemaphore", [](CommandBuffer* buffer) { sceAgcCbMemSemaphore(buffer, nullptr, 0, 0, 0); }},
        {"sceAgcDcbAtomicGds", [](CommandBuffer* buffer) { sceAgcDcbAtomicGds(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); }},
        {"sceAgcDcbContextStateOp", [](CommandBuffer* buffer) { sceAgcDcbContextStateOp(buffer, 0); }},
        {"sceAgcDcbResetQueue", [](CommandBuffer* buffer) { sceAgcDcbResetQueue(buffer, 0, 0); }},
        {"sceAgcDcbSetFlip", [](CommandBuffer* buffer) { sceAgcDcbSetFlip(buffer, 0, 0, 0, 0); }},
        {"sceAgcDcbWaitUntilSafeForRendering", [](CommandBuffer* buffer) { sceAgcDcbWaitUntilSafeForRendering(buffer, 0, 0); }},
        {"sceAgcGetRegisterDefaults", [](CommandBuffer*) { sceAgcGetRegisterDefaults(); }},
        {"sceAgcGetRegisterDefaultsInternal", [](CommandBuffer*) { sceAgcGetRegisterDefaultsInternal(); }},
        {"sceAgcSuspendPoint", [](CommandBuffer*) { sceAgcSuspendPoint(); }},
        {"sceAgcSuspendPointAndCheckStatus", [](CommandBuffer*) { sceAgcSuspendPointAndCheckStatus_nid_no_patch(); }},
    }};
    std::array<std::uint32_t, 8> words{};
    words.fill(Sentinel);
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    try {
        for (const auto& trap : traps) {
            Check(trap, nullptr);
            Check(trap, &buffer);
        }
        if (buffer.cursor_up != words.data()) throw std::runtime_error("a trap advanced the command buffer cursor");
        if (!std::all_of(words.begin(), words.end(), [](std::uint32_t word) { return word == Sentinel; }))
            throw std::runtime_error("a trap wrote to the command buffer");
        std::puts("AGC Vsh trap export tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
