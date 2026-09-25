#include "Optimization/ResourceTracker.hpp"
#include "Optimization/SrtWalker.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
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
    }

    void Run() {
        if (m_program.Resources().resourceTrackingComplete) {
            fail("resources already tracked");
        }
        if (!m_program.Resources().srtPlanComplete) {
            fail("SRT plan is not ready");
        }
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
                plan.handle->ReplaceArgument(dword + 1u, plan.roots[dword + 4u]);
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

    bool TryMakeIndirectImage(IrValue& handle, IndirectImagePlan& plan) {
        if (handle.Opcode() != IrOpcode::GetImageResource || handle.ArgumentCount() != 8u) {
            return false;
        }

        std::array<IrValue*, 8> heapReads {};
        IrValue* heapHandle = nullptr;
        IrValue* heapOffset = nullptr;
        for (std::uint32_t dword = 0; dword < heapReads.size(); dword++) {
            heapReads[dword] = handle.Argument(dword)->Resolve();
            std::uint32_t memoryIndex = 0;
            const MemoryInfo* memory = ScalarReadMemory(*heapReads[dword], memoryIndex);
            if (memory == nullptr || memory->offset != dword * sizeof(std::uint32_t) || !MemoryIndexBelongsTo(memoryIndex, *heapReads[dword])) {
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

        IrValue* shift = heapOffset;
        std::uint32_t shiftAmount = 0;
        if (shift->Opcode() != IrOpcode::ShiftLeftLogical32 || shift->ArgumentCount() != 2u || !immediateU32(shift->Argument(1), shiftAmount) || shiftAmount != 5u) {
            return false;
        }
        IrValue* materialRead = shift->Argument(0)->Resolve();
        std::uint32_t materialMemoryIndex = 0;
        const MemoryInfo* materialMemory = ScalarReadMemory(*materialRead, materialMemoryIndex);
        if (materialMemory == nullptr || materialMemory->offset != 0u || !MemoryIndexBelongsTo(materialMemoryIndex, *materialRead)) {
            return false;
        }
        IrValue* materialHandle = materialRead->Argument(0)->Resolve();

        IrValue* selector = nullptr;
        std::uint32_t selectorStride = 0;
        std::uint32_t selectorOffset = 0;
        if (!MatchMaterialOffset(materialRead->Argument(1), selector, selectorStride, selectorOffset)) {
            return false;
        }

        const std::array<const IrValue*, 1> materialUsers {shift};
        std::array<const IrValue*, 8> heapUsers {};
        std::copy(heapReads.begin(), heapReads.end(), heapUsers.begin());
        const std::array<const IrValue*, 1> imageUsers {&handle};
        if (!usesOnly(*materialRead, materialUsers) || !usesOnly(*shift, heapUsers)) {
            return false;
        }
        for (const auto* read : heapReads) {
            if (!usesOnly(*read, imageUsers)) {
                return false;
            }
        }

        DescriptorSource materialSource;
        DescriptorSource heapSource;
        std::uint32_t materialSourceIndex = 0;
        std::uint32_t heapSourceIndex = 0;
        if (!MakeRuntimeBufferSource(*materialHandle, materialSourceIndex, materialSource) || !MakeRuntimeBufferSource(*heapHandle, heapSourceIndex, heapSource)) {
            return false;
        }

        DescriptorSource imageSource;
        imageSource.dwordCount = 8u;
        std::copy(materialSource.dwords.begin(), materialSource.dwords.begin() + 4u, imageSource.dwords.begin());
        std::copy(heapSource.dwords.begin(), heapSource.dwords.begin() + 4u, imageSource.dwords.begin() + 4u);
        imageSource.indirectImage = DescriptorSource::IndirectImage {materialSourceIndex, heapSourceIndex, selectorStride, selectorOffset, 0u};

        plan.handle = &handle;
        plan.source = InternSource(imageSource);
        plan.key = materialRead;
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

    void PlanIndirectImages() {
        for (auto& block : m_program.Blocks()) {
            for (IrValue* inst : block->Instructions()) {
                if (ImageOpcodeInfoOf(inst->Opcode()).access == ImageAccess::None || inst->ArgumentCount() == 0u) {
                    continue;
                }
                IrValue* handle = inst->Argument(0)->Resolve();
                if (FindIndirectImage(*handle) != nullptr) {
                    continue;
                }
                IndirectImagePlan plan;
                if (TryMakeIndirectImage(*handle, plan)) {
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
        if (m_info.buffers.size() >= ShaderInfo::MaxBuffers) {
            return std::numeric_limits<std::uint32_t>::max();
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
            if (image.source == source && image.resourceClass == resourceClass && image.dimension == memory.imageDimension && image.mipMode == mip && image.depthCompare == depth && image.r128 == memory.imageR128) {
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
        Merge(image, op, pc);
        m_info.images.push_back(image);
        return static_cast<std::uint32_t>(m_info.images.size() - 1);
    }

    static void Merge(ImageResource& image, IrOpcode op, std::uint32_t pc) {
        const auto access = ImageOpcodeInfoOf(op).access;
        const bool atomic = access == ImageAccess::Atomic;
        const bool write = access == ImageAccess::Write || atomic;
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
            GetHandle(inst.Argument(0), IrOpcode::GetBufferResource, 4, handle, source);
            resource = AddBuffer(source, memory, op, flags.pc);
            if (resource == std::numeric_limits<std::uint32_t>::max()) {
                fail("buffer resource limit exceeded");
            }
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
};

}

void ResourceTracker::Track(IrProgram& program) const {
    Tracker(program).Run();
}

}
