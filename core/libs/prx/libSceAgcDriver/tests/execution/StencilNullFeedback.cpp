#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include <spirv-tools/libspirv.hpp>
#endif

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

PFN_vkCmdBindPipeline NativeBind = nullptr;
std::atomic<std::uint64_t> GraphicsBinds{0u};

VKAPI_ATTR void VKAPI_CALL CountBind(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipeline pipeline) {
    if (point == VK_PIPELINE_BIND_POINT_GRAPHICS) ++GraphicsBinds;
    NativeBind(commands, point, pipeline);
}

struct BindTrace {
    AgcDriver::Graphics::DeviceFunctions* functions;
    explicit BindTrace(const AgcDriver::VulkanDevice& device) {
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
        functions = const_cast<AgcDriver::Graphics::DeviceFunctions*>(context.functions);
        Require(functions != nullptr && functions->cmdBindPipeline != nullptr, "native pipeline bind table is unavailable");
        NativeBind = functions->cmdBindPipeline;
        functions->cmdBindPipeline = CountBind;
    }
    ~BindTrace() { functions->cmdBindPipeline = NativeBind; }
};

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t WaveSize = 64;
constexpr std::byte Kept{0x40};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::byte, Width * Height * 4> NullBindingTexture{};
alignas(256) std::array<std::byte, 128 * 128 * 4> DepthMemory{};
alignas(256) std::array<std::byte, 256 * 256> StencilMemory{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PlainPixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> ColumnPixelCode{
    0x7e040f00, 0x7e0e02f2, 0xf8000082, 0x00000200, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> RowPixelCode{
    0x7e040f01, 0x7e0e02f2, 0xf8000082, 0x00000200, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::array<float, 4>, 6> Vertices{{
    {-1.0f, -1.0f, 0.25f, 1.0f}, {3.0f, -1.0f, 0.25f, 1.0f}, {-1.0f, 3.0f, 0.25f, 1.0f},
    {-1.0f, -1.0f, 0.75f, 1.0f}, {3.0f, -1.0f, 0.75f, 1.0f}, {-1.0f, 3.0f, 0.75f, 1.0f},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

ShaderRecompiler::ShaderPixelStageInfo PixelInfo(std::uint32_t shaderControl) {
    const AgcDriver::Registers context{
        {0x1b3, 0x700u}, {0x1b4, 0x700u}, {0x1b6, 0u}, {0x1c5, 9u}, {0x203, shaderControl},
    };
    return AgcDriver::Graphics::DecodePixelStageInfo(context, {0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u});
}

constexpr std::uint32_t ImageElements = 2u;
constexpr std::uint32_t StorageFloat2DHeap = ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding + 2u;

void Emit(std::vector<std::uint32_t>& output, spv::Op opcode, std::initializer_list<std::uint32_t> operands) {
    output.push_back((static_cast<std::uint32_t>(operands.size() + 1u) << spv::WordCountShift) | static_cast<std::uint32_t>(opcode));
    output.insert(output.end(), operands.begin(), operands.end());
}

bool StartsDeclarations(spv::Op opcode) {
    return opcode == spv::OpUndef || opcode == spv::OpVariable || (opcode >= spv::OpTypeVoid && opcode <= spv::OpTypeForwardPointer) || (opcode >= spv::OpConstantTrue && opcode <= spv::OpSpecConstantOp);
}

void DeclareImageHeap(std::vector<std::uint32_t>& module, std::uint32_t set, std::uint32_t binding, std::uint32_t elements) {
    Require(module.size() >= 5u && module[0] == spv::MagicNumber, "the null-image fixture module is not SPIR-V");
    std::uint32_t scalar = 0u, floating = 0u;
    std::size_t entry = 0u, declarations = 0u, functions = 0u;
    for (std::size_t offset = 5u; offset < module.size();) {
        const auto count = module[offset] >> spv::WordCountShift;
        const auto opcode = static_cast<spv::Op>(module[offset] & spv::OpCodeMask);
        Require(count != 0u && count <= module.size() - offset, "the null-image fixture module has a truncated instruction");
        if (opcode == spv::OpFunction) {
            functions = offset;
            break;
        }
        if (opcode == spv::OpEntryPoint) {
            Require(entry == 0u, "the null-image fixture module has several entry points");
            entry = offset;
        }
        if (declarations == 0u && StartsDeclarations(opcode)) declarations = offset;
        if (opcode == spv::OpTypeInt && module[offset + 2u] == 32u && module[offset + 3u] == 0u) scalar = module[offset + 1u];
        if (opcode == spv::OpTypeFloat && module[offset + 2u] == 32u) floating = module[offset + 1u];
        offset += count;
    }
    Require(entry != 0u && declarations != 0u && functions != 0u && floating != 0u, "the null-image fixture module has no entry point, declarations or scalar shader types");
    auto id = module[3];
    const bool declareScalar = scalar == 0u;
    if (declareScalar) scalar = id++;
    const auto length = id++, image = id++, array = id++, pointer = id++, variable = id++;
    std::vector<std::uint32_t> annotations;
    Emit(annotations, spv::OpDecorate, {variable, spv::DecorationDescriptorSet, set});
    Emit(annotations, spv::OpDecorate, {variable, spv::DecorationBinding, binding});
    std::vector<std::uint32_t> types;
    if (declareScalar) Emit(types, spv::OpTypeInt, {scalar, 32u, 0u});
    Emit(types, spv::OpConstant, {scalar, length, elements});
    Emit(types, spv::OpTypeImage, {image, floating, spv::Dim2D, 0u, 0u, 0u, 2u, spv::ImageFormatUnknown});
    Emit(types, spv::OpTypeArray, {array, image, length});
    Emit(types, spv::OpTypePointer, {pointer, spv::StorageClassUniformConstant, array});
    Emit(types, spv::OpVariable, {pointer, variable, spv::StorageClassUniformConstant});
    module.insert(module.begin() + static_cast<std::ptrdiff_t>(functions), types.begin(), types.end());
    module.insert(module.begin() + static_cast<std::ptrdiff_t>(declarations), annotations.begin(), annotations.end());
    if (module[1] >= 0x00010400u) {
        module.insert(module.begin() + static_cast<std::ptrdiff_t>(entry + (module[entry] >> spv::WordCountShift)), variable);
        module[entry] += 1u << spv::WordCountShift;
    }
    module[3] = id;
}

void AttachNullImages(ShaderRecompiler::RecompileResult& result, const ShaderRecompiler::SpirvTarget& target, bool depthAlias = false) {
    namespace Runtime = ShaderRecompiler::RuntimeAbi;
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(depthAlias ? DepthMemory.data() : NullBindingTexture.data()));
    const auto format = depthAlias ? 22u : 56u;
    const std::array<std::uint32_t, 8> words{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u) | (format << 20u) | (((Width - 1u) & 3u) << 30u), ((Width - 1u) >> 2u) | ((Height - 1u) << 14u), 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
    const auto set = target.registrationModules ? Runtime::FragmentDescriptorSet : Runtime::DescriptorSet;
    const auto binding = Runtime::BindingNumber(Runtime::Stage::Fragment, static_cast<Runtime::Binding>(StorageFloat2DHeap));
    ShaderRecompiler::DescriptorBinding images{ShaderRecompiler::DescriptorKind::StorageImage, ShaderRecompiler::DescriptorRole::GuestImages, set, binding, ImageElements, std::vector<std::uint32_t>(ImageElements * words.size())};
    std::copy(words.begin(), words.end(), images.guestDescriptor.begin());
    images.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    images.imageWritten = {false, false};
    images.imageAtomic = {false, false};
    images.imageAtomic64 = {false, false};
    result.bindings.push_back(std::move(images));
    auto module = result.spirv.Copy();
    DeclareImageHeap(module, set, binding, ImageElements);
    result.spirv = std::move(module);
}

#if ANYPS5_ENABLE_SPIRV_TOOLS
void RequireValidModule(const ShaderRecompiler::RecompileResult& result, const ShaderRecompiler::SpirvTarget& target) {
    std::string diagnostics;
    spvtools::SpirvTools validator(target.spirvVersion >= 0x00010400u ? SPV_ENV_VULKAN_1_1_SPIRV_1_4 : SPV_ENV_VULKAN_1_1);
    validator.SetMessageConsumer([&](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
        if (message != nullptr) diagnostics += std::string(message) + '\n';
    });
    const auto& words = result.spirv.Words();
    const bool valid = validator.Validate(words.data(), words.size());
    Require(valid, "the null-image fixture emitted invalid SPIR-V:\n" + diagnostics);
}
#endif

void Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> pixelCode, std::uint32_t shaderControl, const VkStencilOpState& stencil, std::uint32_t vertices = 3u, std::uint32_t instances = 1u, bool depthFail = false, const std::function<void(AgcDriver::Graphics::State&)>& alter = {}, const std::function<void(AgcDriver::Pm4::DrawParameters&)>& alterDraw = {}, std::span<const std::array<float, 4>> vertexData = Vertices, std::span<const std::uint32_t> vertexCode = VertexCode, std::span<const std::uint32_t> pixelData = {}, std::span<const ShaderRecompiler::MemoryRegion> pixelSrt = {}, bool nullImages = false, bool depthAlias = false) {
    Pixels.fill(Kept);
    auto target = device.Target();
    std::vector<std::uint32_t> capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end());
    std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityStencilExportEXT));
    target.supportedCapabilities = capabilities;

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(vertexData.data(), 16u, static_cast<std::uint32_t>(vertexData.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(vertexCode.data()), std::as_bytes(vertexCode)}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}},
        {WaveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    auto pixelUserData = pixelData.empty() ? std::vector<std::uint32_t>(8, 0u) : std::vector<std::uint32_t>(pixelData.begin(), pixelData.end());
    std::vector<ShaderRecompiler::MemoryRegion> pixelMemory{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}};
    pixelMemory.insert(pixelMemory.end(), pixelSrt.begin(), pixelSrt.end());
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, PixelInfo(shaderControl), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    auto pixelResult = ShaderRecompiler::Recompile(fragment);
    if (nullImages) {
        AttachNullImages(pixelResult, target, depthAlias);
#if ANYPS5_ENABLE_SPIRV_TOOLS
        RequireValidModule(pixelResult, target);
#endif
    }
    if (!pixelResult.stencilReferences.empty()) {
        Require(pixelResult.stencilReferenceDword * sizeof(std::uint32_t) + sizeof(std::uint32_t) == pixelResult.pushConstants.size(), "dynamic reference is not reserved after the stage payload");
        for (std::size_t cursor = 5; cursor < pixelResult.spirv.size();) {
            const auto count = pixelResult.spirv[cursor] >> 16u;
            Require(count != 0u && count <= pixelResult.spirv.size() - cursor, "invalid materialized replay module");
            Require((pixelResult.spirv[cursor] & 0xffffu) != spv::OpSpecConstant, "expected reference remains a pipeline specialization");
            cursor += count;
        }
    }
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, WaveSize, WaveSize, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{reinterpret_cast<std::uintptr_t>(DepthMemory.data()), reinterpret_cast<std::uintptr_t>(StencilMemory.data()), {Width, Height}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0};
    state.depthTest = depthFail;
    state.depthCompare = depthFail ? VK_COMPARE_OP_NEVER : VK_COMPARE_OP_ALWAYS;
    state.stencilTest = true;
    state.stencilFront = stencil;
    state.stencilBack = stencil;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    if (alter) alter(state);
    AgcDriver::Pm4::DrawParameters draw{0, vertices, 0, instances, 0, false};
    if (alterDraw) alterDraw(draw);
    const auto bindsBefore = GraphicsBinds.load();
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    if (!pixelResult.stencilReferences.empty()) Require(GraphicsBinds.load() - bindsBefore == 1u, "stencil replay rebound its shader pipeline within an original draw");
}

