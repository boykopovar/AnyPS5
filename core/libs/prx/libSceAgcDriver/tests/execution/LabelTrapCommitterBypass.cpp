#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using AgcDriver::DriverDetail::DrawPipeline;

constexpr int Skipped = 77;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

#if defined(__linux__)
constexpr std::uint8_t Unstored = 0x11;
constexpr std::uint8_t Stored = 0x33;

std::atomic<bool> resolverGaveUp{false};
std::atomic<unsigned> resolved{0};

void ResolveAfterHeldReads(std::uintptr_t begin, std::uintptr_t end) {
    const auto bytes = static_cast<std::size_t>(end - begin);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (DrawPipeline::HeldReads(begin, bytes)) {
        if (std::chrono::steady_clock::now() > deadline) {
            resolverGaveUp.store(true, std::memory_order_release);
            break;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    std::memset(reinterpret_cast<void*>(begin), Stored, bytes);
    resolved.fetch_add(1, std::memory_order_acq_rel);
}

int CommitterTouchesTrappedRangeItHolds() {
    if (DrawPipeline::Depth() == 0) {
        std::cout << "skipped, the draw pipeline is off\n";
        return Skipped;
    }
    if (!GuestWriteWatch::GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveAfterHeldReads)) {
        std::cout << "skipped, the host lacks the userfaultfd write watch read traps need\n";
        return Skipped;
    }
    GuestWriteWatch::GuestWriteWatchSetReadTrapBypass_nid_postfix(&AgcDriver::Graphics::LabelTrapBypassed);
    const auto page = static_cast<std::size_t>(::getpagesize());
    const auto bytes = 2 * page;
    void* mapped = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED, "cannot map the test range");
    auto* base = static_cast<std::uint8_t*>(mapped);
    std::memset(base, Unstored, bytes);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(base, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, bytes), "the test range is not watched");
    Require(GuestWriteWatch::GuestWriteWatchTrapReads_nid_postfix(address, page), "the test page was not trapped");
    std::atomic<int> seen{-1};
    auto& pipeline = DrawPipeline::Queue0();
    pipeline.Enqueue([&] { seen.store(static_cast<const volatile std::uint8_t*>(base)[7], std::memory_order_release); }, {}, 0, {}, {{address, address + page}});
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    GuestWriteWatch::GuestWriteWatchReleaseReads_nid_postfix(address, page);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(base, bytes);
    ::munmap(base, bytes);
    Require(!resolverGaveUp.load(std::memory_order_acquire), "a draw committer that touched a trapped page its own queued item reads waited for a resolver that waits for that item's reads");
    Require(resolved.load(std::memory_order_acquire) == 0, "a draw committer's touch of a trapped page ran the resolver instead of opening the page");
    Require(seen.load(std::memory_order_acquire) == Unstored, "the draw committer did not read the trapped page as it was");
    std::cout << "label trap committer bypass test passed\n";
    return 0;
}
#endif

}

int main() {
    try {
#if defined(__linux__)
        return CommitterTouchesTrappedRangeItHolds();
#else
        std::cout << "skipped, the label read trap resolver this test drives is the Linux one\n";
        return Skipped;
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
