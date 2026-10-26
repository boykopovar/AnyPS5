#include "prx/libSceAgc/Shader/include/InterpolantMapping.hpp"
#include "VulkanTestDevice.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

alignas(256) const std::array<std::uint32_t, 11> VertexCode{0x7e0002ffu, 0, 0x7e0202ffu, 0, 0x7e0402ffu, 0, 0x7e0602ffu, 0x3f800000u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
alignas(256) const std::array<std::uint32_t, 1> PixelCode{0xbf810000u};

struct VertexHeader {
    Shader shader{};
    std::array<ShaderRegister, 7> registers{};
    ShaderSpecialRegs specials{};
    ShaderUserData users{};
    std::array<ShaderRegister, 1> context{{{0x2d5, 0x2000}}};

    VertexHeader() {
        const auto address = reinterpret_cast<std::uintptr_t>(VertexCode.data());
        shader.file_header = 0x34333231u;
        shader.version = 0x18;
        shader.header_size = sizeof(VertexHeader);
        shader.shader_size = sizeof(VertexCode);
        shader.code = VertexCode.data();
        shader.type = 2;
        registers = {{{0xc8, static_cast<std::uint32_t>(address >> 8u)}, {0xc9, static_cast<std::uint32_t>(address >> 40u)}, {0x8b, 0}, {0x8a, 0}, {0xca, 0}, {0xcb, 0}, {0xcc, 0}}};
        shader.sh_registers = registers.data();
        shader.num_sh_registers = registers.size();
        shader.cx_registers = context.data();
        shader.num_cx_registers = context.size();
        shader.specials = &specials;
        shader.user_data = &users;
        specials.dispatch_modifier = 0x8000;
    }
};

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

class HeldPreparationThreads {
public:
    HeldPreparationThreads() {
        const unsigned hardwareConcurrency = std::thread::hardware_concurrency();
        const unsigned workerCount = std::max(2u, hardwareConcurrency / 2u);
        for (unsigned index = 0; index < hardwareConcurrency + 2u; ++index) {
            AgcDriver::DriverDetail::SubmitRegistrationPreparation([state = state] {
                std::unique_lock lock(state->mutex);
                ++state->active;
                state->changed.notify_all();
                state->changed.wait(lock, [&] { return state->released; });
            });
        }
        std::unique_lock lock(state->mutex);
        const bool workersBusy = state->changed.wait_for(lock, std::chrono::seconds(5), [&] { return state->active >= workerCount; });
        lock.unlock();
        if (!workersBusy) {
            Release();
            throw std::runtime_error("registration preparation workers did not become busy");
        }
    }

    ~HeldPreparationThreads() {
        Release();
    }

    void Release() {
        {
            std::lock_guard lock(state->mutex);
            state->released = true;
        }
        state->changed.notify_all();
    }

private:
    struct State {
        std::mutex mutex;
        std::condition_variable changed;
        unsigned active = 0;
        bool released = false;
    };
    std::shared_ptr<State> state = std::make_shared<State>();
};

void RegistrationBesidePendingLink() {
    VertexHeader vertex;
    VertexHeader other;
    PixelHeader pixel;
    AgcDriverRegisterShader_nid_postfix(&pixel.shader);
    std::future<int> link;
    std::future<void> registration;
    bool registeredBeside = false;
    std::array<ShaderRegister, 32> mapping{};
    {
        HeldPreparationThreads held;
        AgcDriverRegisterShader_nid_postfix(&vertex.shader);
        link = std::async(std::launch::async, [&] { return sceAgcCreateInterpolantMapping_0100(mapping.data(), &vertex.shader, &pixel.shader); });
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Require(link.wait_for(std::chrono::seconds(0)) == std::future_status::timeout, "the link did not wait for the vertex shader's registration preparation");
        registration = std::async(std::launch::async, [&] { AgcDriverRegisterShader_nid_postfix(&other.shader); });
        registeredBeside = registration.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        held.Release();
    }
    registration.get();
    Require(link.get() == 0, "the interpolant mapping failed");
    Require(registeredBeside, "a link waiting for a registration preparation blocked the registration of another shader");
    for (std::uint32_t index = 0; index < mapping.size(); ++index) Require(mapping[index].offset == 0x191u + index, "the interpolant mapping wrote another register");
}

}

int main() {
    try {
        if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
        RegistrationBesidePendingLink();
        AgcDriverShutdown_nid_postfix();
        std::cout << "preparation scope settle tests passed\n";
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
