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

struct KeyDomain {
    std::uint32_t source = 0;
    std::uint32_t offset = 0;
    std::uint32_t stride = 4;
    bool value = false;

    bool operator==(const KeyDomain& other) const = default;
};

struct PointerHop {
    std::uint32_t offset = 0;
    std::uint32_t immediate = 0;

    bool operator==(const PointerHop& other) const = default;
};

struct TableColumn {
    std::uint32_t heapSource = 0;
    std::uint32_t stride = 0;
    std::uint32_t addend = 0;
    std::uint32_t offset = 0;
    std::uint32_t dwordCount = 0;
    std::uint32_t maxKey = 0xffffffffu;
    bool sampler = false;
    bool address = false;
    bool buffer = false;
    std::vector<KeyDomain> keyDomains;
    bool unkeyed = false;
    bool indirect = false;
    std::uint32_t indirectOffset = 0;
    std::uint32_t indirectImmediate = 0;
    std::vector<PointerHop> pointerHops;
    std::uint32_t foldedAddend = 0;
    std::uint32_t foldedCarry = 0;
    bool selected = false;
    std::uint32_t selection = 0;

    bool operator==(const TableColumn& other) const {
        return heapSource == other.heapSource && stride == other.stride && addend == other.addend && offset == other.offset && dwordCount == other.dwordCount && sampler == other.sampler && address == other.address && buffer == other.buffer && indirect == other.indirect && indirectOffset == other.indirectOffset && indirectImmediate == other.indirectImmediate && pointerHops == other.pointerHops && foldedAddend == other.foldedAddend && foldedCarry == other.foldedCarry && selected == other.selected && selection == other.selection;
    }
};

struct DescriptorSource {
    std::array<IrValue*, 8> dwords {};
    std::uint32_t dwordCount = 0;
    std::optional<TableColumn> tableColumn;

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
    std::vector<std::uint32_t> images;
    std::vector<std::uint32_t> flatSlots;
};

}

#endif
