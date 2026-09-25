#include "BdaAbi.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {
namespace {

struct Decoration {
    std::optional<std::uint32_t> location;
    std::optional<std::uint32_t> builtin;
    std::optional<std::uint32_t> set;
    std::optional<std::uint32_t> binding;
    std::optional<std::uint32_t> stride;
    bool block = false;
    bool patch = false;
    bool perPrimitive = false;
    bool perVertex = false;
};

struct Variable {
    std::uint32_t pointer;
    std::uint32_t storage;
};

struct Module {
    std::map<std::uint32_t, std::vector<std::uint32_t>> types;
    std::map<std::uint32_t, Decoration> decorations;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> builtins;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> offsets;
    std::map<std::uint32_t, Variable> variables;
    std::map<std::uint32_t, std::uint32_t> constants;
    std::set<std::uint32_t> interface;
    std::map<std::uint32_t, std::string> inputs;
    std::map<std::uint32_t, std::string> outputs;
    bool position = false;
    bool primitiveIndices = false;
    bool fragmentBarycentric = false;
    std::map<std::uint32_t, std::vector<std::uint32_t>> modes;

    const std::vector<std::uint32_t>& Type(std::uint32_t id) const {
        const auto it = types.find(id);
        Require(it != types.end(), "SPIR-V refers to an unknown type");
        return it->second;
    }

    std::string Signature(std::uint32_t id, std::uint32_t depth = 0) const {
        Require(depth < 8, "SPIR-V interface type nesting exceeds supported depth");
        const auto& type = Type(id);
        const auto op = static_cast<spv::Op>(type[0] & 0xffffu);
        if (op == spv::OpTypeFloat && type.size() == 3 && type[2] == 32) return "f32";
        if (op == spv::OpTypeInt && type.size() == 4 && type[2] == 32 && type[3] <= 1) return type[3] != 0 ? "i32" : "u32";
        if (op == spv::OpTypeBool && type.size() == 2) return "bool";
        if (op == spv::OpTypeVector && type.size() == 4 && type[3] >= 2 && type[3] <= 4) return Signature(type[2], depth + 1) + "x" + std::to_string(type[3]);
        throw std::runtime_error("AGC graphics: unsupported SPIR-V interface type");
    }

    std::uint32_t AddLocations(std::map<std::uint32_t, std::string>& locations, std::uint32_t location, std::uint32_t typeId, bool patch, std::uint32_t depth = 0) const {
        Require(depth < 8 && location < 256, "graphics interface exceeds supported locations");
        const auto& type = Type(typeId);
        if ((type[0] & 0xffffu) == spv::OpTypeArray) {
            Require(type.size() == 4, "malformed interface array");
            const auto count = constants.find(type[3]);
            Require(count != constants.end() && count->second != 0 && count->second <= 256, "invalid interface array length");
            for (std::uint32_t i = 0; i < count->second; ++i) location = AddLocations(locations, location, type[2], patch, depth + 1);
            return location;
        }
        Require(locations.emplace(location | (patch ? 0x10000u : 0u), std::string(patch ? "patch:" : "vertex:") + Signature(typeId)).second, "duplicate shader interface location");
        return location + 1;
    }

