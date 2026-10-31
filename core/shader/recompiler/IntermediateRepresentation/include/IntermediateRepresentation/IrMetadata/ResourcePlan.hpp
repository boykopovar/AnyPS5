#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_RESOURCEPLAN_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_RESOURCEPLAN_HPP

#include "IntermediateRepresentation/IrBlock.hpp"
#include "IntermediateRepresentation/IrValue.hpp"
#include "IntermediateRepresentation/IrMetadata/ControlFlowInfo.hpp"
#include "IntermediateRepresentation/IrMetadata/DescriptorBinding.hpp"
#include "IntermediateRepresentation/IrMetadata/ResourceInfo.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderInfo.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <algorithm>
#include <atomic>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

struct DescriptorValue {
    std::array<std::uint32_t, 8> dwords = {};
    std::uint32_t dwordCount = 0;
    std::uint8_t availableDwordMask = 0;

    bool operator==(const DescriptorValue& other) const = default;
};

template <typename T>
class SharedValues {
public:
    SharedValues() = default;
    explicit SharedValues(std::vector<T> values) : block(values.empty() ? nullptr : std::make_shared<const Block>(std::move(values))) {}

    [[nodiscard]] std::size_t size() const { return block != nullptr ? block->values.size() : 0u; }
    [[nodiscard]] bool empty() const { return block == nullptr; }
    [[nodiscard]] const T* data() const { return block != nullptr ? block->values.data() : nullptr; }
    [[nodiscard]] const T* begin() const { return data(); }
    [[nodiscard]] const T* end() const { return data() + size(); }
    [[nodiscard]] const T& operator[](std::size_t index) const { return block->values[index]; }
    [[nodiscard]] const T& at(std::size_t index) const {
        if (index >= size()) throw std::out_of_range("SharedValues::at");
        return block->values[index];
    }
    [[nodiscard]] const T& front() const { return at(0); }
    [[nodiscard]] const T& back() const { return at(size() - 1u); }
    [[nodiscard]] std::span<const T> Span() const { return {data(), size()}; }
    [[nodiscard]] std::vector<T> Copy() const { return {begin(), end()}; }
    [[nodiscard]] bool Shares(const SharedValues& other) const { return block == other.block; }

    [[nodiscard]] std::uint64_t Hash() const {
        constexpr std::uint64_t Basis = 0xcbf29ce484222325ull;
        if (block == nullptr) return Basis;
        if (const auto known = block->hash.load(std::memory_order_relaxed); known != 0u) return known;
        std::uint64_t hash = Basis;
        const auto mix = [&](std::uint64_t value) { hash = (hash ^ value) * 0x100000001b3ull; };
        mix(block->values.size());
        for (const auto& value : block->values) {
            if constexpr (std::is_same_v<T, DescriptorValue>) {
                mix(value.dwordCount);
                mix(value.availableDwordMask);
                for (const auto dword : value.dwords) mix(dword);
            } else if constexpr (std::is_integral_v<T>) {
                mix(value);
            } else {
                mix(value.first);
                mix(value.second);
            }
        }
        hash = hash != 0u ? hash : 1u;
        block->hash.store(hash, std::memory_order_relaxed);
        return hash;
    }

    friend bool operator==(const SharedValues& left, const SharedValues& right) { return left.block == right.block || std::ranges::equal(left.Span(), right.Span()); }
    friend bool operator==(const SharedValues& left, const std::vector<T>& right) { return std::ranges::equal(left.Span(), right); }

private:
    struct Block {
        explicit Block(std::vector<T> values) : values(std::move(values)) {}
        std::vector<T> values;
        mutable std::atomic<std::uint64_t> hash{0};
    };
    std::shared_ptr<const Block> block;
};

enum class UniformFillKind { None, Buffer, Image };

struct UniformFill {
    UniformFillKind kind = UniformFillKind::None;
    std::uint32_t resource = 0;
    std::array<std::uint32_t, 3> groupStride {};
    std::uint32_t words = 0;
    std::uint32_t value = 0;

    bool operator==(const UniformFill& other) const = default;
};

struct ImageTableColumnSnapshot {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::uint32_t records = 0;
    std::uint32_t keys = 0;
    std::uint32_t fault = 0;
    bool outside = false;
    bool nested = false;
    SharedValues<std::uint64_t> pointers;
    SharedValues<std::uint32_t> codes;
    SharedValues<DescriptorValue> words;
    SharedValues<std::pair<std::uint64_t, std::uint64_t>> readRanges;

    bool operator==(const ImageTableColumnSnapshot& other) const = default;
};

struct ImageTableSnapshot {
    std::vector<ImageTableColumnSnapshot> tables;
    std::vector<ImageTableColumnSnapshot> nested;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::uint64_t shader = 0;

    bool operator==(const ImageTableSnapshot& other) const = default;
};

struct ResourceSnapshot {
    std::vector<DescriptorValue> buffers;
    std::vector<DescriptorValue> images;
    std::vector<DescriptorValue> samplers;
    std::vector<std::uint32_t> flattenedSrt;
    std::vector<std::uint32_t> userData;
    UniformFill uniformFill;
    std::vector<SrtReadPoison> srtPoison;
    std::uint32_t nullRootReads = 0;
    ImageTableSnapshot tables;
};

