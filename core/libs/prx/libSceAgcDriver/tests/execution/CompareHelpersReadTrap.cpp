#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ParallelCompare.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int Skipped = 77;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

#if defined(__linux__)
constexpr std::size_t Pages = 256;
constexpr std::size_t DifferentPage = 37;

std::atomic<unsigned> resolves{0};

void ResolveUnderTheGpuMutex(std::uintptr_t, std::uintptr_t) {
    const std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    resolves.fetch_add(1, std::memory_order_acq_rel);
}

int HelpersReadTrappedPagesForTheGpuMutexHolder() {
    if (!GuestWriteWatch::GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveUnderTheGpuMutex)) {
        std::cout << "skipped, the host lacks the userfaultfd write watch read traps need\n";
        return Skipped;
    }
    GuestWriteWatch::GuestWriteWatchSetReadTrapBypass_nid_postfix(&AgcDriver::Graphics::LabelTrapBypassed);
    const auto page = static_cast<std::size_t>(::getpagesize());
    const auto bytes = Pages * page;
    void* mapped = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED, "cannot map the test range");
    auto* base = static_cast<std::uint8_t*>(mapped);
    for (std::size_t index = 0; index < bytes; ++index) base[index] = static_cast<std::uint8_t>(index * 7u + index / page);
    std::vector<std::uint8_t> copy(base, base + bytes);
    copy[DifferentPage * page + 3u] ^= 0xffu;
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(base, bytes);
    const auto begin = reinterpret_cast<std::uintptr_t>(base);
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(begin, bytes), "the test range is not watched");
    for (std::size_t index = 0; index < Pages; ++index) Require(GuestWriteWatch::GuestWriteWatchTrapReads_nid_postfix(begin + index * page, page), "a page of the test range was not trapped");
    std::vector<AgcDriver::Graphics::CompareSpan> spans;
    for (std::size_t index = 0; index < Pages; ++index) spans.push_back({base + index * page, copy.data() + index * page, page});
    std::vector<std::uint8_t> equal(Pages, 2u);
    std::atomic<bool> compared{false};
    std::thread holder([&] {
        const std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::Graphics::CompareSpansFrom(spans, equal, 1u);
        compared.store(true, std::memory_order_release);
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (!compared.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!compared.load(std::memory_order_acquire)) {
        std::cerr << "the compare helpers of a GPU mutex holder did not finish reading read-trapped pages within 20 s\n";
        std::_Exit(1);
    }
    holder.join();
    for (std::size_t index = 0; index < Pages; ++index) GuestWriteWatch::GuestWriteWatchReleaseReads_nid_postfix(begin + index * page, page);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(base, bytes);
    ::munmap(base, bytes);
    for (std::size_t index = 0; index < Pages; ++index) Require(equal[index] == (index == DifferentPage ? 0u : 1u), "a span read through a read-trapped page compared wrong");
    Require(resolves.load(std::memory_order_acquire) == 0u, "a page the GPU mutex holder's compare read was resolved instead of read through the bypass");
    std::cout << "compare helpers read trap test passed\n";
    return 0;
}
#endif

}

int main() {
    try {
#if defined(__linux__)
        return HelpersReadTrappedPagesForTheGpuMutexHolder();
#else
        std::cout << "skipped, the label read trap resolver this test drives is the Linux one\n";
        return Skipped;
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
