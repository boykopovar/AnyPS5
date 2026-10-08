#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <chrono>
#include <cstdlib>

namespace AgcDriver::Graphics {

void ShaderResources::prepareAddressBindings(std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots) {
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        std::uint32_t tables = 0;
        std::uint32_t faults = 0;
        for (const auto& binding : shader.program->bindings) {
            if (binding.role != ShaderRecompiler::DescriptorRole::BdaPagetable && binding.role != ShaderRecompiler::DescriptorRole::FaultBuffer) continue;
            Require(binding.kind == ShaderRecompiler::DescriptorKind::StorageBuffer && binding.count == 1 && binding.guestDescriptor.empty() && !binding.readOnly, "invalid BDA descriptor contract");
            if (binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable) ++tables;
            else ++faults;
        }
        const bool rectListFault = shader.stage == ShaderRecompiler::ShaderStage::TessellationControl && tables == 0 && faults == 1;
        Require((tables == faults || rectListFault) && tables <= 1 && faults <= 1, "invalid BDA table and fault descriptors");
        Require(shader.program->bdaAbiVersion == (faults == 0 ? 0u : ShaderRecompiler::BdaAbi::Version), "incompatible BDA ABI version");
        // Rect-list validation needs a fault buffer, but never accesses guest addresses.
        usesBda = usesBda || tables != 0;
        usesFaultBuffer = usesFaultBuffer || faults != 0;
    }
    if (usesBda) {
        Require(context.bufferDeviceAddress, "buffer device address is not enabled");
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        guestMemory.AcquireRegistered();
        const auto snapshotsStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        for (const auto& snapshot : snapshots) guestMemory.AddSnapshot(snapshot);
        if (profile) GuestBufferMemory::CountAddressBuild(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - snapshotsStart).count());
    }
}

VkDescriptorBufferInfo ShaderResources::descriptor(Allocation& allocation) {
    if (allocation.guest) return guestMemory.Descriptor(allocation.address, allocation.size, allocation.adjustment);
    if (allocation.role == ShaderRecompiler::DescriptorRole::GuestBuffers) {
        Require(context.emptyBuffer != VK_NULL_HANDLE, "no placeholder buffer for a null V#");
        return {context.emptyBuffer, 0, allocation.size};
    }
    if (allocation.role == ShaderRecompiler::DescriptorRole::BdaPagetable || allocation.role == ShaderRecompiler::DescriptorRole::FaultBuffer) {
        Require(bda != nullptr, "BDA descriptors have no memory owner");
        return allocation.role == ShaderRecompiler::DescriptorRole::BdaPagetable ? bda->Table() : bda->Fault();
    }
    Require(allocation.buffer != nullptr, "shader data has no buffer owner");
    return {allocation.buffer->Handle(), allocation.bufferOffset, allocation.size};
}

}
