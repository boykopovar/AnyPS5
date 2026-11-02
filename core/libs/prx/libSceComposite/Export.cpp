#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceComposite/include/InventoryExports.hpp"
#include "prx/libSceComposite/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceCompositorAllocateIndex() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorCheckCrash() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorCommandGpuPerfBegin() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorCommandGpuPerfEnd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorCreateIndirectRenderTarget() {
    return static_cast<int>(0x80d40006);
}

int APS5_VABI sceCompositorDeleteIndirectRenderTarget() {
    return static_cast<int>(0x80d40006);
}

int APS5_VABI sceCompositorFlush() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorFlushWithRepeat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetAnotherProcessSystemAddress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetAnotherProcessSystemSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetAnotherProcessVideoAddress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetAnotherProcessVideoSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetClientList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetDmemOffset() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetDummyAddress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetDummySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetRemainedCommandNum() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetRenderTargetResolution() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetSystemAddress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetSystemSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetVideoAddress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetVideoSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorHandleProcessEvents() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorInitWithProcessOrder() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorInsertThreadTraceMarker() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorIsDebugCaptureEnabled() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorIsReady4K() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorLockCommandBuffer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorMakeCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorMapAnotherProcess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorMemoryPoolDecommit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorPrivacyMaskPushArea() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorQuit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorReleaseCommandBuffer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorReleaseIndex() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorReleaseLocalStall() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorReserveCommandBuffer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorResetZoomCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetAgcCompositeCanvasCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetAgcContextCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetCompositeCanvasCommandInC() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetCursorImageAddress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetDebugPositionCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetEndOfFrameCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetEventCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetFilterType() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetFlipCommand2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetGameSufaceControlCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetGnmContextCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetIndirectCanvasCommand() {
    return 0;
}

int APS5_VABI sceCompositorSetIndirectCompositionCommand() {
    return 0;
}

int APS5_VABI sceCompositorSetIndirectRenderTargetConfigCommand() {
    return 0;
}

int APS5_VABI sceCompositorSetInvisibleCanvasCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetKeepDisplayBufferCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetLocalStallCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetMemoryCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetPatchCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetPostEventCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetRepeatCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetResolutionCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetScalingLayoutInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetSeparateContextCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetSystemConfigWithMask() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetVideoOutMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetVirtualCanvasAgcContextCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetVirtualCanvasCommandFooter() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetVirtualCanvasPatchCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetVrMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSetZoomCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorSystemConfig() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorWaitEndOfRendering() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorWaitEvent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorWaitPostEvent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompsoitorGetGpuClock() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompsoitorGetProcessRenderingTime() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompsoitorGetRenderingTime() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCompositorGetMemoryPoolAddress() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorGetMemoryPoolSize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorGetNullCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorGetSecondaryMemoryAddress() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorGetSecondaryMemorySize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorGetTFRing() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorGetVirtualCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorIsVirtualCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorMemoryPoolCommit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorMemoryPoolCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorPermanentMemoryCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorSecondaryMemoryCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorSetCanvasIdToCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorSetFlipCommand3() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorSetMainPort() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorSetMainPortPixelFormat() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorSetMetaIdToCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorSetNativeVrVideoOutMode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorSetTFRing() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorSetVirtualCanvasRenderTargetCommand() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorStartSeeThroughBuffer() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorStopSeeThroughBuffer() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCompositorWaitEndOfRenderingIfGpuWorking() {
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI sceCompositorSecondaryMemoryRelease() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceCompositeUnknown__plus_A5zYfomrKo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+A5zYfomrKo", libSceCompositeUnknown__plus_A5zYfomrKo_nid_no_patch);

int APS5_VABI libSceCompositeUnknown__plus_DMKcAbpcrQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+DMKcAbpcrQ", libSceCompositeUnknown__plus_DMKcAbpcrQ_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_0hVj7ycuNjY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0hVj7ycuNjY", libSceCompositeUnknown_0hVj7ycuNjY_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_3znAWbdvzNo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("3znAWbdvzNo", libSceCompositeUnknown_3znAWbdvzNo_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_4iL4YvcEKdU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4iL4YvcEKdU", libSceCompositeUnknown_4iL4YvcEKdU_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_5vAisOgay3M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5vAisOgay3M", libSceCompositeUnknown_5vAisOgay3M_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_8l316W539Zw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8l316W539Zw", libSceCompositeUnknown_8l316W539Zw_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_FDHuP_plus_z8KnM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("FDHuP+z8KnM", libSceCompositeUnknown_FDHuP_plus_z8KnM_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_HXxq9giIX_minus_I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HXxq9giIX-I", libSceCompositeUnknown_HXxq9giIX_minus_I_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_KpuS3CIcckw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KpuS3CIcckw", libSceCompositeUnknown_KpuS3CIcckw_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_NMGq0MJ2iGo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NMGq0MJ2iGo", libSceCompositeUnknown_NMGq0MJ2iGo_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_RPdjyM2WKcw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RPdjyM2WKcw", libSceCompositeUnknown_RPdjyM2WKcw_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_SpfMcnnpfDA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SpfMcnnpfDA", libSceCompositeUnknown_SpfMcnnpfDA_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_UEW01n5qkdA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("UEW01n5qkdA", libSceCompositeUnknown_UEW01n5qkdA_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_XZzyQX4T5ts_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("XZzyQX4T5ts", libSceCompositeUnknown_XZzyQX4T5ts_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_Yck3L7ExyVc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Yck3L7ExyVc", libSceCompositeUnknown_Yck3L7ExyVc_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_Z25ikyBUYy0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Z25ikyBUYy0", libSceCompositeUnknown_Z25ikyBUYy0_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_aczOzib8mzI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aczOzib8mzI", libSceCompositeUnknown_aczOzib8mzI_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_asXqPHp3O5g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("asXqPHp3O5g", libSceCompositeUnknown_asXqPHp3O5g_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_bwXAoMVv94M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("bwXAoMVv94M", libSceCompositeUnknown_bwXAoMVv94M_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_dTPMTtYqPX4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dTPMTtYqPX4", libSceCompositeUnknown_dTPMTtYqPX4_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_gLAJaS1VbsA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gLAJaS1VbsA", libSceCompositeUnknown_gLAJaS1VbsA_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_inplO6BVf5k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("inplO6BVf5k", libSceCompositeUnknown_inplO6BVf5k_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_j2CEWhcK0gc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("j2CEWhcK0gc", libSceCompositeUnknown_j2CEWhcK0gc_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_lvaCIMhYNh0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lvaCIMhYNh0", libSceCompositeUnknown_lvaCIMhYNh0_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_mzPjqJ_plus_unSw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mzPjqJ+unSw", libSceCompositeUnknown_mzPjqJ_plus_unSw_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_va_plus_6TiqnE1Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("va+6TiqnE1Y", libSceCompositeUnknown_va_plus_6TiqnE1Y_nid_no_patch);

int APS5_VABI libSceCompositeUnknown_yZcGEBwnDAM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yZcGEBwnDAM", libSceCompositeUnknown_yZcGEBwnDAM_nid_no_patch);

}
