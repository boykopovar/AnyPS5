#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ShaderInfoCollector.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

std::string Collect(std::uint32_t attribute, std::uint32_t component, std::uint32_t mode, std::uint32_t inputNum) {
    IrProgram program;
    program.Resources().stage = IrShaderStage::Pixel;
    program.Resources().resourceTrackingComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& parameter = builder.Emit(IrOpcode::GetInterpolationParameter, IrType::U32, {&builder.Constant(attribute), &builder.Constant(component), &builder.Constant(mode)});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&parameter}));
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    ShaderPixelInputInfo pixel {};
    pixel.inputNum = inputNum;
    try {
        ShaderInfoCollector().Collect(program, ShaderStageInputInfo {nullptr, &pixel, nullptr});
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

}

int main() {
    int failures = 0;
    const auto expect = [&](bool value, const char* name, const std::string& message) {
        if (!value) {
            std::fprintf(stderr, "%s: %s\n", name, message.c_str());
            ++failures;
        }
    };
    const auto attribute = Collect(5u, 1u, 0u, 2u);
    expect(attribute.find("interpolation parameter reference") == std::string::npos, "an attribute past the pixel inputs becomes a default", attribute);
    const auto empty = Collect(0u, 0u, 0u, 0u);
    expect(empty.find("interpolation parameter reference") == std::string::npos, "attribute 0 with no SPI inputs becomes a default", empty);
    const auto component = Collect(0u, 4u, 2u, 1u);
    expect(component.find("(attribute 0 component 4 mode 2, 1 inputs)") != std::string::npos, "a component past w", component);
    const auto mode = Collect(1u, 3u, 3u, 3u);
    expect(mode.find("(attribute 1 component 3 mode 3, 3 inputs)") != std::string::npos, "a reserved interpolation mode", mode);
    const auto inRange = Collect(1u, 3u, 2u, 2u);
    expect(inRange.find("interpolation parameter reference") == std::string::npos, "an in-range reference", inRange);
    return failures == 0 ? 0 : 1;
}
