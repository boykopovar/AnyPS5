#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static void Install(const std::shared_ptr<VulkanDevice>& device) {
        auto& driver = Driver::Get();
        driver.WaitIdle();
        std::lock_guard lock(driver.deviceReplacement);
        driver.device = device;
    }
};

}

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        pointer = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, 65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        pointer = static_cast<std::uint32_t*>(std::aligned_alloc(65536, 65536));
#endif
        check(pointer != nullptr, "atomic order allocation failed");
        std::memset(pointer, 0, 65536);
        GuestAllocations::Mutation().Add(pointer, 65536, true, true, true);
    }
    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(pointer);
#ifdef _WIN32
        VirtualFree(pointer, 0, MEM_RELEASE);
#else
        std::free(pointer);
#endif
    }
    std::uint32_t* pointer = nullptr;
};

int run(bool compute, bool deferred) {
    GuestBlock block;
    auto opened = OpenVulkanTestDevice();
    if (!opened) return VulkanTestSkipped;
    const auto device = std::shared_ptr<AgcDriver::VulkanDevice>(std::move(opened));
    try {
        AgcDriver::DriverDetail::DriverTestAccess::Install(device);
        const auto address = reinterpret_cast<std::uintptr_t>(block.pointer);
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const std::array<std::uint32_t, 4> pattern{0x33, 0x33, 0x33, 0x33};
            check(device->FillBuffer(address, 4096, pattern), "atomic order requires a GPU fill of the target allocation");
            device->SubmitRecorded(true);
        }
        std::vector<std::uint32_t> commands;
        if (deferred) commands.insert(commands.end(), {0xc0033700u, 0x100500u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 99});
        const auto control = compute ? 0u : 0x40000000u;
        const auto append = [&](std::uintptr_t target, std::uint32_t operation, std::uint64_t source) {
            commands.insert(commands.end(), {0xc0071e00u, control | 0x200u | operation, static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(target >> 32u), static_cast<std::uint32_t>(source), static_cast<std::uint32_t>(source >> 32u), 0, 0, 6});
        };
        append(address, 0x4f, 5);
        append(address + 8, 0x67, 0x1122334455667788ull);
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        check((compute ? sceAgcDriverSubmitAcb(0x20, &packet) : sceAgcDriverSubmitDcb(&packet)) == 0, "atomic queue submission failed");
        AgcDriverWaitIdle_nid_postfix();
        const std::array<std::uint32_t, 4> expected{deferred ? 104u : 0x38u, 0x33, 0x55667788, 0x11223344};
        check(std::equal(expected.begin(), expected.end(), block.pointer), "atomic did not observe its GPU or deferred-label producer");
        int copyPath = -1;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto outcome = device->CopyBuffer(address + 8192, address, 16, 0, std::numeric_limits<std::size_t>::max(), 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
            copyPath = outcome.path;
            check(copyPath == 1 || copyPath == 3, "atomic consumer did not take a GPU copy path");
            device->WaitIdle();
        }
        check(std::equal(expected.begin(), expected.end(), block.pointer + 2048), "GPU consumer did not observe atomic output");
        std::printf("atomic order passed: queue=%s deferred=%d GPUcopy=%d\n", compute ? "compute" : "graphics", deferred, copyPath);
        AgcDriverShutdown_nid_postfix();
        device->WaitIdle();
        return 0;
    } catch (...) {
        const auto error = std::current_exception();
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        device->WaitIdle();
        std::rethrow_exception(error);
    }
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "graphics";
        check(mode == "graphics" || mode == "compute" || mode == "deferred", "unknown atomic-order mode");
        const auto result = run(mode == "compute", mode == "deferred");
        LibcRunShutdown_nid_postfix();
        return result;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
