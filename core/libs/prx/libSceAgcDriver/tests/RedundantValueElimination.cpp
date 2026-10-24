#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include "Optimization/RedundantValueEliminator.hpp"
#include "Recompiler.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <initializer_list>
#include <set>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Builder {
    IrProgram program;
    IrBlock* block = nullptr;

    Builder() {
        block = &newBlock();
        program.SetEntryBlock(*block);
    }

    IrBlock& newBlock() {
        auto& created = program.CreateBlock();
        program.BlockOrder().push_back(&created);
        return created;
    }

    IrValue& constant(std::uint32_t value) {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::U32);
        created.SetImmediateU32(value);
        return created;
    }

    IrValue& floatConstant(std::uint32_t bits) {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::F32);
        created.SetImmediateU32(bits);
        return created;
    }

    IrValue& truth() {
        auto& created = program.CreateValue(IrOpcode::Void, IrType::Bool);
        created.SetImmediateBool(true);
        return created;
    }

    IrValue& emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments, std::uint64_t flags = 0u) {
        auto& created = program.CreateValue(opcode, type, flags);
        for (auto* argument : arguments) created.AddArgument(argument);
        block->AppendInstruction(&created);
        return created;
    }

    IrValue& unknown() {
        return emit(IrOpcode::LaneId, IrType::U32, {});
    }

    IrValue& flush(IrValue& bits) {
        auto& exponent = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&bits, &constant(0x7f800000u)});
        auto& denormal = emit(IrOpcode::IEqual32, IrType::Bool, {&exponent, &constant(0u)});
        auto& sign = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&bits, &constant(0x80000000u)});
        return emit(IrOpcode::SelectU32, IrType::U32, {&denormal, &sign, &bits});
    }

    void keep(IrValue& value) {
        static_cast<void>(emit(IrOpcode::ReferenceU32, IrType::Void, {&value}));
    }

    std::uint32_t eliminate() {
        return RedundantValueEliminator{}.Eliminate(program).replacedValues;
    }
};

bool expect(const char* name, bool condition) {
    if (!condition) std::fprintf(stderr, "%s\n", name);
    return condition;
}

bool run(const char* name, const std::function<bool()>& test) {
    try {
        return expect(name, test());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", name, error.what());
        return false;
    }
}

bool AllUsed(const std::vector<IrValue*>& values) {
    for (const auto* value : values) {
        if (!value->HasUses()) return false;
    }
    return true;
}

bool PureSpirv(std::uint32_t opcode) {
    switch (opcode) {
        case spv::OpBitcast:
        case spv::OpUConvert:
        case spv::OpSConvert:
        case spv::OpFConvert:
        case spv::OpConvertFToU:
        case spv::OpConvertFToS:
        case spv::OpConvertUToF:
        case spv::OpConvertSToF:
        case spv::OpCompositeConstruct:
        case spv::OpCompositeExtract:
        case spv::OpSNegate:
        case spv::OpFNegate:
        case spv::OpIAdd:
        case spv::OpISub:
        case spv::OpIMul:
        case spv::OpFAdd:
        case spv::OpFSub:
        case spv::OpFMul:
        case spv::OpShiftLeftLogical:
        case spv::OpShiftRightLogical:
        case spv::OpShiftRightArithmetic:
        case spv::OpBitwiseAnd:
        case spv::OpBitwiseOr:
        case spv::OpBitwiseXor:
        case spv::OpNot:
        case spv::OpBitFieldInsert:
        case spv::OpBitFieldUExtract:
        case spv::OpBitFieldSExtract:
        case spv::OpBitReverse:
        case spv::OpBitCount:
        case spv::OpSelect:
        case spv::OpIEqual:
        case spv::OpINotEqual:
        case spv::OpULessThan:
        case spv::OpULessThanEqual:
        case spv::OpUGreaterThan:
        case spv::OpUGreaterThanEqual:
        case spv::OpSLessThan:
        case spv::OpSLessThanEqual:
        case spv::OpSGreaterThan:
        case spv::OpSGreaterThanEqual:
        case spv::OpFOrdEqual:
        case spv::OpFOrdNotEqual:
        case spv::OpFOrdLessThan:
        case spv::OpFOrdGreaterThan:
        case spv::OpFOrdLessThanEqual:
        case spv::OpFOrdGreaterThanEqual:
        case spv::OpFUnordEqual:
        case spv::OpFUnordNotEqual:
        case spv::OpFUnordLessThan:
        case spv::OpFUnordGreaterThan:
        case spv::OpFUnordLessThanEqual:
        case spv::OpFUnordGreaterThanEqual:
        case spv::OpIsNan:
        case spv::OpLogicalAnd:
        case spv::OpLogicalOr:
        case spv::OpLogicalNot:
        case spv::OpLogicalEqual:
        case spv::OpLogicalNotEqual:
            return true;
        default:
            return false;
    }
}

