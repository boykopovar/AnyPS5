#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string_view>
#include <thread>
#ifdef __APPLE__
#include <unistd.h>

namespace {

using namespace AgcDriver::Graphics;
namespace Memory = AgcDriver::GuestMemory;
constexpr std::size_t Block = 65536;
Context ImportContext;
std::uintptr_t Scratch = 0;
std::atomic<unsigned> Resolved{0};
bool LockedCopy = false;
std::uintptr_t TrappedBegin = 0;
std::uintptr_t TrappedEnd = 0;

void Publish(std::uintptr_t begin, std::uintptr_t end) {
    std::unique_lock gpu(Memory::GpuMutex(), std::defer_lock);
    if (LockedCopy && !Memory::GpuMutex().HeldByThisThread()) gpu.lock();
    if (Resolved.load(std::memory_order_acquire) != 0) return;
    static_cast<void>(Memory::CollectWritesUncached(begin, end - begin));
    Require(HostImportFor(ImportContext, Scratch, 16) != nullptr, "resolver scratch import failed");
    GuestWriteWatch::GuestWriteWatchReleaseReads_nid_postfix(begin, end - begin);
    {
        GuestArena::HostWrite write(reinterpret_cast<void*>(begin), end - begin);
        std::memset(reinterpret_cast<void*>(begin), 0x5a, end - begin);
    }
    Resolved.fetch_add(1, std::memory_order_release);
}

void Resolve(std::uintptr_t begin, std::uintptr_t end) {
    std::fprintf(stderr, "resolver entered\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    Publish(begin, end);
    std::fprintf(stderr, "resolver completed\n");
}

void Flush(std::uint64_t address, std::size_t bytes) {
    if (address < TrappedEnd && TrappedBegin < address + bytes) Publish(TrappedBegin, TrappedEnd);
}

std::uintptr_t Allocate(std::size_t bytes) {
    auto* raw = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
    Require(raw != nullptr, "guest arena allocation failed");
    GuestArena::GuestArenaCommit_nid_postfix(raw, bytes, 0x04, Block);
    const auto aligned = reinterpret_cast<std::uintptr_t>(raw);
    std::memset(reinterpret_cast<void*>(aligned), 0x11, bytes);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<void*>(aligned), bytes);
    GuestAllocations::Mutation().Add(reinterpret_cast<void*>(aligned), bytes, true, true, true);
    Require(Memory::Watched(aligned, bytes), "fixture is not watched");
    return aligned;
}

int Run(bool copy, bool lockedCopy) {
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);
    const auto device = OpenVulkanTestDevice();
    if (!device) return VulkanTestSkipped;
    struct ContextReset {
        ~ContextReset() { ImportContext = {}; }
    } reset;
    ImportContext = device->graphicsContext();
    Require(ImportContext.hostImportAlignment != 0, "host import unsupported");
    ImportContext.hostImportAlignment = Block;
    SetImportWatch(ImportContext, ImportWatch::Watch);
    const auto base = Allocate(2 * Block);
    Scratch = Allocate(Block);
    const auto page = static_cast<std::size_t>(getpagesize());
    Memory::Unwatch(base + Block + 2 * page, Block - 2 * page);
    Memory::BumpCollectEpoch();
    Require(Memory::CollectWrites(base, Block + 2 * page) != 0, "fixture collect failed");
    Require(Memory::Accessible(reinterpret_cast<void*>(base), 2 * Block, true), "fixture is inaccessible before trapping");
    Require(GuestWriteWatch::GuestWriteWatchSetReadTrapResolver_nid_postfix(&Resolve), "resolver unavailable");
    const auto trapped = base + Block + page;
    LockedCopy = lockedCopy;
    TrappedBegin = trapped;
    TrappedEnd = trapped + page;
    Require(GuestWriteWatch::GuestWriteWatchTrapReads_nid_postfix(trapped, page), "trap unavailable");
    Require(Memory::Accessible(reinterpret_cast<void*>(base), 2 * Block, true), "fixture did not retain cached logical permissions");
    alarm(10);
    if (lockedCopy) {
        GuestWriteWatch::GuestWriteWatchSetReadTrapBypass_nid_postfix(+[] { return Memory::GpuMutex().HeldByThisThread(); });
        GuestWriteWatch::GuestWriteWatchSetReadTrapLock_nid_postfix(+[] { Memory::GpuMutex().lock(); }, +[] { Memory::GpuMutex().unlock(); });
        Memory::SetFlushHook(&Flush);
    }
    if (copy) {
        std::unique_lock gpu(Memory::GpuMutex(), std::defer_lock);
        if (lockedCopy) gpu.lock();
        GuestBufferMemory memory(ImportContext);
        memory.AcquireRegistered();
        memory.AddReadable(trapped, page);
        memory.Upload(true);
        std::uint32_t adjustment = 0;
        const auto descriptor = memory.Descriptor(trapped, 16, adjustment);
        Buffer readback(ImportContext, 16, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        CommandBatch commands(ImportContext);
        CopyBuffer(ImportContext, commands.Handle(), descriptor.buffer, descriptor.offset + adjustment, readback.Handle(), 0, 16);
        commands.SubmitAndWait();
        readback.Invalidate();
        const auto value = std::to_integer<unsigned>(readback.Bytes().front());
        std::printf("copy returned byte=0x%02x resolves=%u\n", value, Resolved.load(std::memory_order_acquire));
        Require(std::all_of(readback.Bytes().begin(), readback.Bytes().end(), [](auto byte) { return byte == std::byte{0x5a}; }), "copy fallback read stale bytes");
    } else {
        const auto* initial = HostImportFor(ImportContext, base + 256, 16);
        std::printf("first import returned %s\n", initial ? "import" : "nullptr");
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (Resolved.load(std::memory_order_acquire) == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(Resolved.load(std::memory_order_acquire) == 1, "trap was not resolved exactly once");
    Require(HostImportFor(ImportContext, base + 256, 16) != nullptr, "retry did not import the resolved run");
    alarm(0);
    std::puts("import trap probe passed");
    Memory::SetFlushHook(nullptr);
    ClearHostImports(ImportContext.device);
    return 0;
}

}
#endif

int main(int argc, char** argv) {
#ifdef __APPLE__
    try {
        const std::string_view mode = argc == 2 ? std::string_view(argv[1]) : std::string_view();
        const bool lockedCopy = mode == "--locked-copy";
        const bool copy = lockedCopy || mode == "--copy";
        AgcDriver::Graphics::Require(argc == 1 || copy, "invalid import read-trap test arguments");
        return Run(copy, lockedCopy);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
#else
    static_cast<void>(argc);
    static_cast<void>(argv);
    std::puts("skipped, host imports over read-trapped pages require macOS");
    return VulkanTestSkipped;
#endif
}
