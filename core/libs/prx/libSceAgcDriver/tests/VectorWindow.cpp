#include "Recompiler.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/InstructionTranslator.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

constexpr std::uint32_t MoveM0Two = 0xbefc0382u;
constexpr std::uint32_t MoveM0GroupX = 0xbefc0304u;
constexpr std::uint32_t MoveV4One = 0x7e0802f2u;
constexpr std::uint32_t MoveV4Two = 0x7e0802f4u;
constexpr std::uint32_t MoveV5Two = 0x7e0a02f4u;
constexpr std::uint32_t MoveV6Four = 0x7e0c02f6u;
constexpr std::uint32_t MovrelsV1V4 = 0x7e028704u;
constexpr std::uint32_t MovreldV4V0 = 0x7e088500u;
constexpr std::uint32_t MoveV1V5 = 0x7e020305u;
constexpr std::uint32_t EndProgram = 0xbf810000u;
constexpr std::uint32_t MoveV2Zero = 0x7e040280u;
constexpr std::uint32_t MoveV3Zero = 0x7e060280u;
constexpr std::uint32_t WritelaneV4 = 0xd7610004u;
constexpr std::uint32_t FromS8Lane0 = 0x00010008u;
constexpr std::uint32_t ReadlaneS8 = 0xd7600008u;
constexpr std::uint32_t FromV4Lane0 = 0x00010104u;
constexpr std::uint32_t ImageSampleLz2d = 0xf09c0f08u;
constexpr std::uint32_t SampleV8V2S0S8 = 0x00400802u;
constexpr std::uint32_t AddV8V1V8 = 0x4a101101u;
constexpr std::uint32_t BufferStoreDword = 0xe0700000u;
constexpr std::uint32_t StoreV8WithS12 = 0x80030800u;
constexpr std::uint32_t AndM0GroupXOne = 0x877c8104u;
constexpr std::uint32_t MovreldV2V0 = 0x7e048500u;
constexpr std::uint32_t MoveS16Two = 0xbe900382u;
constexpr std::uint32_t BranchForwardTwo = 0xbf820002u;
constexpr std::uint32_t SubtractS16One = 0x80908110u;
constexpr std::uint32_t CompareS16NotZero = 0xbf078010u;
constexpr std::uint32_t BranchBackFourOnScc = 0xbf85fffcu;

