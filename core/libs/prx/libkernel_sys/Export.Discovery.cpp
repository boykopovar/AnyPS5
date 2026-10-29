#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Atfork.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>

namespace {

int SceResult(int error) {
    return error == 0 ? 0 : SceKernelError(error);
}

template<class TCommand>
int WriteAmmCommand(std::initializer_list<std::uint64_t> protections, const std::optional<TCommand>& command, void* dst, std::uint64_t* outSize) {
    for (const auto protection : protections) {
        if (protection > 0xFFFFFFFFu) return SCE_KERNEL_ERROR_EINVAL;
    }
    if (!command) return SCE_KERNEL_ERROR_EINVAL;
    if (dst) std::memcpy(dst, &*command, sizeof(TCommand));
    if (outSize) *outSize = sizeof(TCommand);
    return 0;
}

}

extern "C" {

int APS5_VABI amd64_set_fsbase_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI blockpool_batch_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI blockpool_map_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI blockpool_open_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI blockpool_unmap_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI dynlib_get_obj_member_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI futimesat_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI get_module_info_list_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI get_page_table_stats_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI get_sdk_compiled_version_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI get_self_auth_info_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI get_vm_map_timestamp_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI getfsstat_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ipmimgr_call_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI is_in_sandbox_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI kldload_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI kmq_notify_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI kmq_open_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI kmq_setattr_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI kmq_timedreceive_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI kmq_timedsend_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI kmq_unlink_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ksem_close_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ksem_open_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ksem_unlink_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ktrace_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI mdbg_call_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI mdbg_service_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI mount_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI openintr_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_attr_getaffinity_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_attr_getsolosched_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_attr_setaffinity_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_attr_setcreatesuspend_np_nid_postfix(PthreadAttr* attr) {
    constexpr int GuestEinval = 22;
    if (!attr || !*attr) return GuestEinval;
    (*attr)->_createSuspended = true;
    return 0;
}

int APS5_VABI pthread_attr_setstackaddr_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_get_specificarray_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_get_user_context_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_getstack_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_multi_np_nid_postfix(void) {
    pthread_resume_all_np_nid_postfix();
    return 0;
}

int APS5_VABI pthread_mutex_getprioceiling_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_mutex_init_for_mono_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_mutex_setprioceiling_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_mutexattr_getgen_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_mutexattr_setgen_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI pthread_resume_all_np_nid_postfix(void) {
    ResumeAllGuestThreads_nid_no_patch();
    scePthreadTestcancel();
}

int APS5_VABI pthread_resume_np_nid_postfix(Pthread thread) {
    constexpr int GuestEinval = 22;
    if (!thread) return GuestEinval;
    return ResumeGuestThread_nid_no_patch(thread);
}

int APS5_VABI pthread_resume_user_context_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_set_user_context_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_single_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_suspend_all_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_suspend_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_suspend_user_context_np_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_switch_add_np_nid_postfix(void) {
    return 45;
}

int APS5_VABI pthread_switch_delete_np_nid_postfix(void) {
    return 45;
}

int APS5_VABI ptrace_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pwrite_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI reboot_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI revoke_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI rfork_thread_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI rtld_printf_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpDebugForceCoredumpOnAppClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpDebugTriggerCoredump(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpGetStopInfoGpu(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpInternalGetThreadContextInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAddACInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAddCpumodeEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAddGpuExceptionEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAddProcessToCanvasMap(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAddResource(void) {
    return 0;
}

int APS5_VABI sceKernelAllocateDirectMemory2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAllocateDirectMemoryForApp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAllocateDirectMemoryForMiniApp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAllocateToolMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAllocateTraceDirectMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAprCtrl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetResult_TEST(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAprSubmitCommandBuffer_TEST(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAvailableToolMemorySize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelBacktraceSelf(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelBeginAppMount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelBeginAppMount2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelCallIndirectBuffer(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelCallIndirectBuffer2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelClearBootReqNotifyCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelClearGameDirectMemory(void) {
    return static_cast<int>(0x8002004e);
}

int APS5_VABI sceKernelCloseEport(void) {
    return static_cast<int>(0x80020001);
}

int APS5_VABI sceKernelCloseSema(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelCreateBudget(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelCreateEport(void) {
    return static_cast<int>(0x80020001);
}

int APS5_VABI sceKernelDebugAcquireAndUpdateDebugRegister(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugGetAppStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugGetPauseCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugGetPrivateLogText(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugGetSchedLockMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugGetSdkLogText(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugGpuPaDebugIsInProgress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugInjectProcessEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugPackageCorrupted(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugRaiseExceptionWithContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugRaiseExceptionWithInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDebugReleaseDebugContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDeleteACInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDeleteBudget(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDeleteCpumodeEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDeleteEport(void) {
    return static_cast<int>(0x80020001);
}

int APS5_VABI sceKernelDeleteGpuExceptionEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDeleteProcessFromCanvasMap(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDeleteSavedApp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDirectMemoryQueryForId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelEnableDmemAliasing2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelEnablePthreadObjectCheck(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelEndAppMount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelEndAppMount2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelEventLogInit(void) {
    return 0;
}

int APS5_VABI sceKernelEventLogPread(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelEventLogRead(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelEventLogWrite(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelExpectedHddFormat(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelFlock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetACInfoAllForCoredump(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetAppCategoryType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetAppInfoByAppId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetAppState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetAslrStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetAutoUpdateVersion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetBackupRestoreMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetBackupRestoreModeOfNextBoot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetBasicProductShape(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetBetaUpdateTestForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetBioUsageAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetBootReqNotifyCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetBudget(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetBuildNumber(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetCallRecord(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetChildApp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetCompiledSdkVersionByPath(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetCompiledSdkVersionByPid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetCoredumpAppInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetCpuUsage(void* stats, int32_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetCpuUsageAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetCpuUsageProc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetCpuUsageProc2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetCpuUsageThread(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetCurrentFanDuty(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetDataTransferMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetDebugMenuMiniModeForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetDebugMenuModeForPsmForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetDebugMenuModeForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetDefaultToolMemorySize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetEmergencyErrorLog(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetExecutableModuleHandle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetExtLibcHandle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetFakeFinalizeMenuForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetFirstImageAddr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetFlagedUpdaterForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetForceUpdateModeForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetGPO(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetHwFeatureInfoForDecid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetHwModelName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetHwSerialNumber(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetIdPs(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetIdTableCurrentCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetIoreq(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetIpcPath(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetKnobs(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetKqueueFromEqueue(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetMemoryPstate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetPagingStatsOfAllObjects(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetPagingStatsOfAllThreads(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetParentApp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetPhysPageSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetProcessName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetProcessTypeOfBudget(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetProductCode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetProductStr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetPs4SystemSwVersion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetPsmIntdevModeForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetPsnAccessTraceLogForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetQafExpirationTimeNotafterForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetQafExpirationTimeNotbeforeForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetQafGenerationForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetQafNameForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetRenderingMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetResidentCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetResidentFmemCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetResource(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetResourceLimit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetResourceOne(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetSafemode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetSanitizerMallocReplace(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetSanitizerNewReplace(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetSocClock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetSpecialIForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetSubsysId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetSubsysSubId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetSystemLevelDebuggerModeForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetSystemSwBeta(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetThreadName(uint32_t id, char* name) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUniversalMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUpdVersion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenDataExecutionForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenExpirationTimeNotafterForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenExpirationTimeNotbeforeForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenFakeSharefactoryForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenFlagedUpdaterForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenNpEnvSwitchingForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenSaveDataRepairForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenStoreModeForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenUseSoftwagnerForAcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenUseSoftwagnerForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetUtokenWeakenedPortRestrictionForRcmgr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetVrCaptureSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGiveDirectMemoryToMapper(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelHasNeoMode(void) {
    return 1;
}

int APS5_VABI sceKernelHwGetBdDriveInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelHwHasOpticalOut(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelHwHasWlanBt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccControlBDPowerState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccControlUSBPowerState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetBDPowerState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetBootTimestamp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetCPMode(void) {
    return static_cast<int>(0x8002002d);
}

int APS5_VABI sceKernelIccGetCountTime(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetCpuInfoBit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetErrLog(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetHwInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetPowerNumberOfBootShutdown(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetPowerOperatingTime(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetPowerUpCause(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetSysEventLog(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccGetUSBPowerState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccIndicatorBootDone(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccIndicatorShutdown(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccIndicatorStandby(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccIndicatorStandbyBoot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccIndicatorStandbyShutdown(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccNotifyBootStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccNvsFlush(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccReadPowerBootMessage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccSetBuzzer(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccSetCPMode(void) {
    return static_cast<int>(0x8002002d);
}

int APS5_VABI sceKernelIccSetCpuInfoBit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIccSetDownloadMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelInternalGetKmemStatistics(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelInternalGetMapStatistics(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelInternalHeapPrintBacktraceWithModuleInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelInternalMapDirectMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelInternalMapNamedDirectMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelInternalResumeDirectMemoryRelease(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelInternalSuspendDirectMemoryRelease(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIsAllowedToSelectDvdRegion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIsAuthenticNeo(void) {
    return 1;
}

int APS5_VABI sceKernelIsDebuggerAttached(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIsExperimentalBeta(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIsGenuineKratosCex(void) {
    return 0;
}

int APS5_VABI sceKernelIsGenuineN(void) {
    return 1;
}

int APS5_VABI sceKernelIsInSandbox(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIsM2DeviceAttached(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIsMainOnStanbyMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIsMainOnStandbyMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIsModuleLoaded(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIsNeoMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelIsPs4Process(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelJitGetSharedMemoryInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelKernelHeapUsage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


int APS5_VABI sceKernelLoadStartModuleInternalForMono(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelLwfsAllocateBlock(int32_t fd, int64_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int64_t APS5_VABI sceKernelLwfsLseek(int32_t fd, int64_t offset, int32_t whence) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelLwfsSetAttribute(int32_t fd, int32_t attribute) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelLwfsTrimBlock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int64_t APS5_VABI sceKernelLwfsWrite(int32_t fd, const void* data, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelMapSanitizerShadowMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelMapToolMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelMapTraceMemory(void) {
    return static_cast<int>(0x8002004e);
}

int APS5_VABI sceKernelMapperGetParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelMemoryPoolMove(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelMlockall(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelMsync(void* address, std::size_t length, int flags) {
    return msync_nid_postfix(address, length, flags) == 0 ? 0 : SceKernelError(*__error_nid_postfix());
}

int APS5_VABI sceKernelNormalizePath(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelNotifyAppEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelNotifyAppStateChanged(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelNotifySystemSuspendResumeProgress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelNotifySystemSuspendStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelOpenEport(void) {
    return static_cast<int>(0x80020001);
}

int APS5_VABI sceKernelOpenInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelOpenSema(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelPrepareDirectMemorySwap(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelPrepareToResumeProcess(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelPrepareToSuspendProcess(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelProtectDirectMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelProtectDirectMemoryForPID(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelQueryToolMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelQueryTraceMemory(void) {
    return static_cast<int>(0x80020002);
}

int APS5_VABI sceKernelRandomizedPath(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelRdup(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelReboot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelReleaseToolMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelReleaseTraceDirectMemory(void) {
    return 0;
}

int APS5_VABI sceKernelReportUnpatchedFunctionCall(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelReserve2mbPage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelReserveSystemDirectMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelRestoreApp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelResumeDirectMemoryRelease(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelResumeProcess(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelRtldControl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSandboxPath(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSaveApp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetAppInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetAppState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetBackupRestoreMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetBaseModeClock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetBesteffort(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetBootReqNotify(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetBudget(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetCallRecord(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetCompressionAttribute(int32_t fd, int32_t attribute) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetCoredumpAppInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetDataTransferMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetEmergencyErrorLog(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetFsstParam(int32_t fd, void* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetGPI(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetGameDirectMemoryLimit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetGpuCu(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetIoreq(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetKnobs(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetMemoryPstate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetNeoModeClock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetPackageInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetPhysFmemLimit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetProcessName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetProcessProperty(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetProcessPropertyString(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetSafemode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetSafemodeDirect(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetSuspendState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetTimezoneInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetUpdatemode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetVmContainer(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSettimeofday(const KernelTimeval* tv, const KernelTimezone* tz) {
    return settimeofday_nid_postfix(tv, tz) == 0 ? 0 : SceKernelError(*__error_nid_postfix());
}

int APS5_VABI sceKernelSflashGetWritePrio(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSflashSetWritePrio(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSlvNotifyError(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSpawn(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelStreamWriteActivate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelStreamWriteCreate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelStreamWriteDeactivate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelStreamWriteDelete(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelStreamWriteRead(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelStreamWriteStat(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelStreamWriteWrite(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSuspendDirectMemoryRelease(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSuspendProcess(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSuspendSystem(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSwitchToBaseMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSwitchToNativeMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSwitchToNeoMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelTerminateProcess(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelTerminateSysCore(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelTitleWorkdaroundIsEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelTraceMemoryTypeProtect(void) {
    return static_cast<int>(0x8002004e);
}

int APS5_VABI sceKernelTriggerEport(void) {
    return static_cast<int>(0x80020001);
}

int APS5_VABI sceKernelVirtualQueryAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelWaitCommandBufferCompletion(std::uint32_t id) {
    return AmmSubmitted_nid_no_patch(id) ? 0 : SCE_KERNEL_ERROR_ESRCH;
}

int APS5_VABI sceKernelWriteEventQueueOnCompletionCommand(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelWriteMapCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t* outSize) {
    return WriteAmmCommand({protection}, Apr::EncodeAmmMap(Apr::Opcode::AmmMap, address, 0, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteMapCommand2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelWriteMapDirectCommand(void* dst, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t* outSize) {
    return WriteAmmCommand({protection}, Apr::EncodeAmmMap(Apr::Opcode::AmmMapDirect, address, directOffset, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteMapDirectWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection}, Apr::EncodeAmmMap(Apr::Opcode::AmmMapDirect, address, directOffset, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteMapWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection}, Apr::EncodeAmmMap(Apr::Opcode::AmmMap, address, 0, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteModifyMtypeProtectWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t mask, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection, mask}, Apr::EncodeAmmProtect(Apr::Opcode::AmmModifyMtypeProtect, address, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection), static_cast<std::int32_t>(mask)), dst, outSize);
}

int APS5_VABI sceKernelWriteModifyProtectCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t protection, std::uint64_t mask, std::uint64_t* outSize) {
    return WriteAmmCommand({protection, mask}, Apr::EncodeAmmProtect(Apr::Opcode::AmmModifyProtect, address, size, 0, static_cast<std::int32_t>(protection), static_cast<std::int32_t>(mask)), dst, outSize);
}

int APS5_VABI sceKernelWriteModifyProtectWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t protection, std::uint64_t mask, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection, mask}, Apr::EncodeAmmProtect(Apr::Opcode::AmmModifyProtect, address, size, 0, static_cast<std::int32_t>(protection), static_cast<std::int32_t>(mask)), dst, outSize);
}

int APS5_VABI sceKernelWriteMultiMapCommand(void* dst, std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::uint64_t protection, std::uint64_t* outSize) {
    return WriteAmmCommand({protection}, Apr::EncodeAmmRemap(Apr::Opcode::AmmMultiMap, address, alias, size, static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteMultiMapWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection}, Apr::EncodeAmmRemap(Apr::Opcode::AmmMultiMap, address, alias, size, static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteRemapCommand(void* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::uint64_t protection, std::uint64_t* outSize) {
    return WriteAmmCommand({protection}, Apr::EncodeAmmRemap(Apr::Opcode::AmmRemap, address, source, size, static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteRemapWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection}, Apr::EncodeAmmRemap(Apr::Opcode::AmmRemap, address, source, size, static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteSdkEventLog(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelWriteSdkEventLogV(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelWriteUnmapCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t* outSize) {
    return WriteAmmCommand({}, Apr::EncodeAmmUnmap(address, size), dst, outSize);
}

int APS5_VABI sceKernelWriteWaitOnAddressCommand(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelWriteWaitOnCounterCommand(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelWriteWriteAddressCommand(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelWriteWriteCounterCommand(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelYieldCpumode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcMspaceCreateForMonoMutex(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadAtfork(void (APS5_VABI* prepare)(), void (APS5_VABI* parent)(), void (APS5_VABI* child)()) {
    return SceResult(pthread_atfork_nid_postfix(prepare, parent, child));
}

int APS5_VABI scePthreadAttrGetscope(const PthreadAttr* attr, int* scope) {
    return SceResult(pthread_attr_getscope_nid_postfix(attr, scope));
}

int APS5_VABI scePthreadAttrSetcreatesuspend(PthreadAttr* attr) {
    const int error = pthread_attr_setcreatesuspend_np_nid_postfix(attr);
    return error == 0 ? 0 : SceKernelError(error);
}

int APS5_VABI scePthreadAttrSetscope(PthreadAttr* attr, int scope) {
    return SceResult(pthread_attr_setscope_nid_postfix(attr, scope));
}

int APS5_VABI scePthreadBarrierDestroy(PthreadBarrierPrivate** barrier) {
    return SceResult(pthread_barrier_destroy_nid_postfix(barrier));
}

int APS5_VABI scePthreadBarrierInit(PthreadBarrierPrivate** barrier, PthreadBarrierattrPrivate* const* attr, unsigned count, const char* name) {
    (void)name;
    return SceResult(pthread_barrier_init_nid_postfix(barrier, attr, count));
}

int APS5_VABI scePthreadBarrierWait(PthreadBarrierPrivate** barrier) {
    const int result = pthread_barrier_wait_nid_postfix(barrier);
    return result > 0 ? SceKernelError(result) : result;
}

int APS5_VABI scePthreadBarrierattrDestroy(PthreadBarrierattrPrivate** attr) {
    return SceResult(pthread_barrierattr_destroy_nid_postfix(attr));
}

int APS5_VABI scePthreadBarrierattrGetpshared(PthreadBarrierattrPrivate* const* attr, int* pshared) {
    return SceResult(pthread_barrierattr_getpshared_nid_postfix(attr, pshared));
}

int APS5_VABI scePthreadBarrierattrInit(PthreadBarrierattrPrivate** attr) {
    return SceResult(pthread_barrierattr_init_nid_postfix(attr));
}

int APS5_VABI scePthreadBarrierattrSetpshared(PthreadBarrierattrPrivate** attr, int pshared) {
    return SceResult(pthread_barrierattr_setpshared_nid_postfix(attr, pshared));
}

int APS5_VABI scePthreadGetconcurrency(void) {
    return pthread_getconcurrency_nid_postfix();
}

int APS5_VABI scePthreadGetcpuclockid(Pthread thread, int* clockId) {
    return SceResult(pthread_getcpuclockid_nid_postfix(thread, clockId));
}


int APS5_VABI scePthreadMulti(void) {
    const int error = pthread_multi_np_nid_postfix();
    return error == 0 ? 0 : SceKernelError(error);
}

int APS5_VABI scePthreadMutexGetprioceiling(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadMutexInitForInternalLibc(PthreadMutex* mutex, const PthreadMutexattr* attr) {
    return SceResult(pthread_mutex_init_nid_postfix(mutex, attr));
}

int APS5_VABI scePthreadMutexIsowned(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadMutexSetprioceiling(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadMutexattrGetgen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadMutexattrInitForInternalLibc(PthreadMutexattr* attr) {
    return SceResult(pthread_mutexattr_init_nid_postfix(attr));
}

int APS5_VABI scePthreadMutexattrSetgen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadResume(Pthread thread) {
    const int error = pthread_resume_np_nid_postfix(thread);
    return error == 0 ? 0 : SceKernelError(error);
}

void APS5_VABI scePthreadResumeAll(void) {
    pthread_resume_all_np_nid_postfix();
}

int APS5_VABI scePthreadSetBesteffort(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadSetconcurrency(int level) {
    return SceResult(pthread_setconcurrency_nid_postfix(level));
}

int APS5_VABI scePthreadSingle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadSuspend(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadSuspendAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadTimedjoin(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sched_setscheduler_nid_postfix(int pid, int policy, const KernelSchedParam* param) {
    const auto fail = [](int error) {
        *__error_nid_postfix() = error;
        return -1;
    };
    if (param == nullptr) return fail(14);
    if (pid != 0 && pid == getpid_nid_postfix()) NotImplemented_nid_no_patch("sched_setscheduler: the first thread of the process, which its own pid selects, is not tracked");
    if (pid != 0) return fail(3);
    if (policy != 1 && policy != 2 && policy != 3) return fail(22);
    if (scePthreadSetschedparam(scePthreadSelf(), policy, param) != 0)
        NotImplemented_nid_no_patch("sched_setscheduler: a policy and priority scePthreadSetschedparam does not accept, where FreeBSD answers EPERM or EINVAL and the console's answer is unverified");
    return 0;
}

int APS5_VABI set_phys_fmem_limit_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sigqueue_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sigreturn_nid_postfix(const UserUcontext* context) {
    return SigreturnContext_nid_no_patch(context);
}

int APS5_VABI statfs_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI swapon_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI symlinkat_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sysc_e00_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sysc_m00_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sysc_s00_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI tcgetsid_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI tcsetsid_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI unmount_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI utrace_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI vfork_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI wait3_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI wait4_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
