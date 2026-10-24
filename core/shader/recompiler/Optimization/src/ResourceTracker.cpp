#include "Optimization/ResourceTracker.hpp"
#include "Optimization/SrtWalker.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/SrtWalker/SrtInstructionPredicates.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::uint32_t samplerBorderClampMask = (1u << 2u) | (1u << 5u) | (1u << 8u);
constexpr std::uint32_t samplerDword3ReservedMask = 0x3ffff000u;

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

// Debug aid: APS5_TRACE_BDA=1 names the accesses that make a program address-based (see Collect).
bool bdaTraceEnabled() {
    static const bool enabled = std::getenv("APS5_TRACE_BDA") != nullptr;
    return enabled;
}
constexpr unsigned bdaTraceLimit = 8;

std::string formatHex32(std::uint32_t value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string hex(8u, '0');
    for (std::uint32_t index = 0; index < 8u; index++) {
        hex[7u - index] = digits[(value >> (index * 4u)) & 0xfu];
    }
    return hex;
}

std::string describeValueChain(const IrValue* value, std::uint32_t depth) {
    value = value->Resolve();
    if (value->HasImmediate()) {
        return "Immediate";
    }
    std::string text = std::string(IrOpcodeName(value->Opcode()));
    if (value->Opcode() == IrOpcode::LoadAddressU32 || value->Opcode() == IrOpcode::ReadConstBuffer) {
        text += "@pc=0x" + formatHex32(value->Flags<MemoryFlags>().pc);
    }
    if (depth == 0u || value->ArgumentCount() == 0u) {
        return text;
    }
    text += "(";
    for (std::uint32_t index = 0; index < value->ArgumentCount(); index++) {
        if (index != 0u) {
            text += ", ";
        }
        text += describeValueChain(value->Argument(index), depth - 1u);
    }
    text += ")";
    return text;
}

std::uint32_t possibleU32Bits(const IrValue* value) {
    value = value->Resolve();
    if (value->HasImmediate()) {
        return value->Type() == IrType::U32 ? value->ImmediateU32() : std::numeric_limits<std::uint32_t>::max();
    }
    switch (value->Opcode()) {
        case IrOpcode::BitwiseAnd32:
            return possibleU32Bits(value->Argument(0)) & possibleU32Bits(value->Argument(1));
        case IrOpcode::BitwiseOr32:
            return possibleU32Bits(value->Argument(0)) | possibleU32Bits(value->Argument(1));
        case IrOpcode::ShiftLeftLogical32: {
            const IrValue* shift = value->Argument(1)->Resolve();
            return shift->HasImmediate() && shift->Type() == IrType::U32 ? possibleU32Bits(value->Argument(0)) << (shift->ImmediateU32() & 31u) : std::numeric_limits<std::uint32_t>::max();
        }
        case IrOpcode::ReadFirstLane:
            return value->ArgumentCount() >= 1u ? possibleU32Bits(value->Argument(0)) : std::numeric_limits<std::uint32_t>::max();
        case IrOpcode::SelectU32:
            return value->ArgumentCount() == 3u ? (possibleU32Bits(value->Argument(1)) | possibleU32Bits(value->Argument(2))) : std::numeric_limits<std::uint32_t>::max();
        default:
            return std::numeric_limits<std::uint32_t>::max();
    }
}

std::uint32_t byteExtent(const MemoryInfo& memory) {
    const auto bytes = std::max((memory.dataBits + 7u) / 8u, 1u);
    const auto count = std::max(memory.dataDwords, 1u);
    const auto end = static_cast<std::uint64_t>(memory.offset) + static_cast<std::uint64_t>(bytes) * count;
    return end > std::numeric_limits<std::uint32_t>::max() ? std::numeric_limits<std::uint32_t>::max() : static_cast<std::uint32_t>(end);
}

class Tracker {
public:
    explicit Tracker(IrProgram& program) : m_program(program), m_info(program.Resources().info), m_builder(program) {
        m_info.buffers.clear();
        m_info.images.clear();
        m_info.samplers.clear();
        m_info.sampledPairs.clear();
        m_info.usesDma = false;
        m_info.bdaWrites = false;
    }

    void Run() {
        if (m_program.Resources().resourceTrackingComplete) {
            fail("resources already tracked");
        }
        if (!m_program.Resources().srtPlanComplete) {
            fail("SRT plan is not ready");
        }
        SplitDescriptorPhis();
        PlanTableColumns();
        std::unordered_map<const IrBlock*, std::uint32_t> blockIndices;
        for (const IrBlock* block : m_program.BlockOrder()) {
            blockIndices.emplace(block, static_cast<std::uint32_t>(blockIndices.size()));
        }
        m_blockSources.assign(blockIndices.size(), {});
        m_blockImages.assign(blockIndices.size(), {});
        for (auto& block : m_program.Blocks()) {
            const auto found = blockIndices.find(block.get());
            m_currentBlock = found == blockIndices.end() ? std::numeric_limits<std::uint32_t>::max() : found->second;
            for (IrValue* inst : block->Instructions()) {
                Collect(*inst);
            }
        }
        m_currentBlock = std::numeric_limits<std::uint32_t>::max();
        PlanResourceControlFlow();
        if (m_bdaTraces > bdaTraceLimit) std::fprintf(stderr, "[bda] %u more address accesses in this program not shown\n", m_bdaTraces - bdaTraceLimit);
        CheckTableStores();
        AssignTables();
        LinkImageAliases();
        for (const auto& patch : m_handlePatches) {
            patch.handle->SetFlags<std::uint32_t>(patch.resource);
        }
        for (const auto& patch : m_memoryPatches) {
            auto& memory = m_program.Resources().memoryInfo[patch.index];
            memory.resource = patch.resource;
            if (patch.hasSampler) {
                memory.sampler = patch.sampler;
            }
        }
        std::vector<const IrValue*> referenced;
        for (const auto& plan : m_tableColumns) {
            const auto& column = *m_sources[plan.source].tableColumn;
            IrValue* offset = column.selected ? &m_builder.Constant(column.addend) : plan.offset;
            for (std::uint32_t argument = 0; argument < plan.handle->ArgumentCount(); argument++) {
                plan.handle->ReplaceArgument(argument, offset);
            }
            KeepSourceAlive(*plan.handle, m_sources[column.heapSource], referenced);
            if (column.selected) {
                KeepSourceAlive(*plan.handle, m_sources[column.selection], referenced);
            }
            if (plan.keyDomain.has_value()) {
                KeepSourceAlive(*plan.handle, m_sources[plan.keyDomain->source], referenced);
            }
        }
        for (const auto index : m_tablePlanningMemory) {
            m_program.Resources().memoryInfo[index].planningOnly = true;
        }
        std::erase_if(m_program.Metadata().dynamicReads, [&](IrValue* value) {
            const IrValue* inst = value->Resolve();
            return std::ranges::find(m_tablePlanningReads, inst) != m_tablePlanningReads.end();
        });
        m_program.Resources().descriptorSources = std::move(m_sources);
        m_program.Resources().info = std::move(m_info);
        m_program.Resources().resourceTrackingComplete = true;
    }

private:
    struct HandlePatch {
        IrValue* handle = nullptr;
        std::uint32_t resource = 0;
    };

    struct MemoryPatch {
        std::uint32_t index = 0;
        std::uint32_t resource = 0;
        std::uint32_t sampler = 0;
        bool hasSampler = false;
    };

    struct TableColumnPlan {
        IrValue* handle = nullptr;
        std::uint32_t source = 0;
        IrValue* offset = nullptr;
        std::vector<std::uint32_t> memory;
        std::vector<const IrValue*> reads;
        std::vector<std::uint32_t> hopMemory;
        std::vector<const IrValue*> hopReads;
        std::optional<KeyDomain> keyDomain;
    };

    void MakeSource(const IrValue& handle, std::uint32_t width, bool sampler, bool sampleAdjust, DescriptorSource& descriptor) {
        if (handle.ArgumentCount() != width) {
            fail(std::string(IrOpcodeName(handle.Opcode())) + " has " + std::to_string(handle.ArgumentCount()) + " descriptor dwords, expected " + std::to_string(width));
        }
        descriptor.dwordCount = width;
        for (std::uint32_t i = 0; i < width; i++) {
            descriptor.dwords[i] = handle.Argument(i)->Resolve();
        }
        if (sampleAdjust) {
            descriptor.dwords[3] = canonicalizeSampleAdjustDword3(descriptor.dwords[3]);
        }
        const IrValue* dword0 = descriptor.dwords[0]->Resolve();
        if (sampler && dword0->HasImmediate() && dword0->Type() == IrType::U32 && (dword0->ImmediateU32() & samplerBorderClampMask) == 0u) {
            descriptor.dwords[3] = &m_builder.Constant(0u);
        }
    }

    IrValue* canonicalizeSampleAdjustDword3(IrValue* value) {
        for (;;) {
            value = value->Resolve();
            if ((possibleU32Bits(value) & ~samplerDword3ReservedMask) == 0u) {
                return &m_builder.Constant(0u);
            }
            if (value->Opcode() != IrOpcode::BitwiseOr32) {
                return value;
            }
            IrValue* left = value->Argument(0)->Resolve();
            IrValue* right = value->Argument(1)->Resolve();
            const bool leftReserved = (possibleU32Bits(left) & ~samplerDword3ReservedMask) == 0u;
            const bool rightReserved = (possibleU32Bits(right) & ~samplerDword3ReservedMask) == 0u;
            if (leftReserved) {
                value = right;
            } else if (rightReserved) {
                value = left;
            } else {
                return value;
            }
        }
    }

    bool ValidateSource(const DescriptorSource& descriptor, std::uint32_t& badDword) const {
        constexpr SrtWalker walker;
        for (std::uint32_t i = 0; i < descriptor.dwordCount; i++) {
            badDword = i;
            if (descriptor.dwords[i]->Resolve()->Type() != IrType::U32) {
                return false;
            }
            if (!walker.ValidateRuntimeValue(m_program.Resources(), descriptor.dwords[i])) {
                return false;
            }
        }
        return true;
    }

    std::uint32_t InternSource(const DescriptorSource& descriptor) {
        for (std::uint32_t candidate = 0; candidate < m_sources.size(); candidate++) {
            const auto& current = m_sources[candidate];
            if (current.dwordCount != descriptor.dwordCount || current.tableColumn != descriptor.tableColumn) {
                continue;
            }
            bool same = true;
            for (std::uint32_t i = 0; i < descriptor.dwordCount; i++) {
                same = same && EquivalentValue(m_program.Resources(), current.dwords[i], descriptor.dwords[i]);
            }
            if (same) {
                NoteBlockSource(candidate);
                return candidate;
            }
        }
        m_sources.push_back(descriptor);
        NoteBlockSource(static_cast<std::uint32_t>(m_sources.size() - 1));
        return static_cast<std::uint32_t>(m_sources.size() - 1);
    }

    void NoteBlockImage(std::uint32_t image) {
        if (m_currentBlock >= m_blockImages.size()) {
            return;
        }
        auto& images = m_blockImages[m_currentBlock];
        if (std::ranges::find(images, image) == images.end()) {
            images.push_back(image);
        }
    }

    void NoteBlockSource(std::uint32_t source) {
        if (m_currentBlock >= m_blockSources.size()) {
            return;
        }
        auto& sources = m_blockSources[m_currentBlock];
        if (std::ranges::find(sources, source) == sources.end()) {
            sources.push_back(source);
        }
    }

