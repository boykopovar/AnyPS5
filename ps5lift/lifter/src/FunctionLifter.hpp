#ifndef PS5LIFT_FUNCTIONLIFTER_HPP
#define PS5LIFT_FUNCTIONLIFTER_HPP

#include "ModuleLifter.hpp"

#include <lifter/Discovery.hpp>

#include <ps5lift/State.h>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Ps5Lift::Lifting {

enum class StringRead { Compare, Scan, Load };

struct UnsupportedInstruction : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] void UnexpectedMnemonic(const char* function, ZydisMnemonic mnemonic);
int GprIndex(ZydisRegister reg);
int VectorIndex(ZydisRegister reg);
int MmxIndex(ZydisRegister reg);
bool IsHighByte(ZydisRegister reg);
bool AssignsStackPointer(const Instruction& instruction);
bool IsRegister(const ZydisDecodedOperand& operand, ZydisRegister reg);
bool WritesGpr(const Instruction& instruction, int gpr);

enum class Condition {
    O, NO, B, AE, E, NE, BE, A, S, NS, P, NP, L, GE, LE, G
};

class FunctionLifter {
public:
    FunctionLifter(ModuleLifter& module, const DecodedFunction& function)
        : _module(module), _function(function), _host(module.Guest(function.Entry)), _builder(module.Context()) {}

    void Lift();

private:
    ModuleLifter& _module;
    const DecodedFunction& _function;
    llvm::Function* _host;
    llvm::IRBuilder<> _builder;
    llvm::Value* _state = nullptr;
    llvm::AllocaInst* _gpr[PS5LIFT_GPR_COUNT]{};
    llvm::AllocaInst* _flags[PS5LIFT_FLAG_COUNT]{};
    static constexpr int VectorCount = 16;
    llvm::AllocaInst* _vectors[VectorCount]{};
    std::array<bool, PS5LIFT_GPR_COUNT> _usedGprs{};
    std::array<bool, VectorCount> _usedVectors{};
    bool _usesFlags = false;
    bool _assignsStackPointer = false;
    bool _framePointer = false;
    bool _frameEscapes = false;
    std::map<std::uint64_t, llvm::BasicBlock*> _blocks;
    const Instruction* _instruction = nullptr;
    llvm::Value* _entryRsp = nullptr;
    llvm::BasicBlock* _unwind = nullptr;
    llvm::LLVMContext& Context() { return _module.Context(); }
    llvm::IntegerType* I64() { return _builder.getInt64Ty(); }
    llvm::IntegerType* Int(const unsigned bits) { return llvm::IntegerType::get(Context(), bits); }
    llvm::ConstantInt* Const(const unsigned bits, const std::uint64_t value);
    llvm::Value* StateField(const std::size_t offset);
    static llvm::Align StateAlignment(const std::size_t offset);
    llvm::Value* StateLoad(llvm::Type* type, const std::size_t offset);
    void StateStore(llvm::Value* value, const std::size_t offset);
    static std::size_t VectorOffset(const int index);
    void ScanInstructions();
    bool KeepsFramePointer() const;
    bool IsFrameRegister(const ZydisRegister reg) const;
    bool HandsOutFrameAddress(const Instruction& instruction) const;
    bool CrossesCalls(const int flag) const;
    void Flush(const RegisterSet registers = EveryRegister);
    void Reload(const RegisterSet registers = EveryRegister);
    bool Terminated() const;
    void StartDeadBlock();
    llvm::Value* Gpr(const int index);
    void SetGpr(const int index, llvm::Value* value);
    llvm::Value* Flag(const int flag);
    void SetFlag(const int flag, llvm::Value* value);
    llvm::Value* Vector(const int index);
    void SetVector(const int index, llvm::Value* value);
    void WriteVector(const int index, llvm::Value* value, const bool zeroUpper);
    std::size_t MmxOffset(const int index) const;
    bool UsesMmx() const;
    void EnterMmx();
    llvm::Value* ReadRegister(const ZydisRegister reg);
    void WriteRegister(const ZydisRegister reg, llvm::Value* value);
    llvm::Value* Address(const ZydisDecodedOperand& operand, const bool applySegment = true);
    bool NeedsOrdering(const ZydisDecodedOperand& operand, const unsigned bits) const;
    bool InPrivateFrame(const ZydisDecodedOperand& operand) const;
    bool InReadOnlySegment(const ZydisDecodedOperand& operand, const unsigned bits) const;
    llvm::Value* Load(llvm::Value* address, const unsigned bits, const bool ordered);
    void Store(llvm::Value* address, llvm::Value* value, const bool ordered);
    llvm::LoadInst* GuestLoad(llvm::Value* address, const unsigned bits);
    llvm::StoreInst* GuestStore(llvm::Value* address, llvm::Value* value);

