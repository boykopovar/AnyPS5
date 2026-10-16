#include "SpirvBackend/SpirvOptimizer.hpp"
#include "SpirvBackend/SpirvSpecialization.hpp"
#include <spirv-tools/libspirv.hpp>
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;
constexpr std::uint32_t Vulkan11 = 0x00401000u;
constexpr std::uint32_t Vulkan13 = 0x00403000u;
constexpr std::uint32_t Spirv11 = 0x00010100u;
constexpr std::uint32_t Spirv13 = 0x00010300u;
constexpr std::uint32_t Spirv16 = 0x00010600u;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message.c_str());
        ++failures;
    }
}

struct NarrowType {
    const char* name;
    unsigned width;
    bool signedInteger;
    bool floating;
    const char* arithmeticCapability;
    const char* unrelatedCapability;
};

constexpr std::array Types{
    NarrowType{"u8", 8, false, false, "Int8", "Int16"},
    NarrowType{"i8", 8, true, false, "Int8", "Int16"},
    NarrowType{"u16", 16, false, false, "Int16", "Int8"},
    NarrowType{"i16", 16, true, false, "Int16", "Int8"},
    NarrowType{"f16", 16, false, true, "Float16", "Int8"},
};

enum class ArithmeticSupport { StorageOnly, Explicit, ImpliedInt8 };

std::string moduleSource(const NarrowType& type, bool vector, ArithmeticSupport support) {
    const auto width = std::to_string(type.width);
    const std::string typeOp = type.floating ? "OpTypeFloat " : "OpTypeInt ";
    const std::string signedness = type.floating ? "" : (type.signedInteger ? " 1" : " 0");
    const std::string conversion = type.floating ? "OpFConvert" : (type.signedInteger ? "OpSConvert" : "OpUConvert");
    const std::string wide = type.floating || type.signedInteger ? "%wide" : "%index";
    std::string text = "OpCapability Shader\nOpCapability ";
    text += type.width == 8 ? "StorageBuffer8BitAccess\n" : "StorageBuffer16BitAccess\n";
    text += "OpCapability " + std::string(type.unrelatedCapability) + "\n";
    if (support == ArithmeticSupport::Explicit) text += "OpCapability " + std::string(type.arithmeticCapability) + "\n";
    if (support == ArithmeticSupport::ImpliedInt8) {
        text += "OpCapability DotProductInput4x8BitKHR\nOpExtension \"SPV_KHR_integer_dot_product\"\n";
    }
    text += type.width == 8 ? "OpExtension \"SPV_KHR_8bit_storage\"\n" : "OpExtension \"SPV_KHR_16bit_storage\"\n";
    text += R"(OpExtension "SPV_KHR_storage_buffer_storage_class"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpDecorate %buffer DescriptorSet 0
OpDecorate %buffer Binding 0
%void = OpTypeVoid
%function = OpTypeFunction %void
%index = OpTypeInt 32 0
%zero = OpConstant %index 0
)";
    if (wide == "%wide") text += "%wide = " + typeOp + "32" + signedness + "\n";
    text += "%small = " + typeOp + width + signedness + "\n";
    text += "%zeroValue = OpConstant " + wide + " 0\n";
    text += "%value = OpConstant " + wide + " " + std::string(type.floating ? "1.5" : (type.signedInteger ? "-257" : "257")) + "\n";
    if (vector) {
        text += "%wideValue = OpTypeVector " + wide + " 2\n%narrow = OpTypeVector %small 2\n";
        text += "%input = OpConstantComposite %wideValue %zeroValue %value\n";
    }
    const std::string narrow = vector ? "%narrow" : "%small";
    text += "%block = OpTypeStruct " + narrow + "\n";
    text += "%blockPointer = OpTypePointer StorageBuffer %block\n";
    text += "%pointer = OpTypePointer StorageBuffer " + narrow + "\n";
    text += R"(%buffer = OpVariable %blockPointer StorageBuffer
