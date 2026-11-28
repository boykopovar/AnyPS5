#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

namespace AgcDriver::Graphics {

namespace {

constexpr std::uint32_t ProbeWidth = 256u;
constexpr std::uint32_t ProbeHeight = 256u;
constexpr auto ProbeIdle = std::chrono::milliseconds(5);

std::uint32_t Hash(std::uint32_t x) {
    x ^= x >> 16u;
    x *= 0x7feb352du;
    x ^= x >> 15u;
    x *= 0x846ca68bu;
    x ^= x >> 16u;
    return x;
}

}

TextureDetiler::WordGroupProbeResult TextureDetiler::ProbeWordGroups(const Context& device) {
    auto context = device;
    context.detileWordGroupsFaulty = false;
    TextureDetiler detiler(context);
    WordGroupProbeResult result;
    for (const std::uint32_t elementBytes : {1u, 2u}) {
        TileMipLayout layout{};
        layout.width = ProbeWidth;
        layout.height = ProbeHeight;
        layout.pitchBytes = ProbeWidth * elementBytes;
        layout.linearSize = static_cast<std::uint64_t>(layout.pitchBytes) * ProbeHeight;
        layout.tiledSize = layout.linearSize;
        const auto bytes = static_cast<std::size_t>(layout.linearSize);
        Buffer source(context, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        Buffer destination(context, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        const auto expected = [&](std::size_t index) { return static_cast<std::byte>(Hash(static_cast<std::uint32_t>(index) + elementBytes * 0x9e3779b9u)); };
        for (std::size_t index = 0; index < bytes; ++index) {
            source.Bytes()[index] = expected(index);
            destination.Bytes()[index] = ~expected(index);
        }
        std::this_thread::sleep_for(ProbeIdle);
        CommandBatch batch(context);
        detiler.BeginBatch();
        detiler.Dispatch(batch.Handle(), TextureTileMode::kLinear, elementBytes, source.Handle(), 0, destination.Handle(), 0, layout);
        const VkMemoryBarrier toHost{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &toHost, 0, nullptr, 0, nullptr);
        batch.SubmitAndWait();
        for (std::size_t index = 0; index < bytes; ++index) {
            if (destination.Bytes()[index] != expected(index)) ++result.mismatches;
        }
        result.compared += static_cast<std::uint32_t>(bytes);
    }
    return result;
}

}