    void PlanResourceControlFlow() {
        const auto& order = m_program.BlockOrder();
        const auto& info = m_program.Metadata().blockInfo;
        if (order.empty() || order.size() != info.size()) {
            return;
        }
        std::unordered_map<const IrBlock*, std::uint32_t> indexOf;
        std::unordered_map<std::uint32_t, std::uint32_t> idToIndex;
        for (std::uint32_t index = 0; index < order.size(); index++) {
            indexOf.emplace(order[index], index);
            idToIndex.emplace(info[index].id, index);
        }
        std::vector<ResourceBlock> controlFlow(order.size());
        for (std::uint32_t index = 0; index < order.size(); index++) {
            auto& block = controlFlow[index];
            const auto& terminator = info[index].terminator;
            const auto target = [&](std::uint32_t id) { return idToIndex.find(id); };
            const auto trueEdge = target(terminator.trueBlock);
            const auto falseEdge = target(terminator.falseBlock);
            if (terminator.kind == TerminatorKind::ConditionalBranch && !terminator.loopHeader && info[index].condition != nullptr && trueEdge != idToIndex.end() && falseEdge != idToIndex.end()) {
                block.condition = info[index].condition;
                block.successors = {trueEdge->second, falseEdge->second};
            } else if (terminator.kind != TerminatorKind::Return) {
                for (const IrBlock* successor : order[index]->Successors()) {
                    const auto found = indexOf.find(successor);
                    if (found == indexOf.end()) {
                        return;
                    }
                    block.successors.push_back(found->second);
                }
            }
            block.sources = m_blockSources[index];
            block.images = m_blockImages[index];
            for (const IrValue* inst : order[index]->Instructions()) {
                if (inst->Opcode() != IrOpcode::ReadConst || inst->ArgumentCount() != 2u || inst->Argument(0)->Resolve()->Opcode() != IrOpcode::GetSrtResource) continue;
                const IrValue* slot = inst->Argument(1)->Resolve();
                if (!slot->HasImmediate() || slot->Type() != IrType::U32 || slot->ImmediateU32() >= m_program.Resources().srtReads.size()) fail("resource control flow has an invalid flat SRT slot");
                if (std::ranges::find(block.flatSlots, slot->ImmediateU32()) == block.flatSlots.end()) block.flatSlots.push_back(slot->ImmediateU32());
            }
        }
        std::vector<std::uint8_t> unconditional(order.size());
        for (std::uint32_t index = 0; index < order.size() && unconditional[index] == 0u;) {
            unconditional[index] = 1u;
            if (controlFlow[index].successors.size() != 1u) {
                break;
            }
            index = controlFlow[index].successors.front();
        }
        std::vector<std::uint8_t> reached(m_sources.size());
        std::vector<std::uint8_t> reachedImages(m_info.images.size());
        std::vector<std::uint8_t> reachedFlat(m_program.Resources().srtReads.size());
        for (std::uint32_t index = 0; index < order.size(); index++) {
            if (unconditional[index] == 0u) {
                continue;
            }
            for (const auto source : controlFlow[index].sources) {
                reached[source] = 1u;
            }
            for (const auto image : controlFlow[index].images) {
                reachedImages[image] = 1u;
            }
            for (const auto slot : controlFlow[index].flatSlots) {
                reachedFlat[slot] = 1u;
            }
        }
        bool guarded = false;
        for (const auto& block : controlFlow) {
            for (const auto source : block.sources) {
                guarded = guarded || reached[source] == 0u;
            }
            for (const auto image : block.images) {
                guarded = guarded || reachedImages[image] == 0u;
            }
            for (const auto slot : block.flatSlots) {
                guarded = guarded || reachedFlat[slot] == 0u;
            }
        }
        if (guarded) {
            m_program.Resources().controlFlow = std::move(controlFlow);
        }
    }

    static bool immediateU32(IrValue* value, std::uint32_t& result) {
        value = value->Resolve();
        if (!value->HasImmediate() || value->Type() != IrType::U32) {
            return false;
        }
        result = value->ImmediateU32();
        return true;
    }

    const MemoryInfo* ScalarReadMemory(const IrValue& read, std::uint32_t& index) const {
        if (read.Opcode() != IrOpcode::ReadConstBuffer || read.ArgumentCount() != 2u) {
            return nullptr;
        }
        index = read.Flags<MemoryFlags>().index;
        if (index >= m_program.Resources().memoryInfo.size()) {
            return nullptr;
        }
        const auto& memory = m_program.Resources().memoryInfo[index];
        return memory.kind == ResourceKind::ScalarBuffer && memory.dataBits == 32u && memory.dataDwords == 1u ? &memory : nullptr;
    }

    const MemoryInfo* ScalarAddressMemory(const IrValue& read, std::uint32_t& index) const {
        if (read.Opcode() != IrOpcode::LoadAddressU32 || read.ArgumentCount() != 4u) {
            return nullptr;
        }
        index = read.Flags<MemoryFlags>().index;
        if (index >= m_program.Resources().memoryInfo.size()) {
            return nullptr;
        }
        const auto& memory = m_program.Resources().memoryInfo[index];
        std::uint32_t high = 0;
        const IrValue* active = read.Argument(3)->Resolve();
        const bool always = active->HasImmediate() && active->Type() == IrType::U1 && active->ImmediateBool();
        return memory.kind == ResourceKind::ScalarAddress && !memory.addressIsFull && memory.dataBits == 32u && memory.dataDwords == 1u && immediateU32(read.Argument(2), high) && high == 0u && always ? &memory : nullptr;
    }

    static bool AccessesMemory(IrOpcode op) {
        return BufferAccessOf(op) != BufferAccess::None || AddressOpcodeInfoOf(op).access != AddressAccess::None || ImageOpcodeInfoOf(op).access != ImageAccess::None;
    }

    void IndexMemoryUsers() {
        for (const auto& block : m_program.Blocks()) {
            for (const IrValue* inst : block->Instructions()) {
                if (AccessesMemory(inst->Opcode())) {
                    m_memoryUsers[inst->Flags<MemoryFlags>().index].push_back(inst);
                }
            }
        }
        m_memoryUsersIndexed = true;
    }

    bool MemoryIndexBelongsTo(std::uint32_t index, const IrValue& owner) const {
        if (m_memoryUsersIndexed) {
            const auto users = m_memoryUsers.find(index);
            return users == m_memoryUsers.end() || std::ranges::all_of(users->second, [&](const IrValue* user) { return user == &owner; });
        }
        for (const auto& block : m_program.Blocks()) {
            for (const IrValue* inst : block->Instructions()) {
                if (!AccessesMemory(inst->Opcode()) || inst == &owner) {
                    continue;
                }
                if (inst->Flags<MemoryFlags>().index == index) {
                    return false;
                }
            }
        }
        return true;
    }

    bool MakeRuntimeSource(const IrValue& handle, IrOpcode expected, std::uint32_t width, std::uint32_t& source, DescriptorSource& descriptor) {
        if (handle.Opcode() != expected) {
            return false;
        }
        MakeSource(handle, width, false, false, descriptor);
        std::uint32_t badDword = 0;
        if (!ValidateSource(descriptor, badDword)) {
            return false;
        }
        source = InternSource(descriptor);
        return true;
    }

    static bool MatchScale(IrValue* value, IrValue*& key, std::uint32_t& stride) {
        value = value->Resolve();
        if (value->ArgumentCount() != 2u) {
            return false;
        }
        std::uint32_t immediate = 0;
        if (value->Opcode() == IrOpcode::ShiftLeftLogical32) {
            if (!immediateU32(value->Argument(1), immediate) || immediate >= 32u) {
                return false;
            }
            key = value->Argument(0)->Resolve();
            stride = 1u << immediate;
            return true;
        }
        if (value->Opcode() != IrOpcode::IMul32) {
            return false;
        }
        if (immediateU32(value->Argument(1), immediate)) {
            key = value->Argument(0)->Resolve();
        } else if (immediateU32(value->Argument(0), immediate)) {
            key = value->Argument(1)->Resolve();
        } else {
            return false;
        }
        stride = immediate;
        return true;
    }

    static bool MatchAffineOffset(IrValue* value, IrValue*& key, std::uint32_t& stride, std::uint32_t& addend) {
        value = value->Resolve();
        addend = 0;
        bool matched = MatchScale(value, key, stride);
        if (!matched && value->Opcode() == IrOpcode::IAdd32 && value->ArgumentCount() == 2u) {
            if (immediateU32(value->Argument(0), addend)) {
                matched = MatchScale(value->Argument(1), key, stride);
            } else if (immediateU32(value->Argument(1), addend)) {
                matched = MatchScale(value->Argument(0), key, stride);
            }
        }
        return matched && stride >= 4u && stride % 4u == 0u && key->Type() == IrType::U32;
    }

    static bool MatchMaskedOffset(IrValue* value, IrValue*& key, std::uint32_t& stride) {
        value = value->Resolve();
        if (value->Opcode() != IrOpcode::BitwiseAnd32 || value->ArgumentCount() != 2u) {
            return false;
        }
        std::uint32_t mask = 0;
        IrValue* scaled = nullptr;
        if (immediateU32(value->Argument(1), mask)) {
            scaled = value->Argument(0);
        } else if (immediateU32(value->Argument(0), mask)) {
            scaled = value->Argument(1);
        } else {
            return false;
        }
        return MatchScale(scaled, key, stride) && std::has_single_bit(stride) && stride >= 4u && (mask & ~(stride - 1u)) != 0u && key->Type() == IrType::U32;
    }

    static bool ReadsThroughDescriptor(const IrValue* value) {
        std::vector<const IrValue*> pending{value};
        std::vector<const IrValue*> seen;
        while (!pending.empty()) {
            const IrValue* current = pending.back()->Resolve();
            pending.pop_back();
            if (std::find(seen.begin(), seen.end(), current) != seen.end()) continue;
            seen.push_back(current);
            if (current->Opcode() == IrOpcode::ReadConstBuffer || current->Opcode() == IrOpcode::LoadAddressU32) return true;
            for (std::size_t index = 0; index < current->ArgumentCount(); index++) pending.push_back(current->Argument(index));
        }
        return false;
    }

    std::optional<KeyDomain> MatchKeyDomain(IrValue* key) {
        key = key->Resolve();
        DescriptorSource direct;
        direct.dwordCount = 1u;
        direct.dwords[0] = key;
        std::uint32_t badDword = 0;
        if (ValidateSource(direct, badDword) && !ReadsThroughDescriptor(key)) {
            KeyDomain domain{InternSource(direct), 0u, 4u};
            domain.value = true;
            return domain;
        }
        std::uint32_t memoryIndex = 0;
        const MemoryInfo* memory = ScalarReadMemory(*key, memoryIndex);
        if (memory == nullptr) {
            return std::nullopt;
        }
        DescriptorSource source;
        std::uint32_t sourceIndex = 0;
        if (!MakeRuntimeSource(*key->Argument(0)->Resolve(), IrOpcode::GetBufferResource, 4u, sourceIndex, source)) {
            return std::nullopt;
        }
        IrValue* selector = nullptr;
        std::uint32_t stride = 0;
        std::uint32_t addend = 0;
        if (MatchAffineOffset(key->Argument(1), selector, stride, addend)) {
            return KeyDomain{sourceIndex, memory->offset + addend, stride};
        }
        const auto bits = possibleU32Bits(key->Argument(1));
        const auto zeros = bits == 0u ? 31u : std::min<std::uint32_t>(static_cast<std::uint32_t>(std::countr_zero(bits)), 31u);
        return KeyDomain{sourceIndex, memory->offset, std::max(4u, 1u << zeros)};
    }

    static IrValue* SplitImmediateAddend(IrValue* value, std::uint32_t& addend) {
        value = value->Resolve();
        addend = 0;
        while (value->Opcode() == IrOpcode::IAdd32 && value->ArgumentCount() == 2u) {
            std::uint32_t immediate = 0;
            IrValue* rest = nullptr;
            if (immediateU32(value->Argument(0), immediate)) rest = value->Argument(1)->Resolve();
            else if (immediateU32(value->Argument(1), immediate)) rest = value->Argument(0)->Resolve();
            else break;
            addend += immediate;
            value = rest;
        }
        return value;
    }

    struct FirstLaneOffset {
        const IrValue* lane = nullptr;
        const IrValue* mask = nullptr;
        std::uint32_t shift = 0;
        std::uint32_t addend = 0;
    };