%main = OpFunction %void None %function
%entry = OpLabel
%address = OpAccessChain %pointer %buffer %zero
%dead = OpIAdd %index %zero %zero
)";
    if (vector) {
        text += "%converted = " + conversion + " " + narrow + " %input\n";
        text += "OpStore %address %converted\n";
    } else {
        text += "%convertedZero = " + conversion + " " + narrow + " %zeroValue\n";
        text += "OpStore %address %convertedZero\n";
        text += "%converted = " + conversion + " " + narrow + " %value\n";
        text += "OpStore %address %converted\n";
    }
    return text + "OpReturn\nOpFunctionEnd\n";
}

std::vector<std::uint32_t> assemble(const std::string& source) {
    spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_1);
    tools.SetMessageConsumer([](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
        std::fprintf(stderr, "SPIRV-Tools: %s\n", message);
    });
    std::vector<std::uint32_t> words;
    check(tools.Assemble(source, &words), "test shader did not assemble");
    return words;
}

std::size_t opcodeCount(const std::vector<std::uint32_t>& words, spv::Op opcode) {
    std::size_t count = 0;
    for (std::size_t offset = 5; offset < words.size(); offset += words[offset] >> spv::WordCountShift) {
        if ((words[offset] & spv::OpCodeMask) == static_cast<std::uint32_t>(opcode)) ++count;
    }
    return count;
}

std::set<std::uint32_t> capabilities(const std::vector<std::uint32_t>& words) {
    std::set<std::uint32_t> result;
    for (std::size_t offset = 5; offset < words.size(); offset += words[offset] >> spv::WordCountShift) {
        if ((words[offset] & spv::OpCodeMask) == spv::OpCapability) result.insert(words[offset + 1]);
    }
    return result;
}

std::vector<std::uint32_t> swapByteOrder(std::vector<std::uint32_t> words) {
    for (auto& word : words) {
        word = (word >> 24u) | ((word & 0x00ff0000u) >> 8u) |
               ((word & 0x0000ff00u) << 8u) | (word << 24u);
    }
    return words;
}

void testNarrowConversions(bool optimizationEnabled) {
    spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_1);
    for (const auto& type : Types) {
        for (const bool vector : {false, true}) {
            for (const auto support : {ArithmeticSupport::StorageOnly, ArithmeticSupport::Explicit, ArithmeticSupport::ImpliedInt8}) {
                if (support == ArithmeticSupport::ImpliedInt8 && (type.width != 8 || !vector)) continue;
                const bool arithmetic = support != ArithmeticSupport::StorageOnly;
                const std::string name = std::string(type.name) + (vector ? " vector" : " scalar") +
                    (support == ArithmeticSupport::ImpliedInt8 ? " implied Int8" : (arithmetic ? " arithmetic" : " storage only"));
                auto native = assemble(moduleSource(type, vector, support));
                for (const auto& [version, swapped] : {std::pair{Spirv13, false}, std::pair{Spirv11, false}, std::pair{Spirv11, true}}) {
                    native[1] = version;
                    const auto input = swapped ? swapByteOrder(native) : native;
                    const auto label = name + (version == Spirv13 ? " SPIR-V 1.3" : " SPIR-V 1.1") + (swapped ? " swapped" : " native");
                    if (!validator.Validate(input)) {
                        check(false, label + ": invalid test input");
                        continue;
                    }
                    try {
                        const auto output = ShaderRecompiler::ValidateAndOptimizeSpirv(input, Vulkan11, Spirv13, false);
                        if (!validator.Validate(output)) {
                            check(false, label + ": optimizer returned invalid SPIR-V");
                            continue;
                        }
                        const auto normalized = output.front() == spv::MagicNumber ? output : swapByteOrder(output);
                        check(capabilities(normalized) == capabilities(native), label + ": optimizer changed device capabilities");
                        check(opcodeCount(normalized, spv::OpStore) == (vector ? 1u : 2u), label + ": optimizer lost buffer stores");
                        if (!optimizationEnabled) {
                            check(output == input, label + ": none mode changed the shader");
                            const auto specialized = ShaderRecompiler::ValidateAndOptimizeSpirv(input, Vulkan11, Spirv13, false, true, true);
                            check(specialized == input, label + ": none mode changed the specialized shader");
                        } else {
                            check(opcodeCount(normalized, spv::OpIAdd) == 0, label + ": optimizer did not remove dead arithmetic");
                            if (arithmetic && !type.floating) {
                                const auto conversion = type.signedInteger ? spv::OpSConvert : spv::OpUConvert;
                                check(opcodeCount(normalized, conversion) == 0, label + ": safe constant folding was disabled");
                            }
                        }
                    } catch (const std::exception& error) {
                        check(false, label + ": " + error.what());
                    }
                }
            }
        }
    }
}

