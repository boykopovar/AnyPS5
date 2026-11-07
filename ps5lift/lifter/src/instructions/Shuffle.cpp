#include "../FunctionLifter.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace Ps5Lift::Lifting {

void FunctionLifter::VectorMove() {
    if (_instruction->Info.operand_count_visible != 2) throw UnsupportedInstruction("merge form");
    const auto& destination = Operand(0);
    const bool toVector = destination.type == ZYDIS_OPERAND_TYPE_REGISTER && VectorIndex(destination.reg.value) >= 0;
    auto* value = Read(1);
    const auto source = value->getType()->getIntegerBitWidth();
    if (!toVector) {
        Write(0, _builder.CreateZExtOrTrunc(value, Int(Bits(0))));
        return;
    }
    const auto target = source > 128 ? 256u : 128u;
    const auto mnemonic = _instruction->Info.mnemonic;
    const bool scalarMerge = (mnemonic == ZYDIS_MNEMONIC_MOVSS || mnemonic == ZYDIS_MNEMONIC_MOVSD) && Operand(1).type == ZYDIS_OPERAND_TYPE_REGISTER;
    if (source < target && scalarMerge) {
        auto* old = ReadVector(0, 128);
        auto* mask = llvm::ConstantInt::get(Context(), llvm::APInt::getHighBitsSet(128, 128 - source));
        value = _builder.CreateOr(_builder.CreateAnd(old, mask), _builder.CreateZExt(value, Int(128)));
    } else if (source < target) {
        value = _builder.CreateZExt(value, Int(target));
    }
    WriteRegister(destination.reg.value, value);
}

void FunctionLifter::Broadcast(const unsigned elementBits) {
    const auto& destination = Operand(0);
    const auto width = ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, destination.reg.value);
    auto* element = ReadAs(1, elementBits);
    auto* vectorType = llvm::FixedVectorType::get(Int(elementBits), width / elementBits);
    auto* splat = _builder.CreateVectorSplat(vectorType->getNumElements(), element);
    WriteRegister(destination.reg.value, _builder.CreateBitCast(splat, Int(width)));
}

void FunctionLifter::ExtractElement(const unsigned element) {
    auto* source = AsVector(ReadRegister(Operand(1).reg.value), element);
    const auto count = llvm::cast<llvm::FixedVectorType>(source->getType())->getNumElements();
    auto* value = _builder.CreateExtractElement(source, _builder.getInt64(Immediate(2) & (std::min(count, 128 / element) - 1)));
    Write(0, _builder.CreateZExtOrTrunc(value, Int(Bits(0))));
}

void FunctionLifter::Extract128() {
    auto* source = AsVector(ReadRegister(Operand(1).reg.value), 128);
    Write(0, _builder.CreateExtractElement(source, _builder.getInt64(Immediate(2) & 1)));
}

void FunctionLifter::ShuffleInLanes(const unsigned element, const unsigned first) {
    const auto width = Bits(0);
    const unsigned laneBits = std::min(width, 128u);
    const unsigned lanes = width / laneBits;
    const unsigned count = width / element;
    const unsigned perLane = laneBits / element;
    auto* source = AsVector(ReadAs(1, width), element);
    const auto control = Immediate(2);
    std::vector<int> mask(count);
    for (unsigned lane = 0; lane < lanes; ++lane) {
        for (unsigned i = 0; i < perLane; ++i) {
            const auto base = lane * perLane;
            const bool picked = i >= first && i < first + 4;
            mask[base + i] = static_cast<int>(picked ? base + first + ((control >> (2 * (i - first))) & 3) : base + i);
        }
    }
    Write(0, AsInteger(_builder.CreateShuffleVector(source, source, mask)));
}

void FunctionLifter::PermuteQwords() {
    const auto width = Bits(0);
    const unsigned count = width / 64;
    auto* source = AsVector(ReadAs(1, width), 64);
    const auto control = Immediate(2);
    std::vector<int> mask(count);
    for (unsigned i = 0; i < count; ++i) mask[i] = static_cast<int>((control >> (2 * i)) & 3);
    Write(0, AsInteger(_builder.CreateShuffleVector(source, source, mask)));
}

void FunctionLifter::ShufflePairs() {
    const unsigned element = 32;
    const auto width = Bits(0);
    const unsigned laneBits = std::min(width, 128u);
    const unsigned lanes = width / laneBits;
    const unsigned count = width / element;
    const int base = Base(3);
    const auto [a, b, va, vb] = ReadVectorOperands(base, width, element);
    const auto control = Immediate(base + 2);
    std::vector<int> mask(count);
    for (unsigned lane = 0; lane < lanes; ++lane) {
        for (unsigned i = 0; i < 4; ++i) {
            const auto selected = static_cast<int>(lane * 4 + ((control >> (2 * i)) & 3));
            mask[lane * 4 + i] = i < 2 ? selected : static_cast<int>(count) + selected;
        }
    }
    Write(0, AsInteger(_builder.CreateShuffleVector(va, vb, mask)));
}

