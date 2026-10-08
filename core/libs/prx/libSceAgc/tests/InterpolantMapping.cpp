#include "SceShaders.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <span>

extern "C" int APS5_VABI sceAgcCreateInterpolantMapping(ShaderRegister* regs, const Shader* gs, const Shader* ps);
extern "C" int APS5_VABI sceAgcUnknownCreateInterpolantMapping(ShaderRegister* regs, const Shader* gs, const Shader* ps);

namespace {

using Registers = std::array<ShaderRegister, 32>;
const Shader* mappedPixel = nullptr;
Registers preparedMapping{};
unsigned mappings = 0;
unsigned links = 0;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::exception& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

Registers filled() {
    Registers regs{};
    std::memset(regs.data(), 0xcc, sizeof(regs));
    return regs;
}

ShaderSemantic semantic(std::uint32_t id, std::uint32_t hardwareMapping, std::uint32_t f16) {
    ShaderSemantic value{};
    value.semantic = id;
    value.hardware_mapping = hardwareMapping;
    value.is_f16 = f16;
    return value;
}

void checkIdentity(const Registers& regs, std::uint32_t first, const char* message) {
    for (std::uint32_t i = first; i < regs.size(); ++i) {
        check(regs[i].offset == ShaderRegs::SPI_PS_INPUT_CNTL_0 + i && regs[i].value == i, message);
    }
}

void testIdentity() {
    auto regs = filled();
    check(sceAgcUnknownCreateInterpolantMapping(regs.data(), nullptr, nullptr) == 0, "mapping without shaders failed");
    checkIdentity(regs, 0, "mapping without a pixel shader is not the identity");
    Shader ps{};
    regs = filled();
    check(sceAgcUnknownCreateInterpolantMapping(regs.data(), nullptr, &ps) == 0, "mapping without inputs failed");
    checkIdentity(regs, 0, "mapping without inputs is not the identity");
}

void testMapping() {
    std::array<ShaderSemantic, 5> inputs{
        semantic(1, 0, 2), semantic(2, 0, 2), semantic(5, 0, 2), semantic(3, 0, 0), semantic(4, 0, 3)};
    inputs[0].is_flat_shaded = 1;
    inputs[0].default_value = 1;
    inputs[0].default_value_hi = 3;
    inputs[1].default_value_hi = 1;
    inputs[2].is_flat_shaded = 1;
    inputs[2].default_value_hi = 2;
    inputs[3].is_flat_shaded = 1;
    inputs[3].default_value = 2;
    std::array<ShaderSemantic, 4> outputs{semantic(1, 7, 3), semantic(2, 4, 1), semantic(3, 9, 0), semantic(4, 2, 1)};
    Shader gs{};
    gs.output_semantics = outputs.data();
    gs.num_output_semantics = static_cast<std::uint16_t>(outputs.size());
    Shader ps{};
    ps.input_semantics = inputs.data();
    ps.num_input_semantics = static_cast<std::uint32_t>(inputs.size());

    auto split = filled();
    check(sceAgcUnknownCreateInterpolantMapping(split.data(), &gs, &ps) == 0, "split f16 mapping failed");
    check(std::memcmp(split.data(), preparedMapping.data(), sizeof(split)) == 0, "prepared split mapping differs from published registers");
    for (std::uint32_t i = 0; i < inputs.size(); ++i) {
        check(split[i].offset == ShaderRegs::SPI_PS_INPUT_CNTL_0 + i, "interpolant register offset changed");
    }
    check(split[0].value == 0x02680707u, "high f16 half provided by the gs is not mapped");
    check(split[1].value == 0x02280124u, "high f16 half missing from the gs does not use its default");
    check(split[2].value == 0x02480220u, "unmatched high f16 half does not use its default");
    check(split[3].value == 0x00000609u, "32-bit interpolant mapping changed");
    check(split[4].value == 0x03180002u, "full f16 interpolant mapping changed");
    checkIdentity(split, static_cast<std::uint32_t>(inputs.size()), "unused interpolants are not the identity");

    auto regular = filled();
    check(sceAgcCreateInterpolantMapping(regular.data(), &gs, &ps) == 0, "mapping failed");
    check(std::memcmp(regular.data(), preparedMapping.data(), sizeof(regular)) == 0, "prepared mapping differs from published registers");
    check(regular[3].value == split[3].value && regular[4].value == split[4].value, "mappings differ outside the high f16 half mode");
    checkIdentity(regular, static_cast<std::uint32_t>(inputs.size()), "unused interpolants are not the identity");
}

void testRejections() {
    std::array<ShaderSemantic, 33> inputs{};
    Shader gs{};
    Shader ps{};
    ps.input_semantics = inputs.data();
    ps.num_input_semantics = static_cast<std::uint32_t>(inputs.size());
    std::array<ShaderRegister, 64> regs{};
    std::memset(regs.data(), 0xcc, sizeof(regs));
    const auto saved = regs;
    expectFailure([&] { sceAgcCreateInterpolantMapping(regs.data(), &gs, &ps); });
    expectFailure([&] { sceAgcUnknownCreateInterpolantMapping(regs.data(), &gs, &ps); });
    check(std::memcmp(regs.data(), saved.data(), sizeof(regs)) == 0, "rejected mapping wrote registers");
    ps.num_input_semantics = 1;
    expectFailure([&] { sceAgcUnknownCreateInterpolantMapping(regs.data(), nullptr, &ps); });
    expectFailure([&] { sceAgcUnknownCreateInterpolantMapping(nullptr, &gs, &ps); });
}

}

extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    check(shader != nullptr && context.size() == preparedMapping.size() && primitive.empty(), "invalid mapping preparation call");
    mappedPixel = shader;
    std::copy(context.begin(), context.end(), preparedMapping.begin());
    ++mappings;
}

extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    check(vertex != nullptr && pixel == mappedPixel && primitiveType == 0u, "invalid mapping link call");
    ++links;
}

int main() {
    try {
        testIdentity();
        testMapping();
        testRejections();
        check(mappings == 3u && links == 2u, "mapping preparation call count changed");
        LibcRunShutdown_nid_postfix();
        std::puts("AGC interpolant mapping tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