    void Builtin(std::uint32_t value, std::uint32_t type, std::uint32_t storage, ShaderRecompiler::ShaderStage stage, const VkPhysicalDeviceSubgroupProperties& subgroup) {
        using Stage = ShaderRecompiler::ShaderStage;
        const bool vertex = stage == Stage::Vertex || stage == Stage::Local;
        const bool input = storage == spv::StorageClassInput;
        const auto& raw = Type(type);
        if (value == spv::BuiltInBaryCoordKHR || value == spv::BuiltInBaryCoordNoPerspKHR) {
            Require(fragmentBarycentric && stage == Stage::Fragment && input && Signature(type) == "f32x3", "invalid barycentric built-in: expected fragment Float32 vec3 input with FragmentBarycentricKHR");
            return;
        }
        if (value == spv::BuiltInSubgroupLocalInvocationId) {
            Require(input && Signature(type) == "u32", "invalid subgroup local invocation ID input");
            Require((subgroup.supportedStages & VulkanStage(stage)) != 0 && (subgroup.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT) != 0, "subgroup local invocation ID is unsupported for this shader stage");
            return;
        }
        if ((value == spv::BuiltInTessLevelOuter || value == spv::BuiltInTessLevelInner) && (stage == Stage::TessellationControl || stage == Stage::TessellationEvaluation)) {
            Require(raw.size() == 4 && (raw[0] & 0xffffu) == spv::OpTypeArray && Signature(raw[2]) == "f32", "invalid tessellation level type");
            const auto count = constants.find(raw[3]);
            Require(count != constants.end() && count->second == (value == spv::BuiltInTessLevelOuter ? 4u : 2u), "invalid tessellation level count");
            Require(input == (stage == Stage::TessellationEvaluation), "invalid tessellation level direction");
            return;
        }
        if (stage == Stage::TessellationControl || stage == Stage::TessellationEvaluation || stage == Stage::Mesh) {
            const auto signature = Signature(type);
            if (!input && value == spv::BuiltInPosition) {
                Require(signature == "f32x4" && !position, "invalid or duplicate position output");
                position = true;
                return;
            }
            if (stage == Stage::TessellationControl || stage == Stage::TessellationEvaluation) {
                Require(input && ((value == spv::BuiltInPosition && signature == "f32x4") || (value == spv::BuiltInTessCoord && stage == Stage::TessellationEvaluation && signature == "f32x3") || ((value == spv::BuiltInInvocationId || value == spv::BuiltInPrimitiveId || value == spv::BuiltInPatchVertices) && (signature == "i32" || signature == "u32"))), "unsupported tessellation built-in");
                return;
            }
            if (!input && value == spv::BuiltInPrimitiveTriangleIndicesEXT) primitiveIndices = true;
            Require((input && ((value == spv::BuiltInWorkgroupId || value == spv::BuiltInLocalInvocationId || value == spv::BuiltInGlobalInvocationId || value == spv::BuiltInNumWorkgroups) && signature == "u32x3")) || (input && value == spv::BuiltInLocalInvocationIndex && signature == "u32") || (!input && value == spv::BuiltInPrimitiveTriangleIndicesEXT && signature == "u32x3") || (!input && value == spv::BuiltInCullPrimitiveEXT && signature == "bool"), "unsupported mesh built-in");
            return;
        }
        const auto signature = Signature(type);
        if (vertex && storage == spv::StorageClassInput) {
            Require((value == spv::BuiltInVertexIndex || value == spv::BuiltInInstanceIndex) && signature == "i32", "unsupported vertex built-in input");
        } else if (vertex && storage == spv::StorageClassOutput) {
            Require(value == spv::BuiltInPosition && signature == "f32x4" && !position, "unsupported or duplicate vertex built-in output");
            position = true;
        } else {
            Require(storage == spv::StorageClassInput && ((value == spv::BuiltInFragCoord && signature == "f32x4") || (value == spv::BuiltInFrontFacing && signature == "bool")), "unsupported fragment built-in");
        }
    }
};

Module Inspect(const CompiledShader& compiled, const State& state, const VkPhysicalDeviceSubgroupProperties& subgroup, bool fragmentShaderBarycentric) {
    using Stage = ShaderRecompiler::ShaderStage;
    Require(compiled.program != nullptr, "missing compiled shader");
    const auto& shader = *compiled.program;
    const auto stage = compiled.stage;
    const bool vertex = stage == Stage::Vertex || stage == Stage::Local;
    const bool fragment = stage == Stage::Fragment;
    const bool mesh = stage == Stage::Mesh;
    const bool control = stage == Stage::TessellationControl;
    const bool evaluation = stage == Stage::TessellationEvaluation;
    const bool rectListControl = state.rectList && control;
    if (rectListControl) {
        Require(shader.bdaAbiVersion == ShaderRecompiler::BdaAbi::Version, "incompatible rect-list fault ABI version");
        Require(shader.bindings.size() == 1, "rect-list control shader must declare exactly one fault buffer");
        const auto& binding = shader.bindings.front();
        Require(binding.role == ShaderRecompiler::DescriptorRole::FaultBuffer && binding.kind == ShaderRecompiler::DescriptorKind::StorageBuffer && binding.descriptorSet == 0 && binding.count == 1 && binding.guestDescriptor.empty() && !binding.readOnly, "invalid rect-list fault buffer contract");
    }
    const auto model = mesh ? spv::ExecutionModelMeshEXT : control ? spv::ExecutionModelTessellationControl : evaluation ? spv::ExecutionModelTessellationEvaluation : vertex ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
    const auto& words = shader.spirv;

    Require(words.size() >= 5, "SPIR-V header is truncated: " + std::to_string(words.size()) + " words");
    Require(words[0] == spv::MagicNumber, "invalid SPIR-V magic number: " + std::to_string(words[0]));
    Require(words[1] >= 0x10000u && words[1] <= 0x10400u, "unsupported SPIR-V version: " + std::to_string(words[1]));
    Require(words[3] != 0, "invalid SPIR-V bound: 0");
    Require(words[4] == 0, "unsupported SPIR-V schema: " + std::to_string(words[4]));

    Module module;
    std::uint32_t entries = 0;
    std::uint32_t memoryModels = 0;
    std::uint32_t entryPoint = 0;
    std::set<std::uint32_t> executionModeTargets;
    bool upperLeft = false;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        Require(count != 0 && count <= words.size() - cursor, "truncated SPIR-V instruction");
        const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
        const auto instruction = std::span(words).subspan(cursor, count);
        switch (op) {
            case spv::OpCapability: {
                Require(count == 2, "invalid OpCapability instruction");

                const auto capability = static_cast<spv::Capability>(instruction[1]);
                const bool isBarycentricCapability = capability == spv::CapabilityFragmentBarycentricKHR;
                if (isBarycentricCapability) {
                    Require(fragment, "FragmentBarycentricKHR requires a fragment shader");
                    Require(fragmentShaderBarycentric, "device does not support enabled VK_KHR_fragment_shader_barycentric with fragmentShaderBarycentric");
                    module.fragmentBarycentric = true;
                }
                VkSubgroupFeatureFlags subgroupOperations = 0;
                switch (capability) {
                    case spv::CapabilityGroupNonUniform: subgroupOperations = VK_SUBGROUP_FEATURE_BASIC_BIT; break;
                    case spv::CapabilityGroupNonUniformBallot: subgroupOperations = VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT; break;
                    case spv::CapabilityGroupNonUniformShuffle: subgroupOperations = VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_BIT; break;
                    default: break;
                }
                const bool isSubgroupCapability = subgroupOperations != 0;
                if (isSubgroupCapability) {
                    Require((subgroup.supportedStages & VulkanStage(stage)) != 0, "subgroup capability " + std::to_string(instruction[1]) + " is unsupported for shader stage " + std::to_string(VulkanStage(stage)));
                    Require((subgroup.supportedOperations & subgroupOperations) == subgroupOperations, "device lacks operations for subgroup capability " + std::to_string(instruction[1]));
                }

                const bool isBaseCapability =
                    capability == spv::CapabilityShader ||
                    capability == spv::CapabilitySignedZeroInfNanPreserve;

                const bool isBdaCapability =
                    shader.bdaAbiVersion == ShaderRecompiler::BdaAbi::Version &&
                    (capability == spv::CapabilityInt64 ||
                     capability == spv::CapabilityPhysicalStorageBufferAddresses ||
                     capability == spv::CapabilityStorageBuffer8BitAccess);

                const bool isTessellationCapability =
                    (control || evaluation) &&
                    capability == spv::CapabilityTessellation;

                const bool isMeshCapability =
                    mesh &&
                    capability == spv::CapabilityMeshShadingEXT;

                // Enabled unconditionally or by the device setup in VulkanDevice.
                const bool isFeatureCapability =
                    capability == spv::CapabilityImageGatherExtended ||
                    capability == spv::CapabilityImageQuery ||
                    capability == spv::CapabilityStorageImageWriteWithoutFormat ||
                    capability == spv::CapabilityStorageImageReadWithoutFormat ||
                    capability == spv::CapabilityInt64 ||
                    capability == spv::CapabilityInt16 ||
                    capability == spv::CapabilityFloat16 ||
                    capability == spv::CapabilityStorageBuffer8BitAccess ||
                    capability == spv::CapabilityPhysicalStorageBufferAddresses;

                Require(
                    isBaseCapability ||
                    isBarycentricCapability ||
                    isSubgroupCapability ||
                    isBdaCapability ||
                    isTessellationCapability ||
                    isMeshCapability ||
                    isFeatureCapability,
                    std::string("SPIR-V requires unsupported device capability ") +
                        std::to_string(static_cast<std::uint32_t>(capability)));

                break;
            }

            case spv::OpExtension: {
                const auto bytes = std::as_bytes(instruction.subspan(1));
                const auto* text = reinterpret_cast<const char*>(bytes.data());
                const auto end = std::find(text, text + bytes.size(), '\0');
                Require(end != text + bytes.size(), "unterminated SPIR-V extension");
                const std::string_view extension(text, static_cast<std::size_t>(end - text));
                if (extension == "SPV_KHR_fragment_shader_barycentric") {
                    Require(fragment && fragmentShaderBarycentric, "SPV_KHR_fragment_shader_barycentric requires enabled fragmentShaderBarycentric in a fragment shader");
                    break;
                }
                Require(extension == "SPV_KHR_float_controls" || (mesh && extension == "SPV_EXT_mesh_shader") || (shader.bdaAbiVersion == ShaderRecompiler::BdaAbi::Version && (extension == "SPV_KHR_physical_storage_buffer" || extension == "SPV_KHR_8bit_storage")), "unsupported SPIR-V extension");
                break;
            }
            case spv::OpDecorateId:
            case spv::OpDecorationGroup:
            case spv::OpGroupDecorate:
            case spv::OpGroupMemberDecorate:
            case spv::OpSpecConstant:
            case spv::OpSpecConstantTrue:
            case spv::OpSpecConstantFalse:
            case spv::OpSpecConstantComposite:
            case spv::OpSpecConstantOp:
                throw std::runtime_error("AGC graphics: unsupported SPIR-V extension, grouped decoration or specialization constant");
            case spv::OpMemoryModel: {
                // Address-based shaders use PhysicalStorageBuffer64; everything else (including the
                // generated rect-list stages) is Logical.
                Require(count == 3, "invalid SPIR-V OpMemoryModel word count: " + std::to_string(count));
                Require(instruction[1] == spv::AddressingModelPhysicalStorageBuffer64 || instruction[1] == spv::AddressingModelLogical, "unsupported SPIR-V addressing model: " + std::to_string(instruction[1]));
                Require(instruction[2] == spv::MemoryModelGLSL450, "unsupported SPIR-V memory model: " + std::to_string(instruction[2]));
                ++memoryModels;
                break;
            }
            case spv::OpEntryPoint:
                Require(count >= 5 && instruction[1] == model && instruction[3] == 0x6e69616du && instruction[4] == 0, "expected a main entry point for the assigned graphics stage");
                entryPoint = instruction[2];
                ++entries;
                for (std::size_t i = 5; i < count; ++i) Require(module.interface.insert(instruction[i]).second, "duplicate SPIR-V interface ID");
                break;
            case spv::OpExecutionMode:
                Require(count >= 3 && module.modes.emplace(instruction[2], std::vector<std::uint32_t>(instruction.begin() + 3, instruction.end())).second, "duplicate or malformed execution mode");
                executionModeTargets.insert(instruction[1]);
                if (instruction[2] == spv::ExecutionModeOriginUpperLeft) upperLeft = true;
                break;
            case spv::OpExecutionModeId:
                throw std::runtime_error("AGC graphics: execution mode IDs require unsupported specialization");
            case spv::OpDecorate: {
                Require(count >= 3, "malformed SPIR-V decoration");
                auto& decoration = module.decorations[instruction[1]];
                const auto kind = static_cast<spv::Decoration>(instruction[2]);
                if (kind == spv::DecorationLocation || kind == spv::DecorationBuiltIn || kind == spv::DecorationDescriptorSet || kind == spv::DecorationBinding || kind == spv::DecorationArrayStride) {
                    Require(count == 4, "malformed SPIR-V literal decoration");
                    auto* field = kind == spv::DecorationLocation ? &decoration.location : kind == spv::DecorationBuiltIn ? &decoration.builtin : kind == spv::DecorationDescriptorSet ? &decoration.set : kind == spv::DecorationBinding ? &decoration.binding : &decoration.stride;
                    Require(!field->has_value(), "duplicate SPIR-V decoration");
                    *field = instruction[3];
                }
                Require(kind != spv::DecorationComponent && kind != spv::DecorationIndex && kind != spv::DecorationStream && kind != spv::DecorationXfbBuffer && kind != spv::DecorationXfbStride, "unsupported shader interface packing or transform feedback");
                if (kind == spv::DecorationPatch) decoration.patch = true;
                if (kind == spv::DecorationPerPrimitiveEXT) decoration.perPrimitive = true;
                if (kind == spv::DecorationPerVertexKHR) {
                    Require(count == 3, "malformed PerVertexKHR decoration");
                    decoration.perVertex = true;
                }
                if (kind == spv::DecorationBlock) decoration.block = true;
                break;
            }
            case spv::OpMemberDecorate:
                Require(count >= 4, "malformed SPIR-V member decoration");
                if (instruction[3] == spv::DecorationBuiltIn || instruction[3] == spv::DecorationOffset) {
                    Require(count == 5, "malformed SPIR-V member literal decoration");
                    auto& fields = instruction[3] == spv::DecorationBuiltIn ? module.builtins : module.offsets;
                    Require(fields.emplace(std::make_pair(instruction[1], instruction[2]), instruction[4]).second, "duplicate SPIR-V member decoration");
                }
                break;
            case spv::OpTypeBool:
            case spv::OpTypeInt:
            case spv::OpTypeFloat:
            case spv::OpTypeVector:
            case spv::OpTypeMatrix:
            case spv::OpTypeArray:
            case spv::OpTypeRuntimeArray:
            case spv::OpTypeStruct:
            case spv::OpTypePointer:
                Require(count >= 2 && module.types.emplace(instruction[1], std::vector<std::uint32_t>(instruction.begin(), instruction.end())).second, "invalid or duplicate SPIR-V type");
                break;
            case spv::OpVariable:
                Require(count >= 4 && module.variables.emplace(instruction[2], Variable{instruction[1], instruction[3]}).second, "invalid or duplicate SPIR-V variable");
                break;
            case spv::OpConstant:
                if (count == 4) module.constants.emplace(instruction[2], instruction[3]);
                break;
            default: break;
        }
        cursor += count;
    }
    Require(entries == 1 && memoryModels == 1, "SPIR-V must contain one entry point and memory model");
    Require(entryPoint != 0 && std::all_of(executionModeTargets.begin(), executionModeTargets.end(), [&](auto target) { return target == entryPoint; }), "execution mode refers to a different entry point");
    Require(!fragment || upperLeft, "fragment coordinates must use an upper-left origin");
    if (const auto preserve = module.modes.find(spv::ExecutionModeSignedZeroInfNanPreserve); preserve != module.modes.end()) {
        Require(preserve->second == std::vector<std::uint32_t>{32u}, "SignedZeroInfNanPreserve requires Float32");
        module.modes.erase(preserve);
    }
    const auto mode = [&](std::uint32_t name, std::vector<std::uint32_t> operands) {
        const auto it = module.modes.find(name);
        Require(it != module.modes.end() && it->second == operands, "missing or incompatible shader execution mode");
    };
    if (mesh) {
        Require(state.stages.mesh.has_value(), "mesh configuration is missing");
        const auto& config = *state.stages.mesh;
        mode(spv::ExecutionModeLocalSize, {config.threadsPerGroup, 1, 1});
        mode(spv::ExecutionModeOutputVertices, {config.maxVertices});
        mode(spv::ExecutionModeOutputPrimitivesEXT, {config.maxPrimitives});
        mode(spv::ExecutionModeOutputTrianglesEXT, {});
        Require(module.modes.size() == 4, "unsupported mesh execution mode");
    } else if (control) {
        Require(state.rectList || state.stages.tessellation.has_value(), "tessellation configuration is missing");
        mode(spv::ExecutionModeOutputVertices, {state.rectList ? 4u : state.stages.tessellation->outputControlPoints});
        Require(module.modes.size() == 1, "unsupported tessellation-control execution mode");
    } else if (evaluation) {
        mode(state.rectList ? spv::ExecutionModeQuads : spv::ExecutionModeTriangles, {});
        mode(state.rectList ? spv::ExecutionModeSpacingEqual : spv::ExecutionModeSpacingFractionalOdd, {});
        mode(spv::ExecutionModeVertexOrderCw, {});
        Require(module.modes.size() == 3, "unsupported tessellation-evaluation execution mode");
    } else if (fragment) {
        for (const auto& [name, operands] : module.modes) Require(operands.empty() && (name == spv::ExecutionModeOriginUpperLeft || name == spv::ExecutionModeEarlyFragmentTests), "unsupported fragment execution mode");
    } else Require(module.modes.empty(), "unsupported vertex execution mode");
    std::set<std::pair<std::uint32_t, std::uint32_t>> descriptors;
    bool push = false;
    for (const auto& [id, variable] : module.variables) {
        if (variable.storage == spv::StorageClassFunction || variable.storage == spv::StorageClassPrivate) continue;
        const auto& pointer = module.Type(variable.pointer);
        Require(pointer.size() == 4 && (pointer[0] & 0xffffu) == spv::OpTypePointer && pointer[2] == variable.storage, "invalid SPIR-V variable pointer");
        auto typeId = pointer[3];
        const auto& decoration = module.decorations[id];
        const bool input = variable.storage == spv::StorageClassInput;
        const bool output = variable.storage == spv::StorageClassOutput;
        const bool tessellationLevels = decoration.builtin && (*decoration.builtin == spv::BuiltInTessLevelOuter || *decoration.builtin == spv::BuiltInTessLevelInner);
        const bool vertexArray = !tessellationLevels && ((control && (input || output)) || (evaluation && input) || (mesh && output)) && !decoration.patch;
        const auto& outer = module.Type(typeId);
        if (decoration.perVertex) {
            Require(fragment && input && module.fragmentBarycentric && !decoration.patch && !decoration.perPrimitive && !decoration.builtin && decoration.location.has_value(), "PerVertexKHR requires a fragment location input with FragmentBarycentricKHR");
            Require(outer.size() == 4 && (outer[0] & 0xffffu) == spv::OpTypeArray, "PerVertexKHR input must be an array");
            const auto length = module.constants.find(outer[3]);
            Require(length != module.constants.end() && length->second == 3, "PerVertexKHR input must contain three vertices");
            typeId = outer[2];
        }
        if (vertexArray && (outer[0] & 0xffffu) == spv::OpTypeArray) {
            Require(outer.size() == 4, "malformed per-vertex interface array");
            const auto length = module.constants.find(outer[3]);
            Require(length != module.constants.end() && length->second != 0, "invalid per-vertex interface array length");
            if (mesh) {
                const bool primitive = decoration.perPrimitive || (decoration.builtin && (*decoration.builtin == spv::BuiltInPrimitiveTriangleIndicesEXT || *decoration.builtin == spv::BuiltInCullPrimitiveEXT));
                Require(length->second == (primitive ? state.stages.mesh->maxPrimitives : state.stages.mesh->maxVertices), "mesh interface array disagrees with output limits");
            } else {
                const auto expected = state.rectList ? (control && input ? 3u : 4u) : control && input ? state.stages.tessellation->inputControlPoints : state.stages.tessellation->outputControlPoints;
                Require(length->second == expected, "tessellation interface array disagrees with control-point count");
            }
            typeId = outer[2];
        }
        const auto& type = module.Type(typeId);
        if (variable.storage == spv::StorageClassInput || variable.storage == spv::StorageClassOutput) {
            Require(module.interface.contains(id), "SPIR-V input or output is absent from the entry point interface");
            if (decoration.location) {
                Require(!vertexArray || (outer[0] & 0xffffu) == spv::OpTypeArray, "per-vertex interface lacks a control-point or mesh-output dimension");
                Require(!decoration.perPrimitive, "per-primitive user outputs are unsupported");
                Require(!decoration.builtin, "shader input cannot have both location and built-in decorations");
                auto& locations = variable.storage == spv::StorageClassInput ? module.inputs : module.outputs;
                module.AddLocations(locations, *decoration.location, typeId, decoration.patch);
            } else if (decoration.builtin) {
                module.Builtin(*decoration.builtin, typeId, variable.storage, stage, subgroup);
            } else {
                Require((type[0] & 0xffffu) == spv::OpTypeStruct, "shader interface lacks a location or built-in");
                for (std::size_t i = 2; i < type.size(); ++i) {
                    const auto builtin = module.builtins.find({typeId, static_cast<std::uint32_t>(i - 2)});
                    Require(builtin != module.builtins.end(), "interface blocks with non-built-in members are unsupported");
                    module.Builtin(builtin->second, type[i], variable.storage, stage, subgroup);
                }
            }
        } else if (variable.storage == spv::StorageClassPushConstant) {
            Require(!push && !shader.pushConstants.empty() && (type[0] & 0xffffu) == spv::OpTypeStruct, "invalid push constant interface");
            push = true;
            Require(type.size() == 3 && module.decorations[typeId].block, "push constant variable must be a Block struct with exactly one member");
            const auto offset = module.offsets.find({typeId, 0});
            Require(offset != module.offsets.end() && offset->second == 0, "push constant member must have offset zero");
            const auto& array = module.Type(type[2]);
            Require(array.size() == 4 && (array[0] & 0xffffu) == spv::OpTypeArray && module.Signature(array[2]) == "u32", "push constant member must be an array of u32");
            const auto length = module.constants.find(array[3]);
            Require(length != module.constants.end() && length->second == PipelinePushConstantBytes / 4, "push constant array must contain 32 elements");
            const auto stride = module.decorations[type[2]].stride;
            Require(stride.has_value() && *stride == 4, "push constant array must have an ArrayStride of 4");
        } else if (variable.storage == spv::StorageClassWorkgroup) {
            Require(mesh, "workgroup memory outside a mesh shader");
        } else if (variable.storage == spv::StorageClassUniformConstant) {
            Require(decoration.set && decoration.binding, "unbound shader resource");
            const auto key = std::make_pair(*decoration.set, *decoration.binding);
            Require(descriptors.insert(key).second, "duplicate SPIR-V resource binding");
            const auto binding = std::find_if(shader.bindings.begin(), shader.bindings.end(), [&](const auto& item) { return item.descriptorSet == key.first && item.binding == key.second; });
            Require(binding != shader.bindings.end(), "SPIR-V resource is absent from recompiler binding metadata");
            Require(binding->kind == ShaderRecompiler::DescriptorKind::Sampler || binding->kind == ShaderRecompiler::DescriptorKind::SampledImage || binding->kind == ShaderRecompiler::DescriptorKind::StorageImage, "SPIR-V descriptor type disagrees with recompiler binding metadata");
        } else {
            Require(variable.storage == spv::StorageClassStorageBuffer && decoration.set && decoration.binding, "unsupported or unbound shader resource");
            const auto key = std::make_pair(*decoration.set, *decoration.binding);
            Require(descriptors.insert(key).second, "duplicate SPIR-V resource binding");
            const auto binding = std::find_if(shader.bindings.begin(), shader.bindings.end(), [&](const auto& item) { return item.descriptorSet == key.first && item.binding == key.second; });
            Require(binding != shader.bindings.end(), "SPIR-V resource is absent from recompiler binding metadata");
            Require(binding->kind == ShaderRecompiler::DescriptorKind::StorageBuffer, "SPIR-V descriptor type disagrees with recompiler binding metadata");
            Require(!binding->readOnly, "read-only descriptor metadata is unsupported because the recompiler emits no NonWritable decoration");
            const bool array = binding->role == ShaderRecompiler::DescriptorRole::GuestBuffers;
            Require(array || (shader.bdaAbiVersion == ShaderRecompiler::BdaAbi::Version && (binding->role == ShaderRecompiler::DescriptorRole::BdaPagetable || binding->role == ShaderRecompiler::DescriptorRole::FaultBuffer)) || binding->role == ShaderRecompiler::DescriptorRole::ShaderData || binding->role == ShaderRecompiler::DescriptorRole::FlattenedSrt, "SPIR-V descriptor role is unsupported");
            auto blockId = typeId;
            if (array) {
                Require(type.size() == 4 && (type[0] & 0xffffu) == spv::OpTypeArray, "guest buffer descriptors must be declared as a descriptor array");
                const auto count = module.constants.find(type[3]);
                Require(count != module.constants.end() && count->second == binding->count, "descriptor array length disagrees with recompiler binding count");
                blockId = type[2];
            } else {
                Require(binding->count == 1, "non-array descriptor must have a binding count of one");
            }
            const auto& block = module.Type(blockId);
            Require(block.size() == 3 && (block[0] & 0xffffu) == spv::OpTypeStruct && module.decorations[blockId].block, "descriptor must be a Block struct with exactly one member");
            const auto& words = module.Type(block[2]);
            Require(words.size() == 3 && (words[0] & 0xffffu) == spv::OpTypeRuntimeArray && module.Signature(words[2]) == "u32", "descriptor block member must be a runtime array of u32");
            const auto member = module.offsets.find({blockId, 0});
            Require(member != module.offsets.end() && member->second == 0, "descriptor block member must have offset zero");
        }
    }
    if (vertex) {
        std::set<std::uint32_t> locations;
        for (const auto& attribute : shader.vertexAttributes) {
            Require(locations.insert(attribute.location).second, "duplicate vertex attribute metadata");
            const auto input = module.inputs.find(attribute.location);
            Require(input != module.inputs.end() && input->second == "vertex:" + VertexAttributeSignature(attribute), "vertex attribute metadata disagrees with shader input");
        }
        Require(locations.size() == module.inputs.size(), "vertex input is missing attribute metadata");
    } else {
        Require(shader.vertexAttributes.empty(), "vertex attribute metadata is invalid for this stage");
    }
    Require(descriptors.size() == shader.bindings.size(), "recompiler binding metadata contains undeclared resources");
    Require(push == !shader.pushConstants.empty(), "recompiler push constant metadata disagrees with SPIR-V");
    for (const auto id : module.interface) Require(module.variables.contains(id), "entry point interface contains an unknown variable");
    if (mesh) Require(module.primitiveIndices, "mesh shader does not export primitive indices");
    if (stage == Stage::Vertex || mesh || evaluation) Require(module.position, "vertex shader does not export position");
    return module;
}

}

