#include <Testing/Test.hpp>
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

using Testing::Case;
using Testing::Fail;
using Testing::Require;

constexpr std::uint32_t Vulkan11 = 0x00401000u;
constexpr std::uint32_t Spirv11 = 0x00010100u;
constexpr std::uint32_t Spirv13 = 0x00010300u;

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
    Require(tools.Assemble(source, &words), "test shader did not assemble");
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

bool OptimizationEnabled() {
    const char* mode = std::getenv("APS5_SPIRV_OPT");
    return mode == nullptr || std::string(mode) != "none";
}

void RequireNarrowConversions(const NarrowType& type) {
    const bool optimizationEnabled = OptimizationEnabled();
    spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_1);
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
                Require(validator.Validate(input), label + ": invalid test input");
                std::vector<std::uint32_t> output;
                try {
                    output = ShaderRecompiler::ValidateAndOptimizeSpirv(input, Vulkan11, Spirv13, false);
                } catch (const std::exception& error) {
                    Fail(label + ": " + error.what());
                }
                Require(validator.Validate(output), label + ": optimizer returned invalid SPIR-V");
                const auto normalized = output.front() == spv::MagicNumber ? output : swapByteOrder(output);
                Require(capabilities(normalized) == capabilities(native), label + ": optimizer changed device capabilities");
                Require(opcodeCount(normalized, spv::OpStore) == (vector ? 1u : 2u), label + ": optimizer lost buffer stores");
                if (!optimizationEnabled) {
                    Require(output == input, label + ": none mode changed the shader");
                } else {
                    Require(opcodeCount(normalized, spv::OpIAdd) == 0, label + ": optimizer did not remove dead arithmetic");
                    if (arithmetic && !type.floating) {
                        const auto conversion = type.signedInteger ? spv::OpSConvert : spv::OpUConvert;
                        Require(opcodeCount(normalized, conversion) == 0, label + ": safe constant folding was disabled");
                    }
                }
            }
        }
    }
}

void expectRejected(const std::vector<std::uint32_t>& words, std::uint32_t vulkanVersion, std::uint32_t spirvVersion, const std::string& reason, const char* diagnostic = nullptr) {
    const auto text = Testing::RequireThrowsMessage<std::exception>([&] { static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(words, vulkanVersion, spirvVersion, false)); }, "accepted " + reason);
    if (diagnostic != nullptr) Require(text.starts_with(diagnostic), reason + ": unexpected rejection: " + text);
}

void RequireSpecializedSelectionExit(bool useSwitch) {
    {
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
        const auto words = assemble(source);
        std::vector<std::uint32_t> specialized;
        try {
            static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(words, Vulkan11, Spirv13, false, false));
            specialized = ShaderRecompiler::SpecializeSpirv(words);
            static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(specialized, Vulkan11, Spirv13, false, false));
        } catch (const std::exception& error) {
            Fail(std::string("specialized selection exit: ") + error.what());
        }
        Require(opcodeCount(specialized, spv::OpStore) == 1u, "specialization retained the dead selection arm");
        Require(ShaderRecompiler::SpecializeSpirv(specialized) == specialized, "selection exit specialization is not stable");
    }
}

void rejectMalformed(const std::vector<std::uint32_t>& words, const char* reason) {
    auto native = words;
    if (native.size() > 1) native[1] = Spirv11;
    constexpr auto diagnostic = "SPIR-V validation before optimization failed:";
    expectRejected(native, Vulkan11, Spirv13, reason, diagnostic);
    expectRejected(swapByteOrder(native), Vulkan11, Spirv13, std::string(reason) + " (swapped)", diagnostic);
}

std::vector<std::uint32_t> ValidModule() {
    return assemble(moduleSource(Types.front(), false, ArithmeticSupport::StorageOnly));
}

const Case u8Conversions{"ValidateAndOptimizeSpirv_U8Conversions_KeepCapabilitiesAndStores", [] { RequireNarrowConversions(Types[0]); }};
const Case i8Conversions{"ValidateAndOptimizeSpirv_I8Conversions_KeepCapabilitiesAndStores", [] { RequireNarrowConversions(Types[1]); }};
const Case u16Conversions{"ValidateAndOptimizeSpirv_U16Conversions_KeepCapabilitiesAndStores", [] { RequireNarrowConversions(Types[2]); }};
const Case i16Conversions{"ValidateAndOptimizeSpirv_I16Conversions_KeepCapabilitiesAndStores", [] { RequireNarrowConversions(Types[3]); }};
const Case f16Conversions{"ValidateAndOptimizeSpirv_F16Conversions_KeepCapabilitiesAndStores", [] { RequireNarrowConversions(Types[4]); }};

const Case incompatibleVersions{"ValidateAndOptimizeSpirv_IncompatibleVulkanOrSpirvVersion_IsRejected", [] {
    const auto valid = ValidModule();
    expectRejected(valid, 0x00400000u, Spirv13, "SPIR-V 1.3 with Vulkan 1.0");
    expectRejected(valid, Vulkan11, 0x00010200u, "a module newer than the requested SPIR-V version");
    expectRejected(valid, 0u, Spirv13, "an unsupported Vulkan version");
}};

const Case illegalConstant{"ValidateAndOptimizeSpirv_UnusedStorageOnlyNarrowConstant_IsRejected", [] {
    auto invalid = moduleSource(Types.front(), false, ArithmeticSupport::StorageOnly);
    invalid.insert(invalid.find("%main = OpFunction"), "%illegal = OpConstantNull %small\n");
    expectRejected(assemble(invalid), Vulkan11, Spirv13, "an illegal storage-only narrow constant even when unused");
}};

const Case malformedHeader{"ValidateAndOptimizeSpirv_MalformedHeader_IsRejectedBeforeOptimization", [] {
    const auto valid = ValidModule();
    for (std::size_t length = 0; length < 5; ++length) {
        rejectMalformed(std::vector<std::uint32_t>(valid.begin(), valid.begin() + length), "an incomplete module header");
    }
    auto malformed = valid;
    malformed[0] = 0;
    rejectMalformed(malformed, "an invalid module magic");
}};

const Case malformedInstructions{"ValidateAndOptimizeSpirv_MalformedInstruction_IsRejectedBeforeOptimization", [] {
    const auto valid = ValidModule();
    auto malformed = valid;
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
}};

const Case conditionalSelectionExit{"SpecializeSpirv_ConstantConditionalSelectionExit_DropsTheDeadArmStably", [] {
    RequireSpecializedSelectionExit(false);
}};

const Case switchSelectionExit{"SpecializeSpirv_ConstantSwitchSelectionExit_DropsTheDeadArmStably", [] {
    RequireSpecializedSelectionExit(true);
}};

} // namespace
