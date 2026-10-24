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

extern "C" int APS5_VABI sceAgcCreateInterpolantMapping_0100(ShaderRegister* regs, const Shader* gs, const Shader* ps);
extern "C" int APS5_VABI sceAgcCreateInterpolantMapping_10000(ShaderRegister* regs, const Shader* gs, const Shader* ps);
extern "C" int APS5_VABI sceAgcUpdateInterpolantMapping_10000(ShaderRegister* regs, const Shader* gs, const Shader* ps);
extern "C" int APS5_VABI sceAgcCreateInterpolantMapping_nid_postfix(ShaderRegister* regs, const Shader* gs, const Shader* ps);
extern "C" int APS5_VABI sceAgcUpdateInterpolantMapping_nid_postfix(ShaderRegister* regs, const Shader* gs, const Shader* ps);

namespace {

using Registers = std::array<ShaderRegister, 32>;
const Shader* mappedPixel = nullptr;
Registers preparedMapping{};
std::size_t preparedCount = 0;
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
    check(sceAgcCreateInterpolantMapping_10000(regs.data(), nullptr, nullptr) == 0, "mapping without shaders failed");
    checkIdentity(regs, 0, "mapping without a pixel shader is not the identity");
    Shader ps{};
    regs = filled();
    check(sceAgcCreateInterpolantMapping_10000(regs.data(), nullptr, &ps) == 0, "mapping without inputs failed");
    checkIdentity(regs, 0, "mapping without inputs is not the identity");
    regs = filled();
    check(sceAgcUpdateInterpolantMapping_10000(regs.data(), nullptr, &ps) == 0, "update without inputs failed");
    check(preparedCount == 0u, "update without inputs prepared registers");
    const auto untouched = filled();
    check(std::memcmp(regs.data(), untouched.data(), sizeof(regs)) == 0, "update without inputs wrote registers");
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
    check(sceAgcCreateInterpolantMapping_10000(split.data(), &gs, &ps) == 0, "split f16 mapping failed");
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

    auto update = filled();
    const auto untouched = filled();
    const std::size_t written = inputs.size() * sizeof(ShaderRegister);
    check(sceAgcUpdateInterpolantMapping_10000(update.data(), &gs, &ps) == 0, "update mapping failed");
    check(preparedCount == inputs.size(), "update prepared registers past its inputs");
    check(std::memcmp(update.data(), preparedMapping.data(), written) == 0, "prepared update differs from published registers");
    check(std::memcmp(update.data(), split.data(), written) == 0, "update mapping differs from the split f16 mapping");
    check(std::memcmp(update.data() + inputs.size(), untouched.data(), sizeof(update) - written) == 0, "update wrote past its inputs");

    auto regular = filled();
    check(sceAgcCreateInterpolantMapping_0100(regular.data(), &gs, &ps) == 0, "mapping failed");
    check(std::memcmp(regular.data(), preparedMapping.data(), sizeof(regular)) == 0, "prepared mapping differs from published registers");
    check(regular[3].value == split[3].value && regular[4].value == split[4].value, "mappings differ outside the high f16 half mode");
    checkIdentity(regular, static_cast<std::uint32_t>(inputs.size()), "unused interpolants are not the identity");
}