void expectRejected(const std::vector<std::uint32_t>& words, std::uint32_t vulkanVersion, std::uint32_t spirvVersion, const char* reason, const char* diagnostic = nullptr) {
    try {
        static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(words, vulkanVersion, spirvVersion, false));
        check(false, std::string("accepted ") + reason);
    } catch (const std::exception& error) {
        if (diagnostic != nullptr) check(std::string(error.what()).starts_with(diagnostic), std::string(reason) + ": unexpected rejection: " + error.what());
    }
}

void testSpecializedSelectionExit() {
    for (const bool useSwitch : {false, true}) {
        const std::string source = std::string(R"(OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main" %index
OpExecutionMode %main LocalSize 8 1 1
OpDecorate %index BuiltIn LocalInvocationIndex
%void = OpTypeVoid
%bool = OpTypeBool
%uint = OpTypeInt 32 0
%input = OpTypePointer Input %uint
%private = OpTypePointer Private %uint
%function = OpTypeFunction %void
%zero = OpConstant %uint 0
%true = OpConstantTrue %bool
%index = OpVariable %input Input
%output = OpVariable %private Private
%main = OpFunction %void None %function
%entry = OpLabel
%lane = OpLoad %uint %index
%condition = OpIEqual %bool %lane %zero
OpSelectionMerge %exit None
)") + (useSwitch ? "OpSwitch %zero %body 1 %dead\n" : "OpBranchConditional %true %body %dead\n") + R"(%body = OpLabel
OpBranchConditional %condition %exit %nested
%nested = OpLabel
OpSelectionMerge %join None
OpBranchConditional %condition %write %join
%write = OpLabel
OpStore %output %lane
OpBranch %join
%join = OpLabel
OpBranch %exit
%dead = OpLabel
OpStore %output %zero
OpBranch %exit
%exit = OpLabel
OpReturn
OpFunctionEnd
)";
        try {
            const auto words = assemble(source);
            static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(words, Vulkan11, Spirv13, false, false));
            const auto specialized = ShaderRecompiler::SpecializeSpirv(words);
            static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(specialized, Vulkan11, Spirv13, false, false));
            check(opcodeCount(specialized, spv::OpStore) == 1u, "specialization retained the dead selection arm");
            check(ShaderRecompiler::SpecializeSpirv(specialized) == specialized, "selection exit specialization is not stable");
        } catch (const std::exception& error) {
            check(false, std::string("specialized selection exit: ") + error.what());
        }
    }
}

void testSpecializedDeadContinue() {
    for (const bool folded : {false, true}) {
        const std::string source = std::string(R"(OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 8 1 1
%void = OpTypeVoid
%bool = OpTypeBool
%uint = OpTypeInt 32 0
%private = OpTypePointer Private %uint
%function = OpTypeFunction %void
%zero = OpConstant %uint 0
%one = OpConstant %uint 1
%false = OpConstantFalse %bool
%output = OpVariable %private Private
%main = OpFunction %void None %function
%entry = OpLabel
OpBranch %header
%header = OpLabel
%count = OpPhi %uint %zero %entry %next %continue
OpLoopMerge %exit %continue None
OpBranch %body
%body = OpLabel
OpStore %output %count
)") + (folded ? "OpBranchConditional %false %continue %exit\n" : "OpBranch %exit\n") + R"(%continue = OpLabel
%next = OpIAdd %uint %count %one
OpBranch %header
%exit = OpLabel
OpReturn
OpFunctionEnd
)";
        const std::string label = folded ? "loop exit on a constant condition" : "loop with an unreachable continue target";
        try {
            const auto words = assemble(source);
            static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(words, Vulkan11, Spirv13, false, false));
            const auto specialized = ShaderRecompiler::SpecializeSpirv(words);
            static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(specialized, Vulkan11, Spirv13, false, false));
            check(opcodeCount(specialized, spv::OpLoopMerge) == 1u, label + ": specialization lost the loop");
            check(opcodeCount(specialized, spv::OpIAdd) == 0u, label + ": specialization retained the dead continue block");
            check(ShaderRecompiler::SpecializeSpirv(specialized) == specialized, label + ": specialization is not stable");
        } catch (const std::exception& error) {
            check(false, label + ": " + error.what());
        }
    }
}

