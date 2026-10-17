#include "ShaderCapture.hpp"
#include "DrawStages.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestSamplerResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace WasmGpu {
namespace {

const char* Directory() {
    static const char* directory = std::getenv("APS5_CAPTURE_SHADERS");
    return directory;
}

std::string Hex(std::uint64_t value) {
    char text[32];
    std::snprintf(text, sizeof(text), "%llx", static_cast<unsigned long long>(value));
    return text;
}

void WriteFile(const std::string& path, const std::string& content) {
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) throw std::runtime_error("cannot write " + path);
    const auto written = std::fwrite(content.data(), 1, content.size(), file);
    std::fclose(file);
    if (written != content.size()) throw std::runtime_error("cannot write " + path);
}

std::string RegisterDump(const AgcDriver::QueueState& queue) {
    std::string text;
    const auto dump = [&](const char* bank, const AgcDriver::Registers& registers) {
        registers.ForEach([&](std::uint32_t offset, std::uint32_t value) {
            char line[64];
            std::snprintf(line, sizeof(line), "%s %x %08x\n", bank, offset, value);
            text += line;
        });
    };
    dump("context", queue.context);
    dump("shader", queue.shader);
    dump("uconfig", queue.userConfig);
    return text;
}

void Append(std::string& text, const char* format, ...) {
    char line[512];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    text += line;
}

void DescribeResult(std::string& text, const char* stage, const ShaderRecompiler::RecompileResult& result) {
    for (const auto& binding : result.bindings) {
        Append(text, "%s binding %u role %u count %u words", stage, binding.binding, static_cast<unsigned>(binding.role), binding.count);
        for (const auto word : binding.guestDescriptor) Append(text, " %08x", word);
        text += "\n";
        const auto words = std::span(binding.guestDescriptor);
        if (binding.role == ShaderRecompiler::DescriptorRole::GuestImages) {
            for (std::size_t offset = 0; offset + 8 <= words.size(); offset += 8) {
                if (AgcDriver::Graphics::IsNullTextureResource(words.subspan(offset, 8))) continue;
                const auto texture = AgcDriver::Graphics::DecodeTextureResource(words.subspan(offset, 8));
                Append(text, "  texture %llx %ux%u depth %u format %u tile %u dimension %u mips %u sel %u%u%u%u\n", static_cast<unsigned long long>(texture.baseAddress), texture.width, texture.height, texture.depthOrLastArray, texture.format, static_cast<unsigned>(texture.tileMode), static_cast<unsigned>(texture.dimension), texture.mipCount, texture.dstSelX, texture.dstSelY, texture.dstSelZ, texture.dstSelW);
            }
        }
        if (binding.role == ShaderRecompiler::DescriptorRole::GuestSamplers) {
            for (std::size_t offset = 0; offset + 4 <= words.size(); offset += 4) {
                const auto sampler = AgcDriver::Graphics::DecodeSamplerResource(words.subspan(offset, 4));
                Append(text, "  sampler mag %d min %d mip %d address %d %d %d lod %.2f..%.2f bias %.2f unnormalized %d\n", sampler.magFilter, sampler.minFilter, sampler.mipmapMode, sampler.addressModeU, sampler.addressModeV, sampler.addressModeW, sampler.minLod, sampler.maxLod, sampler.lodBias, sampler.unnormalizedCoordinates);
            }
        }
    }
    for (const auto& attribute : result.vertexAttributes) Append(text, "%s attribute %u components %u fetch %u format components %u v# %08x %08x %08x %08x\n", stage, attribute.location, attribute.components, attribute.fetchIndex, attribute.formatComponents, attribute.resource.fields[0], attribute.resource.fields[1], attribute.resource.fields[2], attribute.resource.fields[3]);
    for (const auto& parameter : result.fragmentParameters) Append(text, "%s parameter location %u flat %d\n", stage, parameter.location, parameter.flat);
    for (const auto& constant : result.specialization) Append(text, "%s constant %u = %u\n", stage, constant.id, constant.value);
}

