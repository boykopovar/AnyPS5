#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>

using namespace ShaderRecompiler;

namespace {

enum class Bitcast { None, F32, F16 };

struct RoundTrip {
    const char* name;
    unsigned bits;
    Bitcast bitcast;
};

IrValue& convert(IrBuilder& builder, IrValue& source, const RoundTrip& test) {
    if (test.bitcast == Bitcast::F16) {
        return builder.BitCastU32FromF16(builder.BitCastF16(source));
    }
    auto& input = test.bitcast == Bitcast::F32 ? builder.BitCastU32(builder.BitCastF32(source)) : source;
    auto& narrowed = builder.Emit(test.bits == 16 ? IrOpcode::ConvertU16U32 : IrOpcode::ConvertU8U32,
                                 test.bits == 16 ? IrType::U16 : IrType::U8, {&input});
    auto& widened = builder.Emit(test.bits == 16 ? IrOpcode::ConvertU32U16 : IrOpcode::ConvertU32U8,
                                IrType::U32, {&narrowed});
    return test.bitcast == Bitcast::F32 ? builder.BitCastU32(builder.BitCastF32(widened)) : widened;
}

void fold(IrProgram& program, bool cleanup) {
    ConstantFolder().Fold(program);
    if (cleanup) {
        DeadCodeEliminator().RemoveIdentities(program);
        DeadCodeEliminator().Eliminate(program);
        ConstantFolder().Fold(program);
        DeadCodeEliminator().RemoveIdentities(program);
        DeadCodeEliminator().Eliminate(program);
    }
    ValidateProgram(program, true);
}

bool checkRoundTrip(const RoundTrip& test, std::uint32_t pattern, std::uint32_t laneValue, bool dynamic, bool cleanup) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    BlockInfo info;
    info.id = 0;
    program.Metadata().blockInfo.push_back(info);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    IrValue* lane = dynamic ? &builder.Emit(IrOpcode::LaneId, IrType::U32, {}) : nullptr;
    auto& source = dynamic ? builder.Emit(IrOpcode::IAdd32, IrType::U32, {lane, &builder.Constant(pattern)})
                           : builder.Constant(pattern);
    auto& result = convert(builder, source, test);
    auto& user = builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&result});
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ValidateProgram(program, true);
    fold(program, cleanup);
    if (dynamic) {
        if (user.Argument(0)->Resolve()->HasImmediate() || !lane->HasUses()) {
            std::fprintf(stderr, "%s: optimized result must still depend on LaneId\n", test.name);
            return false;
        }
        lane->ReplaceUsesWith(&builder.Constant(laneValue), true);
        fold(program, cleanup);
    }
    const auto expected = (pattern + laneValue) & (test.bits == 16 ? 0xffffu : 0xffu);
    const auto* actual = user.Argument(0)->Resolve();
    if (!actual->HasImmediate() || actual->Type() != IrType::U32 || actual->ImmediateU32() != expected) {
        std::fprintf(stderr, "%s: pattern=%08x lane=%u dynamic=%d cleanup=%d expected=%08x actual=%08x immediate=%d\n",
                     test.name, pattern, laneValue, dynamic, cleanup, expected,
                     actual->HasImmediate() ? actual->ImmediateU32() : 0u, actual->HasImmediate());
        return false;
    }
    return true;
}

}

int main() {
    try {
        constexpr RoundTrip cases[] = {
            {"U16 round trip", 16, Bitcast::None},
            {"U8 round trip", 8, Bitcast::None},
            {"U16 with F32 bitcasts", 16, Bitcast::F32},
            {"U8 with F32 bitcasts", 8, Bitcast::F32},
            {"F16 bitcast round trip", 16, Bitcast::F16},
        };
        constexpr std::uint32_t patterns[] = {
            0u, 1u, 0x7fu, 0x80u, 0xffu, 0x100u, 0x7fffu, 0x8000u,
            0xffffu, 0x10000u, 0x12345678u, 0x80000000u, 0xffff0000u, 0xffffff00u, 0xffffffffu,
        };
        constexpr std::uint32_t lanes[] = {0u, 1u, 7u, 31u, 32u, 63u};
        unsigned total = 0;
        unsigned failures = 0;
        for (const auto& test : cases) {
            for (auto pattern : patterns) {
                ++total;
                failures += !checkRoundTrip(test, pattern, 0u, false, true);
                for (auto lane : lanes) {
                    for (bool cleanup : {false, true}) {
                        ++total;
                        failures += !checkRoundTrip(test, pattern, lane, true, cleanup);
                    }
                }
            }
        }
        std::printf("Integer narrowing: %u cases, %u failures\n", total, failures);
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
