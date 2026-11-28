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
#include <thread>
#include <utility>
#include <unistd.h>

namespace {

using namespace AgcDriver::Graphics;
namespace Memory = AgcDriver::GuestMemory;
constexpr std::size_t Block = 65536;
constexpr std::byte Stored{0x5a};
Context ImportContext;
std::uintptr_t Scratch = 0;
std::uintptr_t LateTrap = 0;
std::size_t LateTrapBytes = 0;
std::atomic<unsigned> Resolves{0};
std::atomic<bool> ResolvedOnImporter{false};
std::thread::id Importer;

void Resolve(std::uintptr_t begin, std::uintptr_t end) {
    if (std::this_thread::get_id() == Importer) ResolvedOnImporter.store(true, std::memory_order_release);
    static_cast<void>(Memory::CollectWritesUncached(begin, end - begin));
    Require(HostImportFor(ImportContext, Scratch, 16) != nullptr, "the resolver could not import its scratch block");
    {
        GuestArena::HostWrite write(reinterpret_cast<void*>(begin), end - begin);
        std::memset(reinterpret_cast<void*>(begin), std::to_integer<int>(Stored), end - begin);
    }
    Resolves.fetch_add(1, std::memory_order_release);
}

void TrapDuringImport(std::uint64_t, std::size_t) {
    if (const auto trap = std::exchange(LateTrap, 0); trap != 0) Require(GuestWriteWatch::GuestWriteWatchTrapReads_nid_postfix(trap, LateTrapBytes), "the read trap is unavailable");
}

std::uintptr_t Allocate(std::size_t bytes) {
    auto* raw = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
    Require(raw != nullptr, "guest arena allocation failed");
    GuestArena::GuestArenaCommit_nid_postfix(raw, bytes, 0x04, Block);
    const auto address = reinterpret_cast<std::uintptr_t>(raw);
    std::memset(raw, 0x11, bytes);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(raw, bytes);
    GuestAllocations::Mutation().Add(raw, bytes, true, true, true);
    Require(Memory::Watched(address, bytes), "the guest block is not watched");
    return address;
}

int Run() {
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);
    const auto device = OpenVulkanTestDevice();
    if (!device) return VulkanTestSkipped;
    struct Reset {
        ~Reset() {
            Memory::SetUnwatchHook(nullptr);
            ImportContext = {};
        }
    } reset;
    ImportContext = device->graphicsContext();
    Require(ImportContext.hostImportAlignment != 0, "host imports are unsupported");
    ImportContext.hostImportAlignment = Block;
    SetImportWatch(ImportContext, ImportWatch::Unwatch);
    const auto base = Allocate(2 * Block);
    Scratch = Allocate(Block);
    const auto page = static_cast<std::size_t>(getpagesize());
    Require(GuestWriteWatch::GuestWriteWatchSetReadTrapResolver_nid_postfix(&Resolve), "the read-trap resolver is unavailable");
    const auto trapped = base + Block + page;
    LateTrap = trapped;
    LateTrapBytes = page;
    Memory::SetUnwatchHook(&TrapDuringImport);
    Importer = std::this_thread::get_id();
    alarm(10);
    Require(HostImportFor(ImportContext, base + 256, 16) == nullptr, "an import over a page trapped while it was being made did not wait for the trap");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (Resolves.load(std::memory_order_acquire) == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(Resolves.load(std::memory_order_acquire) == 1, "the trap was not resolved exactly once");
    Require(!ResolvedOnImporter.load(std::memory_order_acquire), "the trap was resolved on the importing thread");
    Require(HostImportFor(ImportContext, base + 256, 16) != nullptr, "the import was refused again after its trap was resolved");
    Require(std::all_of(reinterpret_cast<const std::byte*>(trapped), reinterpret_cast<const std::byte*>(trapped + page), [](std::byte byte) { return byte == Stored; }), "the trapped page does not hold what its resolver stored");
    alarm(0);
    std::puts("an import over a page trapped while it was being made was refused once and made after the trap was resolved off the importing thread");
    ClearHostImports(ImportContext.device);
    return 0;
}

}

int main() {
    try {
        return Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
