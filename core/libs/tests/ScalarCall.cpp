#include "Translation/TranslationContext.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <initializer_list>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

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

static RdnaInstruction Sop2(std::uint32_t pc, std::uint32_t destination, std::uint32_t op, std::uint32_t source0, std::uint32_t source1) {
    const std::array<std::uint32_t, 1> code{0x80000000u | (op << 23u) | (destination << 16u) | (source1 << 8u) | source0};
    return DecodeRdnaSop2(pc, code, 0u);
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

static std::vector<std::uint32_t> UserDataCaller() {
    return {0x7e020280u, 0xbe940383u, 0xbe8e0300u, 0xbe8f0301u, 0xbe8e210eu, 0xbe8e0302u, 0xbe8f0303u, 0xbe8e210eu,
        0x80948114u, 0xbf078014u, 0xbf85fff7u, 0xbf810000u, 0x30306c73u};
}

static std::vector<UserDataCall> UserDataCalls(const std::vector<std::uint32_t>& code, std::uint32_t userDataCount = 8u) {
    const SwappcInfo swappc{false, 0u, userDataCount};
    return FindUserDataCalls(RdnaInstructionDecoder{}.Decode(code), swappc);
}

static void CheckUserDataCalls() {
    const SwappcInfo swappc{false, 0u, 8u};
    const auto caller = UserDataCaller();
    const auto program = RdnaInstructionDecoder{}.Decode(caller);
    const auto calls = FindUserDataCalls(program, swappc);
    Require(calls.size() == 2u);
    Require(calls[0].programCounter == 0x10u && calls[0].linkRegister == 14u && calls[0].userDataRegister == 0u);
    Require(calls[1].programCounter == 0x1cu && calls[1].linkRegister == 14u && calls[1].userDataRegister == 2u);
    ExpectThrow("not statically resolvable", [&] { static_cast<void>(GraphBuilder{}.Build(program, &swappc)); });

    auto overwritten = caller;
    overwritten[1] = 0xbe800380u;
    const auto second = UserDataCalls(overwritten);
    Require(second.size() == 1u && second[0].programCounter == 0x1cu);
    auto entered = caller;
    entered[10] = 0xbf85fff8u;
    const auto afterLabel = UserDataCalls(entered);
    Require(afterLabel.size() == 1u && afterLabel[0].programCounter == 0x1cu);
    auto looped = caller;
    looped[8] = 0xbe820380u;
    const auto beforeWrite = UserDataCalls(looped);
    Require(beforeWrite.size() == 1u && beforeWrite[0].programCounter == 0x10u);
    Require(UserDataCalls({0xbe842104u, 0xbf810000u}, 6u).empty());
    const auto direct = UserDataCalls({0xbe8e2104u, 0xbf810000u}, 6u);
    Require(direct.size() == 1u && direct[0].linkRegister == 14u && direct[0].userDataRegister == 4u);
    const std::vector<std::uint32_t> branchedCopy{0xbe8e0300u, 0xbe8f0301u, 0xbf880001u, 0x7e020280u, 0xbe8e210eu, 0xbf810000u};
    const auto acrossBranch = UserDataCalls(branchedCopy);
    Require(acrossBranch.size() == 1u && acrossBranch[0].programCounter == 0x10u && acrossBranch[0].linkRegister == 14u && acrossBranch[0].userDataRegister == 0u);
    auto skippedWrite = branchedCopy;
    skippedWrite[3] = 0xbe8e0380u;
    Require(UserDataCalls(skippedWrite).empty());
    const std::vector<std::uint32_t> loopedCall{0xbe8e0300u, 0xbe8f0301u, 0xbe8e210eu, 0xbf85fffeu, 0xbf810000u};
    Require(UserDataCalls(loopedCall).empty());
    auto separateLink = loopedCall;
    separateLink[2] = 0xbe90210eu;
    const auto keptPointer = UserDataCalls(separateLink);
    Require(keptPointer.size() == 1u && keptPointer[0].programCounter == 0x8u && keptPointer[0].linkRegister == 16u && keptPointer[0].userDataRegister == 0u);

    const std::vector<std::uint32_t> addFive{0x4a020285u, 0xbe80200eu};
    const std::vector<std::uint32_t> addSixtyFour{0x4a0202c0u, 0xbe80200eu};
    Require(UserDataCalleeLength(addFive, 14u) == 2u);
    Require(!UserDataCalleeLength(std::vector<std::uint32_t>{0x4a020285u}, 14u).has_value());
    ExpectThrow("reads its program counter", [] { static_cast<void>(UserDataCalleeLength(std::vector<std::uint32_t>{0xbe901f00u, 0xbe80200eu}, 14u)); });
    ExpectThrow("makes a call", [] { static_cast<void>(UserDataCalleeLength(std::vector<std::uint32_t>{0xbe92210eu, 0xbe80200eu}, 14u)); });
    ExpectThrow("ends the program", [] { static_cast<void>(UserDataCalleeLength(std::vector<std::uint32_t>{0xbf810000u}, 14u)); });
    ExpectThrow("jumps through another register pair", [] { static_cast<void>(UserDataCalleeLength(std::vector<std::uint32_t>{0xbe802010u}, 14u)); });
    ExpectThrow("before code it branches to", [] { static_cast<void>(UserDataCalleeLength(std::vector<std::uint32_t>{0xbf840001u, 0xbe80200eu, 0x4a020285u, 0xbe80200eu}, 14u)); });

    const std::vector<std::vector<std::uint32_t>> callees{addFive, addSixtyFour};
    const auto linked = LinkUserDataCalls(program, calls, callees);
    Require(linked.size() == 17u && linked[4] == 0xbb0e0007u && linked[7] == 0xbb0e0006u && linked[16] == 0xbf9f0000u);
    Require(std::equal(addFive.begin(), addFive.end(), linked.begin() + 12) && std::equal(addSixtyFour.begin(), addSixtyFour.end(), linked.begin() + 14));
    auto readsPc = caller;
    readsPc[1] = 0xbe941f00u;
    const auto readsPcProgram = RdnaInstructionDecoder{}.Decode(readsPc);
    Require(LinkUserDataCalls(readsPcProgram, FindUserDataCalls(readsPcProgram, swappc), callees).size() == 17u);

    const auto linkedProgram = RdnaInstructionDecoder{}.Decode(linked);
    const auto cfg = GraphBuilder{}.Build(linkedProgram, &swappc);
    const auto block = [&](std::uint32_t programCounter) { return cfg.FindBlockByProgramCounter(programCounter).id; };
    Require(cfg.FindBlockByProgramCounter(0x08u).terminator.trueBlock == block(0x30u));
    Require(cfg.FindBlockByProgramCounter(0x30u).terminator.trueBlock == block(0x14u));
    Require(cfg.FindBlockByProgramCounter(0x14u).terminator.trueBlock == block(0x38u));
    Require(cfg.FindBlockByProgramCounter(0x38u).terminator.trueBlock == block(0x20u));
    ShaderComputeInputInfo computeInfo{};
    TranslateOptions options{};
    options.stage = ShaderStageKind::Compute;
    options.inputInfo.compute = &computeInfo;
    options.userDataCount = 8u;
    static_cast<void>(InstructionTranslator{}.Translate(linkedProgram, cfg, options));

    auto readsLink = linked;
    readsLink[8] = 0xbe94030eu;
    ExpectThrow("escapes the constant-offset call/return model", [&] { static_cast<void>(GraphBuilder{}.Build(RdnaInstructionDecoder{}.Decode(readsLink), &swappc)); });
    auto writesInside = linked;
    writesInside[12] = 0xbe8e0300u;
    ExpectThrow("escapes the constant-offset call/return model", [&] { static_cast<void>(GraphBuilder{}.Build(RdnaInstructionDecoder{}.Decode(writesInside), &swappc)); });
    auto sameCopy = linked;
    sameCopy[7] = 0xbb0e0004u;
    ExpectThrow("is shared with another call", [&] { static_cast<void>(GraphBuilder{}.Build(RdnaInstructionDecoder{}.Decode(sameCopy), &swappc)); });
}

static std::vector<std::uint32_t> TableCaller() {
    return {0x8f148414u, 0xf4080406u, 0xfa000270u, 0xbf8cc07fu, 0xf4280208u, 0x28000000u, 0xbf8cc07fu, 0xbe8e0308u, 0xbe8f0309u, 0xbe8e210eu, 0xbf810000u};
}

static ControlFlowGraph BuildCode(std::span<const std::uint32_t> code, const SwappcInfo& swappc) {
    return GraphBuilder{}.Build(RdnaInstructionDecoder{}.Decode(code), &swappc);
}

static IrProgram TranslateCode(std::span<const std::uint32_t> code, const SwappcInfo& swappc) {
    const auto program = RdnaInstructionDecoder{}.Decode(code);
    const auto cfg = GraphBuilder{}.Build(program, &swappc);
    ShaderComputeInputInfo computeInfo{};
    TranslateOptions options{};
    options.stage = ShaderStageKind::Compute;
    options.inputInfo.compute = &computeInfo;
    options.userDataCount = swappc.userDataCount;
    return InstructionTranslator{}.Translate(program, cfg, options);
}

static void CheckConstantTableCall() {
    const std::vector<std::uint32_t> caller{0xbeea03ffu, 0xe0040000u, 0xbeeb038fu, 0xf4080135u, 0xfa000000u, 0xbf8cc07fu, 0xf4240382u, 0xfa000020u, 0xbf8cc07fu, 0xbf130e80u,
        0xbf840004u, 0xf4240402u, 0xfa000028u, 0xbf8cc07fu, 0xbe8e210eu, 0xbf810000u};
    const SwappcInfo swappc{false, 0u, 16u};
    const auto calls = FindTableCalls(RdnaInstructionDecoder{}.Decode(caller), swappc);
    Require(calls.size() == 1u);
    Require(calls[0].constantPointer == std::optional<std::uint64_t>(0xfe0040000ull) && calls[0].descriptorOffset == 0u && calls[0].entryOffset == 0x20u && calls[0].entryStride == 0u);
    Require(calls[0].linkRegister == 14u && calls[0].targetRegister == 14u);
    auto changedHigh = caller;
    changedHigh[2] = 0xbeeb0380u;
    const auto zero = FindTableCalls(RdnaInstructionDecoder{}.Decode(changedHigh), swappc);
    Require(zero.size() == 1u && zero[0].constantPointer == std::optional<std::uint64_t>(0xe0040000ull));
    auto computed = caller;
    computed[2] = 0xbeeb0302u;
    Require(FindTableCalls(RdnaInstructionDecoder{}.Decode(computed), swappc).empty());
}

static void CheckTableCalls() {
    CheckConstantTableCall();
    const SwappcInfo swappc{false, 0u, 16u};
    const auto caller = TableCaller();
    const auto program = RdnaInstructionDecoder{}.Decode(caller);
    const auto calls = FindTableCalls(program, swappc);
    Require(calls.size() == 1u);
    Require(calls[0].programCounter == 0x24u && calls[0].linkRegister == 14u && calls[0].targetRegister == 14u && calls[0].userDataRegister == 12u);
    Require(calls[0].descriptorOffset == 0x270u && calls[0].entryOffset == 0u && calls[0].entryStride == 16u);
    Require(FindUserDataCalls(program, swappc).empty());
    const auto table = [&](std::size_t word, std::uint32_t value, std::uint32_t userDataCount = 16u) {
        auto code = caller;
        code[word] = value;
        return FindTableCalls(RdnaInstructionDecoder{}.Decode(code), SwappcInfo{false, 0u, userDataCount});
    };
    Require(table(0u, 0x8f148114u).empty());
    Require(table(2u, 0x28000270u).empty());
    Require(table(8u, 0xbe8f030au).empty());
    Require(table(10u, 0xbf810000u, 12u).empty());
    Require(table(5u, 0x28000008u).size() == 1u && table(5u, 0x28000008u)[0].entryOffset == 8u);
    Require(table(0u, 0xbe940380u).empty());
    const auto slice = [&](std::uint32_t load, std::uint32_t bufferLoad) {
        auto code = caller;
        code[1] = load;
        code[4] = bufferLoad;
        return FindTableCalls(RdnaInstructionDecoder{}.Decode(code), swappc);
    };
    for (const auto& [load, bufferLoad, offset] : {std::tuple{0xf40c0606u, 0xf428020cu, 0x270u}, std::tuple{0xf40c0606u, 0xf428020eu, 0x280u}, std::tuple{0xf4100606u, 0xf4280210u, 0x290u}, std::tuple{0xf4100606u, 0xf4280212u, 0x2a0u}}) {
        const auto sliced = slice(load, bufferLoad);
        Require(sliced.size() == 1u && sliced[0].userDataRegister == 12u && sliced[0].descriptorOffset == offset && sliced[0].entryOffset == 0u && sliced[0].entryStride == 16u);
    }
    Require(slice(0xf40c0606u, 0xf4280210u).empty());
    Require(slice(0xf40c0606u, 0xf428020du).empty());
    Require(slice(0xf40c0686u, 0xf428020eu).empty());
    Require(slice(0xf40c0686u, 0xf428020cu).empty());
    Require(slice(0xf40c0586u, 0xf428020eu).empty());
    auto readsLane = caller;
    readsLane.insert(readsLane.begin(), {0xd4c2000bu, 0x00020300u});
    const auto readsLaneProgram = RdnaInstructionDecoder{}.Decode(readsLane);
    Require(FindTableCalls(readsLaneProgram, SwappcInfo{false, 0u, 16u, 32u}).size() == 1u);
    Require(FindTableCalls(readsLaneProgram, SwappcInfo{false, 0u, 16u, 64u}).empty());

    ExpectThrow("not statically resolvable", [&] { static_cast<void>(BuildCode(caller, swappc)); });
    const std::array<std::uint32_t, 1> fallback{0x24u};
    SwappcInfo listed = swappc;
    listed.linkedCallFallbacks = fallback;
    Require(BuildCode(caller, listed).unlinkedCallProgramCounters == std::vector<std::uint32_t>{0x24u});
    const auto unlinked = TranslateCode(caller, listed);
    Require(unlinked.Info().usesFaultBuffer && ProgramToString(unlinked).find("UnlinkedCall") != std::string::npos);

    const std::vector<std::uint32_t> first{0xbf068000u, 0x4a020285u, 0xbe80200eu};
    const std::vector<std::uint32_t> second{0x4a0202c0u, 0xbf068001u, 0xbe80200eu};
    const std::vector<std::vector<TableCallee>> callees{{{0x1122334455ull, first}, {0x66778899aaull, second}}};
    const auto linkedCalls = LinkCalls(program, {}, {}, calls, callees);
    const auto& linked = linkedCalls.code;
    Require(linkedCalls.fallbacks == std::vector<std::uint32_t>{53u * 4u});
    ExpectThrow("is shared with another call", [&] { static_cast<void>(BuildCode(linked, swappc)); });
    SwappcInfo linkedSwappc = swappc;
    linkedSwappc.linkedCallFallbacks = linkedCalls.fallbacks;
    Require(linked.size() == 56u && linked[9] == 0xbf820001u && linked[11] == 0xbf850007u && linked[55] == 0xbf9f0000u);
    Require(linked[12] == 0xbf07ff0eu && linked[13] == 0x22334455u && linked[14] == 0xbf850010u && linked[15] == 0xbf07ff0fu && linked[16] == 0x11u && linked[17] == 0xbf85000du && linked[18] == 0xbf820007u);
    Require(linked[19] == 0xbf07ff0eu && linked[21] == 0xbf850010u && linked[24] == 0xbf85000du && linked[25] == 0xbf068080u && linked[26] == 0xbb0e0001u && linked[27] == 0xbf82ffeeu);
    Require(linked[32] == 0x778899aau && linked[33] == 0xbf850010u && linked[37] == 0xbf820007u && linked[40] == 0xbf85000bu && linked[43] == 0xbf850008u && linked[45] == 0xbb0e0001u && linked[46] == 0xbf82ffecu);
    Require(linked[50] == 0xbf078080u && linked[51] == 0xbf820001u && linked[52] == 0xbf068080u && linked[53] == 0xbe8e210eu && linked[54] == 0xbf82fff7u);
    Require(std::equal(first.begin(), first.end(), linked.begin() + 28) && std::equal(second.begin(), second.end(), linked.begin() + 47));
    const auto cfg = BuildCode(linked, linkedSwappc);
    const auto block = [&](std::uint32_t word) { return cfg.FindBlockByProgramCounter(word * 4u).id; };
    Require(cfg.FindBlockByProgramCounter(26u * 4u).terminator.trueBlock == block(28u));
    Require(cfg.FindBlockByProgramCounter(28u * 4u).terminator.trueBlock == block(27u));
    Require(cfg.FindBlockByProgramCounter(45u * 4u).terminator.trueBlock == block(47u));
    Require(cfg.FindBlockByProgramCounter(47u * 4u).terminator.trueBlock == block(46u));
    Require(cfg.unlinkedCallProgramCounters == std::vector<std::uint32_t>{53u * 4u});
    static_cast<void>(TranslateCode(linked, linkedSwappc));

    std::vector<std::uint32_t> large(0x3000u, 0xbf800000u);
    large.push_back(0xbe80200eu);
    const std::vector<std::vector<TableCallee>> far{{{0x10ull, large}, {0x20ull, large}, {0x30ull, large}, {0x40ull, first}}};
    const auto farLinked = LinkCalls(program, {}, {}, calls, far);
    const auto farCall = 12u + 3u * (16u + 0x3001u) + 14u;
    Require(farLinked.code[farCall] == 0xbb0e0001u && std::equal(first.begin(), first.end(), farLinked.code.begin() + farCall + 2u));
    SwappcInfo farSwappc = swappc;
    farSwappc.linkedCallFallbacks = farLinked.fallbacks;
    const auto farCfg = BuildCode(farLinked.code, farSwappc);
    Require(farCfg.FindBlockByProgramCounter(farCall * 4u).terminator.trueBlock == farCfg.FindBlockByProgramCounter((farCall + 2u) * 4u).id);
    Require(farCfg.FindBlockByProgramCounter((farCall + 2u) * 4u).terminator.trueBlock == farCfg.FindBlockByProgramCounter((farCall + 1u) * 4u).id);
    auto twoCallers = caller;
    twoCallers.pop_back();
    twoCallers.insert(twoCallers.end(), caller.begin(), caller.end());
    const auto twoProgram = RdnaInstructionDecoder{}.Decode(twoCallers);
    auto secondCall = calls[0];
    secondCall.programCounter += 40u;
    const std::array<TableCall, 2> twoCalls{calls[0], secondCall};
    const std::vector<std::vector<TableCallee>> twoCallees{far[0], {{0x50ull, first}, {0x60ull, second}}};
    const auto twoLinked = LinkCalls(twoProgram, {}, {}, twoCalls, twoCallees);
    const auto& two = twoLinked.code;
    const auto follow = [&](std::size_t word, std::size_t& hops) {
        hops = 0;
        while (two[word] >> 16u == 0xbf82u) {
            word = static_cast<std::size_t>(static_cast<std::int64_t>(word) + 1 + static_cast<std::int16_t>(two[word] & 0xffffu));
            ++hops;
        }
        return word;
    };
    std::size_t hops = 0;
    Require(follow(9u, hops) == 23u && hops == 1u && two[21] >> 16u == 0xbf82u && two[22] >> 16u == 0xbf82u);
    Require(follow(39u, hops) == 10u && hops == 1u);
    const auto secondChain = follow(19u, hops);
    Require(hops == 6u && two[secondChain] == 0xbf850007u && two[secondChain + 1u] == 0xbf07ff0eu && two[secondChain + 2u] == 0x50u);
    Require(two[secondChain + 15u] == 0xbb0e0001u && std::equal(first.begin(), first.end(), two.begin() + static_cast<std::ptrdiff_t>(secondChain + 17u)));
    Require(follow(secondChain + 16u, hops) == 20u && hops == 6u);
    SwappcInfo twoSwappc = swappc;
    twoSwappc.linkedCallFallbacks = twoLinked.fallbacks;
    const auto twoCfg = BuildCode(two, twoSwappc);
    Require(twoCfg.FindBlockByProgramCounter(static_cast<std::uint32_t>(secondChain + 15u) * 4u).terminator.trueBlock == twoCfg.FindBlockByProgramCounter(static_cast<std::uint32_t>(secondChain + 17u) * 4u).id);
    Require(twoCfg.unlinkedCallProgramCounters.size() == 2u);
    std::vector<std::uint32_t> oversized(0x8000u, 0xbf800000u);
    const std::vector<std::vector<TableCallee>> overflows{{{0x10ull, oversized}, {0x20ull, first}}};
    ExpectThrow("out of the 16-bit branch range", [&] { static_cast<void>(LinkCalls(program, {}, {}, calls, overflows)); });

    const std::vector<std::uint32_t> jumpTable{
        0xbe8c1f00u, 0x800cff0cu, 0x00000058u, 0x820d800du, 0x83928202u, 0xd4c40016u, 0x02000500u, 0x8f128212u, 0xf4000386u, 0x24000000u,
        0xbf8cc07fu, 0x7d820003u, 0x808c0e0cu, 0x828d800du, 0xbefd210cu, 0x7e020281u, 0xbf820003u, 0x7e020282u, 0xbf820001u, 0x7e020283u,
        0xe0700000u, 0x80000100u, 0xbf810000u, 0x00000020u, 0x00000018u, 0x00000010u};
    ExpectThrow("jumps through a table in its code", [&] { static_cast<void>(LinkCalls(RdnaInstructionDecoder{}.Decode(jumpTable), {}, {}, {}, {})); });
    const std::array<std::uint32_t, 1> compare{0xbf068008u};
    const auto comparesLink = Build({DecodeRdnaSopc(0u, compare, 0u), Call(4u, 8u, 1u), Sopp(8u, 1u), Sop1(12u, 0u, 0x20u, 8u)});
    Require(comparesLink.FindBlockByProgramCounter(0u).terminator.trueBlock == comparesLink.FindBlockByProgramCounter(12u).id);
    const auto readsFirst = Build({Sop1(0u, 0u, 3u, 8u), Call(4u, 8u, 1u), Sopp(8u, 1u), Sop1(12u, 0u, 0x20u, 8u)});
    Require(readsFirst.FindBlockByProgramCounter(0u).terminator.trueBlock == readsFirst.FindBlockByProgramCounter(12u).id);
    ExpectThrow("escapes the constant-offset call/return model", [] {
        Build({Sop1(0u, 0u, 3u, 8u), Call(4u, 8u, 2u), Sopp(8u, 2u, 0xfffdu), Sopp(12u, 1u), Sop1(16u, 0u, 0x20u, 8u)});
    });
    static_cast<void>(Build({Call(0u, 8u, 3u), Sop2(4u, 8u, 0x15u, 0u, 2u), Sop2(8u, 10u, 0x1bu, 8u, 2u), Sopp(12u, 1u), Sop1(16u, 0u, 0x20u, 8u)}));
    ExpectThrow("escapes the constant-offset call/return model", [] {
        Build({Call(0u, 8u, 3u), Sop2(4u, 8u, 0x0eu, 0u, 2u), Sop2(8u, 10u, 0x1bu, 8u, 2u), Sopp(12u, 1u), Sop1(16u, 0u, 0x20u, 8u)});
    });
}

enum class HoistedShape { InLoop, Hoisted, SecondWriterOnArm, LoadOnArmOnly, WriterAfterCall, WriterAfterLoop, CallOnArm, ConstantPointer, SkipsNullTarget, LongBranchWriter };

static std::vector<std::uint32_t> HoistedTableCaller(HoistedShape shape) {
    std::vector<std::uint32_t> code;
    const auto descriptorLoad = [&] { code.insert(code.end(), {0xf4081800u, 0xfa000010u, 0xbf8cc07fu}); };
    if (shape == HoistedShape::ConstantPointer) code.insert(code.end(), {0xbeea03ffu, 0xe0040000u, 0xbeeb038fu, 0xf4081835u, 0xfa000000u, 0xbf8cc07fu});
    else if (shape != HoistedShape::InLoop && shape != HoistedShape::LoadOnArmOnly) descriptorLoad();
    code.push_back(0xbf068002u);
    code.push_back(shape == HoistedShape::LoadOnArmOnly ? 0xbf850003u : 0xbf850001u);
    if (shape == HoistedShape::LoadOnArmOnly) descriptorLoad();
    else code.push_back(shape == HoistedShape::SecondWriterOnArm ? 0xbee00380u : shape == HoistedShape::CallOnArm ? 0xbb1e0000u : shape == HoistedShape::LongBranchWriter ? 0xbf820000u : 0x7e020280u);
    const auto arm = code.size() - 1u;
    code.push_back(0xbee5037eu);
    const auto loop = code.size();
    code.push_back(0xbee4037eu);
    if (shape == HoistedShape::InLoop) descriptorLoad();
    code.insert(code.end(), {0x7ed40500u, 0x7da4006au, 0x8f6a846au, 0xf4280430u, 0xd4000000u, 0xbf8cc07fu, 0xbe8e0310u, 0xbe8f0311u, 0xbe900312u, 0xbe910313u});
    if (shape == HoistedShape::SkipsNullTarget) code.insert(code.end(), {0xbf130e80u, 0xbf840001u});
    code.push_back(0xbe8e210eu);
    if (shape == HoistedShape::WriterAfterCall) code.push_back(0xbee10301u);
    code.push_back(0x897e7e64u);
    code.push_back(0xbf890000u | ((static_cast<std::uint32_t>(loop) - static_cast<std::uint32_t>(code.size()) - 1u) & 0xffffu));
    code.push_back(0xbefe0365u);
    if (shape == HoistedShape::WriterAfterLoop) code.push_back(0xbee00380u);
    code.push_back(0xbf810000u);
    if (shape == HoistedShape::CallOnArm) {
        code[arm] |= static_cast<std::uint32_t>(code.size() - arm - 1u);
        code.insert(code.end(), {0xbe80201eu, 0xbf9f0000u});
    }
    if (shape == HoistedShape::LongBranchWriter) {
        code[arm] |= static_cast<std::uint32_t>(code.size() - arm - 1u);
        const auto getpc = code.size() + 3u;
        code.insert(code.end(), {0xf4081800u, 0xfa000020u, 0xbf8cc07fu, 0xbe841f00u, 0x8084ff04u, static_cast<std::uint32_t>((getpc - arm) * 4u), 0xbe802004u, 0xbf9f0000u});
    }
    return code;
}

static void CheckHoistedTableCalls() {
    const std::vector<std::uint32_t> callee{0x4a0202c0u, 0xbe80200eu};
    const std::vector<std::vector<TableCallee>> callees{{{0x1122334455ull, callee}}};
    for (const auto shape : {HoistedShape::InLoop, HoistedShape::Hoisted, HoistedShape::SecondWriterOnArm, HoistedShape::LoadOnArmOnly, HoistedShape::WriterAfterCall, HoistedShape::WriterAfterLoop, HoistedShape::CallOnArm,
             HoistedShape::ConstantPointer, HoistedShape::SkipsNullTarget, HoistedShape::LongBranchWriter}) {
        const bool linkable = shape == HoistedShape::InLoop || shape == HoistedShape::Hoisted || shape == HoistedShape::WriterAfterLoop || shape == HoistedShape::ConstantPointer ||
                              shape == HoistedShape::SkipsNullTarget;
        for (const std::uint32_t waveSize : {32u, 64u}) {
            const auto code = HoistedTableCaller(shape);
            const auto program = RdnaInstructionDecoder{}.Decode(code);
            SwappcInfo swappc{false, 0u, 16u, waveSize};
            const auto calls = FindTableCalls(program, swappc);
            if (!linkable) {
                Require(calls.empty());
                ExpectThrow("not statically resolvable", [&] { static_cast<void>(BuildCode(code, swappc)); });
                continue;
            }
            const bool constant = shape == HoistedShape::ConstantPointer;
            Require(calls.size() == 1u && calls[0].userDataRegister == 0u && calls[0].descriptorOffset == (constant ? 0u : 0x10u) && calls[0].entryOffset == 0u && calls[0].entryStride == 16u);
            Require(calls[0].constantPointer == (constant ? std::optional<std::uint64_t>(0xfe0040000ull) : std::nullopt));
            const auto linked = LinkCalls(program, {}, {}, calls, callees);
            swappc.linkedCallFallbacks = linked.fallbacks;
            static_cast<void>(TranslateCode(linked.code, swappc));
        }
    }
    const std::vector<std::uint32_t> calledEntry{
        0xf4081800u, 0xfa000000u, 0xbf8cc07fu, 0xbb1e0001u, 0xbf810000u, 0xbf840003u, 0xf4081800u, 0xfa000010u,
        0xbf8cc07fu, 0xf4280430u, 0xfa000000u, 0xbf8cc07fu, 0xbe8e0310u, 0xbe8f0311u, 0xbe8e210eu, 0xbe80201eu, 0xbf9f0000u};
    Require(FindTableCalls(RdnaInstructionDecoder{}.Decode(calledEntry), SwappcInfo{false, 0u, 16u}).empty());
    const std::vector<std::uint32_t> fallsIntoEntry{
        0xbf840002u, 0xbb1e0003u, 0xbf810000u, 0xf4081800u, 0xfa000010u, 0xbf8cc07fu, 0xf4280430u, 0xfa000000u,
        0xbf8cc07fu, 0xbe8e0310u, 0xbe8f0311u, 0xbe8e210eu, 0xbe80201eu, 0xbf9f0000u};
    Require(FindTableCalls(RdnaInstructionDecoder{}.Decode(fallsIntoEntry), SwappcInfo{false, 0u, 16u}).empty());
    const std::vector<std::uint32_t> callBetween{
        0xf4081800u, 0xfa000010u, 0xbf8cc07fu, 0xbb1e0007u, 0xf4280430u, 0xfa000000u, 0xbf8cc07fu, 0xbe8e0310u,
        0xbe8f0311u, 0xbe8e210eu, 0xbf810000u, 0xbe80201eu, 0xbf9f0000u};
    const auto between = FindTableCalls(RdnaInstructionDecoder{}.Decode(callBetween), SwappcInfo{false, 0u, 16u});
    Require(between.size() == 1u && between[0].descriptorOffset == 0x10u);
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
            CheckUserDataCalls();
            CheckTableCalls();
            CheckHoistedTableCalls();
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