void testInvalidInputIsRejected() {
    const auto valid = assemble(moduleSource(Types.front(), false, ArithmeticSupport::StorageOnly));
    expectRejected(valid, 0x00400000u, Spirv13, "SPIR-V 1.3 with Vulkan 1.0");
    expectRejected(valid, Vulkan11, 0x00010200u, "a module newer than the requested SPIR-V version");
    expectRejected(valid, 0u, Spirv13, "an unsupported Vulkan version");
    auto invalid = moduleSource(Types.front(), false, ArithmeticSupport::StorageOnly);
    invalid.insert(invalid.find("%main = OpFunction"), "%illegal = OpConstantNull %small\n");
    expectRejected(assemble(invalid), Vulkan11, Spirv13, "an illegal storage-only narrow constant even when unused");
    const auto rejectMalformed = [](const std::vector<std::uint32_t>& words, const char* reason) {
        auto native = words;
        if (native.size() > 1) native[1] = Spirv11;
        constexpr auto diagnostic = "SPIR-V validation before optimization failed:";
        expectRejected(native, Vulkan11, Spirv13, reason, diagnostic);
        expectRejected(swapByteOrder(native), Vulkan11, Spirv13, (std::string(reason) + " (swapped)").c_str(), diagnostic);
    };
    for (std::size_t length = 0; length < 5; ++length) {
        rejectMalformed(std::vector<std::uint32_t>(valid.begin(), valid.begin() + length), "an incomplete module header");
    }
    auto malformed = valid;
    malformed[0] = 0;
    rejectMalformed(malformed, "an invalid module magic");
    malformed = valid;
    malformed[5] = spv::OpCapability;
    rejectMalformed(malformed, "a zero-length SPIR-V instruction");
    malformed[5] = (0xffffu << spv::WordCountShift) | spv::OpCapability;
    rejectMalformed(malformed, "an instruction extending beyond the module");
    for (const auto& instruction : {
             std::vector<std::uint32_t>{(1u << spv::WordCountShift) | spv::OpCapability},
             std::vector<std::uint32_t>{(3u << spv::WordCountShift) | spv::OpTypeInt, 1u, 8u},
             std::vector<std::uint32_t>{(2u << spv::WordCountShift) | spv::OpTypeFloat, 1u}}) {
        malformed.assign(valid.begin(), valid.begin() + 5);
        malformed.insert(malformed.end(), instruction.begin(), instruction.end());
        rejectMalformed(malformed, "truncated capability or type operands");
    }
}