    static bool ParseFirstLaneOffset(IrValue* value, const IrValue* mask, FirstLaneOffset& form, std::uint32_t depth) {
        std::uint32_t addend = 0;
        value = SplitImmediateAddend(value, addend);
        while (mask != nullptr && Detail::IsRuntimeSelect(value->Opcode()) && value->ArgumentCount() == 3u && Detail::ActiveMaskImplies(mask, value->Argument(0))) {
            value = SplitImmediateAddend(value->Argument(1), addend);
        }
        std::uint32_t shift = 0;
        if (depth != 0u && value->Opcode() == IrOpcode::ShiftLeftLogical32 && value->ArgumentCount() == 2u && immediateU32(value->Argument(1), shift) && shift < 32u) {
            if (!ParseFirstLaneOffset(value->Argument(0)->Resolve(), mask, form, depth - 1u) || form.shift + shift >= 32u) return false;
            form.shift += shift;
            form.addend <<= shift;
        } else if (depth != 0u && mask == nullptr && value->Opcode() == IrOpcode::ReadFirstLane && value->ArgumentCount() == 2u) {
            if (!ParseFirstLaneOffset(value->Argument(0)->Resolve(), value->Argument(1)->Resolve(), form, depth - 1u)) return false;
        } else {
            if (mask == nullptr) return false;
            form = FirstLaneOffset{value, mask, 0u, 0u};
        }
        form.addend += addend;
        return true;
    }

    bool SameFirstLaneOffset(const FirstLaneOffset& left, const FirstLaneOffset& right) const {
        return left.shift == right.shift && EquivalentValue(m_program.Resources(), left.mask, right.mask) && EquivalentValue(m_program.Resources(), left.lane, right.lane);
    }

    bool TryMakeTableColumn(IrValue& handle, bool sampler, bool r128, TableColumnPlan& plan) {
        const auto expected = sampler ? IrOpcode::GetSamplerResource : IrOpcode::GetImageResource;
        const std::uint32_t width = sampler ? 4u : 8u;
        if (handle.Opcode() != expected || handle.ArgumentCount() != width) {
            return false;
        }
        std::uint32_t words = width;
        if (!sampler && r128) {
            bool zeroTail = true;
            for (std::uint32_t dword = 4u; dword < 8u; dword++) {
                std::uint32_t immediate = 0;
                zeroTail = zeroTail && immediateU32(handle.Argument(dword), immediate) && immediate == 0u;
            }
            if (zeroTail) {
                words = 4u;
            }
        }
        IrValue* heapHandle = nullptr;
        IrValue* offset = nullptr;
        IrValue* base = nullptr;
        std::uint32_t baseAddend = 0;
        std::uint32_t carry = 0;
        bool folded = false;
        std::uint32_t first = 0;
        bool address = false;
        FirstLaneOffset baseLane;
        bool laneBase = false;
        for (std::uint32_t dword = 0; dword < words; dword++) {
            IrValue* read = ResolveInvariantPhi(m_program.Resources(), handle.Argument(dword));
            if (read == nullptr) return false;
            std::uint32_t memoryIndex = 0;
            const MemoryInfo* memory = ScalarReadMemory(*read, memoryIndex);
            const bool addressRead = memory == nullptr && (memory = ScalarAddressMemory(*read, memoryIndex)) != nullptr;
            if (memory == nullptr || !MemoryIndexBelongsTo(memoryIndex, *read)) {
                return false;
            }
            std::uint32_t readAddend = 0;
            IrValue* readBase = SplitImmediateAddend(read->Argument(1), readAddend);
            bool sameBase = dword == 0u || EquivalentValue(m_program.Resources(), base, readBase);
            if (!sameBase && laneBase) {
                FirstLaneOffset lane;
                if (ParseFirstLaneOffset(read->Argument(1)->Resolve(), nullptr, lane, phiSearchDepth) && SameFirstLaneOffset(baseLane, lane)) {
                    readAddend = lane.addend;
                    sameBase = true;
                }
            }
            if (dword == 0u) {
                first = memory->offset;
                address = addressRead;
                base = readBase;
                baseAddend = readAddend;
                laneBase = ParseFirstLaneOffset(read->Argument(1)->Resolve(), nullptr, baseLane, phiSearchDepth) && baseLane.addend == baseAddend;
            } else if (addressRead != address || memory->offset + readAddend != first + baseAddend + dword * static_cast<std::uint32_t>(sizeof(std::uint32_t))) {
                return false;
            } else if (readAddend != baseAddend) {
                if (readAddend < baseAddend || static_cast<std::uint64_t>(memory->offset) + (readAddend - baseAddend) > 0x7fffffffu) {
                    return false;
                }
                folded = true;
                carry = std::max(carry, readAddend - baseAddend);
            }
            IrValue* currentHandle = read->Argument(0)->Resolve();
            if (heapHandle != nullptr && currentHandle != heapHandle && !EquivalentValue(m_program.Resources(), heapHandle, currentHandle)) {
                return false;
            }
            if (heapHandle == nullptr) heapHandle = currentHandle;
            if (dword == 0u) {
                offset = read->Argument(1)->Resolve();
            } else if (!sameBase) {
                return false;
            }
            plan.memory.push_back(memoryIndex);
            plan.reads.push_back(read);
        }
        if (address) {
            DescriptorSource direct;
            MakeSource(handle, width, false, false, direct);
            std::uint32_t badDword = 0;
            if (ValidateSource(direct, badDword)) {
                return false;
            }
        }
        IrValue* key = nullptr;
        std::uint32_t stride = 0;
        std::uint32_t addend = 0;
        bool masked = false;
        std::uint32_t descriptorOffset = 0;
        if (address && immediateU32(offset, descriptorOffset)) {
            return TryMakeIndirectColumn(handle, *heapHandle, sampler, words, descriptorOffset, first, plan);
        }
        if (!MatchAffineOffset(offset, key, stride, addend)) {
            if (!MatchMaskedOffset(offset, key, stride)) {
                return false;
            }
            masked = true;
        }
        DescriptorSource heapSource;
        std::uint32_t heapIndex = 0;
        if (!MakeRuntimeSource(*heapHandle, address ? IrOpcode::GetAddressResource : IrOpcode::GetBufferResource, address ? 2u : 4u, heapIndex, heapSource)) {
            return false;
        }
        TableColumn column;
        column.heapSource = heapIndex;
        column.stride = stride;
        column.addend = addend;
        column.offset = first;
        column.dwordCount = words;
        column.maxKey = possibleU32Bits(key);
        column.sampler = sampler;
        column.address = address;
        column.foldedAddend = folded ? baseAddend : 0u;
        column.foldedCarry = folded ? carry : 0u;
        if (!address && !sampler && std::ranges::find(m_unselectedHandles, &handle) == m_unselectedHandles.end()) {
            DescriptorSource direct;
            MakeSource(handle, width, false, false, direct);
            DescriptorSource selection;
            selection.dwordCount = 1u;
            selection.dwords[0] = offset;
            std::uint32_t badDword = 0;
            if (ValidateSource(direct, badDword) && ValidateSource(selection, badDword)) {
                column.selected = true;
                column.selection = InternSource(selection);
            }
        }
        const auto domain = masked ? std::optional<KeyDomain>{} : MatchKeyDomain(key);
        DescriptorSource columnSource = heapSource;
        columnSource.tableColumn = column;
        plan.handle = &handle;
        plan.source = InternSource(columnSource);
        plan.keyDomain = domain;
        auto& shared = *m_sources[plan.source].tableColumn;
        shared.maxKey = std::max(shared.maxKey, column.maxKey);
        if (!domain.has_value()) {
            shared.unkeyed = true;
            shared.keyDomains.clear();
        } else if (!shared.unkeyed && std::find(shared.keyDomains.begin(), shared.keyDomains.end(), *domain) == shared.keyDomains.end()) {
            shared.keyDomains.push_back(*domain);
        }
        plan.offset = offset;
        return true;
    }

    IrValue* UnwrapTableRead(IrValue* value) const {
        if (value == nullptr) return nullptr;
        value = value->Resolve();
        if (!value->IsPhi()) return value;
        IrValue* candidate = nullptr;
        std::vector<IrValue*> pending{value};
        std::unordered_set<const IrValue*> visited;
        while (!pending.empty()) {
            auto* current = pending.back()->Resolve();
            pending.pop_back();
            if (!visited.insert(current).second) continue;
            if (current->IsPhi()) {
                for (std::size_t i = 0; i < current->ArgumentCount(); i++) {
                    pending.push_back(current->Argument(i));
                }
                continue;
            }
            std::uint32_t memoryIndex = 0;
            IrValue* key = nullptr;
            std::uint32_t stride = 0;
            std::uint32_t addend = 0;
            if (ScalarReadMemory(*current, memoryIndex) == nullptr || !MemoryIndexBelongsTo(memoryIndex, *current) ||
                !MatchAffineOffset(current->Argument(1), key, stride, addend)) {
                return nullptr;
            }
            if (candidate == nullptr) {
                candidate = current;
            } else if (!EquivalentValue(m_program.Resources(), candidate, current)) {
                std::uint32_t candidateIndex = 0;
                const auto* candMem = ScalarReadMemory(*candidate, candidateIndex);
                const auto* curMem = ScalarReadMemory(*current, memoryIndex);
                if (candMem == nullptr || curMem == nullptr || candMem->offset != curMem->offset ||
                    !EquivalentValue(m_program.Resources(), candidate->Argument(0), current->Argument(0)) ||
                    !EquivalentValue(m_program.Resources(), candidate->Argument(1), current->Argument(1))) {
                    return nullptr;
                }
            }
        }
        return candidate;
    }

    bool BlockDominates(const IrBlock* dominator, const IrBlock* block) const {
        if (dominator == block) return true;
        std::vector<const IrBlock*> pending{block};
        std::unordered_set<const IrBlock*> visited{block};
        while (!pending.empty()) {
            const IrBlock* current = pending.back();
            pending.pop_back();
            if (current->Predecessors().empty()) return false;
            for (const IrBlock* predecessor : current->Predecessors()) {
                if (predecessor != dominator && visited.insert(predecessor).second) pending.push_back(predecessor);
            }
        }
        return true;
    }

    bool AvailableAt(IrValue* value, const IrBlock* block) const {
        value = value->Resolve();
        if (value->HasImmediate() || value->Parent() == nullptr) return true;
        return BlockDominates(value->Parent(), block);
    }

    bool TryMakeIndirectColumn(IrValue& handle, IrValue& descriptorPointer, bool sampler, std::uint32_t words, std::uint32_t descriptorOffset, std::uint32_t descriptorImmediate, TableColumnPlan& plan) {
        std::vector<PointerHop> hops;
        IrValue* pointer = &descriptorPointer;
        while (pointer->Opcode() == IrOpcode::GetAddressResource && pointer->ArgumentCount() == 2u) {
            std::uint32_t memoryIndex = 0;
            if (ScalarAddressMemory(*pointer->Argument(0)->Resolve(), memoryIndex) == nullptr) {
                break;
            }
            PointerHop hop;
            IrValue* next = nullptr;
            for (std::uint32_t dword = 0; dword < 2u; dword++) {
                IrValue* read = pointer->Argument(dword)->Resolve();
                const MemoryInfo* memory = ScalarAddressMemory(*read, memoryIndex);
                std::uint32_t readOffset = 0;
                if (memory == nullptr || !MemoryIndexBelongsTo(memoryIndex, *read) || !immediateU32(read->Argument(1), readOffset)) {
                    return false;
                }
                IrValue* readPointer = read->Argument(0)->Resolve();
                if (dword == 0u) {
                    hop = PointerHop{readOffset, memory->offset};
                    next = readPointer;
                } else if (readOffset != hop.offset || memory->offset != hop.immediate + static_cast<std::uint32_t>(sizeof(std::uint32_t)) || (readPointer != next && !EquivalentValue(m_program.Resources(), next, readPointer))) {
                    return false;
                }
                plan.hopMemory.push_back(memoryIndex);
                plan.hopReads.push_back(read);
            }
            hops.insert(hops.begin(), hop);
            pointer = next;
        }
        if (pointer->Opcode() != IrOpcode::GetAddressResource || pointer->ArgumentCount() != 2u) {
            return false;
        }
        IrValue* bufferHandle = nullptr;
        IrValue* offset = nullptr;
        std::uint32_t first = 0;
        for (std::uint32_t dword = 0; dword < 2u; dword++) {
            IrValue* read = UnwrapTableRead(pointer->Argument(dword));
            if (read == nullptr) {
                return false;
            }
            std::uint32_t memoryIndex = 0;
            const MemoryInfo* memory = ScalarReadMemory(*read, memoryIndex);
            if (memory == nullptr || !MemoryIndexBelongsTo(memoryIndex, *read)) {
                return false;
            }
            if (dword == 0u) {
                first = memory->offset;
                bufferHandle = read->Argument(0)->Resolve();
                offset = read->Argument(1)->Resolve();
            } else if (memory->offset != first + static_cast<std::uint32_t>(sizeof(std::uint32_t)) || read->Argument(0)->Resolve() != bufferHandle || !EquivalentValue(m_program.Resources(), offset, read->Argument(1))) {
                return false;
            }
        }
        if (!AvailableAt(offset, handle.Parent())) {
            return false;
        }
        IrValue* key = nullptr;
        std::uint32_t stride = 0;
        std::uint32_t addend = 0;
        if (!MatchAffineOffset(offset, key, stride, addend)) {
            return false;
        }
        DescriptorSource heapSource;
        std::uint32_t heapIndex = 0;
        if (!MakeRuntimeSource(*bufferHandle, IrOpcode::GetBufferResource, 4u, heapIndex, heapSource)) {
            return false;
        }
        TableColumn column;
        column.heapSource = heapIndex;
        column.stride = stride;
        column.addend = addend;
        column.offset = first;
        column.dwordCount = words;
        column.maxKey = possibleU32Bits(key);
        column.sampler = sampler;
        column.indirect = true;
        column.indirectOffset = descriptorOffset;
        column.indirectImmediate = descriptorImmediate;
        column.pointerHops = std::move(hops);
        const auto domain = MatchKeyDomain(key);
        DescriptorSource columnSource = heapSource;
        columnSource.tableColumn = column;
        plan.handle = &handle;
        plan.source = InternSource(columnSource);
        plan.keyDomain = domain;
        auto& shared = *m_sources[plan.source].tableColumn;
        shared.maxKey = std::max(shared.maxKey, column.maxKey);
        if (!domain.has_value()) {
            shared.unkeyed = true;
            shared.keyDomains.clear();
        } else if (!shared.unkeyed && std::find(shared.keyDomains.begin(), shared.keyDomains.end(), *domain) == shared.keyDomains.end()) {
            shared.keyDomains.push_back(*domain);
        }
        plan.offset = offset;
        return true;
    }