void testUpdateMapping() {
    std::array<ShaderSemantic, 4> inputs{semantic(1, 0, 0), semantic(2, 0, 1), semantic(3, 0, 2), semantic(4, 0, 3)};
    inputs[0].is_custom = 1;
    inputs[0].default_value = 2;
    inputs[1].default_value = 1;
    inputs[1].default_value_hi = 1;
    inputs[2].default_value_hi = 2;
    inputs[3].is_custom = 1;
    inputs[3].default_value = 3;
    inputs[3].default_value_hi = 3;
    std::array<ShaderSemantic, 3> outputs{semantic(1, 5, 0), semantic(2, 7, 1), semantic(4, 9, 3)};
    Shader gs{};
    gs.output_semantics = outputs.data();
    gs.num_output_semantics = static_cast<std::uint16_t>(outputs.size());
    Shader ps{};
    ps.input_semantics = inputs.data();
    ps.num_input_semantics = static_cast<std::uint32_t>(inputs.size());

    auto update = filled();
    const auto untouched = filled();
    check(sceAgcUpdateInterpolantMapping_10000(update.data(), &gs, &ps) == 0, "update mapping failed");
    check(update[0].value == 0x00000625u, "custom interpolant update changed");
    check(update[1].value == 0x01380107u, "low f16 half update changed");
    check(update[2].value == 0x02480220u, "unmatched high f16 half update changed");
    check(update[3].value == 0x03680709u, "full f16 interpolant update changed");
    for (std::uint32_t i = 0; i < inputs.size(); ++i) {
        check(update[i].offset == ShaderRegs::SPI_PS_INPUT_CNTL_0 + i, "updated register offset changed");
    }
    const std::size_t written = inputs.size() * sizeof(ShaderRegister);
    check(std::memcmp(update.data() + inputs.size(), untouched.data(), sizeof(update) - written) == 0, "update wrote past its inputs");

    auto create = filled();
    check(sceAgcCreateInterpolantMapping_10000(create.data(), &gs, &ps) == 0, "split f16 mapping failed");
    check(std::memcmp(create.data(), update.data(), written) == 0, "update differs from the split f16 mapping");
    checkIdentity(create, static_cast<std::uint32_t>(inputs.size()), "unused interpolants are not the identity");

    inputs[1].default_value = 0;
    inputs[3].default_value = 0;
    update = filled();
    check(sceAgcUpdateInterpolantMapping_10000(update.data(), &gs, &ps) == 0, "update with split f16 defaults failed");
    check(update[1].value == 0x01380007u && update[3].value == 0x03680409u, "f16 DEFAULT_VAL does not come from the low default");
}

void testUnversionedMapping() {
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
    const std::array<std::uint32_t, 5> flatDefault{0x02680527u, 0x02380024u, 0x02580020u, 0x00000629u, 0x03180002u};
    const std::size_t written = inputs.size() * sizeof(ShaderRegister);
    const auto untouched = filled();

    auto create = filled();
    check(sceAgcCreateInterpolantMapping_nid_postfix(create.data(), &gs, &ps) == 0, "unversioned mapping failed");
    for (std::uint32_t i = 0; i < inputs.size(); ++i) {
        check(create[i].offset == ShaderRegs::SPI_PS_INPUT_CNTL_0 + i && create[i].value == flatDefault[i], "unversioned mapping changed");
    }
    checkIdentity(create, static_cast<std::uint32_t>(inputs.size()), "unused interpolants are not the identity");

    auto update = filled();
    check(sceAgcUpdateInterpolantMapping_nid_postfix(update.data(), &gs, &ps) == 0, "unversioned update failed");
    check(std::memcmp(update.data(), create.data(), written) == 0, "unversioned update differs from its mapping");
    check(std::memcmp(update.data() + inputs.size(), untouched.data(), sizeof(update) - written) == 0, "unversioned update wrote past its inputs");
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
    expectFailure([&] { sceAgcCreateInterpolantMapping_0100(regs.data(), &gs, &ps); });
    expectFailure([&] { sceAgcCreateInterpolantMapping_10000(regs.data(), &gs, &ps); });
    expectFailure([&] { sceAgcUpdateInterpolantMapping_10000(regs.data(), &gs, &ps); });
    check(std::memcmp(regs.data(), saved.data(), sizeof(regs)) == 0, "rejected mapping wrote registers");
    ps.num_input_semantics = 1;
    expectFailure([&] { sceAgcCreateInterpolantMapping_10000(regs.data(), nullptr, &ps); });
    expectFailure([&] { sceAgcCreateInterpolantMapping_10000(nullptr, &gs, &ps); });
    expectFailure([&] { sceAgcUpdateInterpolantMapping_10000(regs.data(), nullptr, &ps); });
    expectFailure([&] { sceAgcUpdateInterpolantMapping_10000(nullptr, &gs, &ps); });
    expectFailure([&] { sceAgcUpdateInterpolantMapping_10000(regs.data(), &gs, nullptr); });
}

}

extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    check(shader != nullptr && context.size() <= preparedMapping.size() && primitive.empty(), "invalid mapping preparation call");
    mappedPixel = shader;
    std::copy(context.begin(), context.end(), preparedMapping.begin());
    preparedCount = context.size();
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
        testUpdateMapping();
        testUnversionedMapping();
        testRejections();
        check(mappings == 10u && links == 8u, "mapping preparation call count changed");
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