    template <typename Access>
    Access* Ordered(Access* access, const llvm::AtomicOrdering ordering) {
        access->setAtomic(ordering);
        access->setAlignment(llvm::Align(access->getAccessType()->getIntegerBitWidth() / 8));
        return access;
    }

    struct AlignmentSplit {
        llvm::BasicBlock* Aligned;
        llvm::BasicBlock* Unaligned;
        llvm::BasicBlock* Done;
    };

    AlignmentSplit SplitOnAlignment(llvm::Value* address, const unsigned bits);
    const ZydisDecodedOperand& Operand(const int index) const { return _instruction->Operands[index]; }
    unsigned Bits(const int index) const { return Operand(index).size; }
    llvm::Value* Read(const int index);
    llvm::Value* ReadVector(const int index, const unsigned bits);
    llvm::Value* ReadAs(const int index, const unsigned bits);
    void Write(const int index, llvm::Value* value);
    llvm::Value* SignBit(llvm::Value* value);
    llvm::Value* Parity(llvm::Value* value);
    void ResultFlags(llvm::Value* result);
    void AdjustFlag(llvm::Value* a, llvm::Value* b, llvm::Value* result);
    void LogicFlags(llvm::Value* result);
    llvm::Value* Add(llvm::Value* a, llvm::Value* b, llvm::Value* carry, const bool setCarry = true);
    llvm::Value* Sub(llvm::Value* a, llvm::Value* b, llvm::Value* borrow, const bool setCarry = true);
    llvm::Value* Test(const Condition condition);
    void Push(llvm::Value* value);
    llvm::Value* Pop(const unsigned bits = 64);
    void CallGuest(llvm::Value* callee, const RegisterEffects& effects, llvm::ArrayRef<llvm::Value*> extra = {});
    void ResumeIfStackSwitched(llvm::Value* rsp, llvm::Value* popped);
    llvm::BasicBlock* UnwindBlock();
    llvm::FunctionCallee DispatchCallee();
    void TailCall(const std::uint64_t target);
    llvm::BasicBlock* Target(const std::uint64_t target);
    void Unsupported(const std::string& what);
    void Trap();
    void StringOp(const Ps5LiftStringOperation operation, const unsigned width);
    void ReadString(const StringRead kind, const unsigned width);
    bool Locked() const;
    llvm::Value* MemoryPointer(const int index);
    void Shift(const ZydisMnemonic mnemonic);
    void ShiftDouble(const ZydisMnemonic mnemonic);
    void AddWithFlagCarry(const int flag);
    llvm::Value* LowAccumulator(const unsigned bits);
    void WriteAccumulatorPair(const unsigned bits, llvm::Value* low, llvm::Value* high);
    llvm::Value* AccumulatorPair(const unsigned bits);
    void Multiply(const bool isSigned);
    void Divide(const bool isSigned);
    void SignedMultiply();
    void BitScan(const ZydisMnemonic mnemonic);
    void BitTest(const ZydisMnemonic mnemonic);
    llvm::Value* BitUpdate(const ZydisMnemonic mnemonic, llvm::Value* value, llvm::Value* mask);
    llvm::Value* BitStringAddress(llvm::Value* offset);
    static bool Lockable(const ZydisMnemonic mnemonic);
    llvm::Value* AtomicUpdate(const llvm::AtomicRMWInst::BinOp operation, llvm::Value* operand);
    llvm::Value* AtomicUpdate(llvm::Value* address, const llvm::AtomicRMWInst::BinOp operation, llvm::Value* operand);

    template <typename Update>
    llvm::Value* AtomicLoop(const unsigned bits, Update update) {
        auto* pointer = MemoryPointer(0);
        auto* initial = _builder.CreateAlignedLoad(Int(bits), pointer, llvm::Align(bits / 8));
        initial->setAtomic(llvm::AtomicOrdering::Monotonic);
        initial->setMetadata(llvm::LLVMContext::MD_tbaa, _module.GuestTag());
        auto* entry = _builder.GetInsertBlock();
        auto* loop = llvm::BasicBlock::Create(Context(), "atomic", _host);
        auto* done = llvm::BasicBlock::Create(Context(), "atomic_done", _host);
        _builder.CreateBr(loop);
        _builder.SetInsertPoint(loop);
        auto* old = _builder.CreatePHI(Int(bits), 2);
        old->addIncoming(initial, entry);
        auto* exchange = _builder.CreateAtomicCmpXchg(pointer, old, update(old), llvm::MaybeAlign(bits / 8), llvm::AtomicOrdering::SequentiallyConsistent, llvm::AtomicOrdering::SequentiallyConsistent);
        old->addIncoming(_builder.CreateExtractValue(exchange, 0), loop);
        _builder.CreateCondBr(_builder.CreateExtractValue(exchange, 1), done, loop);
        _builder.SetInsertPoint(done);
        return old;
    }

