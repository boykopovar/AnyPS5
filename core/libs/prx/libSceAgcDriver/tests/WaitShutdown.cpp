#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/CommandBuffer.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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

    alignas(alignof(std::uint32_t)) char unalignedBuffer[sizeof(MockQueueContext) + 4]{};
    auto* unalignedCtx = reinterpret_cast<void*>(unalignedBuffer + 1);
    Require(sceAgcDriverSubmitCommandBuffer(unalignedCtx, &emptyPacket) == 0, "unaligned context submit must succeed");
    Require(sceAgcDriverSubmitMultiCommandBuffers(unalignedCtx, cmdBufs, cmdSizes, 1) == 0, "unaligned context multi submit must succeed");

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

    struct GuardedPages {
        std::size_t pageSize = 0;
        std::uint8_t* base = nullptr;
        GuardedPages() {
#ifdef _WIN32
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            pageSize = info.dwPageSize;
            base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Require(base != nullptr, "VirtualAlloc failed");
#else
            const long size = sysconf(_SC_PAGESIZE);
            pageSize = size > 0 ? static_cast<std::size_t>(size) : 4096u;
            void* mapped = mmap(nullptr, 2 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(mapped != MAP_FAILED, "mmap failed");
            base = static_cast<std::uint8_t*>(mapped);
#endif
            std::memset(base, 0, pageSize);
#ifdef _WIN32
            DWORD previous = 0;
            Require(VirtualProtect(base, pageSize, PAGE_READONLY, &previous) != 0, "VirtualProtect readonly failed");
            Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "VirtualProtect noaccess failed");
#else
            Require(mprotect(base, pageSize, PROT_READ) == 0, "mprotect readonly failed");
            Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0, "mprotect noaccess failed");
#endif
        }
        ~GuardedPages() {
            if (!base) return;
#ifdef _WIN32
            VirtualFree(base, 0, MEM_RELEASE);
#else
            munmap(base, 2 * pageSize);
#endif
        }
        void* ReadOnly() const { return base; }
        void* NoAccess() const { return base + pageSize; }
        void* Straddling() const { return base + pageSize - 6; }
    } pages;

    Require(sceAgcDriverSubmitCommandBuffer(pages.ReadOnly(), &emptyPacket) == 0, "read-only context submit must succeed");
    Require(sceAgcDriverSubmitMultiCommandBuffers(pages.ReadOnly(), cmdBufs, cmdSizes, 1) == 0, "read-only context multi submit must succeed");

    const std::array<void*, 4> badContexts{
        reinterpret_cast<void*>(std::uintptr_t{1}),
        reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - 1),
        pages.NoAccess(),
        pages.Straddling(),
    };
    for (void* badContext : badContexts) {
        errno = E2BIG;
        Require(sceAgcDriverSubmitCommandBuffer(badContext, &emptyPacket) == EINVAL, "unreadable context submit must return EINVAL");
        Require(errno == E2BIG, "unreadable context submit modified errno");
        errno = E2BIG;
        Require(sceAgcDriverSubmitMultiCommandBuffers(badContext, cmdBufs, cmdSizes, 1) == EINVAL, "unreadable context multi submit must return EINVAL");
        Require(errno == E2BIG, "unreadable context multi submit modified errno");
    }
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
