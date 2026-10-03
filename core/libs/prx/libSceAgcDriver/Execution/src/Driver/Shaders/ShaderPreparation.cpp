#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderPreparation.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "Recompiler.hpp"
#include <array>
#include <cstdlib>
#include <exception>
#include <span>
#include <vector>

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::uint8_t ComputeShaderType = 0;
constexpr std::uint32_t ComputePgmRsrc2 = 0x213;
constexpr std::uint32_t DispatchWave32 = 0x8000;

bool Enabled() {
    static const bool enabled = std::getenv("APS5_NO_SHADER_PREPARATION") == nullptr;
    return enabled;
}

}

ShaderPreparation::ShaderPreparation(const DevicePointer& device) : device(device) {
}

ShaderPreparation::~ShaderPreparation() {
    Stop();
}

void ShaderPreparation::Enqueue(const Shader& shader, std::shared_ptr<const ShaderSnapshot> snapshot) {
    if (!Enabled() || shader.type != ComputeShaderType || shader.special_sizes_bytes < sizeof(ShaderSpecialRegs) || shader.num_sh_registers == 0) return;
    if (!GuestMemory::Accessible(shader.specials, sizeof(ShaderSpecialRegs)) || !GuestMemory::Accessible(shader.sh_registers, shader.num_sh_registers * sizeof(ShaderRegister))) return;
    Pending entry{std::move(snapshot), {}, (shader.specials->dispatch_modifier & DispatchWave32) != 0 ? 32u : 64u};
    for (std::uint32_t i = 0; i < shader.num_sh_registers; ++i) entry.registers[shader.sh_registers[i].offset] = shader.sh_registers[i].value;
    {
        std::lock_guard lock(mutex);
        if (stopping) return;
        pending.push_back(std::move(entry));
        if (!worker.joinable()) worker = std::thread([this] { run(); });
    }
    changed.notify_one();
}

void ShaderPreparation::Stop() {
    {
        std::lock_guard lock(mutex);
        stopping = true;
        pending.clear();
    }
    changed.notify_all();
    if (worker.joinable()) worker.join();
}

void ShaderPreparation::run() {
    for (;;) {
        Pending entry;
        {
            std::unique_lock lock(mutex);
            changed.wait(lock, [this] { return stopping || !pending.empty(); });
            if (stopping) return;
            entry = std::move(pending.front());
            pending.pop_front();
        }
        prepare(entry);
    }
}

void ShaderPreparation::prepare(const Pending& entry) const {
    const auto localDevice = device.Load();
    if (localDevice == nullptr) return;
    try {
        const auto& snapshot = *entry.snapshot;
        const auto userCount = (entry.registers.at(ComputePgmRsrc2) >> 1u) & 0x1fu;
        const std::vector<std::uint32_t> userData(userCount, 0u);
        const auto compute = Graphics::DecodeComputeStageInfo(entry.registers);
        const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}};
        const ShaderRecompiler::RecompileRequest request{
            {ShaderRecompiler::ShaderStage::Compute, snapshot.codeAddress, snapshot.code, snapshot.headerAddress, snapshot.header},
            {entry.waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
            localDevice->Target(),
            {0, 0, 0, 128}
        };
        ShaderRecompiler::PrepareSource(request);
    } catch (const std::exception&) {
    }
}

}