    const TableColumnPlan* FindTableColumn(const IrValue& handle) const {
        const auto found = std::ranges::find_if(m_tableColumns, [&](const TableColumnPlan& plan) {
            return plan.handle == &handle;
        });
        return found == m_tableColumns.end() ? nullptr : &*found;
    }

    bool PlanTableColumn(IrValue& handle, bool sampler, bool r128) {
        if (FindTableColumn(handle) != nullptr) {
            return true;
        }
        TableColumnPlan plan;
        if (!TryMakeTableColumn(handle, sampler, r128, plan)) {
            return false;
        }
        m_tableColumns.push_back(std::move(plan));
        return true;
    }

    static constexpr std::uint32_t phiSearchDepth = 8u;

    static bool SplittableImageRead(const IrValue& inst) {
        switch (inst.Opcode()) {
            case IrOpcode::ImageSampleRaw:
            case IrOpcode::ImageGatherRaw:
            case IrOpcode::ImageQueryLod:
            case IrOpcode::ImageQueryDimensions:
            case IrOpcode::ImageRead: return inst.Type() == IrType::U32x4;
            default: return false;
        }
    }

    void CollectDescriptorPhis(IrValue* value, std::vector<IrValue*>& phis, std::uint32_t depth) const {
        value = value->Resolve();
        if (value->HasImmediate()) {
            return;
        }
        if (value->IsPhi()) {
            if (ResolveInvariantPhi(m_program.Resources(), value) == nullptr && std::ranges::find(phis, value) == phis.end()) {
                phis.push_back(value);
            }
            return;
        }
        if (depth != 0u && value->Opcode() == IrOpcode::ReadFirstLane && value->ArgumentCount() == 2u) {
            CollectDescriptorPhis(value->Argument(0), phis, depth - 1u);
            return;
        }
        if (depth == 0u || !Detail::IsRuntimeUniformOp(value->Opcode())) {
            return;
        }
        for (std::size_t index = 0; index < value->ArgumentCount(); index++) {
            CollectDescriptorPhis(value->Argument(index), phis, depth - 1u);
        }
    }

    static IrValue* PhiIncoming(const IrValue& phi, const IrBlock* predecessor) {
        for (std::size_t index = 0; index < phi.ArgumentCount(); index++) {
            if (phi.PhiBlock(index) == predecessor) {
                return phi.Argument(index);
            }
        }
        return nullptr;
    }

    IrValue* EmitBefore(IrValue& position, IrOpcode op, IrType type, std::span<IrValue* const> arguments, std::uint64_t flags = 0) {
        IrValue& value = m_program.CreateValue(op, type, flags);
        for (IrValue* argument : arguments) {
            value.AddArgument(argument);
        }
        position.Parent()->InsertInstructionBefore(&position, &value);
        return &value;
    }

    IrValue* EmitCopy(const IrValue& original, std::span<IrValue* const> arguments, IrValue& position) {
        return EmitBefore(position, original.Opcode(), original.Type(), arguments, original.Flags<std::uint64_t>());
    }

    IrValue* Rematerialize(IrValue* value, IrValue& position, bool emit, std::uint32_t depth) {
        value = value->Resolve();
        if (value->HasImmediate()) {
            return value;
        }
        if (value->IsPhi()) {
            IrValue* invariant = ResolveInvariantPhi(m_program.Resources(), value);
            return invariant == nullptr || invariant->IsPhi() ? nullptr : Rematerialize(invariant, position, emit, depth);
        }
        if (depth == 0u) {
            return nullptr;
        }
        const auto op = value->Opcode();
        const bool register_ = op == IrOpcode::GetUserData;
        const bool argumentless = (op == IrOpcode::GetShaderBase || op == IrOpcode::GetSrtResource) && value->ArgumentCount() == 0u;
        const bool srtRead = op == IrOpcode::ReadConst && value->ArgumentCount() == 2u && value->Argument(0)->Resolve()->Opcode() == IrOpcode::GetSrtResource && value->Argument(1)->Resolve()->HasImmediate();
        if (!register_ && !argumentless && !srtRead && !Detail::IsRuntimeUniformOp(op)) {
            return nullptr;
        }
        std::vector<IrValue*> arguments;
        for (std::size_t index = 0; index < value->ArgumentCount(); index++) {
            IrValue* argument = register_ ? value->Argument(index) : Rematerialize(value->Argument(index), position, emit, depth - 1u);
            if (argument == nullptr) {
                return nullptr;
            }
            arguments.push_back(argument);
        }
        return emit ? EmitCopy(*value, arguments, position) : value;
    }

    IrValue* SubstituteEdge(IrValue* value, const IrBlock* predecessor, IrValue& position, bool emit, std::uint32_t depth) {
        value = value->Resolve();
        std::vector<IrValue*> phis;
        CollectDescriptorPhis(value, phis, depth);
        if (phis.empty()) {
            return value;
        }
        if (value->IsPhi()) {
            IrValue* incoming = PhiIncoming(*value, predecessor);
            return incoming == nullptr ? nullptr : Rematerialize(incoming, position, emit, phiSearchDepth);
        }
        std::vector<IrValue*> arguments;
        for (std::size_t index = 0; index < value->ArgumentCount(); index++) {
            IrValue* argument = SubstituteEdge(value->Argument(index), predecessor, position, emit, depth - 1u);
            if (argument == nullptr) {
                return nullptr;
            }
            arguments.push_back(argument);
        }
        return emit ? EmitCopy(*value, arguments, position) : value;
    }

    IrValue& EdgeSelector(IrBlock& block, const std::vector<bool>& edges) {
        for (const auto& selector : m_edgeSelectors) {
            if (selector.block == &block && selector.edges == edges) {
                return *selector.value;
            }
        }
        IrValue& phi = m_program.CreateValue(IrOpcode::Phi, IrType::Bool);
        const auto& predecessors = block.Predecessors();
        for (std::size_t index = 0; index < predecessors.size(); index++) {
            phi.AddPhiOperand(predecessors[index], &m_builder.ConstantBool(edges[index]));
        }
        block.InsertInstructionBefore(nullptr, &phi);
        m_edgeSelectors.push_back({&block, edges, &phi});
        return phi;
    }

    void SplitDescriptorPhi(IrValue& inst) {
        const auto info = ImageOpcodeInfoOf(inst.Opcode());
        const std::uint32_t handleCount = info.needsSampler ? 2u : 1u;
        if (inst.ArgumentCount() < handleCount || inst.Parent() == nullptr) {
            return;
        }
        std::array<IrValue*, 2> handles {};
        std::array<bool, 2> split {};
        std::vector<IrValue*> phis;
        for (std::uint32_t slot = 0; slot < handleCount; slot++) {
            IrValue* handle = inst.Argument(slot)->Resolve();
            const auto expected = slot == 0u ? IrOpcode::GetImageResource : IrOpcode::GetSamplerResource;
            if (handle->Opcode() != expected || handle->ArgumentCount() != (slot == 0u ? 8u : 4u)) {
                return;
            }
            handles[slot] = handle;
            std::vector<IrValue*> handlePhis;
            for (std::size_t dword = 0; dword < handle->ArgumentCount(); dword++) {
                CollectDescriptorPhis(handle->Argument(dword), handlePhis, phiSearchDepth);
            }
            split[slot] = !handlePhis.empty();
            for (IrValue* phi : handlePhis) {
                if (std::ranges::find(phis, phi) == phis.end()) {
                    phis.push_back(phi);
                }
            }
        }
        if (phis.empty()) {
            return;
        }
        if (!SplitDescriptorEdges(inst, handles, split, handleCount, phis)) {
            SplitDescriptorWeb(inst, handles, split, handleCount);
        }
    }

    bool SplitDescriptorEdges(IrValue& inst, const std::array<IrValue*, 2>& handles, const std::array<bool, 2>& split, std::uint32_t handleCount, const std::vector<IrValue*>& phis) {
        IrBlock* block = phis.front()->Parent();
        if (block == nullptr) {
            return false;
        }
        const auto& predecessors = block->Predecessors();
        if (predecessors.size() < 2u) {
            return false;
        }
        for (const IrValue* phi : phis) {
            if (phi->Parent() != block || phi->PhiBlockCount() != predecessors.size()) {
                return false;
            }
            for (const IrBlock* predecessor : predecessors) {
                if (PhiIncoming(*phi, predecessor) == nullptr) {
                    return false;
                }
            }
        }

        std::vector<std::uint32_t> edgeArm(predecessors.size());
        std::vector<const IrBlock*> arms;
        for (std::size_t edge = 0; edge < predecessors.size(); edge++) {
            std::uint32_t arm = 0;
            for (; arm < arms.size(); arm++) {
                const bool same = std::ranges::all_of(phis, [&](const IrValue* phi) {
                    return EquivalentValue(m_program.Resources(), PhiIncoming(*phi, predecessors[edge]), PhiIncoming(*phi, arms[arm]));
                });
                if (same) {
                    break;
                }
            }
            if (arm == arms.size()) {
                arms.push_back(predecessors[edge]);
            }
            edgeArm[edge] = arm;
        }
        for (const IrBlock* arm : arms) {
            for (std::uint32_t slot = 0; slot < handleCount; slot++) {
                for (std::size_t dword = 0; split[slot] && dword < handles[slot]->ArgumentCount(); dword++) {
                    if (SubstituteEdge(handles[slot]->Argument(dword), arm, inst, false, phiSearchDepth) == nullptr) {
                        return false;
                    }
                }
            }
        }

        std::vector<IrValue*> conditions;
        for (std::uint32_t arm = 0; arm + 1u < arms.size(); arm++) {
            std::vector<bool> edges(edgeArm.size());
            for (std::size_t edge = 0; edge < edgeArm.size(); edge++) {
                edges[edge] = edgeArm[edge] == arm;
            }
            conditions.push_back(&EdgeSelector(*block, edges));
        }
        ReplaceWithArmCopies(inst, handles, split, handleCount, conditions, [&](std::uint32_t arm, std::uint32_t slot, std::size_t dword) {
            return SubstituteEdge(handles[slot]->Argument(dword), arms[arm], inst, true, phiSearchDepth);
        });
        return true;
    }