struct Shape {
    std::size_t functionArrays = 0;
    std::uint32_t arrayLength = 0;
    std::size_t accessChains = 0;
    std::size_t selects = 0;
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint32_t> RecompileProgram(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData) {
    const std::array<std::uint32_t, 7> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformVote, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 32;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 4u, {true, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 32;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    auto result = Recompile(request);
    Require(!result.spirv.empty(), "the relative-addressing program did not recompile");
    return std::vector<std::uint32_t>(result.spirv.Words().begin(), result.spirv.Words().end());
}

std::vector<std::uint32_t> RecompileProgram(std::span<const std::uint32_t> code) {
    const std::array<std::uint32_t, 4> userData{0x10000000u, 0u, 0x40u, 0x31016facu};
    return RecompileProgram(code, userData);
}

Shape Scan(std::span<const std::uint32_t> words) {
    Shape shape;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> constants;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> arrays;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> functionPointers;
    std::vector<std::uint32_t> windowVariables;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> spv::WordCountShift;
        const auto op = static_cast<spv::Op>(words[cursor] & spv::OpCodeMask);
        Require(count != 0 && cursor + count <= words.size(), "truncated SPIR-V instruction");
        if (op == spv::OpConstant && count == 4) constants.emplace_back(words[cursor + 2], words[cursor + 3]);
        if (op == spv::OpTypeArray) arrays.emplace_back(words[cursor + 1], words[cursor + 3]);
        if (op == spv::OpTypePointer && words[cursor + 2] == spv::StorageClassFunction) functionPointers.emplace_back(words[cursor + 1], words[cursor + 3]);
        if (op == spv::OpVariable && words[cursor + 3] == spv::StorageClassFunction) {
            for (const auto& [pointer, pointee] : functionPointers) {
                if (pointer != words[cursor + 1]) continue;
                for (const auto& [array, length] : arrays) {
                    if (array != pointee) continue;
                    ++shape.functionArrays;
                    windowVariables.push_back(words[cursor + 2]);
                    for (const auto& [constant, value] : constants) {
                        if (constant == length) shape.arrayLength = value;
                    }
                }
            }
        }
        if (op == spv::OpAccessChain) {
            for (const auto variable : windowVariables) {
                if (words[cursor + 3] == variable) ++shape.accessChains;
            }
        }
        if (op == spv::OpSelect) ++shape.selects;
        cursor += count;
    }
    return shape;
}

std::size_t WindowStores(std::span<const std::uint32_t> words) {
    std::vector<std::uint32_t> functionPointers;
    std::vector<std::uint32_t> pointers;
    std::size_t stores = 0;
    const auto contains = [](const std::vector<std::uint32_t>& ids, std::uint32_t id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> spv::WordCountShift;
        const auto op = static_cast<spv::Op>(words[cursor] & spv::OpCodeMask);
        Require(count != 0 && cursor + count <= words.size(), "truncated SPIR-V instruction");
        if (op == spv::OpTypePointer && words[cursor + 2] == spv::StorageClassFunction) functionPointers.push_back(words[cursor + 1]);
        if (op == spv::OpVariable && words[cursor + 3] == spv::StorageClassFunction && contains(functionPointers, words[cursor + 1])) pointers.push_back(words[cursor + 2]);
        if (op == spv::OpAccessChain && contains(pointers, words[cursor + 3])) pointers.push_back(words[cursor + 2]);
        if (op == spv::OpStore && contains(pointers, words[cursor + 1])) ++stores;
        cursor += count;
    }
    return stores;
}

IrProgram TranslateStage(ShaderStageKind stage, std::uint32_t waveSize, std::uint32_t base = 0u, std::uint32_t fetchStart = 0u, std::uint32_t fetchCount = 0u) {
    ShaderComputeInputInfo compute{};
    compute.threadIdsNum = 3;
    compute.threadsNum[0] = waveSize;
    ShaderVertexInputInfo vertex{};
    vertex.mesh.threadsNum[0] = waveSize;
    vertex.mesh.inputPrimitive = 1u;
    vertex.mesh.primitivesPerGroup = 1u;
    vertex.mesh.verticesPerGroup = 1u;
    vertex.mesh.esgsItemSize = 1u;
    ShaderPixelInputInfo pixel{};
    std::uint32_t next = 0u;
    for (const auto input : {PixelInput::PerspectiveCenter, PixelInput::PerspectiveCentroid, PixelInput::LinearCenter, PixelInput::LinearCentroid,
                            PixelInput::PositionX, PixelInput::PositionY, PixelInput::PositionZ, PixelInput::PositionW,
                            PixelInput::FrontFace, PixelInput::Ancillary, PixelInput::LineStipple, PixelInput::PositionFixedPoint}) {
        pixel.psInputVgpr[static_cast<std::uint32_t>(input)] = next;
        next += PixelInputVgprCount(input);
    }
    std::vector<std::uint32_t> code{MoveM0Two};
    if (fetchCount != 0u) {
        code.push_back(0xbe802102u);
        code.push_back(0x7e008700u | base);
        code.push_back(0x7e1002f2u);
        vertex.resourcesNum = 1;
        vertex.resourcesDst[0] = {static_cast<int>(fetchStart), static_cast<int>(fetchCount), 0, 0u};
    }
    code.push_back(0x7e008700u | base);
    code.push_back(EndProgram);
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    if (fetchCount != 0u) {
        const auto& call = decoded.instructions.at(1);
        Require(call.op == RdnaOpcode::SSwappcB64 && call.family == RdnaInstructionFamily::SOP1 && call.opcodeId == 0x21u &&
                call.destination.kind == RdnaOperandKind::ScalarRegister && call.destination.reg == 0u &&
                call.source0.kind == RdnaOperandKind::ScalarRegister && call.source0.reg == 2u,
                "external fetch fixture did not decode s_swappc_b64 s[0:1], s[2:3]");
    }
    const SwappcInfo swappc{fetchCount != 0u, 0u, 8u};
    const auto graph = GraphBuilder{}.Build(decoded, &swappc);
    Require(graph.hasFetchCall == (fetchCount != 0u), "stage fixture did not identify its external fetch call");
    TranslateOptions options{};
    options.stage = stage;
    options.waveSize = waveSize;
    options.userDataCount = 8u;
    options.inputInfo = {&vertex, &pixel, &compute};
    return InstructionTranslator{}.Translate(decoded, graph, options);
}

void RequireWindow(const IrProgram& program, std::uint32_t base, std::uint32_t limit, std::span<const std::uint32_t> initialized) {
    Require(program.Metadata().vectorWindowBase == base && program.Metadata().vectorWindowSize == limit - base, "relative window does not include exactly the initialized stage registers");
    std::array<bool, NumVectorRegs> stored{};
    for (const auto* instruction : program.BlockOrder().front()->Instructions()) {
        if (instruction->Opcode() == IrOpcode::StoreVectorWindow) {
            Require(instruction->Argument(0)->HasImmediate(), "stage input uses a dynamic window index");
            const auto index = base + instruction->Argument(0)->ImmediateU32();
            Require(index < limit, "stage input lies past the window");
            stored[index] = true;
        } else if (instruction->Opcode() == IrOpcode::SetVectorRegister) {
            const auto index = instruction->Argument(0)->Register().index;
            Require(index < base || !program.Metadata().vectorWindowIndexedWrites, "stage input escaped the relative window into SSA");
            if (index < base) stored[index] = true;
        }
    }
    for (const auto index : initialized) Require(stored[index], "stage input v" + std::to_string(index) + " was not initialized");
    bool bounded = false;
    for (const auto& block : program.Blocks()) {
        for (const auto* instruction : block->Instructions()) {
            if (instruction->Opcode() == IrOpcode::ULessThan32 && instruction->Argument(1)->HasImmediate() && instruction->Argument(1)->ImmediateU32() == limit - base) bounded = true;
        }
    }
    Require(bounded, "relative access did not use the final stage window bound");
}

template<typename TAction>
bool RunStage(const char* name, TAction action) {
    try {
        action();
        std::printf("vector window stage inputs: %s passed\n", name);
        return true;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "vector window stage inputs: %s: %s\n", name, error.what());
        return false;
    }
}

