#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRVALUE_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRVALUE_HPP

#include "IntermediateRepresentation/IrOpcode.hpp"
#include "IntermediateRepresentation/IrType.hpp"
#include "IntermediateRepresentation/GuestRegister.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ShaderRecompiler {

class IrBlock;
class IrValue;

struct IrUse {
    IrValue* user = nullptr;
    std::size_t operand = 0;

    bool operator==(const IrUse& other) const;
};

class IrValue {
public:
    IrValue(IrOpcode opcode, IrType type, std::uint32_t id);
    IrValue(const IrValue& other) = delete;
    IrValue& operator=(const IrValue& other) = delete;
    IrValue(IrValue&& other) = delete;
    IrValue& operator=(IrValue&& other) = delete;

    [[nodiscard]] IrOpcode Opcode() const;
    [[nodiscard]] IrType Type() const;
    [[nodiscard]] std::uint32_t Id() const;
    [[nodiscard]] const std::vector<IrValue*>& Arguments() const;
    [[nodiscard]] const std::vector<IrValue*>& Uses() const;
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
    [[nodiscard]] const std::vector<IrUse>& OperandUses() const;
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
    std::vector<IrUse> operandUses;
    bool hasImmediate;
    std::vector<IrValue*> arguments;
    std::vector<IrValue*> uses;
    IrBlock* parent;
};

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