void testSpecializedSelectionExitSpirv16() {
    const std::string source = R"(OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main" %index %output
OpExecutionMode %main LocalSize 8 1 1
OpDecorate %index BuiltIn LocalInvocationIndex
%void = OpTypeVoid
%bool = OpTypeBool
%uint = OpTypeInt 32 0
%input = OpTypePointer Input %uint
%private = OpTypePointer Private %uint
%function = OpTypeFunction %void
%zero = OpConstant %uint 0
%true = OpConstantTrue %bool
%index = OpVariable %input Input
%output = OpVariable %private Private
%main = OpFunction %void None %function
%entry = OpLabel
%lane = OpLoad %uint %index
%condition = OpIEqual %bool %lane %zero
OpSelectionMerge %exit None
OpBranchConditional %true %body %dead
%body = OpLabel
OpBranchConditional %condition %exit %nested
%nested = OpLabel
OpSelectionMerge %join None
OpBranchConditional %condition %write %join
%write = OpLabel
OpStore %output %lane
OpBranch %join
%join = OpLabel
%sum = OpIAdd %uint %lane %lane
OpBranch %exit
%dead = OpLabel
OpBranch %exit
%exit = OpLabel
%value = OpPhi %uint %zero %body %sum %join %zero %dead
OpStore %output %value
OpReturn
OpFunctionEnd
)";
    try {
        auto words = assemble(source);
        words[1] = Spirv16;
        static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(words, Vulkan13, Spirv16, false, false));
        const auto specialized = ShaderRecompiler::SpecializeSpirv(words);
        static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(specialized, Vulkan13, Spirv16, false, false));
        check(ShaderRecompiler::SpecializeSpirv(specialized) == specialized, "SPIR-V 1.6 selection exit specialization is not stable");
    } catch (const std::exception& error) {
        check(false, std::string("specialized SPIR-V 1.6 selection exit: ") + error.what());
    }
}

void testLongControlFlow(bool optimizationEnabled, bool large) {
    const char* mode = std::getenv("APS5_SPIRV_OPT");
    const bool full = mode != nullptr && std::string(mode) == "full";
    const std::size_t blocks = large && optimizationEnabled && !full ? 512 : 8;
    std::string source = R"(OpCapability Shader
OpCapability StorageBuffer8BitAccess
OpExtension "SPV_KHR_8bit_storage"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 1 1 1
OpDecorate %array ArrayStride 4
OpMemberDecorate %buffer 0 Offset 0
OpDecorate %buffer Block
OpDecorate %data DescriptorSet 0
OpDecorate %data Binding 0
OpDecorate %byteArray ArrayStride 1
OpMemberDecorate %byteBuffer 0 Offset 0
OpDecorate %byteBuffer Block
OpDecorate %bytes DescriptorSet 0
OpDecorate %bytes Binding 1
%void = OpTypeVoid
%uint = OpTypeInt 32 0
%boolean = OpTypeBool
%byte = OpTypeInt 8 0
%byteArray = OpTypeRuntimeArray %byte
%byteBuffer = OpTypeStruct %byteArray
%byteBufferPointer = OpTypePointer StorageBuffer %byteBuffer
%bytePointer = OpTypePointer StorageBuffer %byte
%array = OpTypeRuntimeArray %uint
%buffer = OpTypeStruct %array
%bufferPointer = OpTypePointer StorageBuffer %buffer
%valuePointer = OpTypePointer StorageBuffer %uint
%function = OpTypeFunction %void
%data = OpVariable %bufferPointer StorageBuffer
%bytes = OpVariable %byteBufferPointer StorageBuffer
%zero = OpConstant %uint 0
%one = OpConstant %uint 1
%two = OpConstant %uint 2
)";
    for (std::size_t i = 0; i < blocks * 4; ++i) source += "%index" + std::to_string(i) + " = OpConstant %uint " + std::to_string(i + 1) + "\n";
    source += "%main = OpFunction %void None %function\n%entry = OpLabel\n%input = OpAccessChain %valuePointer %data %zero %zero\n%seed = OpLoad %uint %input Volatile\n%condition = OpINotEqual %boolean %seed %zero\nOpBranch %block0\n";
    for (std::size_t i = 0; i < blocks; ++i) {
        const auto number = std::to_string(i);
        const auto previous = i == 0 ? "%seed" : "%next" + std::to_string(i - 1);
        source += "%block" + number + " = OpLabel\n%sum" + number + " = OpIAdd %uint " + previous + " %one\n";
        source += "%duplicate" + number + " = OpIAdd %uint " + previous + " %one\n%shared" + number + " = OpIAdd %uint %seed %one\n";
        for (std::size_t j = 0; j < 3; ++j) {
            const auto index = std::to_string(i * 4 + j);
            source += "%slot" + index + " = OpAccessChain %valuePointer %data %zero %index" + index + "\n";
            source += "OpStore %slot" + index + " %" + (j == 0 ? "sum" : j == 1 ? "duplicate" : "shared") + number + " Volatile\n";
        }
        const auto branchIndex = std::to_string(i * 4 + 3);
        source += "%branchSlot" + number + " = OpAccessChain %valuePointer %data %zero %index" + branchIndex + "\n";
        source += "OpSelectionMerge %join" + number + " None\nOpBranchConditional %condition %left" + number + " %right" + number + "\n";
        source += "%left" + number + " = OpLabel\n%leftValue" + number + " = OpIAdd %uint %sum" + number + " %one\nOpStore %branchSlot" + number + " %leftValue" + number + " Volatile\nOpBranch %join" + number + "\n";
        source += "%right" + number + " = OpLabel\n%rightCopy" + number + " = OpIAdd %uint %sum" + number + " %one\nOpStore %branchSlot" + number + " %rightCopy" + number + " Volatile\n";
        source += "%rightValue" + number + " = OpIAdd %uint %sum" + number + " %two\nOpStore %branchSlot" + number + " %rightValue" + number + " Volatile\nOpBranch %join" + number + "\n";
        source += "%join" + number + " = OpLabel\n%next" + number + " = OpPhi %uint %leftValue" + number + " %left" + number + " %rightValue" + number + " %right" + number + "\n";
        if (i + 1 == blocks) source += "%byteSlot = OpAccessChain %bytePointer %bytes %zero %zero\n%truncated = OpUConvert %byte %next" + number + "\nOpStore %byteSlot %truncated Volatile\nOpReturn\n";
        else source += "OpBranch %block" + std::to_string(i + 1) + "\n";
    }
    source += "OpFunctionEnd\n";
    const auto input = assemble(source);
    const auto output = ShaderRecompiler::ValidateAndOptimizeSpirv(input, Vulkan11, Spirv13, false);
    const auto stores = opcodeCount(output, spv::OpStore);
    check(stores >= blocks * 4 + 1 && stores <= blocks * 6 + 1, "long control-flow optimization discarded observable stores");
    const auto adds = opcodeCount(output, spv::OpIAdd);
    if (!optimizationEnabled) check(output == input, "disabled optimization changed the long control-flow module");
    else check(adds >= blocks && adds <= blocks * 4, "control-flow optimization lost arithmetic or retained redundant values");
}

