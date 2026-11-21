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
std::atomic<unsigned> stores{0};

void ResolveAsTheLabelDoes(std::uintptr_t begin, std::uintptr_t end) {
    const auto bytes = static_cast<std::size_t>(end - begin);
    const auto store = [&] {
        if (DrawPipeline::HeldReads(begin, bytes)) return;
        std::memset(reinterpret_cast<void*>(begin), Stored, bytes);
        stores.fetch_add(1, std::memory_order_acq_rel);
    };
    store();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (AgcDriver::Graphics::LabelTrapResolveWaits(begin, bytes)) {
        if (std::chrono::steady_clock::now() > deadline) {
            resolverGaveUp.store(true, std::memory_order_release);
            return;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        store();
    }
}

int CommitterTouchesTrappedPages() {
    if (DrawPipeline::Depth() == 0) {
        std::cout << "skipped, the draw pipeline is off\n";
        return Skipped;
    }
    if (!GuestWriteWatch::GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveAsTheLabelDoes)) {
        std::cout << "skipped, the host lacks the userfaultfd write watch read traps need\n";
        return Skipped;
    }
    GuestWriteWatch::GuestWriteWatchSetReadTrapBypass_nid_postfix(&AgcDriver::Graphics::LabelTrapBypassed);
    const auto page = static_cast<std::size_t>(::getpagesize());
    const auto bytes = 4 * page;
    void* mapped = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED, "cannot map the test range");
    auto* base = static_cast<std::uint8_t*>(mapped);
    std::memset(base, Unstored, bytes);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(base, bytes);
    const auto held = reinterpret_cast<std::uintptr_t>(base);
    const auto free = held + 2 * page;
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(held, bytes), "the test range is not watched");
    Require(GuestWriteWatch::GuestWriteWatchTrapReads_nid_postfix(held, page), "the page the queued item reads was not trapped");
    Require(GuestWriteWatch::GuestWriteWatchTrapReads_nid_postfix(free, page), "the page no queued item reads was not trapped");
    std::atomic<int> seenHeld{-1};
    std::atomic<int> seenFree{-1};
    auto& pipeline = DrawPipeline::Queue0();
    pipeline.Enqueue([&] {
        seenHeld.store(reinterpret_cast<const volatile std::uint8_t*>(held)[7], std::memory_order_release);
        seenFree.store(reinterpret_cast<const volatile std::uint8_t*>(free)[7], std::memory_order_release);
    }, {}, 0, {}, {{held, held + page}});
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    GuestWriteWatch::GuestWriteWatchReleaseReads_nid_postfix(held, page);
    GuestWriteWatch::GuestWriteWatchReleaseReads_nid_postfix(free, page);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(base, bytes);
    ::munmap(base, bytes);
    Require(!resolverGaveUp.load(std::memory_order_acquire), "the resolver of a page the draw committer touched kept waiting for reads only that blocked committer could clear");
    Require(seenHeld.load(std::memory_order_acquire) == Unstored, "the draw committer did not read the page its own queued item reads as it was");
    Require(seenFree.load(std::memory_order_acquire) == Stored, "the draw committer read a trapped page no queued item reads before it was stored");
    Require(stores.load(std::memory_order_acquire) == 1, "the resolver did not store exactly the page no queued item reads");
    std::cout << "label trap committer resolve test passed\n";
    return 0;
}
#endif

}

int main() {
    try {
#if defined(__linux__)
        return CommitterTouchesTrappedPages();
#else
        std::cout << "skipped, the label read trap resolver this test drives is the Linux one\n";
        return Skipped;
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
