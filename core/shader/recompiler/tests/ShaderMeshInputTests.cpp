#include "Optimization/ShaderStageInputInfo.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void reject(TAction action) {
    try { action(); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("expected mesh input rejection");
}

void primitiveCounts() {
    struct Case {
        std::uint32_t primitive;
        std::uint32_t size;
        std::uint32_t step;
        std::uint32_t verticesForFour;
        std::uint32_t primitivesForSeven;
    };
    constexpr std::array cases{
        Case{1, 1, 1, 4, 7}, Case{2, 2, 2, 8, 3}, Case{3, 2, 1, 5, 6},
        Case{4, 3, 3, 12, 2}, Case{5, 3, 1, 6, 5}, Case{6, 3, 1, 6, 5}
    };
    for (const auto& test : cases) {
        ShaderRecompiler::ShaderMeshInputInfo input;
        input.inputPrimitive = test.primitive;
        require(input.InputPrimitiveSize() == test.size && input.InputPrimitiveStep() == test.step, "incorrect primitive assembly");
        require(input.InputVertexCount(0) == 0 && input.InputPrimitiveCount(0) == 0, "empty mesh produces primitives");
        require(input.InputPrimitiveCount(test.size - 1) == 0 && input.InputPrimitiveCount(test.size) == 1, "incomplete primitive count is incorrect");
        require(input.InputVertexCount(4) == test.verticesForFour, "shared vertices were counted incorrectly");
        require(input.InputPrimitiveCount(7) == test.primitivesForSeven, "trailing vertices were counted incorrectly");
        for (std::uint32_t count = 1; count <= 128; ++count) {
            const auto vertices = input.InputVertexCount(count);
            require(input.InputPrimitiveCount(vertices) == count, "mesh count round trip failed");
            require(input.InputPrimitiveCount(vertices - 1) == count - 1, "incomplete final primitive was accepted");
        }
    }
}

void countLimits() {
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    ShaderRecompiler::ShaderMeshInputInfo input;
    input.inputPrimitive = 1;
    require(input.InputVertexCount(maximum) == maximum && input.InputPrimitiveCount(maximum) == maximum, "point count overflowed");
    input.inputPrimitive = 2;
    require(input.InputVertexCount(maximum / 2) == maximum - 1, "large line list count changed");
    reject([&] { static_cast<void>(input.InputVertexCount(maximum / 2 + 1)); });
    input.inputPrimitive = 3;
    require(input.InputVertexCount(maximum - 1) == maximum && input.InputPrimitiveCount(maximum) == maximum - 1, "large line strip count changed");
    reject([&] { static_cast<void>(input.InputVertexCount(maximum)); });
    input.inputPrimitive = 4;
    require(input.InputVertexCount(maximum / 3) == maximum, "large triangle list count changed");
    reject([&] { static_cast<void>(input.InputVertexCount(maximum / 3 + 1)); });
    input.inputPrimitive = 6;
    require(input.InputVertexCount(maximum - 2) == maximum && input.InputPrimitiveCount(maximum) == maximum - 2, "large triangle strip count changed");
    reject([&] { static_cast<void>(input.InputVertexCount(maximum - 1)); });
    for (const auto primitive : {0u, 7u, 9u, 10u, 18u, maximum}) {
        input.inputPrimitive = primitive;
        reject([&] { static_cast<void>(input.InputPrimitiveSize()); });
        reject([&] { static_cast<void>(input.InputPrimitiveStep()); });
        reject([&] { static_cast<void>(input.InputPrimitiveCount(0)); });
        reject([&] { static_cast<void>(input.InputVertexCount(0)); });
    }
}

}

int main() {
    try {
        primitiveCounts();
        countLimits();
        std::cout << "Shader mesh input tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
