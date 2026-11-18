#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/NanPayloadEliminator.hpp"
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
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Builder {
    IrProgram program;
    IrBlock* block = nullptr;

    Builder() {
        auto& created = program.CreateBlock();
        program.BlockOrder().push_back(&created);
        program.SetEntryBlock(created);
        block = &created;
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

    IrValue& f32(IrValue& bits) {
        return emit(IrOpcode::BitCastF32U32, IrType::F32, {&bits});
    }

    IrValue& bits(IrValue& value) {
        return emit(IrOpcode::BitCastU32F32, IrType::U32, {&value});
    }

    IrValue& isNan(IrValue& value) {
        return emit(IrOpcode::UGreaterThan32, IrType::Bool, {&emit(IrOpcode::BitwiseAnd32, IrType::U32, {&value, &constant(0x7fffffffu)}), &constant(0x7f800000u)});
    }

    IrValue& select(IrValue& condition, IrValue& taken, IrValue& otherwise) {
        return emit(IrOpcode::SelectU32, IrType::U32, {&condition, &taken, &otherwise});
    }

    IrValue& nanRule(IrOpcode opcode, IrValue& lhs, IrValue& rhs) {
        auto& result = bits(emit(opcode, IrType::F32, {&f32(lhs), &f32(rhs)}));
        auto& nan = select(isNan(rhs), rhs, select(isNan(lhs), lhs, constant(0xffc00000u)));
        return select(isNan(result), nan, result);
    }

    IrValue& conditionalNanRule(IrValue& lhs, IrValue& rhs) {
        auto& result = emit(IrOpcode::FPMul32, IrType::F32, {&f32(lhs), &f32(rhs)});
        auto& invalid = program.CreateValue(IrOpcode::Void, IrType::U1);
        invalid.SetImmediateBool(false);
        return bits(emit(IrOpcode::NanPropagateF32, IrType::F32, {&result, &constant(5u), &invalid, &f32(lhs), &f32(rhs), &f32(lhs)}));
    }

    IrValue& saturate(IrValue& value) {
        return bits(emit(IrOpcode::FPSaturate32, IrType::F32, {&f32(value)}));
    }

    IrValue& flush(IrValue& value) {
        auto& exponent = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&value, &constant(0x7f800000u)});
        auto& denormal = emit(IrOpcode::IEqual32, IrType::Bool, {&exponent, &constant(0u)});
        auto& sign = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&value, &constant(0x80000000u)});
        return select(denormal, sign, value);
    }

    void keep(IrValue& value) {
        static_cast<void>(emit(IrOpcode::ReferenceU32, IrType::Void, {&value}));
    }

    NanPayloadEliminationStats eliminate() {
        return NanPayloadEliminator{}.Eliminate(program);
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
    const std::array<std::uint32_t, 4> capabilities{1u, 61u, spv::CapabilityDenormPreserve, spv::CapabilitySignedZeroInfNanPreserve};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 32;
    request.context.floatMode = ShaderFloatMode{0xf0u, true, false, false};
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 32u;
    request.target.float32DenormPreserve = true;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    const auto result = Recompile(request);
    const auto& words = result.spirv.Words();
    return {words.begin(), words.end()};
}

std::size_t NanTests(std::span<const std::uint32_t> words) {
    if (words.size() < 5 || words[0] != spv::MagicNumber) throw std::runtime_error("the recompiler returned no SPIR-V module");
    std::set<std::uint32_t> infinities;
    std::size_t tests = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        if (count == 0 || count > words.size() - offset) throw std::runtime_error("the recompiler returned a malformed SPIR-V instruction");
        const auto opcode = words[offset] & spv::OpCodeMask;
        if (opcode == spv::OpConstant && count == 4 && words[offset + 3] == 0x7f800000u) infinities.insert(words[offset + 2]);
        if (opcode == spv::OpUGreaterThan && count == 5 && infinities.contains(words[offset + 4])) ++tests;
        offset += count;
    }
    return tests;
}

}