    template <typename Dword>
    void ReplaceWithArmCopies(IrValue& inst, const std::array<IrValue*, 2>& handles, const std::array<bool, 2>& split, std::uint32_t handleCount, std::span<IrValue* const> conditions, Dword&& dword) {
        std::vector<IrValue*> copies;
        const auto flags = inst.Flags<MemoryFlags>();
        const auto memory = m_program.Resources().memoryInfo.at(flags.index);
        for (std::uint32_t arm = 0; arm <= conditions.size(); arm++) {
            std::vector<IrValue*> arguments(inst.Arguments().begin(), inst.Arguments().end());
            for (std::uint32_t slot = 0; slot < handleCount; slot++) {
                if (!split[slot]) {
                    continue;
                }
                std::vector<IrValue*> dwords;
                for (std::size_t index = 0; index < handles[slot]->ArgumentCount(); index++) {
                    dwords.push_back(dword(arm, slot, index));
                }
                arguments[slot] = EmitCopy(*handles[slot], dwords, inst);
            }
            auto copyFlags = flags;
            if (arm != 0u) {
                copyFlags.index = static_cast<std::uint32_t>(m_program.Resources().memoryInfo.size());
                m_program.Resources().memoryInfo.push_back(memory);
            }
            std::uint64_t rawFlags = 0;
            std::memcpy(&rawFlags, &copyFlags, sizeof(copyFlags));
            copies.push_back(EmitBefore(inst, inst.Opcode(), inst.Type(), arguments, rawFlags));
        }

        IrValue* result = copies.back();
        if (copies.size() > 1u) {
            std::array<IrValue*, 4> components {};
            for (std::uint32_t component = 0; component < components.size(); component++) {
                const auto extract = [&](IrValue* vector) {
                    const std::array<IrValue*, 2> arguments {vector, &m_builder.Constant(component)};
                    return EmitBefore(inst, IrOpcode::CompositeExtractU32x4, IrType::U32, arguments);
                };
                IrValue* selected = extract(copies.back());
                for (std::uint32_t arm = static_cast<std::uint32_t>(copies.size()) - 1u; arm-- > 0u;) {
                    const std::array<IrValue*, 3> arguments {conditions[arm], extract(copies[arm]), selected};
                    selected = EmitBefore(inst, IrOpcode::SelectU32, IrType::U32, arguments);
                }
                components[component] = selected;
            }
            result = EmitBefore(inst, IrOpcode::CompositeConstructU32x4, IrType::U32x4, components);
        }
        inst.ReplaceAllUsesWith(result);
        inst.Invalidate();
        inst.Parent()->RemoveInstruction(&inst);
    }

    static bool HoldsProgramCounter(IrValue* value, std::uint32_t depth) {
        value = value->Resolve();
        if (value->Opcode() == IrOpcode::GetShaderBase) {
            return true;
        }
        if (value->HasImmediate() || depth == 0u || !Detail::IsRuntimeUniformOp(value->Opcode())) {
            return false;
        }
        for (std::size_t index = 0; index < value->ArgumentCount(); index++) {
            if (HoldsProgramCounter(value->Argument(index), depth - 1u)) {
                return true;
            }
        }
        return false;
    }

    struct DescriptorWeb {
        static constexpr std::size_t maxStates = 512u;
        static constexpr std::size_t maxArms = 64u;
        std::vector<std::vector<IrValue*>> arms;
        std::map<std::vector<IrValue*>, IrValue*> selectors;
        std::vector<std::pair<IrBlock*, IrValue*>> phis;
        std::vector<std::tuple<IrBlock*, IrValue*, IrValue*>> lanes;
        std::unordered_map<IrValue*, IrValue*> resolved;
        std::unordered_map<const IrBlock*, std::size_t> order;
    };

    IrValue* WebSelector(std::vector<IrValue*> tuple, DescriptorWeb& web, IrValue& position) {
        for (IrValue*& value : tuple) {
            value = value->Resolve();
            if (!value->IsPhi()) {
                continue;
            }
            const auto [entry, inserted] = web.resolved.try_emplace(value, value);
            if (inserted) {
                IrValue* invariant = ResolveInvariantPhi(m_program.Resources(), value);
                entry->second = invariant == nullptr ? value : invariant->Resolve();
            }
            value = entry->second;
        }
        if (const auto known = web.selectors.find(tuple); known != web.selectors.end()) {
            return known->second;
        }
        const auto phi = std::ranges::find_if(tuple, [](const IrValue* value) { return value->IsPhi(); });
        const auto lane = std::ranges::find_if(tuple, [](const IrValue* value) { return value->Opcode() == IrOpcode::SelectU32 || value->Opcode() == IrOpcode::ReadFirstLane; });
        if (phi == tuple.end() && lane != tuple.end()) {
            return LaneSelector(tuple, **lane, web, position);
        }
        if (phi == tuple.end()) {
            for (IrValue* value : tuple) {
                if (HoldsProgramCounter(value, phiSearchDepth) || Rematerialize(value, position, false, phiSearchDepth) == nullptr) {
                    return nullptr;
                }
            }
            const auto same = std::ranges::find_if(web.arms, [&](const std::vector<IrValue*>& arm) {
                return std::ranges::equal(tuple, arm, [&](IrValue* left, IrValue* right) { return EquivalentValue(m_program.Resources(), left, right); });
            });
            if (same != web.arms.end()) {
                return &m_builder.Constant(static_cast<std::uint32_t>(same - web.arms.begin()));
            }
            if (web.arms.size() == DescriptorWeb::maxArms) {
                return nullptr;
            }
            web.arms.push_back(std::move(tuple));
            return &m_builder.Constant(static_cast<std::uint32_t>(web.arms.size() - 1u));
        }
        if (web.order.empty()) {
            const auto& order = m_program.BlockOrder();
            for (std::size_t index = 0; index < order.size(); index++) {
                web.order.emplace(order[index], index);
            }
        }
        IrBlock* block = nullptr;
        std::size_t latest = 0;
        for (const IrValue* value : tuple) {
            if (!value->IsPhi()) {
                continue;
            }
            const auto found = web.order.find(value->Parent());
            if (found == web.order.end()) {
                return nullptr;
            }
            if (block == nullptr || found->second > latest) {
                block = value->Parent();
                latest = found->second;
            }
        }
        if (block == nullptr || web.selectors.size() == DescriptorWeb::maxStates) {
            return nullptr;
        }
        const auto& predecessors = block->Predecessors();
        if (predecessors.size() < 2u) {
            return nullptr;
        }
        for (const IrValue* value : tuple) {
            if (value->IsPhi() && value->Parent() == block && value->PhiBlockCount() != predecessors.size()) {
                return nullptr;
            }
        }
        IrValue& selector = m_program.CreateValue(IrOpcode::Phi, IrType::U32);
        web.selectors.emplace(tuple, &selector);
        web.phis.emplace_back(block, &selector);
        for (IrBlock* predecessor : predecessors) {
            std::vector<IrValue*> incoming;
            for (IrValue* value : tuple) {
                IrValue* edge = value->IsPhi() && value->Parent() == block ? PhiIncoming(*value, predecessor) : value;
                if (edge == nullptr) {
                    return nullptr;
                }
                incoming.push_back(edge);
            }
            IrValue* edgeSelector = WebSelector(std::move(incoming), web, position);
            if (edgeSelector == nullptr) {
                return nullptr;
            }
            selector.AddPhiOperand(predecessor, edgeSelector);
        }
        return &selector;
    }

    IrValue* LaneSelector(const std::vector<IrValue*>& tuple, IrValue& first, DescriptorWeb& web, IrValue& position) {
        IrBlock* block = first.Parent();
        const auto op = first.Opcode();
        const bool select = op == IrOpcode::SelectU32;
        if (block == nullptr || first.ArgumentCount() != (select ? 3u : 2u) || web.selectors.size() == DescriptorWeb::maxStates) {
            return nullptr;
        }
        IrValue* shared = first.Argument(select ? 0u : 1u);
        std::vector<IrValue*> left;
        std::vector<IrValue*> right;
        for (IrValue* value : tuple) {
            if (value->Opcode() == op && value->Parent() == block && value->ArgumentCount() == first.ArgumentCount() && EquivalentValue(m_program.Resources(), value->Argument(select ? 0u : 1u), shared)) {
                left.push_back(value->Argument(select ? 1u : 0u));
                if (select) {
                    right.push_back(value->Argument(2u));
                }
            } else if (value->HasImmediate() || (select && !value->IsPhi() && value->Opcode() != IrOpcode::ReadFirstLane)) {
                left.push_back(value);
                if (select) {
                    right.push_back(value);
                }
            } else {
                return nullptr;
            }
        }
        auto& instructions = block->Instructions();
        auto latest = instructions.end();
        for (auto it = instructions.begin(); it != instructions.end(); ++it) {
            if (std::ranges::find(tuple, *it) != tuple.end()) {
                latest = it;
            }
        }
        if (latest == instructions.end()) {
            return nullptr;
        }
        IrValue* before = std::next(latest) == instructions.end() ? nullptr : *std::next(latest);
        IrValue* leftSelector = WebSelector(std::move(left), web, position);
        if (leftSelector == nullptr) {
            return nullptr;
        }
        IrValue& node = m_program.CreateValue(op, IrType::U32, first.Flags<std::uint64_t>());
        if (select) {
            IrValue* rightSelector = WebSelector(std::move(right), web, position);
            if (rightSelector == nullptr) {
                return nullptr;
            }
            node.AddArgument(shared);
            node.AddArgument(leftSelector);
            node.AddArgument(rightSelector);
        } else {
            node.AddArgument(leftSelector);
            node.AddArgument(shared);
        }
        web.selectors.emplace(tuple, &node);
        web.lanes.emplace_back(block, before, &node);
        return &node;
    }

    void SplitDescriptorWeb(IrValue& inst, const std::array<IrValue*, 2>& handles, const std::array<bool, 2>& split, std::uint32_t handleCount) {
        std::vector<IrValue*> tuple;
        std::array<std::size_t, 2> offsets {};
        for (std::uint32_t slot = 0; slot < handleCount; slot++) {
            offsets[slot] = tuple.size();
            for (std::size_t dword = 0; split[slot] && dword < handles[slot]->ArgumentCount(); dword++) {
                tuple.push_back(handles[slot]->Argument(dword));
            }
        }
        DescriptorWeb web;
        IrValue* selector = WebSelector(tuple, web, inst);
        const bool select = selector != nullptr && web.arms.size() > 1u;
        if (select) {
            for (const auto& [block, phi] : web.phis) {
                block->InsertInstructionBefore(nullptr, phi);
            }
            for (const auto& [block, before, node] : web.lanes) {
                if (before == nullptr) {
                    block->AppendInstruction(node);
                } else {
                    block->InsertInstructionBefore(before, node);
                }
            }
        } else {
            for (const auto& [block, phi] : web.phis) {
                for (std::size_t index = 0; index < phi->ArgumentCount(); index++) {
                    phi->ReplaceArgument(index, nullptr);
                }
            }
            for (const auto& [block, before, node] : web.lanes) {
                for (std::size_t index = 0; index < node->ArgumentCount(); index++) {
                    node->ReplaceArgument(index, nullptr);
                }
            }
            for (const auto& [block, phi] : web.phis) {
                phi->Invalidate();
            }
            for (const auto& [block, before, node] : web.lanes) {
                node->Invalidate();
            }
        }
        if (selector == nullptr || web.arms.empty()) {
            return;
        }

        std::vector<IrValue*> conditions;
        for (std::uint32_t arm = 0; arm + 1u < web.arms.size(); arm++) {
            const std::array<IrValue*, 2> compare {selector, &m_builder.Constant(arm)};
            conditions.push_back(EmitBefore(inst, IrOpcode::IEqual32, IrType::Bool, compare));
        }
        ReplaceWithArmCopies(inst, handles, split, handleCount, conditions, [&](std::uint32_t arm, std::uint32_t slot, std::size_t dword) {
            return Rematerialize(web.arms[arm][offsets[slot] + dword], inst, true, phiSearchDepth);
        });
    }