std::size_t repeatedBlockBallots(const std::vector<std::uint32_t>& words) {
    std::size_t repeats = 0;
    std::set<std::array<std::uint32_t, 3>> seen;
    for (std::size_t offset = 5; offset < words.size(); offset += words[offset] >> spv::WordCountShift) {
        const auto opcode = words[offset] & spv::OpCodeMask;
        if (opcode == spv::OpLabel || opcode == spv::OpFunctionCall) seen.clear();
        if (opcode == spv::OpGroupNonUniformBallot && !seen.insert({words[offset + 1], words[offset + 3], words[offset + 4]}).second) ++repeats;
    }
    return repeats;
}

std::string ballotModuleSource(const std::string& body) {
    return R"(OpCapability Shader
OpCapability GroupNonUniformBallot
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main"
OpExecutionMode %main LocalSize 32 1 1
OpDecorate %runtime ArrayStride 16
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpDecorate %buffer DescriptorSet 0
OpDecorate %buffer Binding 0
%void = OpTypeVoid
%function = OpTypeFunction %void
%bool = OpTypeBool
%uint = OpTypeInt 32 0
%uvec4 = OpTypeVector %uint 4
%runtime = OpTypeRuntimeArray %uvec4
%block = OpTypeStruct %runtime
%blockPointer = OpTypePointer StorageBuffer %block
%elementPointer = OpTypePointer StorageBuffer %uvec4
%buffer = OpVariable %blockPointer StorageBuffer
%zero = OpConstant %uint 0
%one = OpConstant %uint 1
%two = OpConstant %uint 2
%three = OpConstant %uint 3
%subgroup = OpConstant %uint 3
%helper = OpFunction %void None %function
%helperEntry = OpLabel
OpReturn
OpFunctionEnd
%main = OpFunction %void None %function
%entry = OpLabel
%slot0 = OpAccessChain %elementPointer %buffer %zero %zero
%slot1 = OpAccessChain %elementPointer %buffer %zero %one
%slot2 = OpAccessChain %elementPointer %buffer %zero %two
%slot3 = OpAccessChain %elementPointer %buffer %zero %three
%loaded = OpLoad %uvec4 %slot0
%value = OpCompositeExtract %uint %loaded 0
%predicate = OpINotEqual %bool %value %zero
%other = OpIEqual %bool %value %one
)" + body + "OpReturn\nOpFunctionEnd\n";
}