struct UniformFillPlan {
    UniformFill fill;
    std::array<IrValue*, 4> values{};
};

inline constexpr std::uint32_t NativePushSlotSize = sizeof(PushData);
inline constexpr std::uint32_t NativePushConstantSize = 2u * NativePushSlotSize;

class IrValueArena {
public:
    IrValueArena() = default;
    IrValueArena(const IrValueArena&) = delete;
    IrValueArena& operator=(const IrValueArena&) = delete;
    IrValueArena(IrValueArena&& other) noexcept : values(std::exchange(other.values, nullptr)), count(std::exchange(other.count, 0)), capacity(std::exchange(other.capacity, 0)) {}
    IrValueArena& operator=(IrValueArena&& other) noexcept {
        if (this != &other) {
            release();
            values = std::exchange(other.values, nullptr);
            count = std::exchange(other.count, 0);
            capacity = std::exchange(other.capacity, 0);
        }
        return *this;
    }
    ~IrValueArena() { release(); }

    void Reserve(std::size_t size) {
        if (values != nullptr) throw std::runtime_error("IrValueArena::Reserve called twice");
        if (size == 0) return;
        values = static_cast<IrValue*>(::operator new(size * sizeof(IrValue), std::align_val_t{alignof(IrValue)}));
        capacity = size;
    }
    IrValue* Create(IrOpcode opcode, IrType type, std::uint32_t id) {
        if (count == capacity) throw std::runtime_error("IrValueArena::Create exceeds the reserved size");
        auto* value = new (values + count) IrValue(opcode, type, id);
        ++count;
        return value;
    }
    [[nodiscard]] std::size_t Size() const { return count; }
    [[nodiscard]] IrValue& operator[](std::size_t index) { return values[index]; }
    [[nodiscard]] const IrValue& operator[](std::size_t index) const { return values[index]; }
    [[nodiscard]] IrValue* begin() { return values; }
    [[nodiscard]] IrValue* end() { return values + count; }
    [[nodiscard]] const IrValue* begin() const { return values; }
    [[nodiscard]] const IrValue* end() const { return values + count; }

private:
    void release() {
        if (values == nullptr) return;
        for (std::size_t index = count; index > 0; --index) values[index - 1].~IrValue();
        ::operator delete(values, std::align_val_t{alignof(IrValue)});
        values = nullptr;
        count = 0;
        capacity = 0;
    }
    IrValue* values = nullptr;
    std::size_t count = 0;
    std::size_t capacity = 0;
};

struct CompactPlanValue {
    std::uint64_t immediate = 0;
    std::uint64_t flags = 0;
    std::uint32_t firstArgument = 0;
    std::uint32_t registerIndex = 0;
    IrType type = IrType::Void;
    IrOpcode opcode = IrOpcode::Void;
    std::uint8_t argumentCount = 0;
    bool hasImmediate = false;
};

struct CompactResourcePlan {
    static constexpr std::uint32_t NoValue = 0xffffffffu;
    std::vector<CompactPlanValue> values;
    std::vector<std::uint32_t> arguments;
    std::vector<std::uint32_t> sourceDwords;
    std::vector<std::uint32_t> srtReads;
    std::vector<std::uint32_t> conditions;
};

struct SrtTapeSlot {
    std::once_flag once;
    std::atomic<bool> ready{false};
    std::shared_ptr<const void> tape;
};

struct IrResourcePlan {
    IrShaderStage stage = IrShaderStage::Unknown;
    std::uint64_t shaderHash = 0;
    std::uint32_t userDataBase = 0;
    std::uint32_t userDataCount = 64;
    std::uint32_t srgbDecodeFormats = 0;
    IrValueArena valueStorage;
    std::vector<std::unique_ptr<IrBlock>> blockStorage;
    std::vector<MemoryInfo> memoryInfo;
    std::vector<DescriptorSource> descriptorSources;
    std::vector<ResourceBlock> controlFlow;
    std::vector<std::uint32_t> materializationSources;
    std::vector<SrtRead> srtReads;
    std::vector<std::uint32_t> guardedSrtSlots;
    std::uint32_t srtGuardOffset = 0;
    std::vector<std::uint8_t> cleanFlatSlots;
    // One byte per srtReads slot, 1 when the CPU walk never consumes the slot's value (see
    // Detail::ComputePureFlatSlots): a driver may reuse a capture whose words differ only there.
    std::vector<std::uint8_t> pureFlatSlots;
    bool denseValueIds = false;
    CompactResourcePlan compact;
    bool requiresSpecializationMemory = false;
    bool srtPlanComplete = false;
    bool resourceTrackingComplete = false;
    ShaderInfo info;
    UniformFillPlan uniformFill;
    std::shared_ptr<SrtTapeSlot> tapeSlot = std::make_shared<SrtTapeSlot>();
};

}

#endif