    void SplitDescriptorPhis() {
        std::vector<IrValue*> candidates;
        for (auto& block : m_program.Blocks()) {
            for (IrValue* inst : block->Instructions()) {
                if (SplittableImageRead(*inst)) {
                    candidates.push_back(inst);
                }
            }
        }
        for (IrValue* inst : candidates) {
            SplitDescriptorPhi(*inst);
        }
    }

    void PlanTableColumns() {
        static const bool enabled = std::getenv("APS5_NO_IMAGE_TABLES") == nullptr;
        if (!enabled) {
            return;
        }
        const auto sources = m_sources;
        const auto columns = m_tableColumns;
        const auto reads = m_tablePlanningReads;
        const auto memory = m_tablePlanningMemory;
        PlanTableColumnPass();
        while (!PlannedWithinLimits()) {
            const auto group = LargestSelectedGroup();
            if (group.empty()) {
                return;
            }
            m_unselectedHandles.insert(m_unselectedHandles.end(), group.begin(), group.end());
            m_sources = sources;
            m_tableColumns = columns;
            m_tablePlanningReads = reads;
            m_tablePlanningMemory = memory;
            PlanTableColumnPass();
        }
    }

    bool PlannedWithinLimits() {
        std::vector<ImageResource> images;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
        std::vector<const IrValue*> directImages;
        std::vector<std::pair<const IrValue*, bool>> directSamplers;
        const auto direct = std::numeric_limits<std::uint32_t>::max() / 2u;
        for (auto& block : m_program.Blocks()) {
            for (IrValue* inst : block->Instructions()) {
                const auto imageInfo = ImageOpcodeInfoOf(inst->Opcode());
                if (inst->Opcode() == IrOpcode::ImageBvhIntersectRay || imageInfo.access == ImageAccess::None || imageInfo.resourceClass == ImageResourceClass::None || inst->ArgumentCount() == 0u) {
                    continue;
                }
                const auto flags = inst->Flags<MemoryFlags>();
                if (flags.index >= m_program.Resources().memoryInfo.size()) {
                    continue;
                }
                const auto& memory = m_program.Resources().memoryInfo[flags.index];
                if (memory.kind != ResourceKind::Image || memory.planningOnly || IsTablePlanningMemory(flags.index)) {
                    continue;
                }
                const IrValue* handle = inst->Argument(0)->Resolve();
                const TableColumnPlan* column = FindTableColumn(*handle);
                std::uint32_t source = 0;
                if (column != nullptr) {
                    source = column->source;
                } else {
                    const auto found = std::ranges::find_if(directImages, [&](const IrValue* other) { return SameHandle(*other, *handle); });
                    source = direct + static_cast<std::uint32_t>(found - directImages.begin());
                    if (found == directImages.end()) directImages.push_back(handle);
                }
                const auto key = ImageKey(source, memory, inst->Opcode());
                const auto image = std::ranges::find_if(images, [&](const ImageResource& other) { return SameImageKey(other, key); });
                const auto imageIndex = static_cast<std::uint32_t>(image - images.begin());
                if (image == images.end()) images.push_back(key);
                if (!imageInfo.needsSampler || inst->ArgumentCount() < 2u) {
                    continue;
                }
                const IrValue* samplerHandle = inst->Argument(1)->Resolve();
                const TableColumnPlan* samplerColumn = FindTableColumn(*samplerHandle);
                std::uint32_t sampler = 0;
                if (samplerColumn != nullptr) {
                    sampler = samplerColumn->source;
                } else {
                    const bool sampleAdjust = (memory.imageSampleFlags & RdnaImageSampleFlagAdjust) != 0;
                    const auto found = std::ranges::find_if(directSamplers, [&](const auto& other) { return other.second == sampleAdjust && SameHandle(*other.first, *samplerHandle); });
                    sampler = direct + static_cast<std::uint32_t>(found - directSamplers.begin());
                    if (found == directSamplers.end()) directSamplers.emplace_back(samplerHandle, sampleAdjust);
                }
                if (std::ranges::find(pairs, std::pair{imageIndex, sampler}) == pairs.end()) pairs.emplace_back(imageIndex, sampler);
            }
        }
        return images.size() <= ShaderInfo::MaxImages && pairs.size() <= ShaderInfo::MaxSampledPairs;
    }

    std::vector<const IrValue*> LargestSelectedGroup() const {
        const auto unselected = [](TableColumn column) {
            column.selected = false;
            column.selection = 0u;
            return column;
        };
        std::vector<std::vector<std::uint32_t>> groups;
        for (const auto& plan : m_tableColumns) {
            const auto& column = m_sources[plan.source].tableColumn;
            if (!column.has_value()) {
                continue;
            }
            const auto group = std::ranges::find_if(groups, [&](const std::vector<std::uint32_t>& members) { return unselected(*m_sources[members.front()].tableColumn) == unselected(*column); });
            if (group == groups.end()) {
                groups.push_back({plan.source});
            } else if (std::ranges::find(*group, plan.source) == group->end()) {
                group->push_back(plan.source);
            }
        }
        const std::vector<std::uint32_t>* largest = nullptr;
        for (const auto& group : groups) {
            const bool selected = std::ranges::any_of(group, [&](std::uint32_t source) { return m_sources[source].tableColumn->selected; });
            if (selected && group.size() > 1u && (largest == nullptr || group.size() > largest->size())) {
                largest = &group;
            }
        }
        std::vector<const IrValue*> handles;
        if (largest == nullptr) {
            return handles;
        }
        for (const auto& plan : m_tableColumns) {
            if (std::ranges::find(*largest, plan.source) != largest->end()) {
                handles.push_back(plan.handle);
            }
        }
        return handles;
    }

    void PlanTableColumnPass() {
        IndexMemoryUsers();
        const auto directImages = DirectImageHandles();
        for (auto& block : m_program.Blocks()) {
            for (IrValue* inst : block->Instructions()) {
                const auto imageInfo = ImageOpcodeInfoOf(inst->Opcode());
                if (imageInfo.access == ImageAccess::None || inst->ArgumentCount() == 0u) {
                    continue;
                }
                const auto flags = inst->Flags<MemoryFlags>();
                if (flags.index >= m_program.Resources().memoryInfo.size()) {
                    continue;
                }
                const auto memory = m_program.Resources().memoryInfo[flags.index];
                IrValue& image = *inst->Argument(0)->Resolve();
                if (std::ranges::none_of(directImages, [&](const IrValue* handle) { return SameHandle(*handle, image); })) {
                    static_cast<void>(PlanTableColumn(image, false, memory.imageR128));
                }
                if (imageInfo.needsSampler && inst->ArgumentCount() > 1u && !DirectSampler(*inst->Argument(1)->Resolve())) {
                    static_cast<void>(PlanTableColumn(*inst->Argument(1)->Resolve(), true, false));
                }
            }
        }
        for (const auto& plan : m_tableColumns) {
            for (std::size_t index = 0; index < plan.reads.size(); index++) {
                const IrValue* read = plan.reads[index];
                const bool planned = std::ranges::all_of(read->Uses(), [&](const IrValue* user) {
                    return FindTableColumn(*user) != nullptr;
                });
                if (planned && std::ranges::find(m_tablePlanningReads, read) == m_tablePlanningReads.end()) {
                    m_tablePlanningReads.push_back(read);
                    m_tablePlanningMemory.push_back(plan.memory[index]);
                }
            }
        }
        for (bool marked = true; marked;) {
            marked = false;
            for (const auto& plan : m_tableColumns) {
                for (std::size_t index = 0; index < plan.hopReads.size(); index++) {
                    const IrValue* read = plan.hopReads[index];
                    if (std::ranges::find(m_tablePlanningReads, read) != m_tablePlanningReads.end()) {
                        continue;
                    }
                    const bool planned = std::ranges::all_of(read->Uses(), [&](const IrValue* pointer) {
                        return pointer->Opcode() == IrOpcode::GetAddressResource && std::ranges::all_of(pointer->Uses(), [&](const IrValue* user) {
                            return std::ranges::find(m_tablePlanningReads, user) != m_tablePlanningReads.end();
                        });
                    });
                    if (planned) {
                        m_tablePlanningReads.push_back(read);
                        m_tablePlanningMemory.push_back(plan.hopMemory[index]);
                        marked = true;
                    }
                }
            }
        }
        m_memoryUsersIndexed = false;
        m_memoryUsers.clear();
    }

    void KeepSourceAlive(IrValue& position, const DescriptorSource& source, std::vector<const IrValue*>& referenced) {
        for (std::uint32_t dword = 0; dword < source.dwordCount; dword++) {
            IrValue* value = source.dwords[dword]->Resolve();
            if (value->HasImmediate() || std::ranges::find(referenced, value) != referenced.end()) {
                continue;
            }
            referenced.push_back(value);
            const std::array<IrValue*, 1> arguments{value};
            EmitBefore(position, IrOpcode::ReferenceU32, IrType::Void, arguments);
        }
    }

    bool DirectSampler(const IrValue& handle) {
        if (handle.Opcode() != IrOpcode::GetSamplerResource || handle.ArgumentCount() != 4u || FindTableColumn(handle) != nullptr) {
            return false;
        }
        DescriptorSource descriptor;
        MakeSource(handle, 4u, true, false, descriptor);
        std::uint32_t badDword = 0;
        return ValidateSource(descriptor, badDword);
    }

    bool DirectImage(const IrValue& handle) {
        if (handle.Opcode() != IrOpcode::GetImageResource || handle.ArgumentCount() != 8u) {
            return false;
        }
        DescriptorSource descriptor;
        MakeSource(handle, 8u, false, false, descriptor);
        std::uint32_t badDword = 0;
        return ValidateSource(descriptor, badDword);
    }

    std::vector<const IrValue*> DirectImageHandles() {
        std::vector<const IrValue*> handles;
        for (auto& block : m_program.Blocks()) {
            for (IrValue* inst : block->Instructions()) {
                const auto imageInfo = ImageOpcodeInfoOf(inst->Opcode());
                if (imageInfo.access == ImageAccess::None || inst->ArgumentCount() == 0u) {
                    continue;
                }
                const auto flags = inst->Flags<MemoryFlags>();
                if (flags.index >= m_program.Resources().memoryInfo.size()) {
                    continue;
                }
                const auto& memory = m_program.Resources().memoryInfo[flags.index];
                const bool tableUnsupported = imageInfo.resourceClass == ImageResourceClass::Storage || memory.imagePacked || memory.imageByElements != 0u;
                const IrValue* handle = inst->Argument(0)->Resolve();
                if (tableUnsupported && DirectImage(*handle)) {
                    handles.push_back(handle);
                }
            }
        }
        return handles;
    }

    bool SameHandle(const IrValue& left, const IrValue& right) const {
        if (&left == &right) {
            return true;
        }
        if (left.Opcode() != right.Opcode() || left.ArgumentCount() != right.ArgumentCount()) {
            return false;
        }
        for (std::size_t index = 0; index < left.ArgumentCount(); index++) {
            if (!EquivalentValue(m_program.Resources(), left.Argument(index), right.Argument(index))) {
                return false;
            }
        }
        return true;
    }

    bool IsTablePlanningMemory(std::uint32_t index) const {
        return std::ranges::find(m_tablePlanningMemory, index) != m_tablePlanningMemory.end();
    }

    bool TableSource(std::uint32_t source) const {
        return source < m_sources.size() && m_sources[source].tableColumn.has_value();
    }

    bool SameBufferSource(const IrValue& handle, std::uint32_t source) const {
        const auto& current = m_sources[source];
        if (current.tableColumn.has_value() || current.dwordCount != 4u || handle.ArgumentCount() != 4u) {
            return false;
        }
        for (std::uint32_t dword = 0; dword < 4u; dword++) {
            if (!EquivalentValue(m_program.Resources(), current.dwords[dword], handle.Argument(dword))) {
                return false;
            }
        }
        return true;
    }

