#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <new>

namespace {

namespace Graphics = AgcDriver::Graphics;
using Graphics::Require;

constexpr std::size_t MiB = 1u << 20u;
constexpr std::size_t PieceBytes = 4 * MiB;
constexpr std::size_t AllocationBytes = 8 * PieceBytes;
constexpr std::size_t BoundBytes = 12 * MiB + MiB / 2;

class Pieces {
public:
    explicit Pieces(const Graphics::Context& context) : context(context) {
        block = static_cast<std::uint32_t*>(::operator new(AllocationBytes, std::align_val_t{4 * PieceBytes}));
        std::memset(block, 0x11, AllocationBytes);
        GuestAllocations::Mutation mutation;
        for (std::size_t index = 1; index <= 4; ++index) mutation.Add(reinterpret_cast<std::uint8_t*>(block) + index * PieceBytes, PieceBytes, true, true, true);
    }

    ~Pieces() {
        Graphics::ClearHostImports(context.device);
        {
            GuestAllocations::Mutation mutation;
            for (std::size_t index = 1; index <= 4; ++index) mutation.Remove(reinterpret_cast<std::uint8_t*>(block) + index * PieceBytes);
        }
        ::operator delete(block, std::align_val_t{4 * PieceBytes});
    }

    Pieces(const Pieces&) = delete;
    Pieces& operator=(const Pieces&) = delete;

    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block) + PieceBytes; }

    void Check(std::uint32_t pattern, std::size_t boundBytes) const {
        for (std::size_t index = 0; index < AllocationBytes / sizeof(*block); ++index) {
            const auto byte = index * sizeof(*block);
            const auto expected = byte >= PieceBytes && byte < PieceBytes + boundBytes ? pattern : 0x11111111u;
            Require(block[index] == expected, "a partial joined buffer lost GPU output or changed bytes outside its binding");
        }
    }

private:
    Graphics::Context context;
    std::uint32_t* block = nullptr;
};

void CheckWindow(const Graphics::Context& context, std::size_t window, std::size_t boundBytes, bool imported) {
    Graphics::ClearHostImports(context.device);
    Graphics::SetHostImportWindow(window);
    Pieces pieces(context);
    const auto address = pieces.Address();
    Require(Graphics::HostImportFor(context, address, 16) != nullptr, "the first registered piece was not imported");
    Require(Graphics::HostImportFor(context, address + 4 * PieceBytes - 16, 16) != nullptr, "the last registered piece was not imported");
    Graphics::GuestBufferMemory memory(context);
    memory.AddWritable(address, boundBytes);
    memory.Upload(false);
    const auto copied = memory.CopiedBytes();
    std::uint32_t adjustment = 0;
    const auto view = memory.Descriptor(address, boundBytes, adjustment);
    const auto* direct = Graphics::ExistingHostImportFor(context, address, boundBytes);
    if (imported) {
        Require(copied == 0 && direct != nullptr, "an eligible large binding over four pieces was copied");
        Require(view.buffer == direct->buffer && view.offset + adjustment == address - direct->base, "the partial binding does not refer to its joined import");
    } else {
        Require(copied == boundBytes && direct == nullptr, "an 8 MiB window joined a partial binding spanning 16 MiB");
        memory.WriteBack();
        pieces.Check(0x11111111u, boundBytes);
        std::printf("join window %zu MiB, binding %.1f MiB: %zu copied bytes; initial bytes preserved\n", window / MiB, static_cast<double>(boundBytes) / MiB, static_cast<std::size_t>(copied));
        return;
    }
    constexpr std::uint32_t pattern = 0xa5b6c7d8u;
    Graphics::CommandBatch batch(context);
    const auto commands = batch.Handle();
    context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, view.buffer, view.offset + adjustment, boundBytes, pattern);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    batch.SubmitAndWait();
    memory.WriteBack();
    pieces.Check(pattern, boundBytes);
    std::printf("join window %zu MiB, binding %.1f MiB: %zu copied bytes; GPU output and surrounding bytes preserved\n", window / MiB, static_cast<double>(boundBytes) / MiB, static_cast<std::size_t>(copied));
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = device->graphicsContext();
        if (context.hostImportAlignment == 0) {
            std::puts("skipped, the device does not support host imports");
            return VulkanTestSkipped;
        }
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        CheckWindow(context, 8 * MiB, BoundBytes, false);
        CheckWindow(context, 8 * MiB, 4 * PieceBytes, true);
        CheckWindow(context, 16 * MiB, BoundBytes, true);
        Graphics::SetHostImportWindow(8 * MiB);
        std::puts("host import join window tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
