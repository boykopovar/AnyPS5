#include "Translation/TranslationContext.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include <array>
#include <cstdio>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

static void Require(bool value) {
    if (!value) throw std::runtime_error("scalar call regression");
}

template <typename TFunction>
static void ExpectThrow(const char* needle, TFunction fn) {
    try {
        fn();
    } catch (const std::exception& error) {
        if (std::string(error.what()).find(needle) == std::string::npos) {
            throw std::runtime_error(std::string("expected ") + needle + ", got " + error.what());
        }
        return;
    }
    throw std::runtime_error(std::string("expected throw containing ") + needle);
}

static RdnaInstruction Call(std::uint32_t pc, std::uint32_t link, std::uint32_t immediate) {
    const std::array<std::uint32_t, 1> code{0xbb000000u | (link << 16u) | (immediate & 0xffffu)};
    return DecodeRdnaSopk(pc, code, 0u);
}

static RdnaInstruction Sop1(std::uint32_t pc, std::uint32_t destination, std::uint32_t op, std::uint32_t source) {
    const std::array<std::uint32_t, 1> code{0xbe800000u | (destination << 16u) | (op << 8u) | source};
    return DecodeRdnaSop1(pc, code, 0u);
}

static RdnaInstruction Sopp(std::uint32_t pc, std::uint32_t op, std::uint32_t immediate = 0u) {
    const std::array<std::uint32_t, 1> code{0xbf800000u | (op << 16u) | immediate};
    return DecodeRdnaSopp(pc, code, 0u);
}

static ControlFlowGraph Build(std::initializer_list<RdnaInstruction> instructions) {
    RdnaProgram program;
    program.instructions.assign(instructions);
    return GraphBuilder{}.Build(program);
}

static void CheckDecode() {
    const auto instruction = Call(0x40u, 8u, 7u);
    Require(instruction.op == RdnaOpcode::SCallB64);
    Require(instruction.family == RdnaInstructionFamily::SOPK);
    Require(instruction.destination.kind == RdnaOperandKind::ScalarRegister && instruction.destination.reg == 8u);
    Require(instruction.dataDwordCount == 2u && instruction.wordCount == 1u);
    Require(instruction.branchTarget == 0x60u);
    Require(Call(0x40u, 8u, 0xfff9u).branchTarget == 0x28u);
    Require(Call(0x20000u, 8u, 0x8000u).branchTarget == 4u);
    Require(Call(0u, 8u, 0x7fffu).branchTarget == 0x20000u);
}

static std::string TranslateDump(const RdnaInstruction& instruction) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder() = {&block};
    program.Metadata().blockInfo.resize(1);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
    return ProgramToString(program);
}

static void CheckLink() {
    const auto dump = TranslateDump(Call(0x50u, 8u, 2u));
    Require(dump.find("0x0000000000000054") != std::string::npos);
    Require(dump == TranslateDump(Sop1(0x50u, 8u, 0x1fu, 0u)));
}

static void CheckCallAfterEnd() {
    const std::array<std::uint32_t, 5> code{0xbb080001u, 0xbf810000u, 0xbf800000u, 0xbe802008u, 0xbf810000u};
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    Require(decoded.instructions.size() == code.size());
    const auto cfg = GraphBuilder{}.Build(decoded);
    const auto& caller = cfg.FindBlockByProgramCounter(0u);
    const auto& callee = cfg.FindBlockByProgramCounter(8u);
    Require(caller.terminator.kind == TerminatorKind::Branch && caller.terminator.trueBlock == callee.id);
    Require(callee.terminator.kind == TerminatorKind::Branch && callee.terminator.trueBlock == cfg.FindBlockByProgramCounter(4u).id);
    ShaderComputeInputInfo computeInfo{};
    TranslateOptions options{};
    options.stage = ShaderStageKind::Compute;
    options.inputInfo.compute = &computeInfo;
    static_cast<void>(InstructionTranslator{}.Translate(decoded, cfg, options));
}