    bool SelectionWritten(const TableColumn& column) const {
        const auto written = [&](const auto& matches) {
            return std::any_of(m_info.buffers.begin(), m_info.buffers.end(), [&](const auto& buffer) { return (buffer.written || buffer.atomic) && matches(buffer.source); });
        };
        std::vector<const IrValue*> pending{m_sources[column.selection].dwords[0]};
        std::vector<const IrValue*> seen;
        while (!pending.empty()) {
            const IrValue* current = pending.back()->Resolve();
            pending.pop_back();
            if (std::ranges::find(seen, current) != seen.end()) {
                continue;
            }
            seen.push_back(current);
            const bool pointer = current->Opcode() == IrOpcode::LoadAddressU32;
            const bool buffer = current->Opcode() == IrOpcode::ReadConstBuffer && current->ArgumentCount() != 0u;
            if ((pointer || buffer) && m_info.bdaWrites) {
                return true;
            }
            if (pointer && written([](std::uint32_t) { return true; })) {
                return true;
            }
            if (buffer) {
                const IrValue* handle = current->Argument(0)->Resolve();
                if (written([&](std::uint32_t source) { return SameBufferSource(*handle, source); })) {
                    return true;
                }
            }
            for (std::size_t index = 0; index < current->ArgumentCount(); index++) {
                pending.push_back(current->Argument(index));
            }
        }
        return false;
    }

    void CheckTableStores() {
        for (auto& source : m_sources) {
            if (source.tableColumn.has_value() && source.tableColumn->selected && SelectionWritten(*source.tableColumn)) {
                source.tableColumn->selected = false;
                source.tableColumn->keyDomains.clear();
            }
            if (!source.tableColumn.has_value() || source.tableColumn->keyDomains.empty()) {
                continue;
            }
            auto& column = *source.tableColumn;
            const bool written = std::any_of(column.keyDomains.begin(), column.keyDomains.end(), [&](const KeyDomain& domain) {
                return std::any_of(m_info.buffers.begin(), m_info.buffers.end(), [&](const auto& buffer) { return (buffer.written || buffer.atomic) && buffer.source == domain.source; });
            });
            if (written) {
                column.keyDomains.clear();
            }
        }
    }

    void AssignTables() {
        std::uint32_t next = 0;
        for (auto& image : m_info.images) {
            if (!TableSource(image.source)) {
                continue;
            }
            image.table = next++;
            if (image.atomic64 || image.packed || image.byElements != 0u) {
                image.tableOperation = TableOperation::Unsupported;
            }
        }
        for (auto& sampler : m_info.samplers) {
            if (TableSource(sampler.source)) {
                sampler.table = next++;
            }
        }
    }

    void GetHandle(IrValue* value, IrOpcode expected, std::uint32_t width, IrValue*& handle, std::uint32_t& source, bool sampler = false, bool sampleAdjust = false) {
        handle = value->Resolve();
        if (handle->Opcode() != expected) {
            fail("memory operation requires " + std::string(IrOpcodeName(expected)));
        }
        DescriptorSource descriptor;
        MakeSource(*handle, width, sampler, sampleAdjust, descriptor);
        std::uint32_t badDword = 0;
        if (!ValidateSource(descriptor, badDword)) {
            fail(std::string(IrOpcodeName(expected)) + " dword " + std::to_string(badDword) + " is not a valid runtime value; chain: " + describeValueChain(descriptor.dwords[badDword], 8u));
        }
        source = InternSource(descriptor);
    }

    bool TakeGpuDescriptor(IrValue& inst, std::uint32_t memoryIndex) {
        const IrValue* handle = inst.Argument(0)->Resolve();
        if (handle->Opcode() != IrOpcode::GetBufferResource || handle->ArgumentCount() != 4u) {
            return false;
        }
        DescriptorSource descriptor;
        MakeSource(*handle, 4u, false, false, descriptor);
        std::uint32_t badDword = 0;
        auto& memory = m_program.Resources().memoryInfo[memoryIndex];
        const auto access = BufferAccessOf(inst.Opcode());
        if (ValidateSource(descriptor, badDword)) {
            const auto source = InternSource(descriptor);
            const auto resource = AddBuffer(source, memory, inst.Opcode(), inst.Flags<MemoryFlags>().pc);
            if (resource == std::numeric_limits<std::uint32_t>::max()) fail("buffer resource limit exceeded");
            AddMemoryPatch(memoryIndex, resource, 0u, false);
            memory.gpuDescriptor = false;
            m_info.usesFaultBuffer = m_info.usesFaultBuffer || access == BufferAccess::Atomic;
            return true;
        }
        memory.gpuDescriptor = true;
        m_info.usesDma = true;
        m_info.bdaWrites = m_info.bdaWrites || access == BufferAccess::Write || access == BufferAccess::Atomic;
        return true;
    }

    void ValidateAddressHandle(IrValue* value) const {
        const IrValue* handle = value->Resolve();
        if (handle->Opcode() != IrOpcode::GetAddressResource) {
            fail("address operation requires GetAddressResource");
        }
        if (handle->ArgumentCount() != 2) {
            fail("GetAddressResource must have two address dwords");
        }
    }

    std::uint32_t AddBuffer(std::uint32_t source, const MemoryInfo& memory, IrOpcode op, std::uint32_t pc) {
        for (std::uint32_t i = 0; i < m_info.buffers.size(); i++) {
            if (m_info.buffers[i].source == source) {
                Merge(m_info.buffers[i], memory, op, pc);
                return i;
            }
        }
        BufferResource resource;
        resource.source = source;
        resource.firstUsePc = pc;
        Merge(resource, memory, op, pc);
        m_info.buffers.push_back(resource);
        return static_cast<std::uint32_t>(m_info.buffers.size() - 1);
    }

    static void Merge(BufferResource& resource, const MemoryInfo& memory, IrOpcode op, std::uint32_t pc) {
        const auto access = BufferAccessOf(op);
        const bool atomic = access == BufferAccess::Atomic;
        const bool write = access == BufferAccess::Write || atomic;
        resource.firstUsePc = std::min(resource.firstUsePc, pc);
        resource.maxByteExtent = std::max(resource.maxByteExtent, byteExtent(memory));
        resource.read = resource.read || !write || atomic;
        resource.written = resource.written || write;
        resource.atomic = resource.atomic || atomic;
        resource.formatted = resource.formatted || memory.formatted;
        resource.descriptorFormatted = resource.descriptorFormatted || (memory.formatted && !memory.typed);
        if (memory.formatted && memory.typed) {
            const auto format = GetFormatInfo(DecodeTBufferFormat(memory.dataFormat, memory.numberFormat));
            if (format.byteSize == 0u) fail("typed buffer instruction has an invalid format");
            resource.typedAlignment = std::max(resource.typedAlignment, static_cast<std::uint8_t>(std::min(format.byteSize, 4u)));
        }
        if (memory.formatted && !memory.typed && !write) resource.formattedReadMask |= (1u << std::min(memory.dataDwords, 4u)) - 1u;
        resource.scalar = resource.scalar || op == IrOpcode::ReadConstBuffer || memory.kind == ResourceKind::ScalarBuffer;
    }

    static ImageResource ImageKey(std::uint32_t source, const MemoryInfo& memory, IrOpcode op) {
        ImageResource image;
        image.source = source;
        image.resourceClass = ImageOpcodeInfoOf(op).resourceClass;
        image.depthCompare = (memory.imageSampleFlags & RdnaImageSampleFlagCompare) != 0;
        image.remappedDepth1D = image.depthCompare && (memory.imageDimension == RdnaImageDimension::Dim1D || memory.imageDimension == RdnaImageDimension::Dim1DArray);
        image.dimension = image.remappedDepth1D
            ? (memory.imageDimension == RdnaImageDimension::Dim1D ? RdnaImageDimension::Dim2D : RdnaImageDimension::Dim2DArray)
            : memory.imageDimension;
        image.mipMode = image.resourceClass == ImageResourceClass::Storage && memory.imageHasMip ? ImageMipMode::DynamicStorage : ImageMipMode::None;
        image.r128 = memory.imageR128;
        image.packed = memory.imagePacked;
        image.byElements = memory.imageByElements;
        image.byComponents = memory.imageByElements != 0u ? memory.dataDwords / memory.imageByElements : 0u;
        return image;
    }

    static bool SameImageKey(const ImageResource& image, const ImageResource& key) {
        return image.source == key.source && image.resourceClass == key.resourceClass && image.dimension == key.dimension && image.mipMode == key.mipMode && image.depthCompare == key.depthCompare && image.remappedDepth1D == key.remappedDepth1D && image.r128 == key.r128 && image.packed == key.packed && image.byElements == key.byElements && image.byComponents == key.byComponents;
    }

    std::uint32_t AddImage(std::uint32_t source, const MemoryInfo& memory, IrOpcode op, std::uint32_t pc) {
        const auto key = ImageKey(source, memory, op);
        for (std::uint32_t i = 0; i < m_info.images.size(); i++) {
            auto& image = m_info.images[i];
            if (SameImageKey(image, key)) {
                Merge(image, memory, op, pc);
                return i;
            }
        }
        if (m_info.images.size() >= ShaderInfo::MaxImages) {
            return std::numeric_limits<std::uint32_t>::max();
        }
        ImageResource image = key;
        image.singleMipPlaneBiasCompatible = m_program.Resources().stage == IrShaderStage::Pixel;
        image.volumeArraySampleCompatible = true;
        image.firstUsePc = pc;
        Merge(image, memory, op, pc);
        m_info.images.push_back(image);
        return static_cast<std::uint32_t>(m_info.images.size() - 1);
    }

    static void Merge(ImageResource& image, const MemoryInfo& memory, IrOpcode op, std::uint32_t pc) {
        const auto access = ImageOpcodeInfoOf(op).access;
        const bool atomic = access == ImageAccess::Atomic;
        const bool write = access == ImageAccess::Write || atomic;
        const bool atomic64 = IsImageAtomic64Opcode(op);
        if ((image.read || image.written) && image.atomic64 != atomic64) {
            throw std::runtime_error("an image accessed by 64-bit atomics is also accessed in another way");
        }
        image.atomic64 = atomic64;
        image.firstUsePc = std::min(image.firstUsePc, pc);
        image.read = image.read || !write || atomic;
        image.written = image.written || write;
        image.atomic = image.atomic || atomic;
        image.srgbDecodeCompatible = image.srgbDecodeCompatible && !ImageOpcodeInfoOf(op).needsSampler;
        image.fmaskCompatible = image.fmaskCompatible && op == IrOpcode::ImageRead && memory.dataBits == 32u;
        image.singleMipPlaneBiasCompatible = image.singleMipPlaneBiasCompatible && op == IrOpcode::ImageSampleRaw && memory.imageSampleFlags == RdnaImageSampleFlagBias && memory.dataBits == 32u && !memory.imageR128 && !memory.imagePacked && memory.imageByElements == 0u;
        image.depthBitsCompatible = image.depthBitsCompatible && memory.dataBits == 32u;
        image.constantSwizzleCompatible = image.constantSwizzleCompatible && (op == IrOpcode::ImageSampleRaw || op == IrOpcode::ImageGatherRaw);
        const bool cubeFaceLoad = op == IrOpcode::ImageRead && !memory.imageHasMip && memory.imageSampleFlags == 0u;
        const bool cubeFaceSample = op == IrOpcode::ImageSampleRaw && (memory.imageSampleFlags == 0u || memory.imageSampleFlags == RdnaImageSampleFlagLevelZero || memory.imageSampleFlags == RdnaImageSampleFlagLod);
        const bool cubeFaceStore = op == IrOpcode::ImageWrite && !memory.imageHasMip;
        image.cubeFaceCompatible = image.cubeFaceCompatible && (cubeFaceLoad || cubeFaceSample || cubeFaceStore) && !memory.imagePacked && memory.imageByElements == 0u;
        image.volumeArraySampleCompatible = image.volumeArraySampleCompatible && cubeFaceSample && !memory.imageR128 && !memory.imagePacked && memory.imageByElements == 0u;
        const bool flatVolumeLoad = op == IrOpcode::ImageRead && memory.imageSampleFlags == 0u;
        const bool flatVolumeSample = op == IrOpcode::ImageSampleRaw && memory.imageSampleFlags == RdnaImageSampleFlagLevelZero;
        const bool flatVolumeQuery = op == IrOpcode::ImageQueryDimensions;
        image.flatVolumeCompatible = image.flatVolumeCompatible && (flatVolumeLoad || flatVolumeSample || flatVolumeQuery) && !memory.imagePacked && memory.imageByElements == 0u;
        const bool flatLineLoad = op == IrOpcode::ImageRead && !memory.imageHasMip && memory.imageSampleFlags == 0u;
        const auto lineSampleFlags = memory.imageSampleFlags & ~RdnaImageSampleFlagCompare;
        const bool flatLineSample = op == IrOpcode::ImageSampleRaw && (lineSampleFlags == RdnaImageSampleFlagLevelZero || lineSampleFlags == 0u);
        const bool flatLineExplicitLod = op == IrOpcode::ImageSampleRaw && memory.imageSampleFlags == RdnaImageSampleFlagLod;
        const bool flatLineLevelZero = op == IrOpcode::ImageSampleRaw && memory.imageSampleFlags == RdnaImageSampleFlagLevelZero;
        const bool flatLineAccess = image.dimension == RdnaImageDimension::Dim2DArray ? flatLineLevelZero || flatLineExplicitLod : flatLineLoad || flatLineSample || flatLineExplicitLod;
        image.flatLineCompatible = image.flatLineCompatible && flatLineAccess && !memory.imagePacked && memory.imageByElements == 0u;
        image.flatVolumeLineCompatible = image.flatVolumeLineCompatible && (flatLineLoad || flatLineSample) && !memory.imagePacked && memory.imageByElements == 0u;
        image.dimensionQueryOnly = image.dimensionQueryOnly && op == IrOpcode::ImageQueryDimensions;
        if ((memory.imageSampleFlags & RdnaImageSampleFlagCompare) != 0u) {
            constexpr auto unsupported = RdnaImageSampleFlagLod | RdnaImageSampleFlagDerivative;
            if (op == IrOpcode::ImageGatherRaw || (memory.imageSampleFlags & unsupported) != 0u) image.emulatedCompare |= EmulatedCompare::Unsupported;
            if ((memory.imageSampleFlags & RdnaImageSampleFlagLevelZero) == 0u) image.emulatedCompare |= EmulatedCompare::RequiresSingleLevel;
        } else if (ImageOpcodeInfoOf(op).needsSampler) {
            if (op != IrOpcode::ImageSampleRaw) image.emulatedCompare |= EmulatedCompare::ConvertedSampleUnsupported;
            if ((memory.imageSampleFlags & RdnaImageSampleFlagLevelZero) == 0u) image.emulatedCompare |= EmulatedCompare::ConvertedSampleRequiresSingleLevel;
        }
    }

