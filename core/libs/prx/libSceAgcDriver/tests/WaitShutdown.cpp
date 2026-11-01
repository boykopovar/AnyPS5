#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/CommandBuffer.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

struct alignas(64) LabelState {
    volatile std::uint32_t label = 0;
    volatile std::uint32_t entered = 0;
    volatile std::uint32_t after = 0;
    volatile std::uint32_t queued = 0;
};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void DelayExit() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

std::array<std::uint32_t, 5> WriteData(volatile std::uint32_t* destination) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(destination));
    return {0xc0033700, 0x00100200, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 1};
}

std::vector<std::uint32_t> WaitingCommands(LabelState& state) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&state.label));
    const auto entered = WriteData(&state.entered);
    const std::array<std::uint32_t, 7> wait{0xc0053c00, 0x13, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 1, 0xffffffffu, 0x19};
    const auto after = WriteData(&state.after);
    std::vector<std::uint32_t> words(entered.begin(), entered.end());
    words.insert(words.end(), wait.begin(), wait.end());
    words.insert(words.end(), after.begin(), after.end());
    return words;
}

void Submit(std::uint32_t queue, const std::vector<std::uint32_t>& words) {
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    const auto result = queue == 0 ? sceAgcDriverSubmitDcb(&packet) : sceAgcDriverSubmitAcb(queue, &packet);
    Require(result == 0, "shutdown wait submission failed");
}

void CheckCommandBufferSubmission() {
    struct MockQueueContext {
        std::uint32_t unused = 0;
        std::uint32_t queue_id = 0;
    };
    MockQueueContext graphicsCtx{0, 0};
    MockQueueContext computeCtx{0, 0x20};
    MockQueueContext invalidCtx{0, 1};

    Packet emptyPacket{nullptr, 0, 0, {}};
    Require(sceAgcDriverSubmitCommandBuffer(nullptr, &emptyPacket) == 22, "null context submit must return EINVAL");
    Require(sceAgcDriverSubmitCommandBuffer(&graphicsCtx, nullptr) == 22, "null packet submit must return EINVAL");
    Require(sceAgcDriverSubmitMultiCommandBuffers(nullptr, nullptr, nullptr, 0) == 0, "count 0 multi submit must return 0");

    std::uint32_t* cmdBufs[] = {nullptr};
    const std::uint32_t cmdSizes[] = {0};
    Require(sceAgcDriverSubmitMultiCommandBuffers(nullptr, cmdBufs, cmdSizes, 1) == 22, "null context multi submit must return EINVAL");
    Require(sceAgcDriverSubmitMultiCommandBuffers(&graphicsCtx, nullptr, cmdSizes, 1) == 22, "null cmdBufs multi submit must return EINVAL");
    Require(sceAgcDriverSubmitMultiCommandBuffers(&graphicsCtx, cmdBufs, nullptr, 1) == 22, "null cmdSizes multi submit must return EINVAL");
    Require(sceAgcDriverSubmitMultiCommandBuffers(&graphicsCtx, cmdBufs, cmdSizes, 0) == 0, "empty multi submission failed");
    Require(sceAgcDriverSubmitMultiCommandBuffers(&graphicsCtx, cmdBufs, cmdSizes, 1) == 0, "graphics context multi submit failed");
    Require(sceAgcDriverSubmitMultiCommandBuffers(&computeCtx, cmdBufs, cmdSizes, 1) == 0, "compute context multi submit failed");

    bool submitRejected = false;
    try {
        sceAgcDriverSubmitCommandBuffer(&invalidCtx, &emptyPacket);
    } catch (const std::runtime_error&) {
        submitRejected = true;
    }
    Require(submitRejected, "unsupported queue context was not rejected");

    bool multiRejected = false;
    try {
        sceAgcDriverSubmitMultiCommandBuffers(&invalidCtx, cmdBufs, cmdSizes, 1);
    } catch (const std::runtime_error&) {
        multiRejected = true;
    }
    Require(multiRejected, "unsupported queue context for multi submit was not rejected");
}

void CheckShutdownCancellation() {
    static std::array<LabelState, 3> states{};
    constexpr std::array<std::uint32_t, 3> queues{0, 0x20, 0x28};
    for (std::size_t index = 0; index < queues.size(); ++index) {
        Submit(queues[index], WaitingCommands(states[index]));
        const auto queued = WriteData(&states[index].queued);
        Submit(queues[index], {queued.begin(), queued.end()});
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (const auto& state : states) {
        while (state.entered != 1) {
            Require(std::chrono::steady_clock::now() < deadline, "queue did not reach its unsatisfied wait");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (const auto& state : states) {
        Require(state.label == 0, "unsatisfied label was unexpectedly signalled");
        Require(state.after == 0 && state.queued == 0, "unsatisfied wait completed before shutdown");
    }
    const auto started = std::chrono::steady_clock::now();
    LibcRunShutdown_nid_postfix();
    Require(std::chrono::steady_clock::now() - started < std::chrono::seconds(2), "shutdown did not promptly cancel waiting queues");
    for (const auto& state : states) {
        Require(state.label == 0, "shutdown signalled an unsatisfied label");
        Require(state.after == 0, "shutdown executed commands after an unsatisfied wait");
        Require(state.queued == 0, "shutdown executed a pending submission behind an unsatisfied wait");
    }
    LibcRunShutdown_nid_postfix();
}

}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--resolution-shutdown") == 0) {
        AgcDriverWaitIdle_nid_postfix();
        LibcRegisterShutdown_nid_postfix(DelayExit);
        LibcRequestExit_nid_postfix(0);
        AgcDriverResolveShaderAbi_nid_postfix(nullptr, {}, {});
        return 1;
    }
    try {
        CheckCommandBufferSubmission();
        CheckShutdownCancellation();
        std::puts("unsatisfied label waits cancelled cleanly at shutdown");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try {
            LibcRunShutdown_nid_postfix();
        } catch (const std::exception& shutdown) {
            std::fprintf(stderr, "shutdown: %s\n", shutdown.what());
        }
        return 1;
    }
}