std::set<std::uint32_t> ValidateShaders(std::span<const CompiledShader> shaders, const State& state, const VkPhysicalDeviceSubgroupProperties& subgroup, bool fragmentShaderBarycentric) {
    using Stage = ShaderRecompiler::ShaderStage;
    const bool tessellation = state.stages.path == ShaderPath::Tessellation;
    const bool mesh = state.stages.path == ShaderPath::Geometry;
    Require(state.stages.path == ShaderPath::Vertex || tessellation || mesh, "unsupported graphics shader path");
    Require(state.stages.mesh.has_value() == mesh && state.stages.tessellation.has_value() == tessellation, "graphics stage configuration disagrees with its path");
    Require(!state.rectList || (state.stages.path == ShaderPath::Vertex && state.topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST && state.cullMode == VK_CULL_MODE_NONE), "invalid rect-list pipeline state");
    Require(shaders.size() == (tessellation || state.rectList ? 4u : 2u), "incorrect graphics stage count");
    const std::array<Stage, 4> tessStages{Stage::Local, Stage::TessellationControl, Stage::TessellationEvaluation, Stage::Fragment};
    static_cast<void>(AssemblePushConstants(shaders));
    Module previous;
    for (std::size_t i = 0; i < shaders.size(); ++i) {
        const auto expected = state.rectList ? (i == 0 ? Stage::Vertex : tessStages[i]) : tessellation ? tessStages[i] : i == 1 ? Stage::Fragment : state.stages.path == ShaderPath::Geometry ? Stage::Mesh : Stage::Vertex;
        Require(shaders[i].program != nullptr, "missing compiled shader");
        Require(shaders[i].stage == expected, "graphics stage order disagrees");
        for (const auto& binding : shaders[i].program->bindings) Require(binding.descriptorSet == 0, "graphics resource uses a descriptor set other than zero");
        const auto current = Inspect(shaders[i], state, subgroup, fragmentShaderBarycentric);
        if (i != 0) {
            for (const auto& [location, signature] : current.inputs) {
                const auto output = previous.outputs.find(location);
                Require(output != previous.outputs.end() && output->second == signature, "graphics interfaces disagree at location " + std::to_string(location));
            }
        }
        previous = current;
    }
    // One float4 color per MRT attachment. A pixel shader may also export no color at all when it
    // writes storage images or buffers instead; its attachments are then left untouched.
    const auto attachments = std::max<std::size_t>(state.colors.size(), 1u);
    std::set<std::uint32_t> locations;
    for (const auto& [location, signature] : previous.outputs) {
        Require(location < attachments && signature == "vertex:f32x4", "fragment shader must export float4 colors at locations below the attachment count");
        locations.insert(location);
    }
    return locations;
}

void ValidateShaderPair(const ShaderRecompiler::RecompileResult& vertex, const ShaderRecompiler::RecompileResult& fragment) {
    const std::array<CompiledShader, 2> shaders{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, static_cast<std::uint32_t>(vertex.pushConstants.size())}}};
    State state{};
    state.stages.path = ShaderPath::Vertex;
    ValidateShaders(shaders, state, VkPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES}, false);
}

}