static void CheckRejections() {
    ExpectThrow("invalid instruction boundary", [] {
        Build({Call(0u, 8u, 16u), Sopp(4u, 1u)});
    });
    ExpectThrow("invalid instruction boundary", [] {
        const std::array<std::uint32_t, 7> code{0xbb080002u, 0xbf810000u, 0xbe8003ffu, 0u, 0xbe802008u, 0xbf810000u, 0xbf810000u};
        static_cast<void>(GraphBuilder{}.Build(RdnaInstructionDecoder{}.Decode(code)));
    });
    ExpectThrow("code span bounds", [] {
        const std::array<std::uint32_t, 2> outside{0xbb080010u, 0xbf810000u};
        static_cast<void>(RdnaInstructionDecoder{}.Decode(outside));
    });
    ExpectThrow("code span bounds", [] {
        const std::array<std::uint32_t, 2> outside{0xbb08fffeu, 0xbf810000u};
        static_cast<void>(RdnaInstructionDecoder{}.Decode(outside));
    });
    ExpectThrow("no paired s_setpc_b64 return", [] {
        Build({Call(0u, 8u, 0u), Sopp(4u, 1u)});
    });
    for (const auto link : {9u, 105u, 106u, 125u}) {
        ExpectThrow("ordinary aligned scalar register pair", [=] {
            Build({Call(0u, link, 0u), Sopp(4u, 1u)});
        });
    }
    ExpectThrow("escapes the constant-offset call/return model", [] {
        Build({Call(0u, 8u, 1u), Sopp(4u, 1u), Sop1(8u, 8u, 3u, 128u), Sop1(12u, 0u, 0x20u, 8u)});
    });
    ExpectThrow("recursive scalar call", [] {
        Build({Call(0u, 8u, 0xffffu), Sop1(4u, 0u, 0x20u, 8u), Sopp(8u, 1u)});
    });
    ExpectThrow("is shared with another call", [] {
        Build({Call(0u, 8u, 2u), Call(4u, 8u, 1u), Sopp(8u, 1u), Sop1(12u, 0u, 0x20u, 8u)});
    });
    ExpectThrow("multiple s_setpc_b64 returns", [] {
        Build({Call(0u, 8u, 1u), Sopp(4u, 1u), Sop1(8u, 0u, 0x20u, 8u), Sop1(12u, 0u, 0x20u, 8u)});
    });
    ExpectThrow("control flow enters the call region", [] {
        Build({Call(0u, 8u, 2u), Sopp(4u, 2u, 1u), Sopp(8u, 1u), Sopp(12u, 0u), Sop1(16u, 0u, 0x20u, 8u)});
    });
    ExpectThrow("control flow leaves the call region", [] {
        Build({Call(0u, 8u, 1u), Sopp(4u, 1u), Sopp(8u, 2u, 2u), Sop1(12u, 0u, 0x20u, 8u), Sopp(16u, 1u), Sopp(20u, 1u)});
    });
}

static void CheckGuards(const std::string& only) {
    bool checked = false;
    const auto check = [&](const char* name, const char* needle, auto fn) {
        if (only.empty() || only == name) {
            checked = true;
            ExpectThrow(needle, fn);
        }
    };
    check("tuple", "escapes the constant-offset call/return model", [] {
        auto load = Sopp(8u, 0u);
        load.family = RdnaInstructionFamily::SMEM;
        load.op = RdnaOpcode::SLoadDwordx8;
        load.dataDwordCount = 8u;
        load.destination.kind = RdnaOperandKind::ScalarRegister;
        load.destination.reg = 4u;
        Build({Call(0u, 8u, 1u), Sopp(4u, 1u), load, Sop1(12u, 0u, 0x20u, 8u)});
    });
    check("wrong_return", "escapes the constant-offset call/return model", [] {
        Build({Call(0u, 8u, 1u), Sopp(4u, 1u), Sop1(8u, 0u, 0x20u, 9u)});
    });
    check("fallthrough", "control flow enters the call region", [] {
        Build({Call(0u, 8u, 2u), Sopp(4u, 0u), Sopp(8u, 0u), Sopp(12u, 0u), Sop1(16u, 0u, 0x20u, 8u)});
    });
    check("conditional_fallthrough", "control flow enters the call region", [] {
        Build({Call(0u, 8u, 2u), Sopp(4u, 2u), Sopp(8u, 4u, 2u), Sopp(12u, 0u), Sop1(16u, 0u, 0x20u, 8u), Sopp(20u, 1u)});
    });
    check("return_before_target", "return precedes the call target", [] {
        Build({Call(0u, 8u, 2u), Sopp(4u, 1u), Sop1(8u, 0u, 0x20u, 8u), Sopp(12u, 0u), Sopp(16u, 1u)});
    });
    check("descriptor", "escapes the constant-offset call/return model", [] {
        const std::array<std::uint32_t, 2> code{0xe0381000u, 0x80020401u};
        const auto load = DecodeRdnaMubuf(8u, code, 0u);
        Require(load.source1.reg == 8u);
        Build({Call(0u, 10u, 1u), Sopp(4u, 1u), load, Sop1(16u, 0u, 0x20u, 10u)});
    });
    check("relative_read", "escapes the constant-offset call/return model", [] {
        Build({Call(0u, 8u, 1u), Sopp(4u, 1u), Sop1(8u, 0u, 0x2eu, 0u), Sop1(12u, 0u, 0x20u, 8u)});
    });
    if (only.empty()) {
        for (const auto link : {0u, 104u}) {
            const auto cfg = Build({Call(0u, link, 1u), Sopp(4u, 1u), Sop1(8u, 0u, 0x20u, link)});
            Require(cfg.FindBlockByProgramCounter(0u).terminator.kind == TerminatorKind::Branch);
        }
    }
    Require(checked);
}

int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            CheckDecode();
            CheckLink();
            CheckCallAfterEnd();
            CheckRejections();
            CheckGuards("");
        } else {
            Require(argc == 2);
            CheckGuards(argv[1]);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "scalar call regression: %s\n", error.what());
        return 1;
    }
    return 0;
}