void FunctionLifter::AlignRight() {
    const unsigned element = 8;
    const auto width = Bits(0);
    const unsigned laneBits = std::min(width, 128u);
    const unsigned lanes = width / laneBits;
    const unsigned count = width / element;
    const int base = Base(3);
    const auto [a, b, va, vb] = ReadVectorOperands(base, width, element);
    const auto amount = static_cast<int>(Immediate(base + 2) & 0xff);
    const auto laneBytes = static_cast<int>(laneBits / 8);
    std::vector<int> mask(count);
    for (unsigned lane = 0; lane < lanes; ++lane) {
        const int laneBase = static_cast<int>(lane) * laneBytes;
        for (int i = 0; i < laneBytes; ++i) {
            const int from = i + amount;
            mask[laneBase + i] = from < laneBytes ? static_cast<int>(count) + laneBase + from : from < 2 * laneBytes ? laneBase + from - laneBytes : -1;
        }
    }
    Write(0, AsInteger(ShuffleWithZero(va, vb, mask)));
}

void FunctionLifter::Permute128() {
    const auto width = Bits(0);
    const int base = Base(3);
    const auto [a, b, va, vb] = ReadVectorOperands(base, width, 64);
    const auto control = Immediate(base + 2);
    std::vector<int> mask(4);
    for (int half = 0; half < 2; ++half) {
        const auto selector = (control >> (4 * half)) & 0xf;
        for (int i = 0; i < 2; ++i) {
            const int source = static_cast<int>(selector & 3);
            const int from = (source & 2 ? 4 : 0) + (source & 1) * 2 + i;
            mask[half * 2 + i] = selector & 8 ? -1 : from;
        }
    }
    Write(0, AsInteger(ShuffleWithZero(va, vb, mask)));
}

void FunctionLifter::BlendImmediate(const unsigned element) {
    const auto width = Bits(0);
    const unsigned count = width / element;
    const int base = Base(3);
    const auto [a, b, va, vb] = ReadVectorOperands(base, width, element);
    const auto control = Immediate(base + 2);
    std::vector<int> mask(count);
    for (unsigned i = 0; i < count; ++i) {
        const auto bit = element == 16 ? i % 8 : i;
        mask[i] = (control >> bit) & 1 ? static_cast<int>(count + i) : static_cast<int>(i);
    }
    Write(0, AsInteger(_builder.CreateShuffleVector(va, vb, mask)));
}

void FunctionLifter::BlendVariable(const unsigned element) {
    const auto width = Bits(0);
    const int base = Base(_instruction->Info.encoding == ZYDIS_INSTRUCTION_ENCODING_VEX ? 3 : 2);
    const auto [a, b, va, vb] = ReadVectorOperands(base, width, element);
    auto* selector = _instruction->Info.encoding == ZYDIS_INSTRUCTION_ENCODING_VEX ? ReadAs(base + 2, width) : _builder.CreateTrunc(ReadRegister(ZYDIS_REGISTER_XMM0), Int(width));
    auto* negative = _builder.CreateICmpSLT(AsVector(selector, element), llvm::Constant::getNullValue(va->getType()));
    Write(0, AsInteger(_builder.CreateSelect(negative, vb, va)));
}

void FunctionLifter::InsertElement(const unsigned element) {
    const auto width = Bits(0);
    const unsigned count = width / element;
    const int base = Base(3);
    auto* a = ReadAs(base, width);
    auto* b = Read(base + 1);
    auto* va = AsVector(a, element);
    const auto index = Immediate(base + 2) & (count - 1);
    Write(0, AsInteger(_builder.CreateInsertElement(va, _builder.CreateZExtOrTrunc(b, Int(element)), _builder.getInt64(index))));
}

void FunctionLifter::Insert128() {
    const auto width = Bits(0);
    const int base = Base(3);
    auto* a = ReadAs(base, width);
    auto* b = Read(base + 1);
    Write(0, AsInteger(_builder.CreateInsertElement(AsVector(a, 128), _builder.CreateZExtOrTrunc(b, Int(128)), _builder.getInt64(Immediate(base + 2) & 1))));
}