std::size_t RepeatedInBlock(std::span<const std::uint32_t> words) {
    if (words.size() < 5 || words[0] != spv::MagicNumber) throw std::runtime_error("the recompiler returned no SPIR-V module");
    std::size_t repeated = 0;
    std::set<std::vector<std::uint32_t>> seen;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        if (count == 0 || count > words.size() - offset) throw std::runtime_error("the recompiler returned a malformed SPIR-V instruction");
        const auto opcode = words[offset] & spv::OpCodeMask;
        if (opcode == spv::OpLabel) seen.clear();
        if (PureSpirv(opcode) && count >= 3) {
            std::vector<std::uint32_t> key{opcode, words[offset + 1]};
            key.insert(key.end(), words.begin() + static_cast<std::ptrdiff_t>(offset + 3), words.begin() + static_cast<std::ptrdiff_t>(offset + count));
            if (!seen.insert(std::move(key)).second) ++repeated;
        }
        offset += count;
    }
    return repeated;
}

std::vector<std::uint32_t> Recompiled(std::span<const std::uint32_t> code) {
    const std::array<std::uint32_t, 4> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
    const std::array<std::uint32_t, 2> capabilities{1u, 61u};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 32u;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    const auto& words = Recompile(request).spirv.Words();
    return {words.begin(), words.end()};
}

}

