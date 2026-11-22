#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PATCHVERTEXLAYER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PATCHVERTEXLAYER_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <vector>

namespace AgcDriver::Graphics {

[[nodiscard]] inline std::vector<std::uint32_t> DemoteVertexLayerOutput(std::span<const std::uint32_t> words) {
    if (words.size() < 5u || words[0] != spv::MagicNumber) throw std::runtime_error("Vertex layer output: malformed vertex SPIR-V");
    std::uint32_t layer = 0u;
    for (std::size_t i = 5; i < words.size();) {
        const auto count = words[i] >> 16u;
        if (count == 0u || i + count > words.size()) throw std::runtime_error("Vertex layer output: malformed vertex SPIR-V");
        if ((words[i] & 0xffffu) == spv::OpDecorate && count == 4u && words[i + 2] == spv::DecorationBuiltIn && words[i + 3] == spv::BuiltInLayer) {
            if (layer != 0u) throw std::runtime_error("Vertex layer output: more than one gl_Layer decoration");
            layer = words[i + 1];
        }
        i += count;
    }
    if (layer == 0u) return {};
    std::size_t variable = 0u;
    std::uint32_t outputPointer = 0u;
    for (std::size_t i = 5; i < words.size(); i += words[i] >> 16u) {
        if ((words[i] & 0xffffu) == spv::OpVariable && (words[i] >> 16u) >= 4u && words[i + 2] == layer) {
            if (words[i + 3] != spv::StorageClassOutput) throw std::runtime_error("Vertex layer output: gl_Layer is not an output variable");
            variable = i;
            outputPointer = words[i + 1];
        }
    }
    if (variable == 0u) throw std::runtime_error("Vertex layer output: gl_Layer has no variable");
    std::size_t pointerAt = 0u;
    std::uint32_t pointee = 0u;
    for (std::size_t i = 5; i < words.size(); i += words[i] >> 16u) {
        if ((words[i] & 0xffffu) == spv::OpTypePointer && (words[i] >> 16u) == 4u && words[i + 1] == outputPointer) {
            pointerAt = i;
            pointee = words[i + 3];
        }
    }
    if (pointerAt == 0u || words[pointerAt + 2] != spv::StorageClassOutput) throw std::runtime_error("Vertex layer output: gl_Layer has no output pointer type");
    std::uint32_t privatePointer = 0u;
    for (std::size_t i = 5; i < variable && privatePointer == 0u; i += words[i] >> 16u) {
        if ((words[i] & 0xffffu) == spv::OpTypePointer && (words[i] >> 16u) == 4u && words[i + 2] == spv::StorageClassPrivate && words[i + 3] == pointee) privatePointer = words[i + 1];
    }
    const bool declarePointer = privatePointer == 0u;
    const bool interfaceListsPrivate = words[1] >= 0x00010400u;
    std::vector<std::uint32_t> result(words.begin(), words.begin() + 5);
    result.reserve(words.size() + 4u);
    if (declarePointer) privatePointer = result[3]++;
    for (std::size_t i = 5; i < words.size();) {
        const auto count = words[i] >> 16u;
        const auto opcode = words[i] & 0xffffu;
        if (opcode == spv::OpDecorate && words[i + 1] == layer) {
            i += count;
            continue;
        }
        if (opcode == spv::OpEntryPoint && !interfaceListsPrivate) {
            const auto start = result.size();
            result.push_back(0u);
            std::size_t operand = i + 3;
            while (operand < i + count) {
                const auto word = words[operand++];
                if ((word & 0xff000000u) == 0u || (word & 0x00ff0000u) == 0u || (word & 0x0000ff00u) == 0u || (word & 0x000000ffu) == 0u) break;
            }
            result.insert(result.end(), words.begin() + static_cast<std::ptrdiff_t>(i + 1), words.begin() + static_cast<std::ptrdiff_t>(operand));
            for (; operand < i + count; ++operand) {
                if (words[operand] != layer) result.push_back(words[operand]);
            }
            result[start] = (static_cast<std::uint32_t>(result.size() - start) << 16u) | spv::OpEntryPoint;
            i += count;
            continue;
        }
        result.insert(result.end(), words.begin() + static_cast<std::ptrdiff_t>(i), words.begin() + static_cast<std::ptrdiff_t>(i + count));
        if (i == variable) {
            result[result.size() - count + 1] = privatePointer;
            result[result.size() - count + 3] = spv::StorageClassPrivate;
        }
        if (i == pointerAt && declarePointer) {
            result.insert(result.end(), {(4u << 16u) | spv::OpTypePointer, privatePointer, spv::StorageClassPrivate, pointee});
        }
        i += count;
    }
    return result;
}

}

#endif
