#include "VulkanTestDevice.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

alignas(256) const std::array<std::uint32_t, 1> PixelCode{0xbf810000u};

struct PixelHeader {
    Shader shader{};
    std::array<ShaderRegister, 4> registers{};
    std::array<ShaderRegister, 3> context{{{0x1b3, 2}, {0x1b4, 2}, {0x1b6, 1}}};

    PixelHeader() {
        const auto address = reinterpret_cast<std::uintptr_t>(PixelCode.data());
        shader.file_header = 0x34333231u;
        shader.version = 0x18;
        shader.header_size = sizeof(PixelHeader);
        shader.shader_size = sizeof(PixelCode);
        shader.code = PixelCode.data();
        shader.type = 1;
        registers = {{{0x008, static_cast<std::uint32_t>(address >> 8u)}, {0x009, static_cast<std::uint32_t>(address >> 40u)}, {0x00a, 0}, {0x00b, 0}}};
        shader.sh_registers = registers.data();
        shader.num_sh_registers = registers.size();
        shader.cx_registers = context.data();
        shader.num_cx_registers = context.size();
    }
};

const char* CheckRegistration(bool disabled) {
    PixelHeader pixel;
    AgcDriverRegisterShader_nid_postfix(&pixel.shader);
    auto* trace = AgcDriver::PerformanceTrace::Thread();
    if (disabled) return trace != nullptr ? "an unconfigured trace bound a buffer to a registering thread" : nullptr;
    if (trace == nullptr) return "a title thread that registered a shader has no trace buffer";
    const auto snapshot = trace->Collect();
    if (snapshot.name != "title" || snapshot.dropped) return "the registering thread's trace identity or capacity is incorrect";
    const auto registration = [](const auto& event) { return event.scope != nullptr && std::string_view(event.scope) == "Shader.Register" && event.stage == nullptr && event.length > std::chrono::steady_clock::duration::zero(); };
    if (std::none_of(snapshot.slices.begin(), snapshot.slices.end(), registration) || std::none_of(snapshot.totals.begin(), snapshot.totals.end(), registration)) return "the registration was missing from the title thread's trace slices or totals";
    return nullptr;
}

}

int main(int argc, char** argv) {
    try {
        if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
        const bool disabled = argc > 1 && std::string_view(argv[1]) == "disabled";
        if (AgcDriver::PerformanceTrace::Configured() == disabled) throw std::runtime_error("unexpected trace configuration");
        if (!disabled) AgcDriver::PerformanceTrace::Present(1, 1, nullptr);
        std::string failure = "the registering thread did not run";
        std::thread([&] {
            try {
                const char* message = CheckRegistration(disabled);
                failure = message != nullptr ? message : "";
            } catch (const std::exception& error) {
                failure = error.what();
            }
        }).join();
        if (!failure.empty()) throw std::runtime_error(failure);
        AgcDriverShutdown_nid_postfix();
        std::cout << "registration tracing checked\n";
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
