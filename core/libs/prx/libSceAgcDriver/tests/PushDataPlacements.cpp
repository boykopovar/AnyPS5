#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "PushDataLayout.hpp"
#include "RuntimeAbi.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderInfo.hpp"
#include <cstdio>
#include <exception>
#include <array>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::CompiledShader;
using AgcDriver::Graphics::PushDataPlacements;
using ShaderRecompiler::ShaderStage;
using ShaderRecompiler::StagePushPlacement;
namespace Abi = ShaderRecompiler::RuntimeAbi;
using Item = Abi::RootItem;

void require(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error(what);
}

struct Stage {
    ShaderStage stage;
    std::uint32_t items;
    std::size_t pushed;
    std::uint32_t buffers = 0;
};

std::uint32_t bits(std::initializer_list<Item> items) {
    std::uint32_t mask = 0;
    for (const auto item : items) mask |= Abi::RootItemBit(item);
    return mask;
}

std::vector<StagePushPlacement> place(const std::vector<Stage>& stages) {
    static std::vector<ShaderRecompiler::RecompileResult> programs;
    programs.assign(stages.size(), {});
    std::vector<CompiledShader> shaders;
    for (std::size_t index = 0; index < stages.size(); ++index) {
        programs[index].dataAddressPush = true;
        programs[index].gpuResolvedImages = true;
        programs[index].pushItems = stages[index].items;
        programs[index].pushedData.resize(stages[index].pushed);
        programs[index].pushedBufferCount = stages[index].buffers;
        for (std::size_t dword = 0; dword < stages[index].pushed; ++dword) programs[index].pushedData[dword] = static_cast<std::uint32_t>(dword);
        shaders.push_back({stages[index].stage, &programs[index], 0u});
    }
    return PushDataPlacements(shaders);
}

void disjoint(const std::vector<Stage>& stages, const std::vector<StagePushPlacement>& placements, std::uint64_t occupied, const char* what) {
    std::uint64_t used = occupied;
    std::uint64_t globals = 0;
    for (std::size_t index = 0; index < placements.size(); ++index) {
        const auto claim = [&](std::uint32_t first, std::uint32_t width, bool shared) {
            for (auto dword = first; dword < first + width; ++dword) {
                require(dword < Abi::PushDataDwords, std::string("a push entry leaves the push data: ") + what);
                const auto bit = 1ull << dword;
                if (shared && (globals & bit) != 0u) continue;
                require((used & bit) == 0u, std::string("two push entries share a dword: ") + what);
                used |= bit;
                if (shared) globals |= bit;
            }
        };
        for (std::uint32_t item = 0; item < Abi::RootItemCount; ++item) {
            const bool wanted = (stages[index].items & (1u << item)) != 0u;
            const auto position = placements[index].items[item];
            const bool optional = static_cast<Item>(item) == Item::UserData;
            require(wanted == (position != Abi::NoPushPosition) || (optional && wanted), std::string("a stage's root items and its placement disagree: ") + what);
            if (position == Abi::NoPushPosition) continue;
            const auto kind = static_cast<Item>(item);
            if (Abi::RootAddressItem(kind)) require(position % 2u == 0u, std::string("a root address is not 8-byte aligned: ") + what);
            claim(position, Abi::RootAddressItem(kind) ? 2u : 1u, static_cast<std::uint32_t>(kind) < static_cast<std::uint32_t>(Abi::RootItem::ShaderData));
        }
        const auto& placement = placements[index];
        require(placement.pushedBuffers == stages[index].buffers, std::string("a stage's placement does not count its buffers: ") + what);
        bool overflowed = stages[index].buffers > Abi::PushedBufferLimit;
        for (std::uint32_t buffer = 0; buffer < Abi::PushedBufferLimit; ++buffer) {
            const auto root = placement.bufferRoots[buffer];
            if (buffer >= stages[index].buffers) require(root == Abi::NoPushPosition, std::string("a buffer the stage does not read has a root: ") + what);
            overflowed = overflowed || (buffer < stages[index].buffers && root == Abi::NoPushPosition);
            if (root == Abi::NoPushPosition) continue;
            require(root % 2u == 0u, std::string("a pushed buffer address is not 8-byte aligned: ") + what);
            claim(root, Abi::PushedBufferWords, false);
        }
        require(overflowed == (placement.bufferOverflow != Abi::NoPushPosition), std::string("a stage's overflow address and its unplaced buffers disagree: ") + what);
        if (placement.bufferOverflow != Abi::NoPushPosition) {
            require(placement.bufferOverflow % 2u == 0u, std::string("a buffer overflow address is not 8-byte aligned: ") + what);
            claim(placement.bufferOverflow, 2u, false);
        }
        claim(placements[index].pushed.base, placements[index].pushed.count, false);
        require(placements[index].pushed.count <= stages[index].pushed, std::string("a stage pushes more than it asked for: ") + what);
    }
}

}