std::string StateDump(const DrawStages& draw) {
    std::string text;
    const auto& state = draw.state;
    const auto& color = state.color;
    Append(text, "path %d rectList %d topology %d restart %d cull %u front %d\n", static_cast<int>(state.stages.path), state.rectList, state.topology, state.primitiveRestart, state.cullMode, state.frontFace);
    Append(text, "color has %d targets %zu address %llx extent %ux%u format %d tile %u elementBytes %u\n", state.hasColorTarget, state.colors.size(), static_cast<unsigned long long>(color.address), color.extent.width, color.extent.height, color.format, static_cast<unsigned>(color.tileMode), color.elementBytes);
    Append(text, "depth %d test %d write %d\n", state.depth.has_value(), state.depthTest, state.depthWrite);
    const auto& blend = state.blend;
    Append(text, "blend enable %u color %d %d op %d alpha %d %d op %d mask %x constants %.2f %.2f %.2f %.2f\n", blend.blendEnable, blend.srcColorBlendFactor, blend.dstColorBlendFactor, blend.colorBlendOp, blend.srcAlphaBlendFactor, blend.dstAlphaBlendFactor, blend.alphaBlendOp, blend.colorWriteMask, state.blendConstants[0], state.blendConstants[1], state.blendConstants[2], state.blendConstants[3]);
    Append(text, "viewport %.1f %.1f %.1f %.1f depth %.2f %.2f negativeOneToOne %d scissor %d %d %u %u render %ux%u\n", state.viewport.x, state.viewport.y, state.viewport.width, state.viewport.height, state.viewport.minDepth, state.viewport.maxDepth, state.negativeOneToOne, state.scissor.offset.x, state.scissor.offset.y, state.scissor.extent.width, state.scissor.extent.height, state.renderExtent.width, state.renderExtent.height);
    Append(text, "draw indexed %d index %llx count %u size %u instances %u first %u\n", draw.draw.indexed, static_cast<unsigned long long>(draw.draw.indexAddress), draw.draw.indexCount, draw.draw.indexSize, draw.draw.instanceCount, draw.draw.firstVertex);
    return text;
}

std::mutex g_mutex;
std::set<std::pair<std::uint64_t, std::uint64_t>> g_captured;
std::set<std::string> g_failures;

}  // namespace

bool ShaderCaptureEnabled() {
    return Directory() != nullptr;
}

void CaptureDraw(std::span<const std::uint32_t> packet, const AgcDriver::QueueState& queue) {
    std::string name;
    try {
        auto stages = DecodeDraw(packet, queue);
        const auto vertex = stages.programs.front().Binary().codeAddress;
        const auto pixel = stages.programs.size() > 1 ? stages.programs[1].Binary().codeAddress : 0;
        {
            std::lock_guard lock(g_mutex);
            if (!g_captured.insert({vertex, pixel}).second) return;
        }
        name = std::string(Directory()) + "/" + Hex(vertex) + "-" + Hex(pixel);
        const auto state = StateDump(stages);
        const auto requests = BuildDrawRequests(std::move(stages));
        const ShaderRecompiler::RequestSerializer serializer;
        for (const auto& request : requests->requests) {
            WriteFile(name + (request.shader.stage == ShaderRecompiler::ShaderStage::Fragment ? ".ps.b64" : ".vs.b64"), serializer.Serialize(request));
        }
        WriteFile(name + ".regs", RegisterDump(queue));
        auto& compiled = requests->requests;
        auto text = state;
        const auto vertexResult = ShaderRecompiler::Recompile(compiled.front());
        WriteFile(name + ".vs.wgsl", vertexResult.wgsl);
        DescribeResult(text, "vs", vertexResult);
        if (compiled.size() > 1) {
            auto& pixelRequest = compiled[1];
            pixelRequest.context.pixel = AgcDriver::Graphics::DefaultUnexportedInputs(*pixelRequest.context.pixel, vertexResult.parameterExports);
            const auto pixelResult = ShaderRecompiler::Recompile(pixelRequest);
            WriteFile(name + ".ps.wgsl", pixelResult.wgsl);
            DescribeResult(text, "ps", pixelResult);
        }
        WriteFile(name + ".state", text);
        std::fprintf(stderr, "[gpu] captured %s\n", name.c_str());
    } catch (const std::exception& error) {
        std::lock_guard lock(g_mutex);
        if (g_failures.insert(error.what()).second) std::fprintf(stderr, "[gpu] capture of %s failed: %s\n", name.empty() ? "a draw" : name.c_str(), error.what());
    }
}

}  // namespace WasmGpu