int main() {
    bool passed = true;

    passed &= run("a value recomputed from the same operands in one block is replaced by the first", [] {
        Builder b;
        auto& source = b.unknown();
        auto& first = b.flush(source);
        auto& second = b.flush(source);
        b.keep(first);
        b.keep(second);
        return b.eliminate() == 4u && !second.HasUses() && first.UseCount() == 2u;
    });

    passed &= run("values that differ in an immediate, an operand or the opcode stay", [] {
        Builder b;
        auto& source = b.unknown();
        auto& other = b.unknown();
        auto& masked = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&source, &b.constant(0x7f800000u)});
        auto& otherMask = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&source, &b.constant(0x80000000u)});
        auto& otherSource = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&other, &b.constant(0x7f800000u)});
        auto& otherOpcode = b.emit(IrOpcode::BitwiseOr32, IrType::U32, {&source, &b.constant(0x7f800000u)});
        b.keep(masked);
        b.keep(otherMask);
        b.keep(otherSource);
        b.keep(otherOpcode);
        return b.eliminate() == 0u && AllUsed({&masked, &otherMask, &otherSource, &otherOpcode});
    });

    passed &= run("values that differ only in flags, the result type or the type of an immediate stay", [] {
        Builder b;
        auto& source = b.unknown();
        auto& plain = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&source, &b.constant(0x3f800000u)});
        auto& flagged = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&source, &b.constant(0x3f800000u)}, 1u);
        auto& otherType = b.emit(IrOpcode::BitwiseAnd32, IrType::F32, {&source, &b.constant(0x3f800000u)});
        auto& floatImmediate = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&source, &b.floatConstant(0x3f800000u)});
        b.keep(plain);
        b.keep(flagged);
        b.keep(otherType);
        b.keep(floatImmediate);
        return b.eliminate() == 0u && AllUsed({&plain, &flagged, &otherType, &floatImmediate});
    });

    passed &= run("four-operand expressions retain distinct trailing immediates across a large block", [] {
        Builder b;
        auto& source = b.unknown();
        std::vector<IrValue*> reads;
        std::vector<IrValue*> duplicates;
        std::vector<IrValue*> different;
        for (std::uint32_t index = 0; index < 256u; ++index) {
            const auto composite = [&](std::uint32_t last) -> IrValue& {
                return b.emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {&source, &b.constant(index), &b.constant(7u), &b.constant(last)});
            };
            auto& first = composite(0u);
            auto& second = composite(0u);
            auto& changed = composite(1u);
            auto& firstRead = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&first, &b.constant(3u)});
            auto& secondRead = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&second, &b.constant(3u)});
            auto& changedRead = b.emit(IrOpcode::CompositeExtractU32x4, IrType::U32, {&changed, &b.constant(3u)});
            for (auto* read : {&firstRead, &secondRead, &changedRead}) b.keep(*read);
            reads.push_back(&firstRead);
            duplicates.push_back(&second);
            duplicates.push_back(&secondRead);
            different.push_back(&changedRead);
        }
        if (b.eliminate() != 512u || !AllUsed(different)) return false;
        for (const auto* duplicate : duplicates) if (duplicate->HasUses()) return false;
        for (const auto* read : reads) if (read->UseCount() != 2u) return false;
        return true;
    });

    passed &= run("float immediates compare by bits: +0 and -0 and two NaN payloads stay apart, one NaN merges", [] {
        Builder b;
        auto& x = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&b.unknown()});
        auto& plusZero = b.emit(IrOpcode::FPAdd32, IrType::F32, {&x, &b.floatConstant(0x00000000u)});
        auto& minusZero = b.emit(IrOpcode::FPAdd32, IrType::F32, {&x, &b.floatConstant(0x80000000u)});
        auto& quiet = b.emit(IrOpcode::FPMax32, IrType::F32, {&x, &b.floatConstant(0x7fc00000u)});
        auto& sameQuiet = b.emit(IrOpcode::FPMax32, IrType::F32, {&x, &b.floatConstant(0x7fc00000u)});
        auto& otherPayload = b.emit(IrOpcode::FPMax32, IrType::F32, {&x, &b.floatConstant(0x7fc00001u)});
        for (auto* value : {&plusZero, &minusZero, &quiet, &sameQuiet, &otherPayload}) b.keep(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {value}));
        return b.eliminate() == 2u && !sameQuiet.HasUses() && AllUsed({&quiet, &plusZero, &minusZero, &otherPayload});
    });

    passed &= run("a value recomputed in a dominated block is replaced, one in a sibling block stays", [] {
        Builder b;
        IrBlock& head = *b.block;
        IrBlock& then = b.newBlock();
        IrBlock& otherwise = b.newBlock();
        IrBlock& merge = b.newBlock();
        head.AddBranch(&then);
        head.AddBranch(&otherwise);
        then.AddBranch(&merge);
        otherwise.AddBranch(&merge);
        auto& source = b.unknown();
        auto& other = b.unknown();
        auto& first = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&source});
        b.keep(first);
        b.block = &then;
        auto& dominated = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&source});
        auto& thenOnly = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&other});
        b.keep(dominated);
        b.keep(thenOnly);
        b.block = &otherwise;
        auto& sibling = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&other});
        b.keep(sibling);
        b.block = &merge;
        auto& merged = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&other});
        b.keep(merged);
        return b.eliminate() == 1u && !dominated.HasUses() && first.UseCount() == 2u && AllUsed({&thenOnly, &sibling, &merged});
    });

    passed &= run("GetSrtResource and ReadConst with the same operands merge in a dominated block, while another slot or a sibling block stays", [] {
        Builder b;
        IrBlock& head = *b.block;
        IrBlock& then = b.newBlock();
        IrBlock& otherwise = b.newBlock();
        IrBlock& merge = b.newBlock();
        head.AddBranch(&then);
        head.AddBranch(&otherwise);
        then.AddBranch(&merge);
        otherwise.AddBranch(&merge);
        auto& srtHead = b.emit(IrOpcode::GetSrtResource, IrType::SrtResource, {&b.constant(0u)});
        auto& readHead = b.emit(IrOpcode::ReadConst, IrType::U32, {&srtHead, &b.constant(2u)});
        b.keep(readHead);
        b.block = &then;
        auto& srtDominated = b.emit(IrOpcode::GetSrtResource, IrType::SrtResource, {&b.constant(0u)});
        auto& readDominated = b.emit(IrOpcode::ReadConst, IrType::U32, {&srtDominated, &b.constant(2u)});
        auto& readOtherSlot = b.emit(IrOpcode::ReadConst, IrType::U32, {&srtDominated, &b.constant(3u)});
        auto& srtThenOnly = b.emit(IrOpcode::GetSrtResource, IrType::SrtResource, {&b.constant(1u)});
        auto& readThenOnly = b.emit(IrOpcode::ReadConst, IrType::U32, {&srtThenOnly, &b.constant(4u)});
        b.keep(readDominated);
        b.keep(readOtherSlot);
        b.keep(readThenOnly);
        b.block = &otherwise;
        auto& srtSibling = b.emit(IrOpcode::GetSrtResource, IrType::SrtResource, {&b.constant(1u)});
        auto& readSibling = b.emit(IrOpcode::ReadConst, IrType::U32, {&srtSibling, &b.constant(4u)});
        b.keep(readSibling);
        b.block = &merge;
        auto& srtMerge = b.emit(IrOpcode::GetSrtResource, IrType::SrtResource, {&b.constant(0u)});
        auto& readMerge = b.emit(IrOpcode::ReadConst, IrType::U32, {&srtMerge, &b.constant(2u)});
        b.keep(readMerge);
        return b.eliminate() == 4u && !srtDominated.HasUses() && !readDominated.HasUses() && !srtMerge.HasUses() && !readMerge.HasUses() && srtHead.UseCount() == 4u && readHead.UseCount() == 3u && AllUsed({&readOtherSlot, &srtThenOnly, &readThenOnly, &srtSibling, &readSibling});
    });

    passed &= run("two ReadConst instructions of the same slot separated by a store to the table address stay", [] {
        Builder b;
        auto& tableAddress = b.emit(IrOpcode::GetAddressResource, IrType::AddressResource, {&b.constant(0x1000u), &b.constant(0u)});
        auto& tableRead = b.emit(IrOpcode::LoadAddressU32, IrType::U32, {&tableAddress, &b.constant(8u), &b.constant(0u), &b.truth()});
        b.program.Resources().srtReads.push_back({&tableRead, 0u});
        auto& srt = b.emit(IrOpcode::GetSrtResource, IrType::SrtResource, {});
        auto& first = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        static_cast<void>(b.emit(IrOpcode::StoreAddressU32, IrType::Void, {&tableAddress, &b.constant(8u), &b.constant(0u), &b.constant(7u), &b.truth()}));
        auto& second = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(first);
        b.keep(second);
        return b.eliminate() == 0u && AllUsed({&first, &second});
    });

    passed &= run("a store on one branch keeps ReadConst in the merge block while the clean sibling branch reuses the head read", [] {
        Builder b;
        IrBlock& head = *b.block;
        IrBlock& then = b.newBlock();
        IrBlock& otherwise = b.newBlock();
        IrBlock& merge = b.newBlock();
        head.AddBranch(&then);
        head.AddBranch(&otherwise);
        then.AddBranch(&merge);
        otherwise.AddBranch(&merge);
        auto& tableAddress = b.emit(IrOpcode::GetAddressResource, IrType::AddressResource, {&b.constant(0x1000u), &b.constant(0u)});
        auto& tableRead = b.emit(IrOpcode::LoadAddressU32, IrType::U32, {&tableAddress, &b.constant(8u), &b.constant(0u), &b.truth()});
        b.program.Resources().srtReads.push_back({&tableRead, 0u});
        auto& srt = b.emit(IrOpcode::GetSrtResource, IrType::SrtResource, {});
        auto& readHead = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(readHead);
        b.block = &then;
        static_cast<void>(b.emit(IrOpcode::StoreAddressU32, IrType::Void, {&tableAddress, &b.constant(8u), &b.constant(0u), &b.constant(7u), &b.truth()}));
        auto& readThenAfterStore = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(readThenAfterStore);
        b.block = &otherwise;
        auto& readOtherwise = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(readOtherwise);
        b.block = &merge;
        auto& readMerge = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(readMerge);
        return b.eliminate() == 1u && !readOtherwise.HasUses() && readHead.UseCount() == 2u && AllUsed({&readHead, &readThenAfterStore, &readMerge});
    });

    passed &= run("a store on a loop backedge prevents reusing a pre-loop ReadConst in the loop header", [] {
        Builder b;
        IrBlock& header = b.newBlock();
        IrBlock& body = b.newBlock();
        IrBlock& exit = b.newBlock();
        b.block->AddBranch(&header);
        header.AddBranch(&body);
        header.AddBranch(&exit);
        body.AddBranch(&header);
        auto& tableAddress = b.emit(IrOpcode::GetAddressResource, IrType::AddressResource, {&b.constant(0x1000u), &b.constant(0u)});
        auto& tableRead = b.emit(IrOpcode::LoadAddressU32, IrType::U32, {&tableAddress, &b.constant(8u), &b.constant(0u), &b.truth()});
        b.program.Resources().srtReads.push_back({&tableRead, 0u});
        auto& srt = b.emit(IrOpcode::GetSrtResource, IrType::SrtResource, {});
        auto& beforeLoop = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(beforeLoop);
        b.block = &header;
        auto& inHeader = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(inHeader);
        b.block = &body;
        auto& inBodyBeforeStore = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        static_cast<void>(b.emit(IrOpcode::StoreAddressU32, IrType::Void, {&tableAddress, &b.constant(8u), &b.constant(0u), &b.constant(7u), &b.truth()}));
        auto& inBodyAfterStore = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(inBodyBeforeStore);
        b.keep(inBodyAfterStore);
        b.block = &exit;
        auto& afterLoop = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(afterLoop);
        return b.eliminate() == 2u && !inBodyBeforeStore.HasUses() && !afterLoop.HasUses() && beforeLoop.UseCount() == 1u && inHeader.UseCount() == 3u && AllUsed({&beforeLoop, &inHeader, &inBodyAfterStore});
    });

    passed &= run("a store in an inner loop propagates to the outer loop header across fixed-point iterations", [] {
        Builder b;
        IrBlock& outerHeader = b.newBlock();
        IrBlock& innerHeader = b.newBlock();
        IrBlock& innerBody = b.newBlock();
        IrBlock& outerLatch = b.newBlock();
        IrBlock& exit = b.newBlock();
        b.block->AddBranch(&outerHeader);
        outerHeader.AddBranch(&innerHeader);
        outerHeader.AddBranch(&exit);
        innerHeader.AddBranch(&innerBody);
        innerHeader.AddBranch(&outerLatch);
        innerBody.AddBranch(&innerHeader);
        outerLatch.AddBranch(&outerHeader);
        auto& tableAddress = b.emit(IrOpcode::GetAddressResource, IrType::AddressResource, {&b.constant(0x1000u), &b.constant(0u)});
        auto& tableRead = b.emit(IrOpcode::LoadAddressU32, IrType::U32, {&tableAddress, &b.constant(8u), &b.constant(0u), &b.truth()});
        b.program.Resources().srtReads.push_back({&tableRead, 0u});
        auto& srt = b.emit(IrOpcode::GetSrtResource, IrType::SrtResource, {});
        auto& beforeOuter = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(beforeOuter);
        b.block = &outerHeader;
        auto& inOuterHeader = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(inOuterHeader);
        b.block = &innerHeader;
        auto& inInnerHeader = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(inInnerHeader);
        b.block = &innerBody;
        static_cast<void>(b.emit(IrOpcode::StoreAddressU32, IrType::Void, {&tableAddress, &b.constant(8u), &b.constant(0u), &b.constant(7u), &b.truth()}));
        b.block = &outerLatch;
        auto& inOuterLatch = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(inOuterLatch);
        b.block = &exit;
        auto& inExit = b.emit(IrOpcode::ReadConst, IrType::U32, {&srt, &b.constant(0u)});
        b.keep(inExit);
        return b.eliminate() == 2u && !inOuterLatch.HasUses() && !inExit.HasUses() && beforeOuter.UseCount() == 1u && inOuterHeader.UseCount() == 2u && inInnerHeader.UseCount() == 2u && AllUsed({&beforeOuter, &inOuterHeader, &inInnerHeader});
    });

    passed &= run("a loop body reuses a value of its header, the loop exit does not reuse a value of the body", [] {
        Builder b;
        IrBlock& header = b.newBlock();
        IrBlock& body = b.newBlock();
        IrBlock& exit = b.newBlock();
        b.block->AddBranch(&header);
        header.AddBranch(&body);
        header.AddBranch(&exit);
        body.AddBranch(&header);
        auto& source = b.unknown();
        auto& other = b.unknown();
        b.block = &header;
        auto& inHeader = b.emit(IrOpcode::IAdd32, IrType::U32, {&source, &b.constant(1u)});
        b.keep(inHeader);
        b.block = &body;
        auto& inBody = b.emit(IrOpcode::IAdd32, IrType::U32, {&source, &b.constant(1u)});
        auto& bodyOnly = b.emit(IrOpcode::IAdd32, IrType::U32, {&other, &b.constant(1u)});
        b.keep(inBody);
        b.keep(bodyOnly);
        b.block = &exit;
        auto& afterLoop = b.emit(IrOpcode::IAdd32, IrType::U32, {&other, &b.constant(1u)});
        b.keep(afterLoop);
        return b.eliminate() == 1u && !inBody.HasUses() && inHeader.UseCount() == 2u && AllUsed({&bodyOnly, &afterLoop});
    });

    passed &= run("a branch condition that is replaced names the value that stays", [] {
        Builder b;
        auto& source = b.unknown();
        auto& first = b.emit(IrOpcode::IEqual32, IrType::Bool, {&source, &b.constant(0u)});
        auto& second = b.emit(IrOpcode::IEqual32, IrType::Bool, {&source, &b.constant(0u)});
        b.keep(b.emit(IrOpcode::SelectU32, IrType::U32, {&first, &source, &b.constant(1u)}));
        b.keep(b.emit(IrOpcode::SelectU32, IrType::U32, {&second, &b.constant(1u), &source}));
        BlockInfo info;
        info.condition = &second;
        b.program.Metadata().blockInfo.push_back(info);
        return b.eliminate() == 1u && b.program.Metadata().blockInfo.front().condition == &first;
    });

    passed &= run("descriptor dwords name the surviving values after redundancy and dead code elimination", [] {
        Builder b;
        auto& input = b.unknown();
        DescriptorSource descriptor;
        descriptor.dwordCount = 8u;
        std::array<IrValue*, 8> kept{};
        std::array<IrValue*, 8> removed{};
        for (std::uint32_t index = 0; index < 8u; ++index) {
            auto& first = b.emit(IrOpcode::IAdd32, IrType::U32, {&input, &b.constant(index + 1u)});
            auto& second = b.emit(IrOpcode::IAdd32, IrType::U32, {&input, &b.constant(index + 1u)});
            b.keep(first);
            b.keep(second);
            descriptor.dwords[index] = &second;
            kept[index] = &first;
            removed[index] = &second;
        }
        b.program.Resources().descriptorSources.push_back(descriptor);
        b.program.Resources().descriptorSources.emplace_back();
        if (b.eliminate() != 8u) return false;
        DeadCodeEliminator{}.Eliminate(b.program);
        const auto& descriptors = b.program.Resources().descriptorSources;
        for (std::size_t index = 0; index < kept.size(); ++index) {
            if (descriptors[0].dwords[index] != kept[index] || kept[index]->Parent() != b.block || removed[index]->Parent() != nullptr || descriptors[1].dwords[index] != nullptr) return false;
        }
        return true;
    });

    passed &= run("a selected table column keeps its surviving runtime offset", [] {
        Builder b;
        auto& input = b.unknown();
        auto& first = b.emit(IrOpcode::IMul32, IrType::U32, {&input, &b.constant(48u)});
        auto& second = b.emit(IrOpcode::IMul32, IrType::U32, {&input, &b.constant(48u)});
        b.keep(first);
        b.keep(second);
        DescriptorSource selection;
        selection.dwordCount = 1u;
        selection.dwords[0] = &second;
        DescriptorSource image;
        image.tableColumn.emplace();
        image.tableColumn->selected = true;
        image.tableColumn->selection = 0u;
        b.program.Resources().descriptorSources = {selection, image};
        if (b.eliminate() != 1u) return false;
        DeadCodeEliminator{}.Eliminate(b.program);
        const auto& descriptors = b.program.Resources().descriptorSources;
        const auto selected = descriptors[1].tableColumn->selection;
        return descriptors[selected].dwords[0] == &first && first.Parent() == b.block && second.Parent() == nullptr;
    });

    passed &= run("lane reads are not merged", [] {
        Builder b;
        auto& first = b.unknown();
        auto& second = b.unknown();
        b.keep(first);
        b.keep(second);
        return b.eliminate() == 0u && second.HasUses();
    });

    passed &= run("cross-lane reads with the same operands are not merged", [] {
        Builder b;
        auto& source = b.unknown();
        std::vector<IrValue*> values;
        for (int copy = 0; copy < 2; ++copy) {
            values.push_back(&b.emit(IrOpcode::ReadFirstLane, IrType::U32, {&source, &b.truth()}));
            values.push_back(&b.emit(IrOpcode::ReadLane, IrType::U32, {&source, &b.constant(3u)}));
            values.push_back(&b.emit(IrOpcode::DppMoveU32, IrType::U32, {&source, &b.truth()}));
            values.push_back(&b.emit(IrOpcode::SwizzleU32, IrType::U32, {&source, &b.constant(0x1fu), &b.truth()}));
        }
        for (auto* value : values) b.keep(*value);
        return b.eliminate() == 0u && AllUsed(values);
    });

    passed &= run("loads of one address are not merged, with no store, a store or a barrier between them", [] {
        Builder b;
        auto& address = b.unknown();
        std::vector<IrValue*> loads;
        loads.push_back(&b.emit(IrOpcode::LoadShared, IrType::U32, {&address}));
        loads.push_back(&b.emit(IrOpcode::LoadShared, IrType::U32, {&address}));
        static_cast<void>(b.emit(IrOpcode::StoreShared, IrType::Void, {&address, &b.constant(7u)}));
        loads.push_back(&b.emit(IrOpcode::LoadShared, IrType::U32, {&address}));
        static_cast<void>(b.emit(IrOpcode::Barrier, IrType::Void, {}));
        loads.push_back(&b.emit(IrOpcode::LoadShared, IrType::U32, {&address}));
        for (auto* load : loads) b.keep(*load);
        return b.eliminate() == 0u && AllUsed(loads);
    });

    passed &= run("atomics with the same operands are not merged", [] {
        Builder b;
        auto& address = b.unknown();
        auto& first = b.emit(IrOpcode::SharedAtomicIAdd32, IrType::U32, {&address, &b.constant(1u), &b.truth()});
        auto& second = b.emit(IrOpcode::SharedAtomicIAdd32, IrType::U32, {&address, &b.constant(1u), &b.truth()});
        b.keep(first);
        b.keep(second);
        return b.eliminate() == 0u && AllUsed({&first, &second});
    });

    passed &= run("phis with the same incoming values are not merged", [] {
        Builder b;
        IrBlock& left = b.newBlock();
        IrBlock& right = b.newBlock();
        IrBlock& merge = b.newBlock();
        b.block->AddBranch(&left);
        b.block->AddBranch(&right);
        left.AddBranch(&merge);
        right.AddBranch(&merge);
        auto& one = b.unknown();
        auto& two = b.unknown();
        b.block = &merge;
        auto& first = b.emit(IrOpcode::Phi, IrType::U32, {});
        auto& second = b.emit(IrOpcode::Phi, IrType::U32, {});
        for (auto* phi : {&first, &second}) {
            phi->AddPhiOperand(&left, &one);
            phi->AddPhiOperand(&right, &two);
        }
        b.keep(first);
        b.keep(second);
        return b.eliminate() == 0u && AllUsed({&first, &second});
    });

    passed &= run("float operations with the same operands in the same order are merged", [] {
        Builder b;
        auto& lhs = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&b.unknown()});
        auto& rhs = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&b.unknown()});
        auto& first = b.emit(IrOpcode::FPMul32, IrType::F32, {&lhs, &rhs});
        auto& second = b.emit(IrOpcode::FPMul32, IrType::F32, {&lhs, &rhs});
        auto& swapped = b.emit(IrOpcode::FPMul32, IrType::F32, {&rhs, &lhs});
        b.keep(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&first}));
        b.keep(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&second}));
        b.keep(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&swapped}));
        return b.eliminate() == 2u && !second.HasUses() && swapped.HasUses();
    });

    passed &= run("a block that the entry does not reach throws", [] {
        Builder b;
        static_cast<void>(b.newBlock());
        try {
            static_cast<void>(b.eliminate());
        } catch (const std::runtime_error&) {
            return true;
        }
        return false;
    });

    passed &= run("a wave64 shader that reads one register in several float operations computes nothing twice in a block", [] {
        const std::array<std::uint32_t, 7> code{
            0x10020100u,
            0x10040300u,
            0x10060500u,
            0x06020503u,
            0xe0700000u, 0x80000100u,
            0xbf810000u,
        };
        const auto module = Recompiled(code);
        const auto repeated = RepeatedInBlock(module);
        if (repeated != 0u) std::fprintf(stderr, "  %zu pure instructions repeat an earlier one in their block of the %zu-word module\n", repeated, module.size());
        return repeated == 0u;
    });

    return passed ? 0 : 1;
}
