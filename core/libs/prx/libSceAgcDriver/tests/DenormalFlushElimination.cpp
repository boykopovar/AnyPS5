#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/DenormalFlushEliminator.hpp"
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

    IrValue& emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        auto& created = program.CreateValue(opcode, type);
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

    IrValue& phi(IrBlock& block, std::initializer_list<std::pair<IrBlock*, IrValue*>> incoming) {
        auto& created = program.CreateValue(IrOpcode::Phi, IrType::U32);
        block.AppendInstruction(&created);
        for (const auto& [predecessor, value] : incoming) created.AddPhiOperand(predecessor, value);
        return created;
    }

    void keep(IrValue& value) {
        (void)emit(IrOpcode::ReferenceU32, IrType::Void, {&value});
    }

    std::uint32_t eliminate() {
        return DenormalFlushEliminator{}.Eliminate(program).removedFlushes;
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

std::vector<std::uint32_t> Recompiled(std::span<const std::uint32_t> code) {
    const std::array<std::uint32_t, 4> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
    const std::array<std::uint32_t, 2> capabilities{1u, 61u};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 32;
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

std::size_t ExponentMasks(std::span<const std::uint32_t> words) {
    if (words.size() < 5 || words[0] != spv::MagicNumber) throw std::runtime_error("the recompiler returned no SPIR-V module");
    std::set<std::uint32_t> exponents;
    std::size_t masks = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        if (count == 0 || count > words.size() - offset) throw std::runtime_error("the recompiler returned a malformed SPIR-V instruction");
        const auto opcode = words[offset] & spv::OpCodeMask;
        if (opcode == spv::OpConstant && count == 4 && words[offset + 3] == 0x7f800000u) exponents.insert(words[offset + 2]);
        if (opcode == spv::OpBitwiseAnd && count == 5 && (exponents.contains(words[offset + 3]) || exponents.contains(words[offset + 4]))) ++masks;
        offset += count;
    }
    return masks;
}

}