void testBallotsMergedIntoOneBlockShareOneBallot(bool optimizationEnabled) {
    const auto merged = assemble(ballotModuleSource(R"(%first = OpGroupNonUniformBallot %uvec4 %subgroup %predicate
OpStore %slot1 %first
OpBranch %next
%next = OpLabel
%second = OpGroupNonUniformBallot %uvec4 %subgroup %predicate
OpStore %slot2 %second
)"));
    const auto output = ShaderRecompiler::ValidateAndOptimizeSpirv(merged, Vulkan11, Spirv13, false);
    check(repeatedBlockBallots(output) == 0, "a block merged by the optimizer ballots one predicate twice");
    check(opcodeCount(output, spv::OpStore) == 2, "ballot collapse lost a store");
    if (optimizationEnabled) check(opcodeCount(output, spv::OpGroupNonUniformBallot) == 1, "blocks merged by the optimizer kept both ballots");

    const auto oneBlock = assemble(ballotModuleSource(R"(%first = OpGroupNonUniformBallot %uvec4 %subgroup %predicate
%different = OpGroupNonUniformBallot %uvec4 %subgroup %other
%second = OpGroupNonUniformBallot %uvec4 %subgroup %predicate
OpStore %slot1 %first
OpStore %slot2 %different
OpStore %slot3 %second
)"));
    for (const bool optimize : {false, true}) {
        const auto collapsed = ShaderRecompiler::ValidateAndOptimizeSpirv(oneBlock, Vulkan11, Spirv13, false, optimize);
        const auto label = std::string(optimize ? "optimized" : "unoptimized");
        check(repeatedBlockBallots(collapsed) == 0, label + ": one block ballots one predicate twice");
        check(opcodeCount(collapsed, spv::OpGroupNonUniformBallot) == 2, label + ": a different predicate lost its ballot");
    }

    const auto called = assemble(ballotModuleSource(R"(%first = OpGroupNonUniformBallot %uvec4 %subgroup %predicate
OpStore %slot1 %first
%call = OpFunctionCall %void %helper
%second = OpGroupNonUniformBallot %uvec4 %subgroup %predicate
OpStore %slot2 %second
)"));
    const auto acrossCall = ShaderRecompiler::ValidateAndOptimizeSpirv(called, Vulkan11, Spirv13, false, false);
    check(opcodeCount(acrossCall, spv::OpGroupNonUniformBallot) == 2, "a ballot was shared across a function call");
}

}

int main() {
    const char* mode = std::getenv("APS5_SPIRV_OPT");
    const bool optimizationEnabled = mode == nullptr || std::string(mode) != "none";
    testNarrowConversions(optimizationEnabled);
    testSpecializedSelectionExitSpirv16();
    testInvalidInputIsRejected();
    testSpecializedSelectionExit();
    testSpecializedDeadContinue();
    testLongControlFlow(optimizationEnabled, false);
    testLongControlFlow(optimizationEnabled, true);
    testBallotsMergedIntoOneBlockShareOneBallot(optimizationEnabled);
    if (failures != 0) {
        std::fprintf(stderr, "%d optimizer check(s) failed\n", failures);
        return 1;
    }
    std::puts("SPIR-V optimizer checks passed");
    return 0;
}
