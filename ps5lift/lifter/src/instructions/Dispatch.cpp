#include "../FunctionLifter.hpp"

namespace Ps5Lift::Lifting {

bool FunctionLifter::Lockable(const ZydisMnemonic mnemonic) {
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_ADD: case ZYDIS_MNEMONIC_ADC: case ZYDIS_MNEMONIC_SUB: case ZYDIS_MNEMONIC_SBB: case ZYDIS_MNEMONIC_AND: case ZYDIS_MNEMONIC_OR:
    case ZYDIS_MNEMONIC_XOR: case ZYDIS_MNEMONIC_INC: case ZYDIS_MNEMONIC_DEC: case ZYDIS_MNEMONIC_NEG: case ZYDIS_MNEMONIC_NOT: case ZYDIS_MNEMONIC_BTS:
    case ZYDIS_MNEMONIC_BTR: case ZYDIS_MNEMONIC_BTC: case ZYDIS_MNEMONIC_XCHG: case ZYDIS_MNEMONIC_XADD: case ZYDIS_MNEMONIC_CMPXCHG:
    case ZYDIS_MNEMONIC_CMPXCHG8B: case ZYDIS_MNEMONIC_CMPXCHG16B:
        return true;
    default:
        return false;
    }
}

void FunctionLifter::EmitSemantics(const ZydisMnemonic mnemonic) {
    if (Locked() && !(Lockable(mnemonic) && Operand(0).type == ZYDIS_OPERAND_TYPE_MEMORY)) throw UnsupportedInstruction("lock prefix");
    if (_instruction->Info.meta.branch_type == ZYDIS_BRANCH_TYPE_FAR) throw UnsupportedInstruction("far branch");
    if (UsesMmx()) EnterMmx();
    using K = VectorKind;
    using F = FloatKind;
    constexpr auto Element = [](const char* suffix) { return suffix[1] == 'S' ? 32u : 64u; };
    constexpr auto Scalar = [](const char* suffix) { return suffix[0] == 'S'; };
#define PS5LIFT_BOTH(name) case ZYDIS_MNEMONIC_##name: case ZYDIS_MNEMONIC_V##name
#define PS5LIFT_CONDITION(suffix, condition)                                         \
case ZYDIS_MNEMONIC_J##suffix: return Branch(Test(Condition::condition));        \
case ZYDIS_MNEMONIC_CMOV##suffix: return ConditionalMove(Condition::condition);  \
case ZYDIS_MNEMONIC_SET##suffix: return SetCondition(Condition::condition)
#define PS5LIFT_FLOAT4(name, kind)                                                                  \
PS5LIFT_BOTH(name##PS): return FloatArithmetic({kind, 32, false});                               \
PS5LIFT_BOTH(name##PD): return FloatArithmetic({kind, 64, false});                               \
PS5LIFT_BOTH(name##SS): return FloatArithmetic({kind, 32, true});                                \
PS5LIFT_BOTH(name##SD): return FloatArithmetic({kind, 64, true})
#define PS5LIFT_FMA(prefix, suffix, negate, subtract, alternate)                                                                             \
case ZYDIS_MNEMONIC_##prefix##132##suffix: return FloatArithmetic({F::Fma, Element(#suffix), Scalar(#suffix), 132, negate, subtract, alternate}); \
case ZYDIS_MNEMONIC_##prefix##213##suffix: return FloatArithmetic({F::Fma, Element(#suffix), Scalar(#suffix), 213, negate, subtract, alternate}); \
case ZYDIS_MNEMONIC_##prefix##231##suffix: return FloatArithmetic({F::Fma, Element(#suffix), Scalar(#suffix), 231, negate, subtract, alternate})
    switch (mnemonic) {
    PS5LIFT_CONDITION(O, O);
    PS5LIFT_CONDITION(NO, NO);
    PS5LIFT_CONDITION(B, B);
    PS5LIFT_CONDITION(NB, AE);
    PS5LIFT_CONDITION(Z, E);
    PS5LIFT_CONDITION(NZ, NE);
    PS5LIFT_CONDITION(BE, BE);
    PS5LIFT_CONDITION(NBE, A);
    PS5LIFT_CONDITION(S, S);
    PS5LIFT_CONDITION(NS, NS);
    PS5LIFT_CONDITION(P, P);
    PS5LIFT_CONDITION(NP, NP);
    PS5LIFT_CONDITION(L, L);
    PS5LIFT_CONDITION(NL, GE);
    PS5LIFT_CONDITION(LE, LE);
    PS5LIFT_CONDITION(NLE, G);
    case ZYDIS_MNEMONIC_JRCXZ: return Branch(CounterIsZero(64));
    case ZYDIS_MNEMONIC_JECXZ: return Branch(CounterIsZero(32));
    case ZYDIS_MNEMONIC_LOOP: case ZYDIS_MNEMONIC_LOOPE: case ZYDIS_MNEMONIC_LOOPNE: return Branch(Loop(mnemonic));
    case ZYDIS_MNEMONIC_JMP: return Jump();
    case ZYDIS_MNEMONIC_CALL: return Call();
    case ZYDIS_MNEMONIC_RET: return Return();
    case ZYDIS_MNEMONIC_UD2: case ZYDIS_MNEMONIC_INT3: case ZYDIS_MNEMONIC_HLT: return Trap();
    case ZYDIS_MNEMONIC_INT: return Interrupt();

    case ZYDIS_MNEMONIC_NOP: case ZYDIS_MNEMONIC_ENDBR64: case ZYDIS_MNEMONIC_PAUSE: case ZYDIS_MNEMONIC_PREFETCH: case ZYDIS_MNEMONIC_PREFETCHT0:
    case ZYDIS_MNEMONIC_PREFETCHT1: case ZYDIS_MNEMONIC_PREFETCHT2: case ZYDIS_MNEMONIC_PREFETCHNTA: case ZYDIS_MNEMONIC_PREFETCHW:
    case ZYDIS_MNEMONIC_MONITORX: case ZYDIS_MNEMONIC_MWAITX:
        return;
    case ZYDIS_MNEMONIC_MFENCE: _builder.CreateFence(llvm::AtomicOrdering::SequentiallyConsistent); return;
    case ZYDIS_MNEMONIC_LFENCE: case ZYDIS_MNEMONIC_SFENCE: _builder.CreateFence(llvm::AtomicOrdering::AcquireRelease); return;
    case ZYDIS_MNEMONIC_CLFLUSH: case ZYDIS_MNEMONIC_CLFLUSHOPT: return CacheLine("ps5lift_flush_cache_line");
    case ZYDIS_MNEMONIC_CLWB: return CacheLine("ps5lift_write_back_cache_line");
    case ZYDIS_MNEMONIC_CPUID: return Cpuid();
    case ZYDIS_MNEMONIC_XGETBV: return ExtendedControlRegister();
    case ZYDIS_MNEMONIC_RDTSC: return TimeStamp();
    case ZYDIS_MNEMONIC_RDTSCP: return TimeStampAndProcessor();
    case ZYDIS_MNEMONIC_RDPID: return ProcessorId();
    case ZYDIS_MNEMONIC_RDRAND: case ZYDIS_MNEMONIC_RDSEED: return RandomNumber();
    case ZYDIS_MNEMONIC_LAHF: return LoadFlagsIntoAh();
    case ZYDIS_MNEMONIC_SAHF: return StoreAhIntoFlags();
    case ZYDIS_MNEMONIC_CLD: return SetFlag(PS5LIFT_DF, _builder.getFalse());
    case ZYDIS_MNEMONIC_STD: return SetFlag(PS5LIFT_DF, _builder.getTrue());
    case ZYDIS_MNEMONIC_CLC: return SetFlag(PS5LIFT_CF, _builder.getFalse());
    case ZYDIS_MNEMONIC_STC: return SetFlag(PS5LIFT_CF, _builder.getTrue());
    case ZYDIS_MNEMONIC_CMC: return SetFlag(PS5LIFT_CF, _builder.CreateNot(Flag(PS5LIFT_CF)));

    case ZYDIS_MNEMONIC_MOV: return Write(0, Read(1));
    case ZYDIS_MNEMONIC_MOVZX: return Write(0, _builder.CreateZExt(Read(1), Int(Bits(0))));
    case ZYDIS_MNEMONIC_MOVSX: case ZYDIS_MNEMONIC_MOVSXD: return Write(0, _builder.CreateSExtOrTrunc(Read(1), Int(Bits(0))));
    case ZYDIS_MNEMONIC_LEA: return Write(0, _builder.CreateTrunc(Address(Operand(1), false), Int(Bits(0))));
    case ZYDIS_MNEMONIC_MOVBE: return Write(0, _builder.CreateUnaryIntrinsic(llvm::Intrinsic::bswap, Read(1)));
    case ZYDIS_MNEMONIC_BSWAP: return ByteSwap();
    case ZYDIS_MNEMONIC_PUSH: return Push(_builder.CreateSExtOrTrunc(Read(0), _builder.getIntNTy(_instruction->Info.operand_width)));
    case ZYDIS_MNEMONIC_POP: return Write(0, Pop(_instruction->Info.operand_width));
    case ZYDIS_MNEMONIC_LEAVE: return Leave();
    case ZYDIS_MNEMONIC_XCHG: return Exchange();
    case ZYDIS_MNEMONIC_XADD: return ExchangeAdd();
    case ZYDIS_MNEMONIC_CMPXCHG: return CompareExchange();
    case ZYDIS_MNEMONIC_CMPXCHG8B: case ZYDIS_MNEMONIC_CMPXCHG16B: return CompareExchangeWide();
    case ZYDIS_MNEMONIC_ADD: return Addition();
    case ZYDIS_MNEMONIC_ADC: return CarryArithmetic(true);
    case ZYDIS_MNEMONIC_SBB: return CarryArithmetic(false);
    case ZYDIS_MNEMONIC_SUB: return Subtraction();
    case ZYDIS_MNEMONIC_CMP: Sub(Read(0), ReadAs(1, Bits(0)), _builder.getFalse()); return;
    case ZYDIS_MNEMONIC_AND: return Logic(llvm::Instruction::And, llvm::AtomicRMWInst::And);
    case ZYDIS_MNEMONIC_OR: return Logic(llvm::Instruction::Or, llvm::AtomicRMWInst::Or);
    case ZYDIS_MNEMONIC_XOR: return Logic(llvm::Instruction::Xor, llvm::AtomicRMWInst::Xor);
    case ZYDIS_MNEMONIC_TEST: return LogicFlags(_builder.CreateAnd(Read(0), ReadAs(1, Bits(0))));
    case ZYDIS_MNEMONIC_INC: return Increment();
    case ZYDIS_MNEMONIC_DEC: return Decrement();
    case ZYDIS_MNEMONIC_NEG: return Negation();
    case ZYDIS_MNEMONIC_NOT: return Complement();
    case ZYDIS_MNEMONIC_ADCX: return AddWithFlagCarry(PS5LIFT_CF);
    case ZYDIS_MNEMONIC_ADOX: return AddWithFlagCarry(PS5LIFT_OF);
    case ZYDIS_MNEMONIC_IMUL: return SignedMultiply();
    case ZYDIS_MNEMONIC_MUL: return Multiply(false);
    case ZYDIS_MNEMONIC_DIV: return Divide(false);
    case ZYDIS_MNEMONIC_IDIV: return Divide(true);
    case ZYDIS_MNEMONIC_CBW: case ZYDIS_MNEMONIC_CWDE: case ZYDIS_MNEMONIC_CDQE: case ZYDIS_MNEMONIC_CWD: case ZYDIS_MNEMONIC_CDQ: case ZYDIS_MNEMONIC_CQO:
        return SignExtendAccumulator(mnemonic);
    case ZYDIS_MNEMONIC_SHL: case ZYDIS_MNEMONIC_SHR: case ZYDIS_MNEMONIC_SAR: case ZYDIS_MNEMONIC_ROL: case ZYDIS_MNEMONIC_ROR: return Shift(mnemonic);
    case ZYDIS_MNEMONIC_SHLD: case ZYDIS_MNEMONIC_SHRD: return ShiftDouble(mnemonic);
    case ZYDIS_MNEMONIC_BSF: case ZYDIS_MNEMONIC_BSR: case ZYDIS_MNEMONIC_TZCNT: case ZYDIS_MNEMONIC_LZCNT: case ZYDIS_MNEMONIC_POPCNT: return BitScan(mnemonic);
    case ZYDIS_MNEMONIC_BT: case ZYDIS_MNEMONIC_BTS: case ZYDIS_MNEMONIC_BTR: case ZYDIS_MNEMONIC_BTC: return BitTest(mnemonic);
    case ZYDIS_MNEMONIC_ANDN: case ZYDIS_MNEMONIC_SHLX: case ZYDIS_MNEMONIC_SHRX: case ZYDIS_MNEMONIC_SARX: case ZYDIS_MNEMONIC_RORX: case ZYDIS_MNEMONIC_BEXTR:
    case ZYDIS_MNEMONIC_BZHI: case ZYDIS_MNEMONIC_BLSR: case ZYDIS_MNEMONIC_BLSI: case ZYDIS_MNEMONIC_BLSMSK: case ZYDIS_MNEMONIC_MULX:
        return BitManipulation(mnemonic);
    case ZYDIS_MNEMONIC_CRC32: return Crc32();

    case ZYDIS_MNEMONIC_MOVSB: return StringOp(PS5LIFT_STRING_MOVS, 1);
    case ZYDIS_MNEMONIC_MOVSW: return StringOp(PS5LIFT_STRING_MOVS, 2);
    case ZYDIS_MNEMONIC_MOVSD: return _instruction->Info.operand_count_visible == 0 ? StringOp(PS5LIFT_STRING_MOVS, 4) : VectorMove();
    case ZYDIS_MNEMONIC_MOVSQ: return StringOp(PS5LIFT_STRING_MOVS, 8);
    case ZYDIS_MNEMONIC_STOSB: return StringOp(PS5LIFT_STRING_STOS, 1);
    case ZYDIS_MNEMONIC_STOSW: return StringOp(PS5LIFT_STRING_STOS, 2);
    case ZYDIS_MNEMONIC_STOSD: return StringOp(PS5LIFT_STRING_STOS, 4);
    case ZYDIS_MNEMONIC_STOSQ: return StringOp(PS5LIFT_STRING_STOS, 8);
    case ZYDIS_MNEMONIC_CMPSB: return ReadString(StringRead::Compare, 1);
    case ZYDIS_MNEMONIC_CMPSW: return ReadString(StringRead::Compare, 2);
    case ZYDIS_MNEMONIC_CMPSD: return _instruction->Info.operand_count_visible == 0 ? ReadString(StringRead::Compare, 4) : FloatArithmetic({F::Compare, 64, true});
    case ZYDIS_MNEMONIC_CMPSQ: return ReadString(StringRead::Compare, 8);
    case ZYDIS_MNEMONIC_SCASB: return ReadString(StringRead::Scan, 1);
    case ZYDIS_MNEMONIC_SCASW: return ReadString(StringRead::Scan, 2);
    case ZYDIS_MNEMONIC_SCASD: return ReadString(StringRead::Scan, 4);
    case ZYDIS_MNEMONIC_SCASQ: return ReadString(StringRead::Scan, 8);
    case ZYDIS_MNEMONIC_LODSB: return ReadString(StringRead::Load, 1);
    case ZYDIS_MNEMONIC_LODSW: return ReadString(StringRead::Load, 2);
    case ZYDIS_MNEMONIC_LODSD: return ReadString(StringRead::Load, 4);
    case ZYDIS_MNEMONIC_LODSQ: return ReadString(StringRead::Load, 8);
    PS5LIFT_BOTH(PCMPESTRI): return StringCompare(true, true);
    PS5LIFT_BOTH(PCMPESTRM): return StringCompare(true, false);
    PS5LIFT_BOTH(PCMPISTRI): return StringCompare(false, true);
    PS5LIFT_BOTH(PCMPISTRM): return StringCompare(false, false);

    case ZYDIS_MNEMONIC_MOVAPS: case ZYDIS_MNEMONIC_MOVUPS: case ZYDIS_MNEMONIC_MOVAPD: case ZYDIS_MNEMONIC_MOVUPD:
    case ZYDIS_MNEMONIC_MOVDQA: case ZYDIS_MNEMONIC_MOVDQU: case ZYDIS_MNEMONIC_MOVQ: case ZYDIS_MNEMONIC_MOVD: case ZYDIS_MNEMONIC_MOVSS:
    case ZYDIS_MNEMONIC_VMOVAPS: case ZYDIS_MNEMONIC_VMOVUPS: case ZYDIS_MNEMONIC_VMOVAPD: case ZYDIS_MNEMONIC_VMOVUPD:
    case ZYDIS_MNEMONIC_VMOVDQA: case ZYDIS_MNEMONIC_VMOVDQU: case ZYDIS_MNEMONIC_VMOVQ: case ZYDIS_MNEMONIC_VMOVD:
    case ZYDIS_MNEMONIC_MOVNTDQ: case ZYDIS_MNEMONIC_VMOVNTDQ: case ZYDIS_MNEMONIC_MOVNTPS: case ZYDIS_MNEMONIC_VMOVNTPS: case ZYDIS_MNEMONIC_MOVNTQ:
    case ZYDIS_MNEMONIC_MOVNTDQA: case ZYDIS_MNEMONIC_VMOVNTDQA: case ZYDIS_MNEMONIC_LDDQU: case ZYDIS_MNEMONIC_VLDDQU:
        return VectorMove();
    case ZYDIS_MNEMONIC_VMOVSS: return _instruction->Info.operand_count_visible == 3 ? MergeScalar(32) : VectorMove();
    case ZYDIS_MNEMONIC_VMOVSD: return _instruction->Info.operand_count_visible == 3 ? MergeScalar(64) : VectorMove();
    case ZYDIS_MNEMONIC_MOVHLPS: case ZYDIS_MNEMONIC_VMOVHLPS: case ZYDIS_MNEMONIC_MOVLHPS: case ZYDIS_MNEMONIC_VMOVLHPS:
    case ZYDIS_MNEMONIC_MOVHPS: case ZYDIS_MNEMONIC_VMOVHPS: case ZYDIS_MNEMONIC_MOVLPS: case ZYDIS_MNEMONIC_VMOVLPS:
    case ZYDIS_MNEMONIC_MOVHPD: case ZYDIS_MNEMONIC_VMOVHPD: case ZYDIS_MNEMONIC_MOVLPD: case ZYDIS_MNEMONIC_VMOVLPD:
        return HalfMove(mnemonic);
    case ZYDIS_MNEMONIC_VMASKMOVPS: case ZYDIS_MNEMONIC_VPMASKMOVD: return MaskedMove(32);
    case ZYDIS_MNEMONIC_VMASKMOVPD: case ZYDIS_MNEMONIC_VPMASKMOVQ: return MaskedMove(64);
    case ZYDIS_MNEMONIC_VZEROUPPER: return ZeroUpper();
    case ZYDIS_MNEMONIC_EMMS: return LeaveMmx();
    PS5LIFT_BOTH(PXOR): PS5LIFT_BOTH(XORPS): PS5LIFT_BOTH(XORPD): return VectorBitwise(llvm::Instruction::Xor);
    PS5LIFT_BOTH(PAND): PS5LIFT_BOTH(ANDPS): PS5LIFT_BOTH(ANDPD): return VectorBitwise(llvm::Instruction::And);
    PS5LIFT_BOTH(POR): PS5LIFT_BOTH(ORPS): PS5LIFT_BOTH(ORPD): return VectorBitwise(llvm::Instruction::Or);
    case ZYDIS_MNEMONIC_VPBROADCASTB: return Broadcast(8);
    case ZYDIS_MNEMONIC_VPBROADCASTW: return Broadcast(16);
    case ZYDIS_MNEMONIC_VPBROADCASTD: case ZYDIS_MNEMONIC_VBROADCASTSS: return Broadcast(32);
    case ZYDIS_MNEMONIC_VPBROADCASTQ: case ZYDIS_MNEMONIC_VBROADCASTSD: return Broadcast(64);
    case ZYDIS_MNEMONIC_VBROADCASTF128: case ZYDIS_MNEMONIC_VBROADCASTI128: return Broadcast(128);
    case ZYDIS_MNEMONIC_VPERMILPS: return Operand(2).type == ZYDIS_OPERAND_TYPE_IMMEDIATE ? ShuffleInLanes(32, 0) : PermuteVariable(32);
    case ZYDIS_MNEMONIC_VPERMILPD: return Operand(2).type == ZYDIS_OPERAND_TYPE_IMMEDIATE ? PermuteDoubles() : PermuteVariable(64);
    PS5LIFT_BOTH(SHUFPD): return ShuffleDoubles();
    PS5LIFT_BOTH(MOVSHDUP): return DuplicateSingles(true);
    PS5LIFT_BOTH(MOVSLDUP): return DuplicateSingles(false);
    PS5LIFT_BOTH(MOVDDUP): return DuplicateDoubles();
    PS5LIFT_BOTH(INSERTPS): return InsertSingle();
    PS5LIFT_BOTH(PHMINPOSUW): return MinimumPosition();
    case ZYDIS_MNEMONIC_EXTRQ: return BitField(false);
    case ZYDIS_MNEMONIC_INSERTQ: return BitField(true);
    case ZYDIS_MNEMONIC_PSHUFW: return ShuffleInLanes(16, 0);
    PS5LIFT_BOTH(PADDB): return VectorBinary(K::Add, 8);
    PS5LIFT_BOTH(PADDW): return VectorBinary(K::Add, 16);
    PS5LIFT_BOTH(PADDD): return VectorBinary(K::Add, 32);
    PS5LIFT_BOTH(PADDQ): return VectorBinary(K::Add, 64);
    PS5LIFT_BOTH(PSUBB): return VectorBinary(K::Sub, 8);
    PS5LIFT_BOTH(PSUBW): return VectorBinary(K::Sub, 16);
    PS5LIFT_BOTH(PSUBD): return VectorBinary(K::Sub, 32);
    PS5LIFT_BOTH(PSUBQ): return VectorBinary(K::Sub, 64);
    PS5LIFT_BOTH(PADDSB): return VectorBinary(K::AddSignedSaturate, 8);
    PS5LIFT_BOTH(PADDSW): return VectorBinary(K::AddSignedSaturate, 16);
    PS5LIFT_BOTH(PADDUSB): return VectorBinary(K::AddUnsignedSaturate, 8);
    PS5LIFT_BOTH(PADDUSW): return VectorBinary(K::AddUnsignedSaturate, 16);
    PS5LIFT_BOTH(PSUBSB): return VectorBinary(K::SubSignedSaturate, 8);
    PS5LIFT_BOTH(PSUBSW): return VectorBinary(K::SubSignedSaturate, 16);
    PS5LIFT_BOTH(PSUBUSB): return VectorBinary(K::SubUnsignedSaturate, 8);
    PS5LIFT_BOTH(PSUBUSW): return VectorBinary(K::SubUnsignedSaturate, 16);
    PS5LIFT_BOTH(PMULLW): return VectorBinary(K::MultiplyLow, 16);
    PS5LIFT_BOTH(PMULLD): return VectorBinary(K::MultiplyLow, 32);
    PS5LIFT_BOTH(PMULHW): return VectorBinary(K::MultiplyHighSigned, 16);
    PS5LIFT_BOTH(PMULHRSW): return VectorBinary(K::MultiplyHighRoundScale, 16);
    PS5LIFT_BOTH(PMADDUBSW): return VectorBinary(K::MultiplyAddBytes, 16);
    PS5LIFT_BOTH(PHADDW): return VectorBinary(K::HorizontalAdd, 16);
    PS5LIFT_BOTH(PHADDD): return VectorBinary(K::HorizontalAdd, 32);
    PS5LIFT_BOTH(PHADDSW): return VectorBinary(K::HorizontalAddSaturate, 16);
    PS5LIFT_BOTH(PHSUBW): return VectorBinary(K::HorizontalSub, 16);
    PS5LIFT_BOTH(PHSUBD): return VectorBinary(K::HorizontalSub, 32);
    PS5LIFT_BOTH(PHSUBSW): return VectorBinary(K::HorizontalSubSaturate, 16);
    PS5LIFT_BOTH(PSADBW): return VectorBinary(K::SumAbsoluteDifferences, 64);
    case ZYDIS_MNEMONIC_VTESTPS: return VectorTestSigns(32);
    case ZYDIS_MNEMONIC_VTESTPD: return VectorTestSigns(64);
    PS5LIFT_BOTH(PMULHUW): return VectorBinary(K::MultiplyHighUnsigned, 16);
    PS5LIFT_BOTH(PMULUDQ): return VectorBinary(K::MultiplyEvenUnsigned, 64);
    PS5LIFT_BOTH(PMULDQ): return VectorBinary(K::MultiplyEvenSigned, 64);
    PS5LIFT_BOTH(PMADDWD): return VectorBinary(K::MultiplyAddWords, 32);
    PS5LIFT_BOTH(PMINSB): return VectorBinary(K::MinSigned, 8);
    PS5LIFT_BOTH(PMINSW): return VectorBinary(K::MinSigned, 16);
    PS5LIFT_BOTH(PMINSD): return VectorBinary(K::MinSigned, 32);
    PS5LIFT_BOTH(PMINUB): return VectorBinary(K::MinUnsigned, 8);
    PS5LIFT_BOTH(PMINUW): return VectorBinary(K::MinUnsigned, 16);
    PS5LIFT_BOTH(PMINUD): return VectorBinary(K::MinUnsigned, 32);
    PS5LIFT_BOTH(PMAXSB): return VectorBinary(K::MaxSigned, 8);
    PS5LIFT_BOTH(PMAXSW): return VectorBinary(K::MaxSigned, 16);
    PS5LIFT_BOTH(PMAXSD): return VectorBinary(K::MaxSigned, 32);
    PS5LIFT_BOTH(PMAXUB): return VectorBinary(K::MaxUnsigned, 8);
    PS5LIFT_BOTH(PMAXUW): return VectorBinary(K::MaxUnsigned, 16);
    PS5LIFT_BOTH(PMAXUD): return VectorBinary(K::MaxUnsigned, 32);
    PS5LIFT_BOTH(PCMPEQB): return VectorBinary(K::CompareEqual, 8);
    PS5LIFT_BOTH(PCMPEQW): return VectorBinary(K::CompareEqual, 16);
    PS5LIFT_BOTH(PCMPEQD): return VectorBinary(K::CompareEqual, 32);
    PS5LIFT_BOTH(PCMPEQQ): return VectorBinary(K::CompareEqual, 64);
    PS5LIFT_BOTH(PCMPGTB): return VectorBinary(K::CompareGreater, 8);
    PS5LIFT_BOTH(PCMPGTW): return VectorBinary(K::CompareGreater, 16);
    PS5LIFT_BOTH(PCMPGTD): return VectorBinary(K::CompareGreater, 32);
    PS5LIFT_BOTH(PCMPGTQ): return VectorBinary(K::CompareGreater, 64);
    PS5LIFT_BOTH(PAVGB): return VectorBinary(K::Average, 8);
    PS5LIFT_BOTH(PAVGW): return VectorBinary(K::Average, 16);
    PS5LIFT_BOTH(PANDN): case ZYDIS_MNEMONIC_ANDNPS: case ZYDIS_MNEMONIC_VANDNPS: case ZYDIS_MNEMONIC_ANDNPD: case ZYDIS_MNEMONIC_VANDNPD: return VectorBinary(K::AndNot, 64);
    PS5LIFT_BOTH(PABSB): return VectorAbsolute(8);
    PS5LIFT_BOTH(PABSW): return VectorAbsolute(16);
    PS5LIFT_BOTH(PABSD): return VectorAbsolute(32);
    PS5LIFT_BOTH(PSLLW): return VectorShift(llvm::Instruction::Shl, 16);
    PS5LIFT_BOTH(PSLLD): return VectorShift(llvm::Instruction::Shl, 32);
    PS5LIFT_BOTH(PSLLQ): return VectorShift(llvm::Instruction::Shl, 64);
    PS5LIFT_BOTH(PSRLW): return VectorShift(llvm::Instruction::LShr, 16);
    PS5LIFT_BOTH(PSRLD): return VectorShift(llvm::Instruction::LShr, 32);
    PS5LIFT_BOTH(PSRLQ): return VectorShift(llvm::Instruction::LShr, 64);
    PS5LIFT_BOTH(PSRAW): return VectorShift(llvm::Instruction::AShr, 16);
    PS5LIFT_BOTH(PSRAD): return VectorShift(llvm::Instruction::AShr, 32);
    case ZYDIS_MNEMONIC_VPSLLVD: return VectorShiftVariable(llvm::Instruction::Shl, 32);
    case ZYDIS_MNEMONIC_VPSLLVQ: return VectorShiftVariable(llvm::Instruction::Shl, 64);
    case ZYDIS_MNEMONIC_VPSRLVD: return VectorShiftVariable(llvm::Instruction::LShr, 32);
    case ZYDIS_MNEMONIC_VPSRLVQ: return VectorShiftVariable(llvm::Instruction::LShr, 64);
    case ZYDIS_MNEMONIC_VPSRAVD: return VectorShiftVariable(llvm::Instruction::AShr, 32);
    PS5LIFT_BOTH(PSLLDQ): return ShiftBytes(llvm::Instruction::Shl);
    PS5LIFT_BOTH(PSRLDQ): return ShiftBytes(llvm::Instruction::LShr);
    PS5LIFT_BOTH(PSHUFD): return ShuffleInLanes(32, 0);
    PS5LIFT_BOTH(PSHUFLW): return ShuffleInLanes(16, 0);
    PS5LIFT_BOTH(PSHUFHW): return ShuffleInLanes(16, 4);
    PS5LIFT_BOTH(SHUFPS): return ShufflePairs();
    PS5LIFT_BOTH(PUNPCKLBW): return VectorBinary(K::UnpackLow, 8);
    PS5LIFT_BOTH(PUNPCKLWD): return VectorBinary(K::UnpackLow, 16);
    PS5LIFT_BOTH(PUNPCKLDQ): PS5LIFT_BOTH(UNPCKLPS): return VectorBinary(K::UnpackLow, 32);
    PS5LIFT_BOTH(PUNPCKLQDQ): PS5LIFT_BOTH(UNPCKLPD): return VectorBinary(K::UnpackLow, 64);
    PS5LIFT_BOTH(PUNPCKHBW): return VectorBinary(K::UnpackHigh, 8);
    PS5LIFT_BOTH(PUNPCKHWD): return VectorBinary(K::UnpackHigh, 16);
    PS5LIFT_BOTH(PUNPCKHDQ): PS5LIFT_BOTH(UNPCKHPS): return VectorBinary(K::UnpackHigh, 32);
    PS5LIFT_BOTH(PUNPCKHQDQ): PS5LIFT_BOTH(UNPCKHPD): return VectorBinary(K::UnpackHigh, 64);
    PS5LIFT_BOTH(PACKSSWB): return VectorBinary(K::PackSignedSaturate, 8);
    PS5LIFT_BOTH(PACKSSDW): return VectorBinary(K::PackSignedSaturate, 16);
    PS5LIFT_BOTH(PACKUSWB): return VectorBinary(K::PackUnsignedSaturate, 8);
    PS5LIFT_BOTH(PACKUSDW): return VectorBinary(K::PackUnsignedSaturate, 16);
    PS5LIFT_BOTH(PALIGNR): return AlignRight();
    PS5LIFT_BOTH(PSHUFB): return VectorBinary(K::ShuffleBytes, 8);
    case ZYDIS_MNEMONIC_VPERMQ: case ZYDIS_MNEMONIC_VPERMPD: return PermuteQwords();
    case ZYDIS_MNEMONIC_VPERMD: case ZYDIS_MNEMONIC_VPERMPS: return VectorBinary(K::PermuteDwords, 32);
    case ZYDIS_MNEMONIC_VPERM2I128: case ZYDIS_MNEMONIC_VPERM2F128: return Permute128();
    case ZYDIS_MNEMONIC_VINSERTI128: case ZYDIS_MNEMONIC_VINSERTF128: return Insert128();
    case ZYDIS_MNEMONIC_VEXTRACTI128: case ZYDIS_MNEMONIC_VEXTRACTF128: return Extract128();
    case ZYDIS_MNEMONIC_VPBLENDD: PS5LIFT_BOTH(BLENDPS): return BlendImmediate(32);
    PS5LIFT_BOTH(BLENDPD): return BlendImmediate(64);
    PS5LIFT_BOTH(PBLENDW): return BlendImmediate(16);
    PS5LIFT_BOTH(PBLENDVB): return BlendVariable(8);
    PS5LIFT_BOTH(BLENDVPS): return BlendVariable(32);
    PS5LIFT_BOTH(BLENDVPD): return BlendVariable(64);
    PS5LIFT_BOTH(PMOVZXBW): return VectorExtend(llvm::Instruction::ZExt, 16, 8);
    PS5LIFT_BOTH(PMOVZXBD): return VectorExtend(llvm::Instruction::ZExt, 32, 8);
    PS5LIFT_BOTH(PMOVZXBQ): return VectorExtend(llvm::Instruction::ZExt, 64, 8);
    PS5LIFT_BOTH(PMOVZXWD): return VectorExtend(llvm::Instruction::ZExt, 32, 16);
    PS5LIFT_BOTH(PMOVZXWQ): return VectorExtend(llvm::Instruction::ZExt, 64, 16);
    PS5LIFT_BOTH(PMOVZXDQ): return VectorExtend(llvm::Instruction::ZExt, 64, 32);
    PS5LIFT_BOTH(PMOVSXBW): return VectorExtend(llvm::Instruction::SExt, 16, 8);
    PS5LIFT_BOTH(PMOVSXBD): return VectorExtend(llvm::Instruction::SExt, 32, 8);
    PS5LIFT_BOTH(PMOVSXBQ): return VectorExtend(llvm::Instruction::SExt, 64, 8);
    PS5LIFT_BOTH(PMOVSXWD): return VectorExtend(llvm::Instruction::SExt, 32, 16);
    PS5LIFT_BOTH(PMOVSXWQ): return VectorExtend(llvm::Instruction::SExt, 64, 16);
    PS5LIFT_BOTH(PMOVSXDQ): return VectorExtend(llvm::Instruction::SExt, 64, 32);
    PS5LIFT_BOTH(PMOVMSKB): return MoveMask(8);
    PS5LIFT_BOTH(MOVMSKPS): return MoveMask(32);
    PS5LIFT_BOTH(MOVMSKPD): return MoveMask(64);
    PS5LIFT_BOTH(PEXTRB): return ExtractElement(8);
    PS5LIFT_BOTH(PEXTRW): return ExtractElement(16);
    PS5LIFT_BOTH(PEXTRD): PS5LIFT_BOTH(EXTRACTPS): return ExtractElement(32);
    PS5LIFT_BOTH(PEXTRQ): return ExtractElement(64);
    PS5LIFT_BOTH(PINSRB): return InsertElement(8);
    PS5LIFT_BOTH(PINSRW): return InsertElement(16);
    PS5LIFT_BOTH(PINSRD): return InsertElement(32);
    PS5LIFT_BOTH(PINSRQ): return InsertElement(64);
    PS5LIFT_BOTH(PTEST): return VectorTest();
    PS5LIFT_BOTH(AESENC): PS5LIFT_BOTH(AESENCLAST): PS5LIFT_BOTH(AESDEC): PS5LIFT_BOTH(AESDECLAST): PS5LIFT_BOTH(AESIMC): PS5LIFT_BOTH(AESKEYGENASSIST): PS5LIFT_BOTH(PCLMULQDQ):
        return Crypto(mnemonic);
    case ZYDIS_MNEMONIC_SHA1RNDS4: case ZYDIS_MNEMONIC_SHA1NEXTE: case ZYDIS_MNEMONIC_SHA1MSG1: case ZYDIS_MNEMONIC_SHA1MSG2:
    case ZYDIS_MNEMONIC_SHA256RNDS2: case ZYDIS_MNEMONIC_SHA256MSG1: case ZYDIS_MNEMONIC_SHA256MSG2:
        return Sha(mnemonic);

    PS5LIFT_FLOAT4(ADD, F::Add);
    PS5LIFT_FLOAT4(SUB, F::Sub);
    PS5LIFT_FLOAT4(MUL, F::Mul);
    PS5LIFT_FLOAT4(DIV, F::Div);
    PS5LIFT_FLOAT4(MIN, F::Min);
    PS5LIFT_FLOAT4(MAX, F::Max);
    PS5LIFT_FLOAT4(SQRT, F::Sqrt);
    PS5LIFT_FLOAT4(ROUND, F::Round);
    PS5LIFT_BOTH(CMPPS): return FloatArithmetic({F::Compare, 32, false});
    PS5LIFT_BOTH(CMPPD): return FloatArithmetic({F::Compare, 64, false});
    PS5LIFT_BOTH(CMPSS): return FloatArithmetic({F::Compare, 32, true});
    case ZYDIS_MNEMONIC_VCMPSD: return FloatArithmetic({F::Compare, 64, true});
    case ZYDIS_MNEMONIC_RCPPS: case ZYDIS_MNEMONIC_VRCPPS: return FloatArithmetic({F::Reciprocal, 32, false});
    case ZYDIS_MNEMONIC_RCPSS: case ZYDIS_MNEMONIC_VRCPSS: return FloatArithmetic({F::Reciprocal, 32, true});
    case ZYDIS_MNEMONIC_RSQRTPS: case ZYDIS_MNEMONIC_VRSQRTPS: return FloatArithmetic({F::ReciprocalSqrt, 32, false});
    case ZYDIS_MNEMONIC_RSQRTSS: case ZYDIS_MNEMONIC_VRSQRTSS: return FloatArithmetic({F::ReciprocalSqrt, 32, true});
    case ZYDIS_MNEMONIC_ADDSUBPS: case ZYDIS_MNEMONIC_VADDSUBPS: return FloatArithmetic({F::AddSub, 32, false});
    case ZYDIS_MNEMONIC_ADDSUBPD: case ZYDIS_MNEMONIC_VADDSUBPD: return FloatArithmetic({F::AddSub, 64, false});
    case ZYDIS_MNEMONIC_HADDPS: case ZYDIS_MNEMONIC_VHADDPS: return FloatArithmetic({F::HorizontalAdd, 32, false});
    case ZYDIS_MNEMONIC_HADDPD: case ZYDIS_MNEMONIC_VHADDPD: return FloatArithmetic({F::HorizontalAdd, 64, false});
    case ZYDIS_MNEMONIC_HSUBPS: case ZYDIS_MNEMONIC_VHSUBPS: return FloatArithmetic({F::HorizontalSub, 32, false});
    case ZYDIS_MNEMONIC_HSUBPD: case ZYDIS_MNEMONIC_VHSUBPD: return FloatArithmetic({F::HorizontalSub, 64, false});
    PS5LIFT_FMA(VFMADD, PS, false, false, 0);
    PS5LIFT_FMA(VFMADD, PD, false, false, 0);
    PS5LIFT_FMA(VFMADD, SS, false, false, 0);
    PS5LIFT_FMA(VFMADD, SD, false, false, 0);
    PS5LIFT_FMA(VFMSUB, PS, false, true, 0);
    PS5LIFT_FMA(VFMSUB, PD, false, true, 0);
    PS5LIFT_FMA(VFMSUB, SS, false, true, 0);
    PS5LIFT_FMA(VFMSUB, SD, false, true, 0);
    PS5LIFT_FMA(VFNMADD, PS, true, false, 0);
    PS5LIFT_FMA(VFNMADD, PD, true, false, 0);
    PS5LIFT_FMA(VFNMADD, SS, true, false, 0);
    PS5LIFT_FMA(VFNMADD, SD, true, false, 0);
    PS5LIFT_FMA(VFNMSUB, PS, true, true, 0);
    PS5LIFT_FMA(VFNMSUB, PD, true, true, 0);
    PS5LIFT_FMA(VFNMSUB, SS, true, true, 0);
    PS5LIFT_FMA(VFNMSUB, SD, true, true, 0);
    PS5LIFT_FMA(VFMADDSUB, PS, false, false, 1);
    PS5LIFT_FMA(VFMADDSUB, PD, false, false, 1);
    PS5LIFT_FMA(VFMSUBADD, PS, false, false, 2);
    PS5LIFT_FMA(VFMSUBADD, PD, false, false, 2);
    PS5LIFT_BOTH(CVTSI2SS): PS5LIFT_BOTH(CVTSI2SD): PS5LIFT_BOTH(CVTSS2SD): PS5LIFT_BOTH(CVTSD2SS):
    PS5LIFT_BOTH(CVTTSS2SI): PS5LIFT_BOTH(CVTSS2SI): PS5LIFT_BOTH(CVTTSD2SI): PS5LIFT_BOTH(CVTSD2SI):
    PS5LIFT_BOTH(CVTDQ2PS): PS5LIFT_BOTH(CVTTPS2DQ): PS5LIFT_BOTH(CVTPS2DQ): PS5LIFT_BOTH(CVTDQ2PD): PS5LIFT_BOTH(CVTPS2PD):
    PS5LIFT_BOTH(CVTPD2PS): PS5LIFT_BOTH(CVTPD2DQ): PS5LIFT_BOTH(CVTTPD2DQ): case ZYDIS_MNEMONIC_VCVTPH2PS: case ZYDIS_MNEMONIC_VCVTPS2PH:
        return Convert(mnemonic);
    PS5LIFT_BOTH(COMISS): PS5LIFT_BOTH(UCOMISS): return CompareScalar(32);
    PS5LIFT_BOTH(COMISD): PS5LIFT_BOTH(UCOMISD): return CompareScalar(64);
    PS5LIFT_BOTH(DPPS): return DotProduct(32);
    PS5LIFT_BOTH(DPPD): return DotProduct(64);
    PS5LIFT_BOTH(LDMXCSR): return LoadMxcsr();
    PS5LIFT_BOTH(STMXCSR): return StoreMxcsr();

    case ZYDIS_MNEMONIC_FLD: CallX87(PS5LIFT_X87_LOAD, X87Format(false), X87Address(), X87Index(0)); return;
    case ZYDIS_MNEMONIC_FILD: CallX87(PS5LIFT_X87_LOAD, X87Format(true), X87Address(), 0); return;
    case ZYDIS_MNEMONIC_FST: CallX87(PS5LIFT_X87_STORE, X87Format(false), X87Address(), X87Index(0)); return;
    case ZYDIS_MNEMONIC_FSTP: CallX87(PS5LIFT_X87_STORE, X87Format(false) | PS5LIFT_X87_POP, X87Address(), X87Index(0)); return;
    case ZYDIS_MNEMONIC_FIST: CallX87(PS5LIFT_X87_STORE, X87Format(true), X87Address(), 0); return;
    case ZYDIS_MNEMONIC_FISTP: CallX87(PS5LIFT_X87_STORE, X87Format(true) | PS5LIFT_X87_POP, X87Address(), 0); return;
    case ZYDIS_MNEMONIC_FISTTP: CallX87(PS5LIFT_X87_STORE, X87Format(true) | PS5LIFT_X87_POP | PS5LIFT_X87_TRUNCATE, X87Address(), 0); return;
    case ZYDIS_MNEMONIC_FADD: X87Arithmetic(PS5LIFT_X87_ADD, false, false); return;
    case ZYDIS_MNEMONIC_FADDP: X87Arithmetic(PS5LIFT_X87_ADD, false, true); return;
    case ZYDIS_MNEMONIC_FIADD: X87Arithmetic(PS5LIFT_X87_ADD, true, false); return;
    case ZYDIS_MNEMONIC_FSUB: X87Arithmetic(PS5LIFT_X87_SUBTRACT, false, false); return;
    case ZYDIS_MNEMONIC_FSUBP: X87Arithmetic(PS5LIFT_X87_SUBTRACT, false, true); return;
    case ZYDIS_MNEMONIC_FISUB: X87Arithmetic(PS5LIFT_X87_SUBTRACT, true, false); return;
    case ZYDIS_MNEMONIC_FSUBR: X87Arithmetic(PS5LIFT_X87_SUBTRACT_REVERSE, false, false); return;
    case ZYDIS_MNEMONIC_FSUBRP: X87Arithmetic(PS5LIFT_X87_SUBTRACT_REVERSE, false, true); return;
    case ZYDIS_MNEMONIC_FISUBR: X87Arithmetic(PS5LIFT_X87_SUBTRACT_REVERSE, true, false); return;
    case ZYDIS_MNEMONIC_FMUL: X87Arithmetic(PS5LIFT_X87_MULTIPLY, false, false); return;
    case ZYDIS_MNEMONIC_FMULP: X87Arithmetic(PS5LIFT_X87_MULTIPLY, false, true); return;
    case ZYDIS_MNEMONIC_FIMUL: X87Arithmetic(PS5LIFT_X87_MULTIPLY, true, false); return;
    case ZYDIS_MNEMONIC_FDIV: X87Arithmetic(PS5LIFT_X87_DIVIDE, false, false); return;
    case ZYDIS_MNEMONIC_FDIVP: X87Arithmetic(PS5LIFT_X87_DIVIDE, false, true); return;
    case ZYDIS_MNEMONIC_FIDIV: X87Arithmetic(PS5LIFT_X87_DIVIDE, true, false); return;
    case ZYDIS_MNEMONIC_FDIVR: X87Arithmetic(PS5LIFT_X87_DIVIDE_REVERSE, false, false); return;
    case ZYDIS_MNEMONIC_FDIVRP: X87Arithmetic(PS5LIFT_X87_DIVIDE_REVERSE, false, true); return;
    case ZYDIS_MNEMONIC_FIDIVR: X87Arithmetic(PS5LIFT_X87_DIVIDE_REVERSE, true, false); return;
    case ZYDIS_MNEMONIC_FCOM: CallX87(PS5LIFT_X87_COMPARE, X87Format(false), X87Address(), X87Index(1)); return;
    case ZYDIS_MNEMONIC_FCOMP: CallX87(PS5LIFT_X87_COMPARE, X87Format(false) | PS5LIFT_X87_POP, X87Address(), X87Index(1)); return;
    case ZYDIS_MNEMONIC_FCOMPP: CallX87(PS5LIFT_X87_COMPARE, PS5LIFT_X87_POP_TWICE, nullptr, 1); return;
    case ZYDIS_MNEMONIC_FUCOM: CallX87(PS5LIFT_X87_COMPARE_UNORDERED, PS5LIFT_X87_REGISTER, nullptr, X87Index(1)); return;
    case ZYDIS_MNEMONIC_FUCOMP: CallX87(PS5LIFT_X87_COMPARE_UNORDERED, PS5LIFT_X87_POP, nullptr, X87Index(1)); return;
    case ZYDIS_MNEMONIC_FUCOMPP: CallX87(PS5LIFT_X87_COMPARE_UNORDERED, PS5LIFT_X87_POP_TWICE, nullptr, 1); return;
    case ZYDIS_MNEMONIC_FICOM: CallX87(PS5LIFT_X87_COMPARE, X87Format(true), X87Address(), 0); return;
    case ZYDIS_MNEMONIC_FICOMP: CallX87(PS5LIFT_X87_COMPARE, X87Format(true) | PS5LIFT_X87_POP, X87Address(), 0); return;
    case ZYDIS_MNEMONIC_FCOMI: X87CompareFlags(CallX87(PS5LIFT_X87_COMPARE_FLAGS, 0, nullptr, X87Index(1))); return;
    case ZYDIS_MNEMONIC_FCOMIP: X87CompareFlags(CallX87(PS5LIFT_X87_COMPARE_FLAGS, PS5LIFT_X87_POP, nullptr, X87Index(1))); return;
    case ZYDIS_MNEMONIC_FUCOMI: X87CompareFlags(CallX87(PS5LIFT_X87_COMPARE_FLAGS_UNORDERED, 0, nullptr, X87Index(1))); return;
    case ZYDIS_MNEMONIC_FUCOMIP: X87CompareFlags(CallX87(PS5LIFT_X87_COMPARE_FLAGS_UNORDERED, PS5LIFT_X87_POP, nullptr, X87Index(1))); return;
    case ZYDIS_MNEMONIC_FTST: CallX87(PS5LIFT_X87_TEST, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FXAM: CallX87(PS5LIFT_X87_EXAMINE, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FCHS: CallX87(PS5LIFT_X87_CHANGE_SIGN, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FABS: CallX87(PS5LIFT_X87_ABSOLUTE, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FSQRT: CallX87(PS5LIFT_X87_SQUARE_ROOT, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FRNDINT: CallX87(PS5LIFT_X87_ROUND, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FSCALE: CallX87(PS5LIFT_X87_SCALE, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FPREM: CallX87(PS5LIFT_X87_PARTIAL_REMAINDER, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FPREM1: CallX87(PS5LIFT_X87_IEEE_REMAINDER, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FXCH: CallX87(PS5LIFT_X87_EXCHANGE, 0, nullptr, X87Index(1)); return;
    case ZYDIS_MNEMONIC_FFREE: CallX87(PS5LIFT_X87_FREE, 0, nullptr, X87Index(0)); return;
    case ZYDIS_MNEMONIC_FINCSTP: CallX87(PS5LIFT_X87_INCREMENT_TOP, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FDECSTP: CallX87(PS5LIFT_X87_DECREMENT_TOP, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FNINIT: CallX87(PS5LIFT_X87_INITIALIZE, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FNCLEX: CallX87(PS5LIFT_X87_CLEAR_EXCEPTIONS, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FWAIT: CallX87(PS5LIFT_X87_WAIT, 0, nullptr, 0); return;
    case ZYDIS_MNEMONIC_FNSTSW: return X87StoreStatus();
    case ZYDIS_MNEMONIC_FNSTCW: CallX87(PS5LIFT_X87_STORE_CONTROL, 0, X87Address(), 0); return;
    case ZYDIS_MNEMONIC_FLDCW: CallX87(PS5LIFT_X87_LOAD_CONTROL, 0, X87Address(), 0); return;
    case ZYDIS_MNEMONIC_FNSTENV: return X87Environment(true);
    case ZYDIS_MNEMONIC_FLDENV: return X87Environment(false);
    case ZYDIS_MNEMONIC_FXSAVE: case ZYDIS_MNEMONIC_FXSAVE64: return X87SaveState();
    case ZYDIS_MNEMONIC_FXRSTOR: case ZYDIS_MNEMONIC_FXRSTOR64: return X87RestoreState();
    case ZYDIS_MNEMONIC_FLD1: CallX87(PS5LIFT_X87_LOAD_CONSTANT, 0, nullptr, PS5LIFT_X87_CONSTANT_ONE); return;
    case ZYDIS_MNEMONIC_FLDL2T: CallX87(PS5LIFT_X87_LOAD_CONSTANT, 0, nullptr, PS5LIFT_X87_CONSTANT_LOG2_10); return;
    case ZYDIS_MNEMONIC_FLDL2E: CallX87(PS5LIFT_X87_LOAD_CONSTANT, 0, nullptr, PS5LIFT_X87_CONSTANT_LOG2_E); return;
    case ZYDIS_MNEMONIC_FLDPI: CallX87(PS5LIFT_X87_LOAD_CONSTANT, 0, nullptr, PS5LIFT_X87_CONSTANT_PI); return;
    case ZYDIS_MNEMONIC_FLDLG2: CallX87(PS5LIFT_X87_LOAD_CONSTANT, 0, nullptr, PS5LIFT_X87_CONSTANT_LOG10_2); return;
    case ZYDIS_MNEMONIC_FLDLN2: CallX87(PS5LIFT_X87_LOAD_CONSTANT, 0, nullptr, PS5LIFT_X87_CONSTANT_LN_2); return;
    case ZYDIS_MNEMONIC_FLDZ: CallX87(PS5LIFT_X87_LOAD_CONSTANT, 0, nullptr, PS5LIFT_X87_CONSTANT_ZERO); return;
    case ZYDIS_MNEMONIC_FCMOVB: case ZYDIS_MNEMONIC_FCMOVE: case ZYDIS_MNEMONIC_FCMOVBE: case ZYDIS_MNEMONIC_FCMOVU:
    case ZYDIS_MNEMONIC_FCMOVNB: case ZYDIS_MNEMONIC_FCMOVNE: case ZYDIS_MNEMONIC_FCMOVNBE: case ZYDIS_MNEMONIC_FCMOVNU:
        return X87ConditionalMove(mnemonic);
    default:
        throw UnsupportedInstruction("");
    }
#undef PS5LIFT_BOTH
#undef PS5LIFT_CONDITION
#undef PS5LIFT_FLOAT4
#undef PS5LIFT_FMA
}

}