int main() {
    bool passed = true;

    passed &= run("a flush of a flushed value is removed and the first flush stays", [] {
        Builder b;
        auto& first = b.flush(b.unknown());
        auto& second = b.flush(first);
        b.keep(second);
        return b.eliminate() == 1u && !second.HasUses() && first.HasUses();
    });

    passed &= run("a flush of an unknown value stays", [] {
        Builder b;
        auto& flushed = b.flush(b.unknown());
        b.keep(flushed);
        return b.eliminate() == 0u && flushed.HasUses();
    });

    passed &= run("a sign change or absolute value of a flushed value needs no flush", [] {
        Builder b;
        auto& first = b.flush(b.unknown());
        auto& negated = b.flush(b.emit(IrOpcode::BitwiseXor32, IrType::U32, {&first, &b.constant(0x80000000u)}));
        auto& absolute = b.flush(b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&b.constant(0x7fffffffu), &first}));
        b.keep(negated);
        b.keep(absolute);
        return b.eliminate() == 2u && !negated.HasUses() && !absolute.HasUses();
    });

    passed &= run("a select of flushed values needs no flush, one with an unknown arm does", [] {
        Builder b;
        auto& condition = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.unknown(), &b.constant(16u)});
        auto& both = b.flush(b.emit(IrOpcode::SelectU32, IrType::U32, {&condition, &b.flush(b.unknown()), &b.flush(b.unknown())}));
        auto& mixed = b.flush(b.emit(IrOpcode::SelectU32, IrType::U32, {&condition, &b.flush(b.unknown()), &b.unknown()}));
        b.keep(both);
        b.keep(mixed);
        return b.eliminate() == 1u && !both.HasUses() && mixed.HasUses();
    });

    passed &= run("normal and zero constants need no flush, a denormal constant does", [] {
        Builder b;
        auto& one = b.flush(b.constant(0x3f800000u));
        auto& negativeZero = b.flush(b.constant(0x80000000u));
        auto& denormal = b.flush(b.constant(0x00000001u));
        b.keep(one);
        b.keep(negativeZero);
        b.keep(denormal);
        return b.eliminate() == 2u && !one.HasUses() && !negativeZero.HasUses() && denormal.HasUses();
    });

    passed &= run("an integer or f16 converted to f32 needs no flush", [] {
        Builder b;
        auto& integer = b.flush(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(IrOpcode::ConvertF32U32, IrType::F32, {&b.unknown()})}));
        auto& half = b.flush(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(IrOpcode::ConvertF32F16, IrType::F32, {&b.emit(IrOpcode::BitCastF16U16, IrType::F16, {&b.emit(IrOpcode::ConvertU16U32, IrType::U16, {&b.unknown()})})})}));
        b.keep(integer);
        b.keep(half);
        return b.eliminate() == 2u && !integer.HasUses() && !half.HasUses();
    });

    passed &= run("a clamped flushed value needs no flush, an arithmetic result does", [] {
        Builder b;
        auto& first = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&b.flush(b.unknown())});
        auto& clamped = b.flush(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(IrOpcode::FPSaturate32, IrType::F32, {&first})}));
        auto& product = b.flush(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(IrOpcode::FPMul32, IrType::F32, {&first, &first})}));
        b.keep(clamped);
        b.keep(product);
        return b.eliminate() == 1u && !clamped.HasUses() && product.HasUses();
    });

    passed &= run("a min, max, min3, max3 or med3 of flushed values needs no flush, one with an unknown operand does", [] {
        Builder b;
        const auto value = [&](IrValue& bits) -> IrValue& { return b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&bits}); };
        const auto flushedResult = [&](IrOpcode opcode, std::initializer_list<IrValue*> arguments) -> IrValue& {
            return b.flush(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(opcode, IrType::F32, arguments)}));
        };
        auto& x = value(b.flush(b.unknown()));
        auto& y = value(b.flush(b.unknown()));
        auto& z = value(b.flush(b.unknown()));
        auto& raw = value(b.unknown());
        IrValue* removed[] = {
            &flushedResult(IrOpcode::FPMin32, {&x, &y}),
            &flushedResult(IrOpcode::FPMax32, {&y, &x}),
            &flushedResult(IrOpcode::FPMinTri32, {&x, &y, &z}),
            &flushedResult(IrOpcode::FPMaxTri32, {&z, &y, &x}),
            &flushedResult(IrOpcode::FPMedTri32, {&x, &z, &y}),
        };
        auto& mixedPair = flushedResult(IrOpcode::FPMax32, {&x, &raw});
        auto& mixedMedian = flushedResult(IrOpcode::FPMedTri32, {&x, &y, &raw});
        for (auto* flushed : removed) b.keep(*flushed);
        b.keep(mixedPair);
        b.keep(mixedMedian);
        const bool counted = b.eliminate() == 5u;
        bool kept = false;
        for (auto* flushed : removed) kept |= flushed->HasUses();
        return counted && !kept && mixedPair.HasUses() && mixedMedian.HasUses();
    });

    passed &= run("a floor or ceil needs no flush, even of an unknown value", [] {
        Builder b;
        auto& raw = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&b.unknown()});
        auto& floored = b.flush(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(IrOpcode::FPFloor32, IrType::F32, {&raw})}));
        auto& ceiled = b.flush(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(IrOpcode::FPCeil32, IrType::F32, {&raw})}));
        b.keep(floored);
        b.keep(ceiled);
        return b.eliminate() == 2u && !floored.HasUses() && !ceiled.HasUses();
    });

    passed &= run("a max carried around a loop without a flush keeps its flush and the search ends", [] {
        Builder b;
        IrBlock& head = *b.block;
        IrBlock& body = b.newBlock();
        IrBlock& exit = b.newBlock();
        head.AddBranch(&body);
        body.AddBranch(&body);
        body.AddBranch(&exit);
        auto& initial = b.flush(b.unknown());
        b.block = &body;
        auto& carried = b.program.CreateValue(IrOpcode::Phi, IrType::U32);
        body.AppendInstruction(&carried);
        auto& step = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&b.flush(b.unknown())});
        auto& maximum = b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(IrOpcode::FPMax32, IrType::F32, {&b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&carried}), &step})});
        carried.AddPhiOperand(&head, &initial);
        carried.AddPhiOperand(&body, &maximum);
        auto& flushed = b.flush(maximum);
        b.block = &exit;
        b.keep(flushed);
        return b.eliminate() == 0u && flushed.HasUses();
    });

    passed &= run("a rounded to integral value, a square root or a reciprocal square root needs no flush", [] {
        Builder b;
        auto& unknown = b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&b.unknown()});
        std::uint32_t kept = 0u;
        for (const auto opcode : {IrOpcode::FPFloor32, IrOpcode::FPCeil32, IrOpcode::FPTrunc32, IrOpcode::FPRoundEven32, IrOpcode::FPSqrt, IrOpcode::FPRecipSqrt32}) {
            b.keep(b.flush(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(opcode, IrType::F32, {&unknown})})));
            ++kept;
        }
        auto& fraction = b.flush(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(IrOpcode::FPFract32, IrType::F32, {&unknown})}));
        auto& reciprocal = b.flush(b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&b.emit(IrOpcode::FPRecip32, IrType::F32, {&unknown})}));
        b.keep(fraction);
        b.keep(reciprocal);
        return b.eliminate() == kept && fraction.HasUses() && reciprocal.HasUses();
    });

    passed &= run("a sign bit alone or a value with an exponent bit set needs no flush, other masks do", [] {
        Builder b;
        auto& sign = b.flush(b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&b.unknown(), &b.constant(0x80000000u)}));
        auto& infinity = b.flush(b.emit(IrOpcode::BitwiseOr32, IrType::U32, {&b.constant(0x7f800000u), &b.unknown()}));
        auto& lowExponent = b.flush(b.emit(IrOpcode::BitwiseOr32, IrType::U32, {&b.unknown(), &b.constant(0x00800000u)}));
        auto& quiet = b.flush(b.emit(IrOpcode::BitwiseOr32, IrType::U32, {&b.flush(b.unknown()), &b.constant(0x00400000u)}));
        auto& unknownOr = b.flush(b.emit(IrOpcode::BitwiseOr32, IrType::U32, {&b.unknown(), &b.unknown()}));
        auto& mantissa = b.flush(b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&b.unknown(), &b.constant(0x807fffffu)}));
        for (auto* value : {&sign, &infinity, &lowExponent, &quiet, &unknownOr, &mantissa}) b.keep(*value);
        return b.eliminate() == 3u && !sign.HasUses() && !infinity.HasUses() && !lowExponent.HasUses() && quiet.HasUses() && unknownOr.HasUses() && mantissa.HasUses();
    });

    passed &= run("a phi of flushed values needs no flush", [] {
        Builder b;
        IrBlock& head = *b.block;
        IrBlock& body = b.newBlock();
        IrBlock& merge = b.newBlock();
        head.AddBranch(&body);
        head.AddBranch(&merge);
        body.AddBranch(&merge);
        auto& fromHead = b.flush(b.unknown());
        b.block = &body;
        auto& fromBody = b.flush(b.unknown());
        b.block = &merge;
        auto& merged = b.flush(b.phi(merge, {{&head, &fromHead}, {&body, &fromBody}}));
        b.keep(merged);
        return b.eliminate() == 1u && !merged.HasUses();
    });

    passed &= run("a loop phi needs no flush when every value it takes is flushed, and keeps it otherwise", [] {
        const auto loop = [](bool flushedBackEdge) {
            Builder b;
            IrBlock& head = *b.block;
            IrBlock& body = b.newBlock();
            IrBlock& exit = b.newBlock();
            head.AddBranch(&body);
            body.AddBranch(&body);
            body.AddBranch(&exit);
            auto& initial = b.flush(b.unknown());
            b.block = &body;
            auto& carried = b.program.CreateValue(IrOpcode::Phi, IrType::U32);
            body.AppendInstruction(&carried);
            auto& next = b.flush(carried);
            auto& backEdge = flushedBackEdge ? next : b.emit(IrOpcode::BitwiseXor32, IrType::U32, {&b.unknown(), &next});
            carried.AddPhiOperand(&head, &initial);
            carried.AddPhiOperand(&body, &backEdge);
            b.block = &exit;
            b.keep(next);
            b.eliminate();
            return !next.HasUses();
        };
        return loop(true) && !loop(false);
    });

    passed &= run("a product behind its round-to-nearest tiny-product guard needs no flush, one behind another guard does", [] {
        Builder b;
        const auto value = [&](IrValue& bits) -> IrValue& { return b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&bits}); };
        const auto bitsOf = [&](IrValue& number) -> IrValue& { return b.emit(IrOpcode::BitCastU32F32, IrType::U32, {&number}); };
        const auto guarded = [&](IrValue& lhs, IrValue& rhs, IrValue& guardLhs, IrValue& guardRhs, std::uint32_t threshold) -> IrValue& {
            auto& product = b.emit(IrOpcode::FPMul32, IrType::F32, {&lhs, &rhs});
            auto& scaled = b.emit(IrOpcode::FPMul32, IrType::F32, {&b.emit(IrOpcode::FPMul32, IrType::F32, {&guardLhs, &b.constant(0x5f800000u)}), &guardRhs});
            auto& magnitude = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&bitsOf(scaled), &b.constant(0x7fffffffu)});
            auto& tiny = b.emit(IrOpcode::ULessThan32, IrType::Bool, {&b.emit(IrOpcode::ISub32, IrType::U32, {&magnitude, &b.constant(1u)}), &b.constant(threshold)});
            auto& sign = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&b.emit(IrOpcode::BitwiseXor32, IrType::U32, {&bitsOf(lhs), &bitsOf(rhs)}), &b.constant(0x80000000u)});
            return b.flush(bitsOf(b.emit(IrOpcode::SelectF32, IrType::F32, {&tiny, &value(sign), &product})));
        };
        auto& x = value(b.flush(b.unknown()));
        auto& y = value(b.flush(b.unknown()));
        auto& raw = value(b.unknown());
        IrValue* removed[] = {&guarded(x, y, x, y, 0x207fffffu), &guarded(x, y, y, x, 0x207fffffu)};
        IrValue* kept[] = {&guarded(x, y, x, raw, 0x207fffffu), &guarded(x, y, x, y, 0x20800000u), &guarded(x, raw, x, raw, 0x207fffffu)};
        for (auto* flushed : removed) b.keep(*flushed);
        for (auto* flushed : kept) b.keep(*flushed);
        const bool counted = b.eliminate() == 2u;
        bool flushes = true;
        for (auto* flushed : removed) flushes &= !flushed->HasUses();
        for (auto* flushed : kept) flushes &= flushed->HasUses();
        return counted && flushes;
    });

    passed &= run("an arm a select takes only when its bits are NaN needs no flush, one under another value's NaN test does", [] {
        Builder b;
        auto& x = b.unknown();
        auto& other = b.unknown();
        const auto magnitude = [&](IrValue& bits) -> IrValue& { return b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&bits, &b.constant(0x7fffffffu)}); };
        const auto isNan = [&](IrValue& bits) -> IrValue& { return b.emit(IrOpcode::UGreaterThan32, IrType::Bool, {&magnitude(bits), &b.constant(0x7f800000u)}); };
        const auto chosen = [&](IrValue& condition, IrValue& arm) -> IrValue& { return b.flush(b.emit(IrOpcode::SelectU32, IrType::U32, {&condition, &arm, &b.flush(b.unknown())})); };
        IrValue* removed[] = {
            &chosen(isNan(x), x),
            &chosen(isNan(x), b.emit(IrOpcode::BitwiseOr32, IrType::U32, {&x, &b.constant(0x00400000u)})),
            &chosen(b.emit(IrOpcode::FPIsNan32, IrType::Bool, {&b.emit(IrOpcode::BitCastF32U32, IrType::F32, {&x})}), x),
        };
        IrValue* kept[] = {
            &chosen(isNan(other), x),
            &chosen(b.emit(IrOpcode::UGreaterThan32, IrType::Bool, {&b.constant(0x7f800000u), &magnitude(x)}), x),
        };
        for (auto* flushed : removed) b.keep(*flushed);
        for (auto* flushed : kept) b.keep(*flushed);
        const bool counted = b.eliminate() == 3u;
        bool flushes = true;
        for (auto* flushed : removed) flushes &= !flushed->HasUses();
        for (auto* flushed : kept) flushes &= flushed->HasUses();
        return counted && flushes;
    });

    passed &= run("the sign or exponent bits of any value need no flush, a mask that keeps significand bits does", [] {
        Builder b;
        auto& sign = b.flush(b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&b.unknown(), &b.constant(0x80000000u)}));
        auto& exponent = b.flush(b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&b.constant(0xff800000u), &b.unknown()}));
        auto& significand = b.flush(b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&b.unknown(), &b.constant(0x807fffffu)}));
        b.keep(sign);
        b.keep(exponent);
        b.keep(significand);
        return b.eliminate() == 2u && !sign.HasUses() && !exponent.HasUses() && significand.HasUses();
    });

    passed &= run("two chained v_mul_f32 keep only the input flush of the first source", [] {
        const std::array<std::uint32_t, 5> code{
            0x10020100u,
            0x10040301u,
            0xe0700000u, 0x80000200u,
            0xbf810000u,
        };
        const auto masks = ExponentMasks(Recompiled(code));
        if (masks != 1u) std::fprintf(stderr, "  %zu f32 exponent masks, expected 1\n", masks);
        return masks == 1u;
    });

    return passed ? 0 : 1;
}
