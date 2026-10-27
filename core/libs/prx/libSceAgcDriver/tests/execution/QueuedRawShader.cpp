#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void QueuedCode(bool cached) {
    using namespace AgcDriver::DriverDetail;
    alignas(256) static std::array<std::array<std::uint32_t, 64>, 2> programs{};
    auto& code = programs[cached ? 0 : 1];
    code.fill(0xbf810000u);
    code[0] = 0x7e000200u;
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    const auto before = cached ? ReadRawShader(address) : nullptr;
    std::atomic<bool> finished{false};
    std::atomic<bool> started{false};
    struct Restore {
        std::atomic<bool>& finished;
        bool active = DrawPipeline::Active();
        ~Restore() {
            finished.store(true, std::memory_order_release);
            DrawPipeline::Active() = active;
            DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
        }
    } restore{finished};
    DrawPipeline::Queue0().Enqueue([&, address] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        started.store(true, std::memory_order_release);
        while (!finished.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        const std::uint32_t replacement = 0x7e0002f2u;
        AgcDriver::GuestMemory::Write(address, std::as_bytes(std::span(&replacement, 1)), alignof(std::uint32_t));
    }, {{address, address + sizeof(std::uint32_t)}});
    while (!started.load(std::memory_order_acquire)) std::this_thread::yield();
    DrawPipeline::Active() = true;
    const auto after = ReadRawShader(address);
    finished.store(true, std::memory_order_release);
    DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
    if (after->code.front() != 0x7e0002f2u || (before != nullptr && (before->code.front() != 0x7e000200u || before == after))) {
        throw std::runtime_error(std::string(cached ? "cached" : "fresh") + " raw shader read ignored a queued write to its code");
    }
}

}

int main(int argc, char** argv) {
    try {
        if (AgcDriver::DriverDetail::DrawPipeline::Depth() == 0) return 77;
        if (argc == 1 || std::string(argv[1]) == "cached") QueuedCode(true);
        if (argc == 1 || std::string(argv[1]) == "fresh") QueuedCode(false);
        std::cout << "Raw shader reads observed queued code writes\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