    std::uint32_t AddSampler(std::uint32_t source, std::uint32_t pc) {
        for (std::uint32_t i = 0; i < m_info.samplers.size(); i++) {
            if (m_info.samplers[i].source == source) {
                m_info.samplers[i].firstUsePc = std::min(m_info.samplers[i].firstUsePc, pc);
                return i;
            }
        }
        if (m_info.samplers.size() >= ShaderInfo::MaxSamplers) {
            return std::numeric_limits<std::uint32_t>::max();
        }
        m_info.samplers.push_back({source, pc});
        return static_cast<std::uint32_t>(m_info.samplers.size() - 1);
    }

    void AddSampledPair(std::uint32_t image, std::uint32_t sampler, std::uint32_t pc) {
        for (auto& pair : m_info.sampledPairs) {
            if (pair.image == image && pair.sampler == sampler) {
                pair.firstUsePc = std::min(pair.firstUsePc, pc);
                return;
            }
        }
        if (m_info.sampledPairs.size() >= ShaderInfo::MaxSampledPairs) {
            fail("sampled image/sampler pair limit exceeded");
        }
        m_info.sampledPairs.push_back({image, sampler, pc});
    }

    void AddHandlePatch(IrValue* handle, std::uint32_t resource) {
        for (const auto& patch : m_handlePatches) {
            if (patch.handle == handle) {
                if (patch.resource != resource) {
                    fail(std::string(IrOpcodeName(handle->Opcode())) + " is reused with incompatible resource classes");
                }
                return;
            }
        }
        m_handlePatches.push_back({handle, resource});
    }

    void AddMemoryPatch(std::uint32_t index, std::uint32_t resource, std::uint32_t sampler, bool hasSampler) {
        for (auto& patch : m_memoryPatches) {
            if (patch.index != index) {
                continue;
            }
            if (patch.resource != resource || (hasSampler && patch.hasSampler && patch.sampler != sampler)) {
                fail("memory metadata is reused with incompatible resources");
            }
            if (hasSampler) {
                patch.sampler = sampler;
                patch.hasSampler = true;
            }
            return;
        }
        m_memoryPatches.push_back({index, resource, sampler, hasSampler});
    }

    void Collect(IrValue& inst) {
        const auto op = inst.Opcode();
        if (op == IrOpcode::ImageBvhIntersectRay) {
            m_info.usesDma = true;
            return;
        }
        const auto buffer = BufferAccessOf(op);
        const auto addressInfo = AddressOpcodeInfoOf(op);
        const auto imageInfo = ImageOpcodeInfoOf(op);
        if (buffer == BufferAccess::None && addressInfo.access == AddressAccess::None && imageInfo.access == ImageAccess::None) {
            return;
        }
        const auto flags = inst.Flags<MemoryFlags>();
        if (flags.index >= m_program.Resources().memoryInfo.size()) {
            fail("memory metadata index " + std::to_string(flags.index) + " is out of range");
        }
        if (inst.ArgumentCount() == 0) {
            fail("memory operation has no resource handle");
        }
        const auto& memory = m_program.Resources().memoryInfo[flags.index];
        if (memory.planningOnly || IsTablePlanningMemory(flags.index)) {
            return;
        }
        IrValue* handle = nullptr;
        std::uint32_t source = 0;
        std::uint32_t resource = 0;

        if (buffer != BufferAccess::None) {
            if (!TakeGpuDescriptor(inst, flags.index)) fail("buffer operation requires a four-dword runtime V#");
            return;
        }
        if (addressInfo.access != AddressAccess::None) {
            if (!IsAddressResourceKind(memory.kind)) {
                fail("address operation has invalid resource kind");
            }
            if (memory.kind == ResourceKind::Scratch) {
                handle = inst.Argument(0)->Resolve();
                if (handle->Opcode() != IrOpcode::GetScratchResource || handle->ArgumentCount() != 0) {
                    fail("scratch operation requires GetScratchResource");
                }
                if (m_info.scratchDwords == 0) {
                    fail("scratch operation requires a nonzero AGC per-thread size");
                }
                return;
            }
            ValidateAddressHandle(inst.Argument(0));
            // Debug aid: APS5_TRACE_BDA=1 names every access that makes the program address-based
            // (a raw scalar load the SRT walker left in place, or a flat/global access), so the
            // reason a stage takes the BDA path can be read off without a shader dump.
            // The first few accesses of a program are printed (a Bink kernel has hundreds); Run
            // reports how many more there were.
            if (bdaTraceEnabled() && ++m_bdaTraces <= bdaTraceLimit) {
                const auto* kind = memory.kind == ResourceKind::ScalarAddress ? "scalar address" : memory.kind == ResourceKind::Global ? "global" : "flat";
                const IrValue* offset = inst.ArgumentCount() > 1 ? inst.Argument(1)->Resolve() : nullptr;
                const bool immediateOffset = offset != nullptr && offset->HasImmediate();
                std::fprintf(stderr, "[bda] %s at pc 0x%08x: %s access, offset %s%s\n", std::string(IrOpcodeName(op)).c_str(), flags.pc, kind, immediateOffset ? "immediate" : "dynamic", memory.kind == ResourceKind::ScalarAddress && !immediateOffset ? " (a register offset is not planned by the SRT walker)" : "");
            }
            m_info.usesDma = true;
            m_info.bdaWrites = m_info.bdaWrites || addressInfo.access == AddressAccess::Write || addressInfo.access == AddressAccess::Atomic;
            return;
        }

        if (memory.kind != ResourceKind::Image || imageInfo.resourceClass == ImageResourceClass::None) {
            fail("image operation has invalid resource kind");
        }
        handle = inst.Argument(0)->Resolve();
        const TableColumnPlan* column = FindTableColumn(*handle);
        if (column != nullptr) {
            source = column->source;
        } else {
            GetHandle(inst.Argument(0), IrOpcode::GetImageResource, 8, handle, source);
        }
        resource = AddImage(source, memory, op, flags.pc);
        NoteBlockImage(resource);
        if (resource == std::numeric_limits<std::uint32_t>::max()) {
            fail("image resource limit exceeded");
        }
        AddHandlePatch(handle, resource);
        std::uint32_t sampler = 0;
        if (imageInfo.needsSampler) {
            if (inst.ArgumentCount() < 2) {
                fail("sampled image operation has no sampler handle");
            }
            IrValue* samplerHandle = inst.Argument(1)->Resolve();
            std::uint32_t samplerSource = 0;
            const TableColumnPlan* samplerColumn = FindTableColumn(*samplerHandle);
            if (samplerColumn != nullptr) {
                samplerSource = samplerColumn->source;
            } else {
                const bool sampleAdjust = (memory.imageSampleFlags & RdnaImageSampleFlagAdjust) != 0;
                GetHandle(inst.Argument(1), IrOpcode::GetSamplerResource, 4, samplerHandle, samplerSource, true, sampleAdjust);
            }
            sampler = AddSampler(samplerSource, flags.pc);
            if (sampler == std::numeric_limits<std::uint32_t>::max()) {
                fail("sampler resource limit exceeded");
            }
            AddHandlePatch(samplerHandle, sampler);
            AddSampledPair(resource, sampler, flags.pc);
        }
        AddMemoryPatch(flags.index, resource, sampler, imageInfo.needsSampler);
    }

    const DescriptorSource* Source(std::uint32_t source) const {
        return source < m_sources.size() ? &m_sources[source] : nullptr;
    }

    void LinkImageAliases() {
        for (auto& buffer : m_info.buffers) {
            const DescriptorSource* bufferSource = Source(buffer.source);
            if (bufferSource == nullptr || bufferSource->dwordCount != 4) {
                continue;
            }
            for (std::uint32_t image = 0; image < m_info.images.size(); image++) {
                const DescriptorSource* imageSource = Source(m_info.images[image].source);
                if (imageSource == nullptr || imageSource->dwordCount != 8 || imageSource->tableColumn.has_value()) {
                    continue;
                }
                bool alias = true;
                for (std::uint32_t dword = 0; dword < 4; dword++) {
                    alias = alias && EquivalentValue(m_program.Resources(), bufferSource->dwords[dword], imageSource->dwords[dword]);
                }
                if (alias) {
                    buffer.imageAlias = image;
                    break;
                }
            }
        }
    }

    IrProgram& m_program;
    ShaderInfo m_info;
    IrBuilder m_builder;
    std::vector<DescriptorSource> m_sources;
    std::vector<std::vector<std::uint32_t>> m_blockSources;
    std::vector<std::vector<std::uint32_t>> m_blockImages;
    std::uint32_t m_currentBlock = std::numeric_limits<std::uint32_t>::max();
    std::vector<HandlePatch> m_handlePatches;
    std::vector<MemoryPatch> m_memoryPatches;
    // APS5_TRACE_BDA: address accesses seen by Collect (the first bdaTraceLimit are printed).
    unsigned m_bdaTraces = 0;
    std::unordered_map<std::uint32_t, std::vector<const IrValue*>> m_memoryUsers;
    bool m_memoryUsersIndexed = false;
    std::vector<TableColumnPlan> m_tableColumns;
    std::vector<const IrValue*> m_tablePlanningReads;
    std::vector<std::uint32_t> m_tablePlanningMemory;
    std::vector<const IrValue*> m_unselectedHandles;
    struct EdgeSelectorEntry {
        IrBlock* block = nullptr;
        std::vector<bool> edges;
        IrValue* value = nullptr;
    };
    std::vector<EdgeSelectorEntry> m_edgeSelectors;
};

}

void ResourceTracker::Track(IrProgram& program) const {
    Tracker(program).Run();
}

}