    bool LockedMemory() const;
    void Exchange();
    void ExchangeAdd();
    void CompareExchange();
    void CompareExchangeWide();
    void Crc32();
    void SignExtendAccumulator(const ZydisMnemonic mnemonic);
    void VectorBitwise(const llvm::Instruction::BinaryOps operation);
    void VectorMove();
    void BitManipulation(const ZydisMnemonic mnemonic);
    llvm::Value* LowRdx(const unsigned bits);
    void Broadcast(const unsigned elementBits);

    enum class VectorKind {
        Add, Sub, AddSignedSaturate, AddUnsignedSaturate, SubSignedSaturate, SubUnsignedSaturate,
        MultiplyLow, MultiplyHighSigned, MultiplyHighUnsigned, MultiplyHighRoundScale, MultiplyEvenUnsigned, MultiplyEvenSigned, MultiplyAddWords, MultiplyAddBytes,
        HorizontalAdd, HorizontalAddSaturate, HorizontalSub, HorizontalSubSaturate, SumAbsoluteDifferences,
        MinSigned, MaxSigned, MinUnsigned, MaxUnsigned, CompareEqual, CompareGreater, Average, AndNot,
        UnpackLow, UnpackHigh, PackSignedSaturate, PackUnsignedSaturate, ShuffleBytes, PermuteDwords
    };

    llvm::Value* AsVector(llvm::Value* value, const unsigned element);
    llvm::Value* AsInteger(llvm::Value* vector);
    llvm::Value* Splat(llvm::Value* vector, llvm::Value* scalar);
    llvm::Value* ShuffleWithZero(llvm::Value* a, llvm::Value* b, const std::vector<int>& mask);
    llvm::Value* Elements(llvm::Value* vector, const unsigned start, const unsigned count);
    llvm::Value* Concatenate(llvm::Value* low, llvm::Value* high);
    llvm::Value* LookUpBytes(llvm::Value* table, llvm::Value* indices);
    std::uint64_t Immediate(const int index) const;
    int Base(const int legacyVisible) const;
    llvm::Value* ShiftByCount(const llvm::Instruction::BinaryOps shift, llvm::Value* vector, llvm::Value* counts, const unsigned element);
    llvm::Value* Pairs(llvm::Value* vector, const unsigned count, const unsigned offset);
    llvm::Value* Saturate(llvm::Value* wide, const unsigned element, const bool isSigned);
    ZydisRegister HiddenRegister(std::initializer_list<ZydisRegister> candidates) const;
    void StringCompare(const bool explicitLengths, const bool index);
    void Crypto(const ZydisMnemonic mnemonic);
    void Sha(const ZydisMnemonic mnemonic);
    void MoveMask(const unsigned element);
    void ExtractElement(const unsigned element);
    void Extract128();
    void TestFlags(llvm::Value* a, llvm::Value* b);
    void VectorTest();
    void VectorTestSigns(const unsigned element);
    void VectorAbsolute(const unsigned element);
    void VectorExtend(const llvm::Instruction::CastOps extension, const unsigned element, const unsigned sourceElement);
    void ShuffleInLanes(const unsigned element, const unsigned first);
    void PermuteQwords();
    void ShiftBytes(const llvm::Instruction::BinaryOps shift);
    void VectorShift(const llvm::Instruction::BinaryOps shift, const unsigned element);

    struct VectorOperands {
        llvm::Value* A;
        llvm::Value* B;
        llvm::Value* VA;
        llvm::Value* VB;
    };

    VectorOperands ReadVectorOperands(const int base, const unsigned width, const unsigned element);
    void VectorBinary(const VectorKind kind, const unsigned element);
    void VectorShiftVariable(const llvm::Instruction::BinaryOps shift, const unsigned element);
    void ShufflePairs();
    void AlignRight();
    void Permute128();
    void BlendImmediate(const unsigned element);
    void BlendVariable(const unsigned element);
    void InsertElement(const unsigned element);
    void Insert128();

    enum class FloatKind {
        Add, Sub, Mul, Div, Min, Max, Sqrt, Reciprocal, ReciprocalSqrt, Round, Compare, AddSub, HorizontalAdd, HorizontalSub, Fma
    };

    struct FloatSpec {
        FloatKind Kind;
        unsigned Element;
        bool Scalar;
        int Order = 0;
        bool NegateProduct = false;
        bool SubtractAddend = false;
        int Alternate = 0;
    };

