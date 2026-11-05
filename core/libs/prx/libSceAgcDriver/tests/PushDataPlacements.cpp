#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "PushDataLayout.hpp"
#include "RuntimeAbi.hpp"
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
            require(wanted == (position != Abi::NoPushPosition), std::string("a stage's root items and its placement disagree: ") + what);
            if (!wanted) continue;
            const auto kind = static_cast<Item>(item);
            if (Abi::RootAddressItem(kind)) require(position % 2u == 0u, std::string("a root address is not 8-byte aligned: ") + what);
            claim(position, Abi::RootAddressItem(kind) ? 2u : 1u, static_cast<std::uint32_t>(kind) < static_cast<std::uint32_t>(Abi::RootItem::ShaderData));
        }
        claim(placements[index].pushed.base, placements[index].pushed.count, false);
        require(placements[index].pushed.count <= stages[index].pushed, std::string("a stage pushes more than it asked for: ") + what);
    }
}

}

int main() {
    try {
        const auto vertexItems = bits({Item::RingAddress, Item::ShaderData, Item::FlattenedSrt});
        const auto pixelItems = bits({Item::RingAddress, Item::ShaderData, Item::TableMap});
        {
            const std::vector<Stage> stages{{ShaderStage::Vertex, vertexItems, 6}, {ShaderStage::Fragment, pixelItems, 30}};
            const auto placements = place(stages);
            disjoint(stages, placements, 0, "vertex and pixel");
            require(placements[0].pushed.count == 6 && placements[1].pushed.count == 30, "stages that fit push everything they want");
            require(place({stages[1]})[0] == placements[1], "a pixel stage that fits is placed as it is alone, so its fragment library is shared");
            require(placements[0].Item(Item::RingAddress) != placements[1].Item(Item::RingAddress), "the pre-rasterization and fragment parts hold their own globals");
        }
        {
            const std::vector<Stage> stages{{ShaderStage::Vertex, vertexItems, 40}, {ShaderStage::Fragment, pixelItems, 40}};
            const auto placements = place(stages);
            disjoint(stages, placements, 0, "stages that want too much");
            require(placements[0].pushed.count == 28 && placements[1].pushed.count == 28, "stages that want too much share the room evenly");
        }
        {
            const std::vector<Stage> stages{{ShaderStage::Local, vertexItems, 4}, {ShaderStage::TessellationControl, bits({Item::RingAddress, Item::ShaderData}), 3}, {ShaderStage::TessellationEvaluation, vertexItems, 10}, {ShaderStage::Fragment, pixelItems, 48}};
            const auto placements = place(stages);
            disjoint(stages, placements, ShaderRecompiler::PushFeatureDwords(false, true, false), "a tessellation pipeline");
            require(placements[0].Item(Item::RingAddress) == placements[1].Item(Item::RingAddress), "pre-rasterization stages share their part's globals");
        }
        {
            const std::vector<Stage> stages{{ShaderStage::Mesh, vertexItems, 8}, {ShaderStage::Fragment, pixelItems, 20}};
            disjoint(stages, place(stages), ShaderRecompiler::PushFeatureDwords(true, false, false), "a mesh pipeline");
        }
        {
            const std::vector<Stage> stages{{ShaderStage::Compute, bits({Item::RingAddress, Item::GdsAddress, Item::ShaderData}), 60}};
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
        std::puts("push data placements passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
