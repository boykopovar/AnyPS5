#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRVALUE_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRVALUE_HPP

#include "IntermediateRepresentation/IrOpcode.hpp"
#include "IntermediateRepresentation/IrType.hpp"
#include "IntermediateRepresentation/GuestRegister.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <memory_resource>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ShaderRecompiler {

class IrBlock;
class IrValue;
struct IrUse;

template<typename T>
class IrUseAllocator {
public:
    using value_type = T;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    IrUseAllocator() noexcept = default;
    explicit IrUseAllocator(std::pmr::memory_resource* arena) noexcept : arena(arena) {}
    template<typename U>
    IrUseAllocator(const IrUseAllocator<U>& other) noexcept : arena(other.Arena()) {}
    T* allocate(std::size_t count) {
        if (arena == nullptr) return std::allocator<T>{}.allocate(count);
        return static_cast<T*>(arena->allocate(count * sizeof(T), alignof(T)));
    }
    void deallocate(T* pointer, std::size_t count) noexcept {
        if (arena == nullptr) std::allocator<T>{}.deallocate(pointer, count);
    }
    [[nodiscard]] std::pmr::memory_resource* Arena() const noexcept { return arena; }
    IrUseAllocator select_on_container_copy_construction() const noexcept { return {}; }
    friend bool operator==(const IrUseAllocator& left, const IrUseAllocator& right) noexcept { return left.arena == right.arena; }

private:
    std::pmr::memory_resource* arena = nullptr;
};

using IrUseList = std::vector<IrUse, IrUseAllocator<IrUse>>;

class IrUseArena {
public:
    IrUseArena() = default;
    IrUseArena(IrUseArena&& other) noexcept = default;
    IrUseArena& operator=(IrUseArena&& other) noexcept {
        resource.swap(other.resource);
        return *this;
    }
    std::pmr::memory_resource* Resource() {
        if (resource == nullptr) resource = std::make_unique<std::pmr::monotonic_buffer_resource>();
        return resource.get();
    }

private:
    std::unique_ptr<std::pmr::monotonic_buffer_resource> resource;
};

struct IrUse {
    IrValue* user = nullptr;
    std::size_t operand = 0;

    bool operator==(const IrUse& other) const;
};

class IrOperands {
public:
    IrOperands() = default;
    IrOperands(const IrOperands&) = delete;
    IrOperands& operator=(const IrOperands&) = delete;
    ~IrOperands() {
        if (allocated > InlineCapacity) delete[] heap;
    }

    IrOperands& operator=(std::vector<IrValue*>&& values) {
        if (allocated > InlineCapacity) delete[] heap;
        allocated = InlineCapacity;
        count = 0;
        reserve(values.size());
        for (IrValue* value : values) data()[count++] = value;
        values.clear();
        return *this;
    }

    void reserve(std::size_t size) {
        if (size > allocated) grow(size);
    }
    void push_back(IrValue* value) {
        if (count == allocated) grow(static_cast<std::size_t>(allocated) * 2u);
        data()[count++] = value;
    }
    void clear() {
        count = 0;
    }

    static constexpr std::size_t InlineCapacity = 3;

    [[nodiscard]] std::size_t size() const { return count; }
    [[nodiscard]] std::size_t capacity() const { return allocated; }
    [[nodiscard]] bool empty() const { return count == 0; }
    [[nodiscard]] IrValue* const* begin() const { return data(); }
    [[nodiscard]] IrValue* const* end() const { return data() + count; }
    [[nodiscard]] IrValue* const& operator[](std::size_t index) const { return data()[index]; }
    [[nodiscard]] IrValue*& operator[](std::size_t index) { return data()[index]; }
    [[nodiscard]] IrValue* at(std::size_t index) const {
        if (index >= count) throw std::out_of_range("IrOperands::at index is out of range");
        return data()[index];
    }
    [[nodiscard]] IrValue* front() const { return data()[0]; }
    [[nodiscard]] IrValue* back() const { return data()[count - 1]; }

private:
    [[nodiscard]] IrValue* const* data() const { return allocated > InlineCapacity ? heap : local; }
    [[nodiscard]] IrValue** data() { return allocated > InlineCapacity ? heap : local; }
    void grow(std::size_t size) {
        auto* grown = new IrValue*[size];
        for (std::uint32_t index = 0; index < count; ++index) grown[index] = data()[index];
        if (allocated > InlineCapacity) delete[] heap;
        heap = grown;
        allocated = static_cast<std::uint32_t>(size);
    }

    std::uint32_t count = 0;
    std::uint32_t allocated = InlineCapacity;
    union {
        IrValue* local[InlineCapacity];
        IrValue** heap;
    };
};

class IrValue {
public:
    IrValue(IrOpcode opcode, IrType type, std::uint32_t id, std::pmr::memory_resource* useArena = nullptr);
    IrValue(const IrValue& other) = delete;
    IrValue& operator=(const IrValue& other) = delete;
    IrValue(IrValue&& other) = delete;
    IrValue& operator=(IrValue&& other) = delete;

    [[nodiscard]] IrOpcode Opcode() const;
    [[nodiscard]] IrType Type() const;
    [[nodiscard]] std::uint32_t Id() const;
    [[nodiscard]] const IrOperands& Arguments() const;
    [[nodiscard]] auto Uses() const;
    [[nodiscard]] IrBlock* Parent() const;
    [[nodiscard]] bool HasImmediate() const;
    [[nodiscard]] std::uint32_t ImmediateU32() const;

    void AddArgument(IrValue* argument);
    void ReplaceArgument(std::size_t index, IrValue* argument);
    void ReplaceAllUsesWith(IrValue* replacement);
    void SetImmediateU32(std::uint32_t value);
    void SetParent(IrBlock* parent);