void Check(const std::function<bool(std::uint32_t, std::uint32_t)>& drawn, std::string_view what) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = drawn(x, y) ? std::byte{255} : Kept;
            const auto actual = Pixels[(y * Width + x) * 4u];
            Require(actual == expected, std::string(what) + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + std::to_string(static_cast<unsigned>(actual)) + ", expected " + (drawn(x, y) ? "drawn" : "stencil-rejected"));
        }
    }
}

}


int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const BindTrace binds(*device);
        constexpr std::uint32_t exported = 0x52u;
        constexpr std::uint32_t plain = 0x50u;
        constexpr std::array<std::uint32_t, 12> alternating{
            0x7e040f00u, 0x36060481u,
            0x7c000000u | (0xc2u << 17u) | (3u << 9u) | 0x80u,
            0x7e0802ffu, 0xffu,
            (1u << 25u) | (5u << 17u) | (4u << 9u) | 0x80u,
            0x7e0e02f2u, 0xf8000082u, 5u << 8u,
            0xf800180fu, 0x07070707u, 0xbf810000u
        };
        const auto replace = [](std::uint32_t reference = 0u, std::uint32_t mask = 0xffu) { return VkStencilOpState{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, mask, reference}; };
        const auto equal = [](std::uint32_t reference, std::uint32_t mask = 0xffu) { return VkStencilOpState{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, mask, 0u, reference}; };
        const auto all = [](std::uint32_t, std::uint32_t) { return true; };
        const auto even = [](std::uint32_t x, std::uint32_t) { return x % 2u == 0u; };
        const auto odd = [](std::uint32_t x, std::uint32_t) { return x % 2u != 0u; };
        Draw(*device, PlainPixelCode, plain, replace(), 3u, 1u, false, {}, {}, Vertices, VertexCode, {}, {}, true);
        Check(all, "an empty typed image element remains supported without stencil export");
        Draw(*device, alternating, exported, replace(), 6u, 2u, false, {}, {}, Vertices, VertexCode, {}, {}, true);
        Check(all, "an empty typed image element preserves stencil replay colors");
        Draw(*device, PlainPixelCode, plain, equal(255u));
        Check(even, "an empty typed image element preserves the even stencil reference");
        Draw(*device, PlainPixelCode, plain, equal(0u));
        Check(odd, "an empty typed image element preserves the odd stencil reference");
        bool refused = false;
        try {
            Draw(*device, alternating, exported, replace(), 3u, 1u, false, {}, {}, Vertices, VertexCode, {}, {}, true, true);
        } catch (const std::exception& error) {
            const std::string_view reason(error.what());
            refused = reason.find("stencil replay refuses") != std::string_view::npos && reason.find("feedback") != std::string_view::npos;
            if (!refused) throw;
        }
        Require(refused, "a non-null image aliasing the depth surface bypassed replay feedback checks");
        Require(std::ranges::all_of(Pixels, [](std::byte value) { return value == Kept; }), "refused depth feedback published color results");
        std::puts("stencil null feedback tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