int main() {
    try {
        const bool defaultCap = AgcDriver::Graphics::PushedBufferRecordCap() == Abi::PushedBufferLimit;
        const auto vertexItems = bits({Item::PagetableAddress, Item::ShaderData, Item::SrtAddress});
        const auto pixelItems = bits({Item::PagetableAddress, Item::ShaderData, Item::TableMap});
        {
            const std::vector<Stage> stages{{ShaderStage::Vertex, vertexItems, 6}, {ShaderStage::Fragment, pixelItems, 30}};
            const auto placements = place(stages);
            disjoint(stages, placements, 0, "vertex and pixel");
            require(placements[0].pushed.count == 6 && placements[1].pushed.count == 30, "stages that fit push everything they want");
            require(place({stages[1]})[0] == placements[1], "a pixel stage that fits is placed as it is alone, so its fragment library is shared");
            require(placements[0].Item(Item::PagetableAddress) == placements[1].Item(Item::PagetableAddress), "the pre-rasterization and fragment parts share their globals");
            const std::vector<Stage> fault{{ShaderStage::Vertex, vertexItems | bits({Item::FaultAddress, Item::GdsAddress}), 6}, {ShaderStage::Fragment, pixelItems, 30}};
            disjoint(fault, place(fault), 0, "globals only the pre-rasterization part reads");
            require(place(fault)[1] == place({fault[1]})[0], "globals only the pre-rasterization part reads do not move the fragment part's roots");
        }
        {
            const auto user = Abi::RootItemBit(Item::UserData);
            const std::vector<Stage> stages{{ShaderStage::Vertex, vertexItems | user, 6, 4}, {ShaderStage::Fragment, pixelItems | user, 30, 4}};
            const auto placements = place(stages);
            disjoint(stages, placements, 0, "vertex and pixel stages with user-data roots");
            require(placements[0].Item(Item::UserData) != Abi::NoPushPosition && placements[1].Item(Item::UserData) != Abi::NoPushPosition, "stages with room place their user-data roots");
            const std::vector<Stage> without{{ShaderStage::Vertex, vertexItems, 6, 4}, {ShaderStage::Fragment, pixelItems, 30, 4}};
            const auto plain = place(without);
            for (std::size_t index = 0; index < plain.size(); ++index) {
                for (std::uint32_t item = 0; item < Abi::RootItemCount; ++item) {
                    if (static_cast<Item>(item) != Item::UserData) require(plain[index].items[item] == placements[index].items[item], "a user-data root moves another root item");
                }
                require(plain[index].bufferRoots == placements[index].bufferRoots, "a user-data root moves a pushed buffer");
            }
            const auto pushedTotal = [](const std::vector<StagePushPlacement>& stages) {
                std::size_t total = 0;
                for (const auto& stage : stages) total += stage.pushed.count;
                return total;
            };
            require(pushedTotal(placements) + 2u * placements.size() >= pushedTotal(plain), "a user-data root takes more than its own dwords from pushed data");
        }
        {
            const std::vector<Stage> stages{{ShaderStage::Vertex, vertexItems, 40}, {ShaderStage::Fragment, pixelItems, 40}};
            const auto placements = place(stages);
            disjoint(stages, placements, 0, "stages that want too much");
            require(placements[0].pushed.count == 27 && placements[1].pushed.count == 27, "stages that want too much share the room evenly");
        }
        {
            const std::vector<Stage> stages{{ShaderStage::Local, vertexItems, 4}, {ShaderStage::TessellationControl, bits({Item::PagetableAddress, Item::ShaderData}), 3}, {ShaderStage::TessellationEvaluation, vertexItems, 10}, {ShaderStage::Fragment, pixelItems, 48}};
            const auto placements = place(stages);
            disjoint(stages, placements, ShaderRecompiler::PushFeatureDwords(false, true, false), "a tessellation pipeline");
            require(placements[0].Item(Item::PagetableAddress) == placements[1].Item(Item::PagetableAddress), "pre-rasterization stages share their part's globals");
        }
        {
            const std::vector<Stage> stages{{ShaderStage::Mesh, vertexItems, 8}, {ShaderStage::Fragment, pixelItems, 20}};
            disjoint(stages, place(stages), ShaderRecompiler::PushFeatureDwords(true, false, false), "a mesh pipeline");
        }
        {
            const std::vector<Stage> stages{{ShaderStage::Compute, bits({Item::PagetableAddress, Item::GdsAddress, Item::ShaderData}), 60}};
            const auto placements = place(stages);
            disjoint(stages, placements, ShaderRecompiler::StagePushFeatures(ShaderStage::Compute, 0u), "a compute stage");
            require(placements[0] == ShaderRecompiler::StandalonePushPlacement(ShaderStage::Compute, stages[0].items, stages[0].pushed), "the compiler's standalone compute placement is the driver's");
        }
        {
            std::vector<ShaderRecompiler::RecompileResult> programs(2);
            programs[0].dataAddressPush = true;
            programs[0].gpuResolvedImages = true;
            programs[0].pushItems = vertexItems;
            programs[0].pushedData.resize(48);
            programs[1].pushConstants.resize(12);
            programs[1].stencilReferences.push_back(1u);
            programs[1].stencilReferenceDword = 2u;
            const std::array<CompiledShader, 2> shaders{CompiledShader{ShaderStage::Vertex, &programs[0], 0u}, CompiledShader{ShaderStage::Fragment, &programs[1], 128u}};
            const auto placements = PushDataPlacements(shaders);
            const auto stencil = 128u / 4u + programs[1].stencilReferenceDword;
            const auto pushed = placements[0].pushed;
            require(stencil < pushed.base || stencil >= pushed.base + pushed.count, "a pushed run covers a per-draw stage's stencil reference");
        }
        {
            std::vector<ShaderRecompiler::RecompileResult> programs(3);
            for (const std::size_t index : {0u, 2u}) {
                programs[index].dataAddressPush = true;
                programs[index].gpuResolvedImages = true;
            }
            programs[0].pushItems = vertexItems;
            programs[0].pushedData.resize(6);
            programs[1].pushConstants.resize(8);
            programs[2].pushItems = pixelItems;
            programs[2].pushedData.resize(20);
            const std::array<CompiledShader, 3> shaders{CompiledShader{ShaderStage::Vertex, &programs[0], 0u}, CompiledShader{ShaderStage::TessellationControl, &programs[1], 200u}, CompiledShader{ShaderStage::Fragment, &programs[2], 0u}};
            const auto placements = PushDataPlacements(shaders);
            const std::vector<Stage> stages{{ShaderStage::Vertex, vertexItems, 6}, {ShaderStage::TessellationControl, 0u, 0}, {ShaderStage::Fragment, pixelItems, 20}};
            disjoint(stages, placements, ShaderRecompiler::PushBytesMask(200u, 8u) | ShaderRecompiler::PushFeatureDwords(false, true, false), "a pixel stage wider than the top free run");
            require(placements[2].pushed.count == 20, "a pixel stage wider than the top free run takes a lower run instead of being cut");
        }
        {
            const auto all = (1u << Abi::RootItemCount) - 1u;
            const auto stencil = Abi::RootItemBit(Item::StencilReference);
            const std::vector<Stage> stages{{ShaderStage::Vertex, (all & ~stencil) | Abi::DepthRemapPushBit, 48, Abi::PushedBufferLimit}, {ShaderStage::Geometry, 0u, 0}, {ShaderStage::Fragment, all, 48, Abi::PushedBufferLimit}};
            const auto placements = place(stages);
            disjoint(stages, placements, ShaderRecompiler::PushFeatureDwords(true, false, true), "vertex and pixel stages with every root item and the most pushed buffers");
            require(place({stages[2]})[0].bufferRoots == placements[2].bufferRoots, "a pixel stage's buffer root is placed as it is alone, so its fragment library is shared");
        }
        {
            const auto all = (1u << Abi::RootItemCount) - 1u;
            const auto stencil = Abi::RootItemBit(Item::StencilReference);
            const std::vector<Stage> stages{{ShaderStage::Local, all & ~stencil, 4}, {ShaderStage::TessellationControl, all & ~stencil, 3}, {ShaderStage::TessellationEvaluation, (all & ~stencil) | Abi::DepthRemapPushBit, 10}, {ShaderStage::Fragment, all, 48, Abi::PushedBufferLimit}};
            disjoint(stages, place(stages), ShaderRecompiler::PushFeatureDwords(false, true, true), "a tessellation pipeline whose stages hold every root item and whose pixel stage pushes the most buffers");
        }
        {
            const auto all = (1u << Abi::RootItemCount) - 1u;
            const std::vector<Stage> stages{{ShaderStage::Compute, all & ~Abi::RootItemBit(Item::StencilReference), 60, Abi::PushedBufferLimit}};
            const auto placements = place(stages);
            disjoint(stages, placements, ShaderRecompiler::StagePushFeatures(ShaderStage::Compute, 0u), "a compute stage with every root item and the most pushed buffers");
            require(!defaultCap || placements[0] == ShaderRecompiler::StandalonePushPlacement(ShaderStage::Compute, stages[0].items, stages[0].pushed, stages[0].buffers), "the compiler's standalone compute placement with pushed buffers is the driver's");
        }
        {
            namespace Ids = ShaderRecompiler::PipelineSpecialization;
            for (std::uint32_t buffer = 0; buffer <= Ids::BufferRootCount; ++buffer) {
                const auto id = Ids::BufferRootPosition + buffer;
                require(id >= Ids::SamplerLodBiasBase + Abi::SamplerHeapCapacity && id < Ids::ImageComponentBase, "a buffer root specialization id lies inside another constant's range");
                require(Ids::IsPushPlacement(id), "a buffer root specialization id is not a push placement");
            }
            for (std::uint32_t id = Ids::StoreQuantizeBase; id < Ids::DescriptorIndexBase; ++id) require(!Ids::IsPushPlacement(id), "a store quantization specialization id counts as a push placement");
            for (std::uint32_t id = Ids::HeapCountBase; id < Ids::MipCountBase + ShaderRecompiler::ShaderInfo::MaxImages; ++id) require(!Ids::IsPushPlacement(id), "a heap or mip count specialization id counts as a push placement");
            for (std::uint32_t item = 0; item < Abi::RootItemCount; ++item) {
                const auto id = Ids::RootItemPositionBase + item;
                require(id >= Ids::MipCountBase + ShaderRecompiler::ShaderInfo::MaxImages && id < Ids::StoreQuantizeBase, "a root item specialization id lies inside another constant's range");
                require(Ids::IsPushPlacement(id), "a root item specialization id is not a push placement");
            }
        }
        {
            const auto all = (1u << Abi::RootItemCount) - 1u;
            const auto stencil = Abi::RootItemBit(Item::StencilReference);
            for (const std::uint32_t buffers : {1u, 4u, 5u, 9u, 40u}) {
                const std::vector<Stage> worst{{ShaderStage::Vertex, (all & ~stencil) | Abi::DepthRemapPushBit, 48, buffers}, {ShaderStage::Geometry, all & ~stencil, 8, buffers}, {ShaderStage::Fragment, all, 48, buffers}};
                const auto placements = place(worst);
                disjoint(worst, placements, ShaderRecompiler::PushFeatureDwords(true, false, true), ("stages with every root item and " + std::to_string(buffers) + " buffers each").c_str());
                const std::vector<Stage> typical{{ShaderStage::Vertex, vertexItems, 6, buffers}, {ShaderStage::Fragment, pixelItems, 20, buffers}};
                const auto roomy = place(typical);
                disjoint(typical, roomy, 0, ("typical stages with " + std::to_string(buffers) + " buffers each").c_str());
                require(!defaultCap || (buffers > Abi::PushedBufferLimit) == (roomy[1].bufferOverflow != Abi::NoPushPosition), "a typical pixel stage overflows only buffers past the push limit");
                const auto alone = place({typical[1]})[0];
                require(alone.items == roomy[1].items && alone.bufferRoots == roomy[1].bufferRoots && alone.bufferOverflow == roomy[1].bufferOverflow, "a typical pixel stage with buffers is placed as it is alone, so its fragment library is shared");
                const std::vector<Stage> tessellation{{ShaderStage::Local, all & ~stencil, 4, buffers}, {ShaderStage::TessellationControl, all & ~stencil, 3, buffers}, {ShaderStage::TessellationEvaluation, (all & ~stencil) | Abi::DepthRemapPushBit, 10, buffers}, {ShaderStage::Fragment, all, 48, buffers}};
                disjoint(tessellation, place(tessellation), ShaderRecompiler::PushFeatureDwords(false, true, true), ("a tessellation pipeline with " + std::to_string(buffers) + " buffers per stage").c_str());
                const std::vector<Stage> compute{{ShaderStage::Compute, all & ~stencil, 60, buffers}};
                const auto computed = place(compute);
                disjoint(compute, computed, ShaderRecompiler::StagePushFeatures(ShaderStage::Compute, 0u), ("a compute stage with " + std::to_string(buffers) + " buffers").c_str());
                require(!defaultCap || computed[0] == ShaderRecompiler::StandalonePushPlacement(ShaderStage::Compute, compute[0].items, compute[0].pushed, buffers), "the compiler's standalone compute placement with overflowing buffers is the driver's");
            }
        }
        std::puts("push data placements passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
