#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_CONTROLFLOWINFO_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_CONTROLFLOWINFO_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "IntermediateRepresentation/IrValue.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace ShaderRecompiler {

struct BlockInfo {
    std::uint32_t id = 0;
    std::uint32_t startPc = 0;
    std::uint32_t endPc = 0;
    Terminator terminator;
    IrValue* condition = nullptr;
    IrValue* indirectTarget = nullptr;
};

struct DescriptorSource {
    // A T# loaded from a table buffer (`heapSource`) at `entryOffset + key * 32`, the key a
    // wave-uniform runtime value (handle argument `keyArg`). With `hasMaterial` the key is itself
    // `M[readfirstlane(i) * selectorStride + selectorOffset]` over `materialSource`, so the key
    // set can be enumerated from the material records (see ResourceMaterializer).
    struct IndirectImage {
        std::uint32_t materialSource = 0;
        std::uint32_t heapSource = 0;
        std::uint32_t selectorStride = 0;
        std::uint32_t selectorOffset = 0;
        std::uint32_t keyArg = 0;
        std::uint32_t entryOffset = 0;
        bool hasMaterial = false;

        bool operator==(const IndirectImage& other) const = default;
    };

    std::array<IrValue*, 8> dwords {};
    std::uint32_t dwordCount = 0;
    std::optional<IndirectImage> indirectImage;

    bool operator==(const DescriptorSource& other) const = default;
};

struct SrtRead {
    IrValue* value = nullptr;
    std::uint32_t flatOffset = 0;

    bool operator==(const SrtRead& other) const = default;
};

struct SrtReadPoison {
    std::uint32_t slot = 0;
    std::uint32_t pc = 0;
    std::uint64_t address = 0;

    bool operator==(const SrtReadPoison& other) const = default;
};

struct ResourceBlock {
    IrValue* condition = nullptr;
    std::vector<std::uint32_t> successors;
    std::vector<std::uint32_t> sources;
};

}

#endif
