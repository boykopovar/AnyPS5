#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

#ifdef _WIN32

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t Unit = 65536;
constexpr std::size_t Units = 4;
constexpr std::size_t RangeBytes = Units * Unit;
constexpr std::uint32_t Held = 0x5a17c0deu;

class GuestBlock {
public:
    explicit GuestBlock(std::size_t committed) : committed(committed) {
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(RangeBytes, Unit));
        Require(block != nullptr, "upload cache: cannot allocate the guest block");
        GuestArena::GuestArenaCommit_nid_postfix(block, committed, PAGE_READWRITE, committed);
        Require(AgcDriver::GuestMemory::Watched(Address(), committed), "upload cache: the guest block is not write-watched");
        std::fill(reinterpret_cast<std::uint32_t*>(block), reinterpret_cast<std::uint32_t*>(block + committed), Held);
        GuestAllocations::Mutation().Add(block, committed, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), committed);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
        GuestArena::GuestArenaReset_nid_postfix(block, committed);
        GuestArena::GuestArenaRelease_nid_postfix(block, RangeBytes);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

    void Protect(std::size_t unit, DWORD protection) const {
        GuestAllocations::Mutation mutation;
        mutation.Protect(block + unit * Unit, Unit, protection != PAGE_NOACCESS, protection == PAGE_READWRITE, true, [&] {
            DWORD previous = 0;
            Require(VirtualProtect(block + unit * Unit, Unit, protection, &previous) != 0, "upload cache: cannot change the protection of a guest unit");
            GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(block + unit * Unit), Unit, protection);
        });
    }

private:
    std::size_t committed;
    std::uint8_t* block = nullptr;
};

std::vector<std::uint32_t> Uploaded(const AgcDriver::Graphics::Context& context, std::uint64_t address, const std::function<void()>& afterRegistration) {
    AgcDriver::Graphics::GuestBufferMemory memory(context);
    memory.AddReadable(address, RangeBytes);
    if (afterRegistration) afterRegistration();
    memory.Upload(false);
    std::uint32_t adjustment = 0;
    const auto descriptor = memory.Descriptor(address, RangeBytes, adjustment);
    Require(adjustment == 0 && descriptor.offset == 0 && descriptor.range >= 3 * Unit && descriptor.range <= RangeBytes, "upload cache: the range was not uploaded from its start");
    AgcDriver::Graphics::Buffer readback(context, RangeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    AgcDriver::Graphics::CommandBatch batch(context);
    AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    AgcDriver::Graphics::CopyBuffer(context, batch.Handle(), descriptor.buffer, descriptor.offset, readback.Handle(), 0, descriptor.range);
    batch.SubmitAndWait();
    readback.Invalidate();
    std::vector<std::uint32_t> words(RangeBytes / sizeof(std::uint32_t));
    std::memcpy(words.data(), readback.Bytes().data(), RangeBytes);
    memory.WriteBack();
    return words;
}

bool Holds(const std::vector<std::uint32_t>& words, std::size_t unit, std::uint32_t value) {
    const auto first = words.begin() + static_cast<std::ptrdiff_t>(unit * Unit / sizeof(std::uint32_t));
    return std::all_of(first, first + static_cast<std::ptrdiff_t>(Unit / sizeof(std::uint32_t)), [value](std::uint32_t word) { return word == value; });
}

void Check(const AgcDriver::Graphics::Context& context, bool sparse) {
    const std::string name = sparse ? "a sparse range" : "a whole range";
    GuestBlock block(sparse ? 3 * Unit : RangeBytes);
    const auto without = Uploaded(context, block.Address(), [&] { block.Protect(1, PAGE_NOACCESS); });
    Require(Holds(without, 0, Held) && Holds(without, 2, Held), "upload cache: " + name + " lost its readable units");
    Require(Holds(without, 1, 0u), "upload cache: the unreadable unit of " + name + " did not upload as zeros");
    block.Protect(1, PAGE_READWRITE);
    const auto restored = Uploaded(context, block.Address(), {});
    Require(Holds(restored, 0, Held) && Holds(restored, 2, Held), "upload cache: " + name + " lost its readable units after the unit came back");
    Require(Holds(restored, 1, Held), "upload cache: the zeros read while a unit of " + name + " was unreadable were served to a later upload");
    std::printf("upload cache: %s with a unit unreadable at one upload is read again at the next\n", name.c_str());
}

}

#endif

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_NO_LEASE_MIRROR", "1");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!AgcDriver::GuestMemory::WriteWatched()) {
            std::puts("skipped, guest memory has no write watch");
            return VulkanTestSkipped;
        }
        auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        context.hostImportAlignment = 0;
        Check(context, false);
        Check(context, true);
        std::puts("upload cache unreadable piece tests passed");
        return 0;
#else
        std::puts("skipped, the test changes page protection through the Windows guest arena");
        return VulkanTestSkipped;
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