bool StageInputs() {
    bool passed = true;
    const auto stage = [&](const char* name, ShaderStageKind kind, std::uint32_t limit, std::initializer_list<std::uint32_t> initialized) {
        passed &= RunStage(name, [&] {
            for (const auto wave : {32u, 64u}) RequireWindow(TranslateStage(kind, wave), 0u, limit, {initialized.begin(), initialized.size()});
        });
    };
    stage("compute", ShaderStageKind::Compute, 3u, {0u, 1u, 2u});
    stage("vertex", ShaderStageKind::Vertex, 9u, {5u, 8u});
    stage("local", ShaderStageKind::Local, 6u, {2u, 3u, 5u});
    stage("tessellation control", ShaderStageKind::TessellationControl, 2u, {0u, 1u});
    stage("tessellation evaluation", ShaderStageKind::TessellationEvaluation, 9u, {0u, 5u, 6u, 7u, 8u});
    stage("mesh", ShaderStageKind::Mesh, 9u, {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u});
    stage("pixel", ShaderStageKind::Pixel, 16u, {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u});
    passed &= RunStage("SSA prefix", [] {
        const std::array<std::uint32_t, 3> local{2u, 3u, 5u};
        const std::array<std::uint32_t, 2> vertex{5u, 8u};
        for (const auto wave : {32u, 64u}) {
            RequireWindow(TranslateStage(ShaderStageKind::Local, wave, 4u), 4u, 6u, local);
            RequireWindow(TranslateStage(ShaderStageKind::Vertex, wave, 16u), 16u, 17u, vertex);
        }
    });
    passed &= RunStage("external fetch", [] {
        const std::array<std::uint32_t, 2> vertex{5u, 8u};
        for (const auto wave : {32u, 64u}) {
            for (const auto start : {20u, 252u}) {
                for (const auto count : {1u, 2u, 3u, 4u}) {
                    const auto program = TranslateStage(ShaderStageKind::Vertex, wave, 0u, start, count);
                    RequireWindow(program, 0u, start + count, vertex);
                    std::uint32_t fetched = 0u;
                    std::uint32_t boundedReads = 0u;
                    for (const auto& block : program.Blocks()) {
                        for (const auto* instruction : block->Instructions()) {
                            if (instruction->Opcode() == IrOpcode::StoreVectorWindow) {
                                const auto* value = instruction->Argument(1);
                                if (value->Opcode() == IrOpcode::SelectU32) value = value->Argument(1);
                                if (value->Opcode() == IrOpcode::GetAttribute) {
                                    Require(instruction->Argument(0)->ImmediateU32() == start + fetched && value->Argument(1)->ImmediateU32() == fetched,
                                            "external fetch did not initialize its entire window span");
                                    ++fetched;
                                }
                            }
                            if (instruction->Opcode() == IrOpcode::ULessThan32 && instruction->Argument(1)->HasImmediate() && instruction->Argument(1)->ImmediateU32() == start + count) ++boundedReads;
                        }
                    }
                    Require(fetched == count && boundedReads == 2u, "relative accesses to the external fetch disagree about the window");
                }
            }
        }
    });
    if (passed) std::puts("Vector window stage input and fetch tests passed");
    return passed;
}