    llvm::Type* FloatType(const unsigned bits);
    llvm::Value* AsFloats(llvm::Value* value, const unsigned element);
    llvm::Value* FloatBits(llvm::Value* value);
    llvm::Type* IntegerLike(llvm::Type* type);
    llvm::Value* Quiet(llvm::Value* value);
    llvm::Value* DefaultNan(llvm::Type* type);
    llvm::Value* IsNan(llvm::Value* value);
    bool ExactFloat() const;
    llvm::Value* NanResult(llvm::Value* result, std::initializer_list<llvm::Value*> sources);
    llvm::Value* DenormalsAreZero(llvm::Value* value);
    llvm::Value* FloatConstant(llvm::Type* type, const double value);
    llvm::Value* Arithmetic(const FloatKind kind, llvm::Value* a, llvm::Value* b);
    llvm::Value* RoundValue(llvm::Value* value, const std::uint64_t control);
    llvm::Value* CompareMask(llvm::Value* a, llvm::Value* b, const std::uint64_t predicate);
    llvm::Value* FusedMultiplyAdd(const FloatSpec& spec, llvm::Value* x1, llvm::Value* x2, llvm::Value* x3);
    llvm::Value* LoadScalarFloat(const int index, const unsigned element);
    void FloatArithmetic(const FloatSpec& spec);
    llvm::Value* ConvertNan(llvm::Value* source, llvm::Value* converted);
    llvm::Value* SingleToHalf(llvm::Value* source, const unsigned mode);
    llvm::Value* FloatToInteger(llvm::Value* value, const unsigned bits, const bool truncate);
    void WriteVectorLow(const int index, llvm::Value* vector, const unsigned width);
    void Convert(const ZydisMnemonic mnemonic);
    llvm::Value* ReadRegisterOrMemory(const int index, const unsigned bits);

    template <typename Zero, typename Carry>
    void ComparisonFlags(llvm::Value* unordered, Zero zero, Carry carry) {
        SetFlag(PS5LIFT_ZF, zero());
        SetFlag(PS5LIFT_PF, unordered);
        SetFlag(PS5LIFT_CF, carry());
        for (const auto flag : {PS5LIFT_OF, PS5LIFT_SF, PS5LIFT_AF}) SetFlag(flag, _builder.getFalse());
    }

    void CompareScalar(const unsigned element);
    std::pair<llvm::Value*, llvm::Value*> HorizontalOperands(llvm::Value* a, llvm::Value* b, const unsigned count, const unsigned perLane);
    void HalfMove(const ZydisMnemonic mnemonic);
    void DotProduct(const unsigned element);
    void BitField(const bool insert);
    void MinimumPosition();
    void PermuteVariable(const unsigned element);
    void MaskedMove(const unsigned element);
    void LoadMxcsr();
    void StoreMxcsr();
    void DuplicateSingles(const bool high);
    void DuplicateDoubles();
    void ShuffleDoubles();
    void PermuteDoubles();
    void InsertSingle();
    llvm::Value* CallX87(const std::uint32_t operation, const std::uint32_t format, llvm::Value* address, const std::uint32_t index);
    static bool IsStackRegister(const ZydisDecodedOperand& operand);
    static std::uint32_t StackIndex(const ZydisDecodedOperand& operand);
    const ZydisDecodedOperand* X87Memory() const;
    std::uint32_t X87Format(const bool integer) const;
    std::uint32_t X87Index(const std::uint32_t fallback) const;
    bool X87ToRegister() const;
    void X87CompareFlags(llvm::Value* result);
    llvm::Value* X87Address();
    void X87Arithmetic(const std::uint32_t operation, const bool integer, const bool pop);
    void X87StoreStatus();
    void X87Environment(const bool store);
    void X87SaveState();
    void X87RestoreState();
    void X87ConditionalMove(const ZydisMnemonic mnemonic);
    void EmitInstruction(const Instruction& instruction);
    llvm::Value* Loop(const ZydisMnemonic mnemonic);
    void Branch(llvm::Value* condition);
    llvm::Value* CounterIsZero(const unsigned bits);
    void ConditionalMove(const Condition condition);
    void SetCondition(const Condition condition);
    void RandomNumber();
    void ProcessorId();
    void TimeStamp();
    void TimeStampAndProcessor();
    void LoadFlagsIntoAh();
    void StoreAhIntoFlags();
    void LeaveMmx();
    void CacheLine(const char* function);
    void Cpuid();
    void ExtendedControlRegister();
    void Addition();
    void CarryArithmetic(const bool add);
    void Subtraction();
    void Logic(const llvm::Instruction::BinaryOps operation, const llvm::AtomicRMWInst::BinOp atomic);
    void Increment();
    void Decrement();
    void Negation();
    void Complement();
    void ByteSwap();
    void Leave();
    void MergeScalar(const unsigned element);
    void ZeroUpper();
    void Call();
    void Jump();
    void Return();
    void Interrupt();
    void EmitSemantics(const ZydisMnemonic mnemonic);
};

}

#endif
