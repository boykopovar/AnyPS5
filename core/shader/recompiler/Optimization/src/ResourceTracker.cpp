#include "Optimization/ResourceTracker.hpp"
#include "Optimization/SrtWalker.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/SrtWalker/SrtInstructionPredicates.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>
#include <string>
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
        PlanIndirectImages();
        for (auto& block : m_program.Blocks()) {
            for (IrValue* inst : block->Instructions()) {
                Collect(*inst);
            }
        }
        if (m_bdaTraces > bdaTraceLimit) std::fprintf(stderr, "[bda] %u more address accesses in this program not shown\n", m_bdaTraces - bdaTraceLimit);
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
        for (const auto& plan : m_indirectImages) {
            plan.handle->ReplaceArgument(0, plan.key);
            for (std::uint32_t dword = 0; dword < 4u; dword++) {
                plan.handle->ReplaceArgument(dword + 1u, plan.roots[dword]);
            }
            for (std::uint32_t dword = 5u; dword < plan.roots.size(); dword++) {
                plan.handle->ReplaceArgument(dword, plan.key);
            }
            for (const auto index : plan.memory) {
                m_program.Resources().memoryInfo[index].planningOnly = true;
            }
        }
        std::erase_if(m_program.Metadata().dynamicReads, [&](IrValue* value) {
            const IrValue* inst = value->Resolve();
            return std::ranges::any_of(m_indirectImages, [&](const IndirectImagePlan& plan) {
                return std::ranges::find(plan.reads, inst) != plan.reads.end();
            });
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

    struct IndirectImagePlan {
        IrValue* handle = nullptr;
        std::uint32_t source = 0;
        IrValue* key = nullptr;
        std::array<IrValue*, 8> roots {};
        std::array<std::uint32_t, 8> memory {};
        std::array<const IrValue*, 8> reads {};
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
            if (value->Opcode() != IrOpcode::BitwiseOr32) {
                return value;
            }
            IrValue* left = value->Argument(0)->Resolve();
            IrValue* right = value->Argument(1)->Resolve();
            const bool leftReserved = (possibleU32Bits(left) & ~samplerDword3ReservedMask) == 0u;
            const bool rightReserved = (possibleU32Bits(right) & ~samplerDword3ReservedMask) == 0u;
            if (leftReserved && rightReserved) {
                return &m_builder.Constant(0u);
            }
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
            if (current.dwordCount != descriptor.dwordCount || current.indirectImage != descriptor.indirectImage) {
                continue;
            }
            bool same = true;
            for (std::uint32_t i = 0; i < descriptor.dwordCount; i++) {
                same = same && EquivalentValue(m_program.Resources(), current.dwords[i], descriptor.dwords[i]);
            }
            if (same) {
                return candidate;
            }
        }
        m_sources.push_back(descriptor);
        return static_cast<std::uint32_t>(m_sources.size() - 1);
    }

    static bool immediateU32(IrValue* value, std::uint32_t& result) {
        value = value->Resolve();
        if (!value->HasImmediate() || value->Type() != IrType::U32) {
            return false;
        }
        result = value->ImmediateU32();
        return true;
    }

    static bool usesOnly(const IrValue& value, std::span<const IrValue* const> users) {
        return !value.Uses().empty() && std::ranges::all_of(value.Uses(), [&](const IrValue* user) {
            return std::ranges::find(users, user) != users.end();
        });
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

    bool MemoryIndexBelongsTo(std::uint32_t index, const IrValue& owner) const {
        for (const auto& block : m_program.Blocks()) {
            for (const IrValue* inst : block->Instructions()) {
                const auto op = inst->Opcode();
                if ((BufferAccessOf(op) == BufferAccess::None && AddressOpcodeInfoOf(op).access == AddressAccess::None && ImageOpcodeInfoOf(op).access == ImageAccess::None) || inst == &owner) {
                    continue;
                }
                if (inst->Flags<MemoryFlags>().index == index) {
                    return false;
                }
            }
        }
        return true;
    }

    bool MakeRuntimeBufferSource(const IrValue& handle, std::uint32_t& source, DescriptorSource& descriptor) {
        if (handle.Opcode() != IrOpcode::GetBufferResource) {
            return false;
        }
        MakeSource(handle, 4u, false, false, descriptor);
        std::uint32_t badDword = 0;
        if (!ValidateSource(descriptor, badDword)) {
            return false;
        }
        source = InternSource(descriptor);
        return true;
    }

    bool MatchMaterialOffset(IrValue* value, IrValue*& selector, std::uint32_t& stride, std::uint32_t& offset) const {
        value = value->Resolve();
        offset = 0;
        IrValue* candidate = value;
        if (candidate->Opcode() == IrOpcode::IAdd32 && candidate->ArgumentCount() == 2u) {
            std::uint32_t immediate = 0;
            if (immediateU32(candidate->Argument(0), immediate)) {
                value = candidate->Argument(1)->Resolve();
            } else if (immediateU32(candidate->Argument(1), immediate)) {
                value = candidate->Argument(0)->Resolve();
            } else {
                return false;
            }
            offset = immediate;
        }
        IrValue* multiply = value;
        if (multiply->Opcode() != IrOpcode::IMul32 || multiply->ArgumentCount() != 2u) {
            return false;
        }
        if (immediateU32(multiply->Argument(0), stride)) {
            selector = multiply->Argument(1)->Resolve();
        } else if (immediateU32(multiply->Argument(1), stride)) {
            selector = multiply->Argument(0)->Resolve();
        } else {
            return false;
        }
        return stride != 0u && selector->Opcode() == IrOpcode::ReadFirstLane;
    }

    // The table entry offset `key * 32` (a shift or a multiply), optionally plus an immediate.
    static IrValue* StripKeyMask(IrValue* value) {
        value = value->Resolve();
        if (value->Opcode() != IrOpcode::BitwiseAnd32 || value->ArgumentCount() != 2u) return value;
        std::uint32_t mask = 0;
        IrValue* masked = nullptr;
        if (immediateU32(value->Argument(1), mask)) masked = value->Argument(0);
        else if (immediateU32(value->Argument(0), mask)) masked = value->Argument(1);
        if (masked == nullptr) return value;
        const std::uint64_t span = (static_cast<std::uint64_t>(mask) | 31u) + 1u;
        if ((span & (span - 1u)) != 0u || (mask >> 5u) < 0xffffu || (mask & 0xffffffe0u) != ((span - 1u) & 0xffffffe0u)) return value;
        return masked->Resolve();
    }

    static bool MatchTableOffset(IrValue* value, IrValue*& key, std::uint32_t& entryOffset) {
        value = StripKeyMask(value);
        entryOffset = 0;
        if (value->Opcode() == IrOpcode::IAdd32 && value->ArgumentCount() == 2u) {
            std::uint32_t immediate = 0;
            if (immediateU32(value->Argument(0), immediate)) {
                value = value->Argument(1)->Resolve();
            } else if (immediateU32(value->Argument(1), immediate)) {
                value = value->Argument(0)->Resolve();
            } else {
                return false;
            }
            entryOffset = immediate;
            value = StripKeyMask(value);
        }
        if (value->ArgumentCount() != 2u) {
            return false;
        }
        std::uint32_t scale = 0;
        if (value->Opcode() == IrOpcode::ShiftLeftLogical32) {
            if (!immediateU32(value->Argument(1), scale) || scale != 5u) {
                return false;
            }
            key = value->Argument(0)->Resolve();
            return true;
        }
        if (value->Opcode() != IrOpcode::IMul32) {
            return false;
        }
        if (immediateU32(value->Argument(1), scale)) {
            key = value->Argument(0)->Resolve();
        } else if (immediateU32(value->Argument(0), scale)) {
            key = value->Argument(1)->Resolve();
        } else {
            return false;
        }
        return scale == 32u;
    }

    // A T# whose eight dwords are scalar reads of one V# (the table) at `entryOffset + key * 32`
    // with a wave-uniform runtime key: a bindless image table. The key stays an ordinary value
    // (the SPIR-V selects the bound slot from it); the eight reads become planning-only.
    bool TryMakeTableImage(IrValue& handle, IndirectImagePlan& plan) {
        if (handle.Opcode() != IrOpcode::GetImageResource || handle.ArgumentCount() != 8u) {
            return false;
        }

        std::array<IrValue*, 8> heapReads {};
        IrValue* heapHandle = nullptr;
        IrValue* heapOffset = nullptr;
        std::uint32_t immediateOffset = 0;
        for (std::uint32_t dword = 0; dword < heapReads.size(); dword++) {
            heapReads[dword] = handle.Argument(dword)->Resolve();
            std::uint32_t memoryIndex = 0;
            const MemoryInfo* memory = ScalarReadMemory(*heapReads[dword], memoryIndex);
            if (memory == nullptr || !MemoryIndexBelongsTo(memoryIndex, *heapReads[dword])) {
                return false;
            }
            if (dword == 0u) {
                immediateOffset = memory->offset;
            } else if (memory->offset != immediateOffset + dword * sizeof(std::uint32_t)) {
                return false;
            }
            IrValue* currentHandle = heapReads[dword]->Argument(0)->Resolve();
            if (heapHandle != nullptr && currentHandle != heapHandle) {
                return false;
            }
            heapHandle = currentHandle;
            if (dword == 0u) {
                heapOffset = heapReads[dword]->Argument(1)->Resolve();
            } else if (!EquivalentValue(m_program.Resources(), heapOffset, heapReads[dword]->Argument(1))) {
                return false;
            }
            plan.memory[dword] = memoryIndex;
            plan.reads[dword] = heapReads[dword];
        }

        IrValue* key = nullptr;
        std::uint32_t entryOffset = 0;
        static const bool traceMatch = std::getenv("APS5_TRACE_TABLE_MATCH") != nullptr;
        if (!MatchTableOffset(heapOffset, key, entryOffset) || key->Type() != IrType::U32) {
            if (traceMatch) {
                std::uint32_t mask = 0;
                const IrValue* resolved = heapOffset->Resolve();
                if (resolved->Opcode() == IrOpcode::BitwiseAnd32) immediateU32(resolved->Argument(1), mask);
                std::fprintf(stderr, "[table-match] heap offset is not key * 32 (+ immediate), mask 0x%x: %s\n", mask, describeValueChain(heapOffset, 4u).c_str());
            }
            return false;
        }
        entryOffset += immediateOffset;

        std::vector<const IrValue*> imageUsers {&handle};
        for (const IrValue* user : heapReads[0]->Uses()) {
            if (user == &handle || user->Opcode() != IrOpcode::GetImageResource || user->ArgumentCount() != 8u) continue;
            bool same = true;
            for (std::uint32_t dword = 0; dword < 8u && same; dword++) same = user->Argument(dword)->Resolve() == heapReads[dword];
            if (same) imageUsers.push_back(user);
        }
        for (const auto* read : heapReads) {
            if (!usesOnly(*read, std::span<const IrValue* const>(imageUsers))) {
                if (traceMatch) {
                    std::string users;
                    for (const IrValue* user : read->Uses()) users += std::string(" ") + std::string(IrOpcodeName(user->Opcode())) + (user == &handle ? "(this)" : "");
                    std::fprintf(stderr, "[table-match] a T# dword has users besides its image handle:%s\n", users.c_str());
                }
                return false;
            }
        }

        DescriptorSource heapSource;
        std::uint32_t heapSourceIndex = 0;
        if (!MakeRuntimeBufferSource(*heapHandle, heapSourceIndex, heapSource)) {
            if (traceMatch) std::fprintf(stderr, "[table-match] the heap V# is no runtime buffer source\n");
            return false;
        }

        DescriptorSource::IndirectImage table;
        table.heapSource = heapSourceIndex;
        table.materialSource = heapSourceIndex;
        table.entryOffset = entryOffset;
        DescriptorSource materialSource = heapSource;
        std::uint32_t materialMemoryIndex = 0;
        const MemoryInfo* materialMemory = ScalarReadMemory(*key, materialMemoryIndex);
        if (materialMemory != nullptr && MemoryIndexBelongsTo(materialMemoryIndex, *key)) {
            IrValue* selector = nullptr;
            std::uint32_t selectorStride = 0;
            std::uint32_t selectorOffset = 0;
            DescriptorSource candidateSource;
            std::uint32_t candidateIndex = 0;
            if (MatchMaterialOffset(key->Argument(1), selector, selectorStride, selectorOffset) && MakeRuntimeBufferSource(*key->Argument(0)->Resolve(), candidateIndex, candidateSource)) {
                table.hasMaterial = true;
                table.materialSource = candidateIndex;
                table.selectorStride = selectorStride;
                table.selectorOffset = selectorOffset + materialMemory->offset;
                materialSource = candidateSource;
            }
        }

        DescriptorSource imageSource;
        imageSource.dwordCount = 8u;
        std::copy(heapSource.dwords.begin(), heapSource.dwords.begin() + 4u, imageSource.dwords.begin());
        std::copy(materialSource.dwords.begin(), materialSource.dwords.begin() + 4u, imageSource.dwords.begin() + 4u);
        imageSource.indirectImage = table;

        plan.handle = &handle;
        plan.source = InternSource(imageSource);
        plan.key = key;
        plan.roots = imageSource.dwords;
        return true;
    }

    const IndirectImagePlan* FindIndirectImage(const IrValue& handle) const {
        const auto found = std::ranges::find_if(m_indirectImages, [&](const IndirectImagePlan& plan) {
            return plan.handle == &handle;
        });
        return found == m_indirectImages.end() ? nullptr : &*found;
    }

    bool IsIndirectPlanningMemory(std::uint32_t index) const {
        return std::ranges::any_of(m_indirectImages, [&](const IndirectImagePlan& plan) {
            return std::ranges::find(plan.memory, index) != plan.memory.end();
        });
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
        if (inst.ArgumentCount() < handleCount) {
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
        IrBlock* block = phis.front()->Parent();
        if (block == nullptr || inst.Parent() == nullptr) {
            return;
        }
        const auto& predecessors = block->Predecessors();
        if (predecessors.size() < 2u) {
            return;
        }
        for (const IrValue* phi : phis) {
            if (phi->Parent() != block || phi->PhiBlockCount() != predecessors.size()) {
                return;
            }
            for (const IrBlock* predecessor : predecessors) {
                if (PhiIncoming(*phi, predecessor) == nullptr) {
                    return;
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
                        return;
                    }
                }
            }
        }

        std::vector<IrValue*> copies;
        const auto flags = inst.Flags<MemoryFlags>();
        const auto memory = m_program.Resources().memoryInfo.at(flags.index);
        for (std::uint32_t arm = 0; arm < arms.size(); arm++) {
            std::vector<IrValue*> arguments(inst.Arguments().begin(), inst.Arguments().end());
            for (std::uint32_t slot = 0; slot < handleCount; slot++) {
                if (!split[slot]) {
                    continue;
                }
                std::vector<IrValue*> dwords;
                for (std::size_t dword = 0; dword < handles[slot]->ArgumentCount(); dword++) {
                    dwords.push_back(SubstituteEdge(handles[slot]->Argument(dword), arms[arm], inst, true, phiSearchDepth));
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
                    std::vector<bool> edges(edgeArm.size());
                    for (std::size_t edge = 0; edge < edgeArm.size(); edge++) {
                        edges[edge] = edgeArm[edge] == arm;
                    }
                    const std::array<IrValue*, 3> arguments {&EdgeSelector(*block, edges), extract(copies[arm]), selected};
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

    void PlanIndirectImages() {
        // Kill switch: APS5_NO_BINDLESS_IMAGES=1 leaves table-loaded T#s unplanned (GetHandle then
        // rejects them as before).
        static const bool enabled = std::getenv("APS5_NO_BINDLESS_IMAGES") == nullptr;
        if (!enabled) {
            return;
        }
        for (auto& block : m_program.Blocks()) {
            for (IrValue* inst : block->Instructions()) {
                const auto imageInfo = ImageOpcodeInfoOf(inst->Opcode());
                if (imageInfo.access == ImageAccess::None || inst->ArgumentCount() == 0u) {
                    continue;
                }
                IrValue* handle = inst->Argument(0)->Resolve();
                const IndirectImagePlan* planned = FindIndirectImage(*handle);
                IndirectImagePlan plan;
                if (planned == nullptr && !TryMakeTableImage(*handle, plan)) {
                    continue;
                }
                // A store through a table would mark every bound slot pending write-back.
                if (imageInfo.resourceClass == ImageResourceClass::Storage) {
                    ResourceMaterializer::CountBindlessRejection(BindlessRejection::Storage);
                    fail("bindless storage image tables are unsupported");
                }
                if (planned == nullptr) {
                    m_indirectImages.push_back(std::move(plan));
                }
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
        if (expected == IrOpcode::GetImageResource) {
            for (; badDword < descriptor.dwordCount; badDword++) {
                const IrValue* value2 = descriptor.dwords[badDword]->Resolve();
                if (value2->Opcode() == IrOpcode::ReadConstBuffer) {
                    fail(std::string(IrOpcodeName(expected)) + " dword " + std::to_string(badDword) + " is not a valid runtime value; chain: " + describeValueChain(descriptor.dwords[badDword], 8u));
                }
            }
            badDword = 0;
        }
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
        if (ValidateSource(descriptor, badDword)) {
            return false;
        }
        const auto op = inst.Opcode();
        const bool load = op == IrOpcode::LoadBufferU32 || op == IrOpcode::LoadBufferU32x2 || op == IrOpcode::LoadBufferU32x3 || op == IrOpcode::LoadBufferU32x4 || op == IrOpcode::ReadConstBuffer;
        const bool store = op == IrOpcode::StoreBufferU32 || op == IrOpcode::StoreBufferU32x2 || op == IrOpcode::StoreBufferU32x3 || op == IrOpcode::StoreBufferU32x4;
        auto& memory = m_program.Resources().memoryInfo[memoryIndex];
        if ((!load && !store) || memory.formatted || memory.typed || memory.dataBits != 32u) {
            return false;
        }
        memory.gpuDescriptor = true;
        m_info.usesDma = true;
        m_info.bdaWrites = m_info.bdaWrites || store;
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
        resource.scalar = resource.scalar || op == IrOpcode::ReadConstBuffer || memory.kind == ResourceKind::ScalarBuffer;
    }

    std::uint32_t AddImage(std::uint32_t source, const MemoryInfo& memory, IrOpcode op, std::uint32_t pc) {
        const auto resourceClass = ImageOpcodeInfoOf(op).resourceClass;
        const auto mip = resourceClass == ImageResourceClass::Storage && memory.imageHasMip ? ImageMipMode::DynamicStorage : ImageMipMode::None;
        const bool depth = (memory.imageSampleFlags & RdnaImageSampleFlagCompare) != 0;
        for (std::uint32_t i = 0; i < m_info.images.size(); i++) {
            auto& image = m_info.images[i];
            if (image.source == source && image.resourceClass == resourceClass && image.dimension == memory.imageDimension && image.mipMode == mip && image.depthCompare == depth && image.r128 == memory.imageR128 && image.packed == memory.imagePacked) {
                Merge(image, op, pc);
                return i;
            }
        }
        if (m_info.images.size() >= ShaderInfo::MaxImages) {
            return std::numeric_limits<std::uint32_t>::max();
        }
        ImageResource image;
        image.source = source;
        image.firstUsePc = pc;
        image.resourceClass = resourceClass;
        image.dimension = memory.imageDimension;
        image.mipMode = mip;
        image.depthCompare = depth;
        image.r128 = memory.imageR128;
        image.packed = memory.imagePacked;
        Merge(image, op, pc);
        m_info.images.push_back(image);
        return static_cast<std::uint32_t>(m_info.images.size() - 1);
    }

    static void Merge(ImageResource& image, IrOpcode op, std::uint32_t pc) {
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
        if (memory.planningOnly || IsIndirectPlanningMemory(flags.index)) {
            return;
        }
        IrValue* handle = nullptr;
        std::uint32_t source = 0;
        std::uint32_t resource = 0;

        if (buffer != BufferAccess::None) {
            if (TakeGpuDescriptor(inst, flags.index)) {
                return;
            }
            GetHandle(inst.Argument(0), IrOpcode::GetBufferResource, 4, handle, source);
            resource = AddBuffer(source, memory, op, flags.pc);
            AddHandlePatch(handle, resource);
            AddMemoryPatch(flags.index, resource, 0, false);
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
        const IndirectImagePlan* indirect = FindIndirectImage(*handle);
        if (indirect != nullptr) {
            source = indirect->source;
        } else {
            GetHandle(inst.Argument(0), IrOpcode::GetImageResource, 8, handle, source);
        }
        resource = AddImage(source, memory, op, flags.pc);
        if (resource == std::numeric_limits<std::uint32_t>::max()) {
            fail("image resource limit exceeded");
        }
        AddHandlePatch(handle, resource);
        std::uint32_t sampler = 0;
        if (imageInfo.needsSampler) {
            if (inst.ArgumentCount() < 2) {
                fail("sampled image operation has no sampler handle");
            }
            IrValue* samplerHandle = nullptr;
            std::uint32_t samplerSource = 0;
            const bool sampleAdjust = (memory.imageSampleFlags & RdnaImageSampleFlagAdjust) != 0;
            GetHandle(inst.Argument(1), IrOpcode::GetSamplerResource, 4, samplerHandle, samplerSource, true, sampleAdjust);
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
                if (imageSource == nullptr || imageSource->dwordCount != 8 || imageSource->indirectImage.has_value()) {
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
    std::vector<HandlePatch> m_handlePatches;
    std::vector<MemoryPatch> m_memoryPatches;
    // APS5_TRACE_BDA: address accesses seen by Collect (the first bdaTraceLimit are printed).
    unsigned m_bdaTraces = 0;
    std::vector<IndirectImagePlan> m_indirectImages;
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
