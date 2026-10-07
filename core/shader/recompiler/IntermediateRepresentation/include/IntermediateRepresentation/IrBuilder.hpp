#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRBUILDER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "IntermediateRepresentation/GuestRegister.hpp"
#include <cstring>
#include <initializer_list>

namespace ShaderRecompiler {

class IrBuilder {
public:
    explicit IrBuilder(IrProgram& program);

    void SetInsertionPoint(IrBlock& block);
    [[nodiscard]] IrValue& Constant(std::uint32_t value);
    [[nodiscard]] IrValue& ReadRegister(const GuestRegister& reg);
    void WriteRegister(const GuestRegister& reg, IrValue& value);
    [[nodiscard]] IrValue& Emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments);
    void Branch(IrBlock& target);
    void BranchConditional(IrValue& condition, IrBlock& trueTarget, IrBlock& falseTarget);

    [[nodiscard]] IrValue& ConstantU64(std::uint64_t value);
    [[nodiscard]] IrValue& ConstantF16(std::uint16_t bits);
    [[nodiscard]] IrValue& ConstantF32(float value);
    [[nodiscard]] IrValue& ConstantBool(bool value);
    [[nodiscard]] IrValue& ConstantU8(std::uint8_t value);
    [[nodiscard]] IrValue& ConstantU16(std::uint16_t value);
    [[nodiscard]] IrValue& Emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments,
                                std::uint64_t flags);

    IrValue& GetUserData(ScalarReg reg);
    IrValue& GetScalarReg(ScalarReg reg);
    void SetScalarReg(ScalarReg reg, IrValue& value);
    IrValue& GetThreadBitScalarReg(ScalarReg reg);
    void SetThreadBitScalarReg(ScalarReg reg, IrValue& value);
    IrValue& GetScalarMaskTag(ScalarReg reg);
    void SetScalarMaskTag(ScalarReg reg, IrValue& value);
    IrValue& GetVectorReg(VectorReg reg);
    void SetVectorReg(VectorReg reg, IrValue& value);
    IrValue& GetGotoVariable(std::uint32_t id);
    void SetGotoVariable(std::uint32_t id, IrValue& value);
    IrValue& GetScc();
    void SetScc(IrValue& value);
    IrValue& GetExec();
    void SetExec(IrValue& value);
    IrValue& GetExecLo();
    void SetExecLo(IrValue& value);
    IrValue& GetExecHi();
    void SetExecHi(IrValue& value);
    IrValue& GetVcc();
    void SetVcc(IrValue& value);
    IrValue& GetVccLo();
    void SetVccLo(IrValue& value);
    IrValue& GetVccHi();
    void SetVccHi(IrValue& value);
    IrValue& GetM0();
    void SetM0(IrValue& value);
    IrValue& BitCastF32(IrValue& value);
    IrValue& BitCastU32(IrValue& value);
    IrValue& BitCastF16(IrValue& value);
    IrValue& BitCastU32FromF16(IrValue& value);
    IrValue& ConstructU64(IrValue& low, IrValue& high);
    IrValue& CompositeExtract(IrValue& composite, std::uint32_t index);
    IrValue& IAdd(IrValue& lhs, IrValue& rhs);
    IrValue& ISub(IrValue& lhs, IrValue& rhs);
    IrValue& IMul(IrValue& lhs, IrValue& rhs);
    IrValue& ShiftLeftLogical(IrValue& value, IrValue& shift);
    IrValue& ShiftRightLogical(IrValue& value, IrValue& shift);
    IrValue& ShiftRightArithmetic(IrValue& value, IrValue& shift);
    IrValue& BitwiseAnd(IrValue& lhs, IrValue& rhs);
    IrValue& BitwiseOr(IrValue& lhs, IrValue& rhs);
    IrValue& BitwiseXor(IrValue& lhs, IrValue& rhs);
    IrValue& BitwiseNot(IrValue& value);
    IrValue& Select(IrValue& condition, IrValue& trueValue, IrValue& falseValue);
    IrValue& IEqual(IrValue& lhs, IrValue& rhs);
    IrValue& INotEqual(IrValue& lhs, IrValue& rhs);
    IrValue& ULessThan(IrValue& lhs, IrValue& rhs);
    IrValue& UGreaterThan(IrValue& lhs, IrValue& rhs);
    IrValue& LogicalAnd(IrValue& lhs, IrValue& rhs);
    IrValue& LogicalOr(IrValue& lhs, IrValue& rhs);
    IrValue& LogicalNot(IrValue& value);

    template <typename TFlags>
        requires(sizeof(TFlags) <= sizeof(std::uint64_t) && std::is_trivially_copyable_v<TFlags>)
    [[nodiscard]] IrValue& Emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments, TFlags flags) {
        std::uint64_t rawFlags = 0;
        std::memcpy(&rawFlags, &flags, sizeof(flags));
        return Emit(opcode, type, arguments, rawFlags);
    }

private:
    IrProgram& program;
    IrBlock* insertionPoint;
};

}

#endif