bool run(const char* name, bool (*test)()) {
    try {
        if (test()) return true;
        std::fprintf(stderr, "%s\n", name);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", name, error.what());
    }
    return false;
}

}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--stage-inputs-only") return StageInputs() ? 0 : 1;
    if (argc != 1) return 2;
    bool passed = true;

    passed &= run("observable relative reads and writes index a function-scope window", [] {
        const std::array<std::uint32_t, 12> code{MoveM0GroupX, MoveV4One, MoveV5Two, MoveV6Four, MovrelsV1V4,
            0xe0700000u, 0x80000100u, MovreldV4V0, MoveV1V5, 0xe0700004u, 0x80000100u, EndProgram};
        const auto shape = Scan(RecompileProgram(code));
        if (shape.functionArrays != 1 || shape.arrayLength != 3 || shape.accessChains < 2) std::fprintf(stderr, "  window shape: arrays %zu length %u access chains %zu selects %zu\n", shape.functionArrays, shape.arrayLength, shape.accessChains, shape.selects);
        return shape.functionArrays == 1 && shape.arrayLength == 3 && shape.accessChains >= 2;
    });

    passed &= run("a program without relative addressing keeps its registers in SSA values", [] {
        const std::array<std::uint32_t, 7> code{MoveV4One, MoveV5Two, MoveV6Four, MoveV1V5, 0xe0700000u, 0x80000100u, EndProgram};
        const auto shape = Scan(RecompileProgram(code));
        return shape.functionArrays == 0 && shape.accessChains == 0;
    });

    passed &= run("a sampler dword spilled into a window register with ordinary writes keeps its definition past a relative write", [] {
        const std::array<std::uint32_t, 16> userData{};
        const std::array<std::uint32_t, 14> spilled{AndM0GroupXOne, MoveV2Zero, MoveV3Zero, MoveV4One, WritelaneV4, FromS8Lane0, MovreldV2V0, ReadlaneS8, FromV4Lane0, ImageSampleLz2d, SampleV8V2S0S8, BufferStoreDword, StoreV8WithS12, EndProgram};
        return Scan(RecompileProgram(spilled, userData)).functionArrays == 1;
    });

    passed &= run("a sampler dword spilled into a window register and overwritten by an ordinary write before its v_readlane stays a run-time value", [] {
        const std::array<std::uint32_t, 16> userData{};
        const std::array<std::uint32_t, 14> overwritten{AndM0GroupXOne, MoveV2Zero, MoveV3Zero, WritelaneV4, FromS8Lane0, MovreldV2V0, MoveV4One, ReadlaneS8, FromV4Lane0, ImageSampleLz2d, SampleV8V2S0S8, BufferStoreDword, StoreV8WithS12, EndProgram};
        try {
            RecompileProgram(overwritten, userData);
        } catch (const std::exception& error) {
            return std::string_view(error.what()).find("GetSamplerResource dword 0 is not a valid runtime value; chain: ReadLane(LoadVectorWindow(Immediate), Immediate)") != std::string_view::npos;
        }
        return false;
    });

    passed &= run("a sampler dword spilled into a window register before a loop entered at its condition keeps its definition past the loop's relative write", [] {
        const std::array<std::uint32_t, 16> userData{};
        const std::array<std::uint32_t, 18> code{AndM0GroupXOne, MoveV2Zero, MoveV3Zero, WritelaneV4, FromS8Lane0, MoveS16Two, BranchForwardTwo, MovreldV2V0, SubtractS16One, CompareS16NotZero, BranchBackFourOnScc, ReadlaneS8, FromV4Lane0, ImageSampleLz2d, SampleV8V2S0S8, BufferStoreDword, StoreV8WithS12, EndProgram};
        return Scan(RecompileProgram(code, userData)).functionArrays == 1;
    });

    passed &= run("a sampler dword spilled into a window register with ordinary writes before a loop entered at its condition keeps its definition past the loop's relative write", [] {
        const std::array<std::uint32_t, 16> userData{};
        const std::array<std::uint32_t, 19> code{AndM0GroupXOne, MoveV2Zero, MoveV3Zero, MoveV4One, WritelaneV4, FromS8Lane0, MoveS16Two, BranchForwardTwo, MovreldV2V0, SubtractS16One, CompareS16NotZero, BranchBackFourOnScc, ReadlaneS8, FromV4Lane0, ImageSampleLz2d, SampleV8V2S0S8, BufferStoreDword, StoreV8WithS12, EndProgram};
        return Scan(RecompileProgram(code, userData)).functionArrays == 1;
    });

    passed &= run("a sampler dword spilled into a relatively read register keeps its definition through v_readlane", [] {
        const std::array<std::uint32_t, 14> code{MoveM0GroupX, MoveV2Zero, MoveV3Zero, WritelaneV4, FromS8Lane0, MovrelsV1V4, ReadlaneS8, FromV4Lane0, ImageSampleLz2d, SampleV8V2S0S8, AddV8V1V8, BufferStoreDword, StoreV8WithS12, EndProgram};
        const std::array<std::uint32_t, 16> userData{};
        const auto shape = Scan(RecompileProgram(code, userData));
        return shape.functionArrays == 1;
    });

    passed &= run("a window register written twice before a relative read keeps only its last window store", [] {
        const std::array<std::uint32_t, 9> overwritten{MoveM0GroupX, MoveV4One, MoveV4Two, MoveV5Two, MoveV6Four, MovrelsV1V4, 0xe0700000u, 0x80000100u, EndProgram};
        const std::array<std::uint32_t, 8> once{MoveM0GroupX, MoveV4Two, MoveV5Two, MoveV6Four, MovrelsV1V4, 0xe0700000u, 0x80000100u, EndProgram};
        const auto twice = WindowStores(RecompileProgram(overwritten));
        const auto single = WindowStores(RecompileProgram(once));
        if (twice != single || single < 3u) std::fprintf(stderr, "  window stores: written twice %zu, written once %zu\n", twice, single);
        return twice == single && single >= 3u;
    });

    if (passed) std::puts("Vector window shape and SSA control tests passed");
    return passed ? 0 : 1;
}