int main() {
    bool passed = true;

    passed &= run("an f32 NaN result that is only clamped drops its payload selects", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        b.keep(b.saturate(sum));
        const auto stats = b.eliminate();
        return stats.removedSelects >= 1u && !sum.HasUses();
    });

    passed &= run("conditional NaN repair is removed through a clamp but retained at a raw sink", [] {
        Builder clamped;
        auto& discarded = clamped.conditionalNanRule(clamped.unknown(), clamped.unknown());
        clamped.keep(clamped.saturate(discarded));
        clamped.eliminate();
        Builder raw;
        auto& observed = raw.conditionalNanRule(raw.unknown(), raw.unknown());
        raw.keep(observed);
        raw.eliminate();
        return discarded.Argument(0)->Opcode() != IrOpcode::NanPropagateF32 && observed.Argument(0)->Opcode() == IrOpcode::NanPropagateF32;
    });

    passed &= run("a conditional repair feeding an observed chain stays while a clamped branch is removed", [] {
        Builder b;
        auto& first = b.conditionalNanRule(b.unknown(), b.unknown());
        auto& clamped = b.conditionalNanRule(first, b.unknown());
        auto& observed = b.conditionalNanRule(first, b.unknown());
        b.keep(b.saturate(clamped));
        b.keep(observed);
        b.eliminate();
        return first.Argument(0)->Opcode() == IrOpcode::NanPropagateF32 && observed.Argument(0)->Opcode() == IrOpcode::NanPropagateF32 && clamped.Argument(0)->Opcode() != IrOpcode::NanPropagateF32;
    });

    passed &= run("bit work on conditional repair remains observable through a clamp", [] {
        Builder b;
        auto& repaired = b.conditionalNanRule(b.unknown(), b.unknown());
        auto& changed = b.emit(IrOpcode::BitwiseXor32, IrType::U32, {&repaired, &b.constant(0x7f800000u)});
        b.keep(b.saturate(changed));
        b.eliminate();
        return repaired.Argument(0)->Opcode() == IrOpcode::NanPropagateF32;
    });

    passed &= run("an f32 NaN result kept as raw bits keeps every select", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        b.keep(sum);
        return b.eliminate().removedSelects == 0u && sum.HasUses();
    });

    passed &= run("integer work on the result bits keeps the selects", [] {
        Builder b;
        auto& product = b.nanRule(IrOpcode::FPMul32, b.unknown(), b.unknown());
        b.keep(b.emit(IrOpcode::ShiftRightLogical32, IrType::U32, {&product, &b.constant(16u)}));
        return b.eliminate().removedSelects == 0u && product.HasUses();
    });

    passed &= run("a sign flip passes the payload on to a raw sink", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        b.keep(b.emit(IrOpcode::BitwiseXor32, IrType::U32, {&sum, &b.constant(0x80000000u)}));
        return b.eliminate().removedSelects == 0u && sum.HasUses();
    });

    passed &= run("exponent and ordered tests of the result only see NaN-ness", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        auto& infinite = b.emit(IrOpcode::IEqual32, IrType::Bool, {&b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&sum, &b.constant(0x7f800000u)}), &b.constant(0x7f800000u)});
        auto& below = b.emit(IrOpcode::FPOrdLessThan32, IrType::Bool, {&b.f32(sum), &b.f32(b.constant(0x3f000000u))});
        b.keep(b.select(b.emit(IrOpcode::LogicalOr, IrType::Bool, {&infinite, &below}), b.constant(1u), b.constant(0u)));
        return b.eliminate().removedSelects >= 1u && !sum.HasUses();
    });

    passed &= run("a flush to a signed zero passes a NaN through unchanged", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        b.keep(b.saturate(b.flush(sum)));
        return b.eliminate().removedSelects >= 1u && !sum.HasUses();
    });

    passed &= run("bit work on the non-NaN arm of a NaN guard is never taken for a NaN", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        auto& halved = b.emit(IrOpcode::ISub32, IrType::U32, {&sum, &b.constant(0x00800000u)});
        b.keep(b.saturate(b.select(b.isNan(sum), sum, halved)));
        return b.eliminate().removedSelects >= 1u && !sum.HasUses();
    });

    passed &= run("bit work outside a NaN guard reaches the clamp and keeps the selects", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        auto& halved = b.emit(IrOpcode::ISub32, IrType::U32, {&sum, &b.constant(0x00800000u)});
        b.keep(b.saturate(halved));
        return b.eliminate().removedSelects == 0u && sum.HasUses();
    });

    passed &= run("a NaN result feeding a later NaN rule is removed only when that rule's result is unobserved", [] {
        Builder b;
        auto& first = b.nanRule(IrOpcode::FPMul32, b.unknown(), b.unknown());
        auto& second = b.nanRule(IrOpcode::FPAdd32, first, b.unknown());
        auto& third = b.nanRule(IrOpcode::FPAdd32, first, b.unknown());
        b.keep(b.saturate(second));
        b.keep(third);
        b.eliminate();
        return first.HasUses() && third.HasUses() && !second.HasUses();
    });

    passed &= run("an integer compare of the raw result bits sees the payload and keeps the selects", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        auto& same = b.emit(IrOpcode::IEqual32, IrType::Bool, {&sum, &b.constant(0x7fc00000u)});
        b.keep(b.select(same, b.constant(1u), b.constant(0u)));
        return b.eliminate().removedSelects == 0u && sum.HasUses();
    });

    passed &= run("an xor that clears the exponent turns the payload into a value the clamp sees", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        auto& flipped = b.emit(IrOpcode::BitwiseXor32, IrType::U32, {&sum, &b.constant(0x7f800000u)});
        b.keep(b.saturate(flipped));
        return b.eliminate().removedSelects == 0u && sum.HasUses();
    });

    passed &= run("a compare of the magnitude against a constant inside the NaN range keeps the selects", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        auto& magnitude = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&sum, &b.constant(0x7fffffffu)});
        auto& above = b.emit(IrOpcode::UGreaterThan32, IrType::Bool, {&magnitude, &b.constant(0x7fc00000u)});
        b.keep(b.select(above, b.constant(1u), b.constant(0u)));
        return b.eliminate().removedSelects == 0u && sum.HasUses();
    });

    passed &= run("a compare of the magnitude against the infinity bits gives the same answer for every NaN", [] {
        Builder b;
        auto& sum = b.nanRule(IrOpcode::FPAdd32, b.unknown(), b.unknown());
        auto& magnitude = b.emit(IrOpcode::BitwiseAnd32, IrType::U32, {&sum, &b.constant(0x7fffffffu)});
        auto& above = b.emit(IrOpcode::UGreaterThan32, IrType::Bool, {&magnitude, &b.constant(0x7f800000u)});
        b.keep(b.select(above, b.constant(1u), b.constant(0u)));
        return b.eliminate().removedSelects >= 1u && !sum.HasUses();
    });

    passed &= run("through Recompile, clamped add and multiply stored to a buffer drops the NaN tests an unclamped one keeps", [] {
        std::array<std::uint32_t, 6> clamped{
            0x06020100u,
            0xd5038002u, 0x00020301u,
            0xe0700000u, 0x80000200u,
            0xbf810000u,
        };
        std::array<std::uint32_t, 6> unclamped{
            0x06020100u,
            0xd5030002u, 0x00020301u,
            0xe0700000u, 0x80000200u,
            0xbf810000u,
        };
        for (const auto opcode : {0xd5030002u, 0xd5080002u}) {
            clamped[1] = opcode | 0x8000u;
            unclamped[1] = opcode;
            const auto kept = NanTests(Recompiled(unclamped));
            const auto left = NanTests(Recompiled(clamped));
            if (left != 0u || kept == 0u) {
                std::fprintf(stderr, "  opcode %08x: %zu NaN tests with clamp, %zu without; expected none with clamp and some without\n", opcode, left, kept);
                return false;
            }
        }
        return true;
    });

    return passed ? 0 : 1;
}
