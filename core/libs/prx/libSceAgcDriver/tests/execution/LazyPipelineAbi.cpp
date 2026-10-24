#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparationScope.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Header {
    Shader shader{};
    std::array<ShaderRegister, 8> registers{};
    std::array<ShaderRegister, 8> context{};
    ShaderSpecialRegs specials{};
    ShaderUserData users{};
};

template<std::size_t N>
void Describe(Header& header, const std::array<std::uint32_t, N>& code, unsigned type,
    std::initializer_list<ShaderRegister> registers, std::initializer_list<ShaderRegister> context) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.type = type;
    header.shader.user_data = &header.users;
    std::copy(registers.begin(), registers.end(), header.registers.begin());
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = registers.size();
    std::copy(context.begin(), context.end(), header.context.begin());
    header.shader.cx_registers = header.context.data();
    header.shader.num_cx_registers = context.size();
    header.shader.specials = &header.specials;
    header.specials.dispatch_modifier = 0x8000;
}

std::uint32_t Low(const void* code) { return reinterpret_cast<std::uintptr_t>(code) >> 8u; }
std::uint32_t High(const void* code) { return reinterpret_cast<std::uintptr_t>(code) >> 40u; }

void Run() {
    using namespace AgcDriver::DriverDetail;
    Require(std::getenv("APS5_LAZY_SHADER_PREPARE") != nullptr, "lazy preparation is not enabled");
    alignas(256) static const std::array<std::uint32_t, 11> vertexCode{0x7e0002ffu, 0, 0x7e0202ffu, 0, 0x7e0402ffu, 0, 0x7e0602ffu, 0x3f800000u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
    alignas(256) static const std::array<std::uint32_t, 5> pixelCode{0x7e0002ffu, 0x3f800000u, 0xf800180fu, 0, 0xbf810000u};
    static Header front, pixel;
    Describe(front, vertexCode, 2, {{0xc8, Low(vertexCode.data())}, {0xc9, High(vertexCode.data())}, {0x8a, 0}, {0x8b, 0}, {0xca, 0}, {0xcb, 0}, {0xcc, 0}}, {{0x2d5, 0x2000}});
    Describe(pixel, pixelCode, 1, {{0x8, Low(pixelCode.data())}, {0x9, High(pixelCode.data())}, {0xa, 0}, {0xb, 0}}, {{0x1b6, 0}, {0x1b3, 2}, {0x1b4, 2}, {0x1c5, 4}});
    AgcDriverRegisterShader_nid_postfix(&front.shader);
    AgcDriverRegisterShader_nid_postfix(&pixel.shader);
    const auto registry = Driver::Get().RegisteredShaders();
    const auto frontSnapshot = RegisteredHeader(*registry, &front.shader);
    const auto pixelSnapshot = RegisteredHeader(*registry, &pixel.shader);
    Require(frontSnapshot && pixelSnapshot, "lazy pipeline stages were not registered");
    const std::array<const Shader*, 2> stages{&front.shader, &pixel.shader};
    const std::array<ShaderRegister, 1> primitive{{{0x242, 17}}};
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        AgcDriver::ShaderPreparationScope scope({&front.shader, &pixel.shader});
        AgcDriverResolveShaderAbi_nid_postfix(&front.shader, {}, primitive);
        AgcDriverResolveShaderAbi_nid_postfix(&pixel.shader, {}, {});
        AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, {}, primitive);
        AgcDriverResolveGraphicsAbi_nid_postfix(&front.shader, &pixel.shader, 17);
        scope.Commit();
    }
    {
        ShaderPreparationTransaction transaction;
        for (const auto& snapshot : {frontSnapshot, pixelSnapshot}) {
            const auto& prepared = transaction.Read(*snapshot);
            Require(prepared.deferred && prepared.entries.empty() && prepared.rectangles.empty(), "pipeline ABI resolution eagerly prepared a lazy shader");
        }
        transaction.Commit();
    }
    alignas(256) static const std::array<std::uint32_t, 64> unregisteredCode{0xbf810000u};
    Shader unregistered = pixel.shader;
    unregistered.code = unregisteredCode.data();
    unregistered.shader_size = sizeof(unregisteredCode);
    bool rejected = false;
    try { AgcDriverResolveShaderAbi_nid_postfix(&unregistered, {}, {}); }
    catch (const std::exception&) { rejected = true; }
    Require(rejected, "lazy ABI accepted an unregistered header");

    std::vector<std::uint32_t> commands;
    for (const auto* header : {&front, &pixel}) {
        for (std::size_t i = 0; i < header->shader.num_sh_registers; ++i) commands.insert(commands.end(), {0xc0017600u, header->registers[i].offset, header->registers[i].value});
        for (std::size_t i = 0; i < header->shader.num_cx_registers; ++i) commands.insert(commands.end(), {0xc0016900u, header->context[i].offset, header->context[i].value});
    }
    commands.insert(commands.end(), {0xc0016900u, 0x8e, 0xfu, 0xc0016900u, 0x8f, 0xfu, 0xc0016900u, 0xd, 0x00100010u, 0xc0016900u, 0x204, 0x80000u, 0xc0016900u, 0x1c3, 4, 0xc0017900u, 0x242, 17, 0xc0012d00u, 3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "lazy rectangle draw was not submitted");
    AgcDriverWaitIdle_nid_postfix();
    {
        ShaderPreparationTransaction transaction;
        const auto& frontPrepared = transaction.Read(*frontSnapshot);
        Require(!frontPrepared.entries.empty() && !transaction.Read(*pixelSnapshot).entries.empty(), "first draw did not prepare lazy graphics stages");
        Require(!frontPrepared.rectangles.empty(), "first rectangle draw did not prepare its stage pair");
        transaction.Commit();
    }
    AgcDriverShutdown_nid_postfix();
}
}

int main() {
    try {
        if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
        Run();
        std::cout << "lazy pipeline ABI and first rectangle draw passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