void FunctionLifter::HalfMove(const ZydisMnemonic mnemonic) {
    const bool vex = _instruction->Info.encoding == ZYDIS_INSTRUCTION_ENCODING_VEX;
    if (Operand(0).type == ZYDIS_OPERAND_TYPE_MEMORY) {
        auto* source = AsVector(ReadRegister(Operand(1).reg.value), 64);
        const bool high = mnemonic == ZYDIS_MNEMONIC_MOVHPS || mnemonic == ZYDIS_MNEMONIC_VMOVHPS || mnemonic == ZYDIS_MNEMONIC_MOVHPD || mnemonic == ZYDIS_MNEMONIC_VMOVHPD;
        Write(0, _builder.CreateExtractElement(source, _builder.getInt64(high ? 1 : 0)));
        return;
    }
    const int base = vex ? 1 : 0;
    auto* keep = AsVector(_builder.CreateTrunc(ReadRegister(Operand(base).reg.value), Int(128)), 64);
    auto* other = Operand(base + 1).type == ZYDIS_OPERAND_TYPE_MEMORY ? Load(Address(Operand(base + 1)), 64, NeedsOrdering(Operand(base + 1), 64)) : nullptr;
    auto* otherVector = other == nullptr ? AsVector(_builder.CreateTrunc(ReadRegister(Operand(base + 1).reg.value), Int(128)), 64) : nullptr;
    llvm::Value* result = nullptr;
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_MOVHLPS: case ZYDIS_MNEMONIC_VMOVHLPS:
        result = _builder.CreateInsertElement(keep, _builder.CreateExtractElement(otherVector, _builder.getInt64(1)), _builder.getInt64(0));
        break;
    case ZYDIS_MNEMONIC_MOVLHPS: case ZYDIS_MNEMONIC_VMOVLHPS:
        result = _builder.CreateInsertElement(keep, _builder.CreateExtractElement(otherVector, _builder.getInt64(0)), _builder.getInt64(1));
        break;
    case ZYDIS_MNEMONIC_MOVHPS: case ZYDIS_MNEMONIC_VMOVHPS: case ZYDIS_MNEMONIC_MOVHPD: case ZYDIS_MNEMONIC_VMOVHPD:
        result = _builder.CreateInsertElement(keep, other, _builder.getInt64(1));
        break;
    default:
        result = _builder.CreateInsertElement(keep, other, _builder.getInt64(0));
        break;
    }
    Write(0, AsInteger(result));
}

void FunctionLifter::BitField(const bool insert) {
    const int lengthOperand = insert ? 2 : 1;
    if (Operand(0).type != ZYDIS_OPERAND_TYPE_REGISTER || (insert && Operand(1).type != ZYDIS_OPERAND_TYPE_REGISTER))
        throw std::logic_error("extrq or insertq without its registers");
    if (Operand(lengthOperand).type != ZYDIS_OPERAND_TYPE_IMMEDIATE) throw UnsupportedInstruction("extrq or insertq with its field in a register");
    const auto length = (Immediate(lengthOperand) & 63) == 0 ? 64 : Immediate(lengthOperand) & 63;
    const auto index = Immediate(lengthOperand + 1) & 63;
    if (index + length > 64) throw UnsupportedInstruction("extrq or insertq field past bit 63");
    const auto field = length == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << length) - 1;
    auto* destination = ReadVector(0, 128);
    auto* low = _builder.CreateTrunc(destination, I64());
    llvm::Value* result = nullptr;
    if (insert) {
        auto* source = _builder.CreateAnd(_builder.CreateTrunc(ReadVector(1, 128), I64()), _builder.getInt64(field));
        result = _builder.CreateOr(_builder.CreateAnd(low, _builder.getInt64(~(field << index))), _builder.CreateShl(source, index));
    } else {
        result = _builder.CreateAnd(_builder.CreateLShr(low, index), _builder.getInt64(field));
    }
    auto* high = _builder.CreateAnd(destination, llvm::ConstantInt::get(Int(128), llvm::APInt::getHighBitsSet(128, 64)));
    Write(0, _builder.CreateOr(high, _builder.CreateZExt(result, Int(128))));
}

void FunctionLifter::PermuteVariable(const unsigned element) {
    const auto width = Bits(0);
    const unsigned count = width / element;
    const unsigned perLane = 128 / element;
    auto* source = AsVector(ReadAs(1, width), element);
    auto* selectors = AsVector(ReadAs(2, width), element);
    llvm::Value* result = llvm::Constant::getNullValue(source->getType());
    for (unsigned i = 0; i < count; ++i) {
        auto* selector = _builder.CreateExtractElement(selectors, _builder.getInt64(i));
        auto* pick = element == 32 ? _builder.CreateAnd(selector, 3) : _builder.CreateAnd(_builder.CreateLShr(selector, 1), 1);
        auto* index = _builder.CreateAdd(_builder.CreateZExtOrTrunc(pick, I64()), _builder.getInt64(i / perLane * perLane));
        result = _builder.CreateInsertElement(result, _builder.CreateExtractElement(source, index), _builder.getInt64(i));
    }
    Write(0, AsInteger(result));
}

