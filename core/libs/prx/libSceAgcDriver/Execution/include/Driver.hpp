#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <cstddef>
#include <exception>
#include <utility>
#include <vector>
#include "SceTypes.hpp"
#include "SceShaders.hpp"

namespace AgcDriver {

struct NativeDepthSnapshot;

void Submit(const Packet* packet, std::uint32_t queue);

struct AwaitedWait {
    std::uint64_t address = 0;
    std::uint64_t reference = 0;
    std::uint64_t mask = 0;
    std::uint32_t function = 0;
    std::uint32_t bytes = 0;
};

struct RetiredOwners {
    std::uint64_t color = 0;
    std::uint64_t depthOwners = 0;
    std::uint64_t depthSurfaces = 0;
    std::uint64_t depthPlanes = 0;
};

void PreflightNativeDepthForReplay_nid_no_patch(std::span<const NativeDepthSnapshot> snapshots);
RetiredOwners DiscardNativeDepthForReplay_nid_no_patch();
void RestoreNativeDepthForReplay_nid_no_patch(std::span<const NativeDepthSnapshot> snapshots);

}

extern "C" void AgcDriverPrepareCommandWrite_nid_no_patch(std::uint64_t address, std::size_t bytes);
extern "C" void AgcDriverFlushGuestMemory_nid_no_patch(std::uint64_t address, std::size_t bytes, std::exception_ptr* failure);
extern "C" void AgcDriverGpuOwnedRanges_nid_no_patch(std::uint64_t address, std::size_t bytes, std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges, std::exception_ptr* failure);
extern "C" void AgcDriverResidentOwnersOver_nid_no_patch(std::uint64_t address, std::size_t bytes, bool gpuOnly, std::size_t* owners, std::exception_ptr* failure);
extern "C" void AgcDriverGuestMemoryChangedSince_nid_no_patch(std::uint64_t address, std::size_t bytes, std::uint64_t since, std::vector<std::pair<std::uint64_t, std::uint64_t>>& changed, std::uint64_t* generation, std::exception_ptr* failure);
extern "C" void AgcDriverRetireGpuOwners_nid_no_patch(std::uint64_t address, std::size_t bytes, AgcDriver::RetiredOwners* retired, std::exception_ptr* failure);
extern "C" void AgcDriverOmitGpuOwnersForTest_nid_no_patch(bool omit);
extern "C" void AgcDriverGuestMemoryTransaction_nid_no_patch(void (*body)(void* context), void* context, std::exception_ptr* failure);
extern "C" void AgcDriverSetGuestMemoryTransactionHookForTest_nid_no_patch(void (*hook)());
extern "C" void AgcDriverWaitIdle_nid_postfix();
extern "C" std::size_t AgcDriverRestoreNativeDepthCapture_nid_no_patch(const char* directory);
extern "C" void AgcDriverSubmitGraphics_nid_postfix(std::uint32_t* commands, std::uint32_t dwords);
extern "C" void AgcDriverUseGcnShaders_nid_postfix();
extern "C" void AgcDriverRaiseWorkerThreadPriority_nid_postfix(const char* role);
extern "C" void AgcDriverLockVulkanLoader_nid_postfix();
extern "C" void AgcDriverUnlockVulkanLoader_nid_postfix();
extern "C" void AgcDriverShutdown_nid_postfix();
extern "C" void AgcDriverRequestScreenshot_nid_postfix();
extern "C" void AgcDriverTracePresent_nid_postfix(std::uint32_t width, std::uint32_t height);
extern "C" void AgcDriverSuspendPoint_nid_postfix();
extern "C" void AgcDriverSerialProgress_nid_postfix(std::uint64_t* accepted, std::uint64_t* completed);
extern "C" bool AgcDriverHoldSettled_nid_postfix(std::int64_t milliseconds);
extern "C" void AgcDriverReleaseSettled_nid_postfix();
extern "C" bool AgcDriverSettledHeld_nid_postfix();
extern "C" bool AgcDriverSettled_nid_postfix();
extern "C" std::size_t AgcDriverAwaitedWaits_nid_postfix(AgcDriver::AwaitedWait* waits, std::size_t capacity);
extern "C" void AgcDriverWaitForPipelineJournal_nid_postfix();
extern "C" std::uint64_t AgcDriverDispatchesAdoptedAhead_nid_postfix();
extern "C" std::uint64_t AgcDriverDispatchesRejectedAhead_nid_postfix();
extern "C" void AgcDriverSetAheadAdoptHook_nid_postfix(void (*hook)());
extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader);
extern "C" std::uint32_t AgcDriverInstructionWords_nid_postfix(std::span<const std::uint32_t> code, std::uint32_t index);
extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive);
extern "C" void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive);
extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType);
extern "C" void* AgcDriverStaticTable_nid_postfix();

#endif