    template<typename TValue> requires(sizeof(TValue) <= sizeof(std::uint64_t) && std::is_trivially_copyable_v<TValue>)
    [[nodiscard]] TValue Flags() const {
        TValue result{};
        std::memcpy(&result, &flags, sizeof(result));
        return result;
    }

    template<typename TValue> requires(sizeof(TValue) <= sizeof(std::uint64_t) && std::is_trivially_copyable_v<TValue>)
    void SetFlags(TValue value) {
        flags = 0;
        std::memcpy(&flags, &value, sizeof(value));
    }

    [[nodiscard]] bool IsEmpty() const;
    [[nodiscard]] bool IsIdentity() const;
    [[nodiscard]] bool IsPhi() const;
    [[nodiscard]] IrValue* Resolve() const;
    [[nodiscard]] bool MayHaveSideEffects() const;
    [[nodiscard]] bool HasUses() const;
    [[nodiscard]] std::size_t UseCount() const;
    [[nodiscard]] std::size_t ArgumentCount() const;
    [[nodiscard]] std::size_t PhiBlockCount() const;
    [[nodiscard]] IrValue* Argument(std::size_t index) const;
    [[nodiscard]] IrBlock* PhiBlock(std::size_t index) const;
    [[nodiscard]] const IrUseList& OperandUses() const;
    void MoveUsesToHeap();
    [[nodiscard]] std::uint64_t ImmediateU64() const;
    [[nodiscard]] std::uint16_t ImmediateF16Bits() const;
    [[nodiscard]] float ImmediateF32() const;
    [[nodiscard]] bool ImmediateBool() const;
    [[nodiscard]] std::uint8_t ImmediateU8() const;
    [[nodiscard]] std::uint16_t ImmediateU16() const;
    [[nodiscard]] GuestRegister Register() const;
    void SetImmediateU64(std::uint64_t value);
    void SetImmediateF16Bits(std::uint16_t bits);
    void SetImmediateF32(float value);
    void SetImmediateBool(bool value);
    void SetImmediateU8(std::uint8_t value);
    void SetImmediateU16(std::uint16_t value);
    void SetRegister(const GuestRegister& reg);
    void AddPhiOperand(IrBlock* predecessor, IrValue* value);
    void SetDetachedOperands(std::vector<IrValue*> arguments, std::vector<IrBlock*> phiBlocks);
    void ReplaceOpcode(IrOpcode opcode);
    void Invalidate();
    void ReplaceUsesWith(IrValue* replacement, bool preserve);
    [[nodiscard]] bool operator==(const IrValue& other) const;

private:
    IrOpcode opcode;
    IrType type;
    std::uint32_t id;
    std::uint64_t immediateBits = 0;
    GuestRegister registerValue{};
    std::uint64_t flags = 0;
    std::vector<IrBlock*> phiBlocks;
    IrUseList operandUses;
    bool hasImmediate;
    IrOperands arguments;
    IrBlock* parent;
};

inline auto IrValue::Uses() const {
    return std::views::transform(operandUses, [](const IrUse& use) { return use.user; });
}

inline IrOpcode IrValue::Opcode() const {
    return opcode;
}

inline IrType IrValue::Type() const {
    return type;
}

inline bool IrValue::HasImmediate() const {
    return hasImmediate;
}

inline bool IrValue::IsIdentity() const {
    return opcode == IrOpcode::Identity;
}

inline std::size_t IrValue::ArgumentCount() const {
    return arguments.size();
}

inline IrValue* IrValue::Argument(std::size_t index) const {
    if (index >= arguments.size()) {
        throw std::out_of_range("IrValue::Argument index is out of range");
    }
    return arguments[index];
}

inline IrValue* IrValue::Resolve() const {
    const IrValue* current = this;
    while (current->IsIdentity()) {
        if (current->arguments.empty() || current->arguments.front() == nullptr) {
            throw std::runtime_error("IrValue::Resolve encountered a malformed Identity value");
        }
        current = current->arguments.front();
    }
    return const_cast<IrValue*>(current);
}

template<IrType TValueType>
class IrTypedValue {
public:
    IrTypedValue() = default;

    explicit IrTypedValue(IrValue& value) : value(&value) {
        if (!AreTypesCompatible(value.Type(), TValueType)) {
            throw std::invalid_argument("IrTypedValue constructed from an incompatible IrValue type");
        }
    }

    template<IrType TOtherType> requires((static_cast<std::uint32_t>(TValueType) & static_cast<std::uint32_t>(TOtherType)) != 0u)
    IrTypedValue(const IrTypedValue<TOtherType>& other) : value(other.value) {
    }

    [[nodiscard]] IrValue& Value() const {
        if (value == nullptr) {
            throw std::runtime_error("IrTypedValue::Value called on an empty typed value");
        }
        return *value;
    }

private:
    template<IrType> friend class IrTypedValue;

    IrValue* value = nullptr;
};

using IrU1 = IrTypedValue<IrType::Bool>;
using IrU8 = IrTypedValue<IrType::U8>;
using IrU16 = IrTypedValue<IrType::U16>;
using IrU32 = IrTypedValue<IrType::U32>;
using IrU64 = IrTypedValue<IrType::U64>;
using IrF16 = IrTypedValue<IrType::F16>;
using IrF32 = IrTypedValue<IrType::F32>;
using IrU32F32 = IrTypedValue<static_cast<IrType>(static_cast<std::uint32_t>(IrType::U32) | static_cast<std::uint32_t>(IrType::F32))>;

}

#endif