void FunctionLifter::MaskedMove(const unsigned element) {
    const bool store = Operand(0).type == ZYDIS_OPERAND_TYPE_MEMORY;
    const auto width = Bits(store ? 2 : 0);
    auto* type = llvm::FixedVectorType::get(Int(element), width / element);
    auto* zero = llvm::Constant::getNullValue(type);
    auto* mask = _builder.CreateICmpSLT(AsVector(ReadAs(1, width), element), zero);
    auto* pointer = MemoryPointer(store ? 0 : 2);
    if (store) {
        _builder.CreateMaskedStore(AsVector(ReadAs(2, width), element), pointer, llvm::Align(1), mask)->setMetadata(llvm::LLVMContext::MD_tbaa, _module.GuestTag());
        return;
    }
    auto* loaded = _builder.CreateMaskedLoad(type, pointer, llvm::Align(1), mask, zero);
    loaded->setMetadata(llvm::LLVMContext::MD_tbaa, _module.GuestTag());
    Write(0, AsInteger(loaded));
}

void FunctionLifter::DuplicateSingles(const bool high) {
    const auto width = Bits(0);
    auto* source = AsVector(ReadAs(1, width), 32);
    std::vector<int> mask(width / 32);
    for (unsigned i = 0; i < mask.size(); ++i) mask[i] = static_cast<int>((i & ~1u) + (high ? 1 : 0));
    Write(0, AsInteger(_builder.CreateShuffleVector(source, source, mask)));
}

void FunctionLifter::DuplicateDoubles() {
    const auto width = Bits(0);
    auto* source = AsVector(_builder.CreateZExtOrTrunc(ReadRegisterOrMemory(1, width == 128 ? 64 : 256), Int(width)), 64);
    std::vector<int> mask(width / 64);
    for (unsigned i = 0; i < mask.size(); ++i) mask[i] = static_cast<int>(i & ~1u);
    Write(0, AsInteger(_builder.CreateShuffleVector(source, source, mask)));
}

void FunctionLifter::ShuffleDoubles() {
    const int base = Base(3);
    const auto width = Bits(0);
    auto* a = AsVector(ReadAs(base, width), 64);
    auto* b = AsVector(ReadAs(base + 1, width), 64);
    const auto control = Immediate(base + 2);
    const unsigned count = width / 64;
    std::vector<int> mask(count);
    for (unsigned i = 0; i < count; ++i) {
        const unsigned lane = i / 2 * 2;
        const unsigned pick = lane + ((control >> i) & 1);
        mask[i] = static_cast<int>(i % 2 == 0 ? pick : count + pick);
    }
    Write(0, AsInteger(_builder.CreateShuffleVector(a, b, mask)));
}

void FunctionLifter::PermuteDoubles() {
    const auto width = Bits(0);
    auto* source = AsVector(ReadAs(1, width), 64);
    const auto control = Immediate(2);
    std::vector<int> mask(width / 64);
    for (unsigned i = 0; i < mask.size(); ++i) mask[i] = static_cast<int>(i / 2 * 2 + ((control >> i) & 1));
    Write(0, AsInteger(_builder.CreateShuffleVector(source, source, mask)));
}

void FunctionLifter::InsertSingle() {
    const int base = Base(3);
    auto* destination = AsVector(ReadVector(base, 128), 32);
    const auto control = Immediate(base + 2);
    llvm::Value* element = nullptr;
    if (Operand(base + 1).type == ZYDIS_OPERAND_TYPE_MEMORY) element = Load(Address(Operand(base + 1)), 32, NeedsOrdering(Operand(base + 1), 32));
    else element = _builder.CreateExtractElement(AsVector(_builder.CreateTrunc(ReadRegister(Operand(base + 1).reg.value), Int(128)), 32), _builder.getInt64((control >> 6) & 3));
    llvm::Value* result = _builder.CreateInsertElement(destination, element, _builder.getInt64((control >> 4) & 3));
    for (unsigned i = 0; i < 4; ++i) {
        if ((control >> i) & 1) result = _builder.CreateInsertElement(result, _builder.getInt32(0), _builder.getInt64(i));
    }
    Write(0, AsInteger(result));
}

void FunctionLifter::LeaveMmx() {
    StateStore(_builder.getInt8(0), offsetof(Ps5LiftState, X87Valid));
}

void FunctionLifter::MergeScalar(const unsigned element) {
    auto* upper = AsVector(ReadVector(1, 128), element);
    auto* low = _builder.CreateExtractElement(AsVector(ReadVector(2, 128), element), _builder.getInt64(0));
    Write(0, AsInteger(_builder.CreateInsertElement(upper, low, _builder.getInt64(0))));
}

void FunctionLifter::ZeroUpper() {
    for (int i = 0; i < VectorCount; ++i) SetVector(i, _builder.CreateZExt(_builder.CreateTrunc(Vector(i), Int(128)), Int(256)));
}

}
