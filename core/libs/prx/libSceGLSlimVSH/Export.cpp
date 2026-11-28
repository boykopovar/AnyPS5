#include "prx/libSceGLSlimVSH/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceGLSlimVSH/include/Api.hpp"

extern "C" {

EGLSurface APS5_VABI eglCreatePbufferFromClientBuffer_nid_postfix(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list) {
    (void)dpy;
    (void)buftype;
    (void)buffer;
    (void)config;
    (void)attrib_list;
    NotImplemented_nid_no_patch(__func__);
    return {};
}



void APS5_VABI glDrawElements_nid_postfix(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    (void)mode;
    (void)count;
    (void)type;
    (void)indices;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI glOrbisMapTextureResourceSCE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI glOrbisTexImageCanvas2DSCE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI glOrbisTexImageResourceSCE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI glOrbisUnmapTextureResourceSCE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePigletAllocateSystemMemoryEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePigletAllocateVideoMemoryEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePigletGetConfigurationVSH() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePigletSetConfigurationVSH() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePigletReleaseSystemMemoryEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePigletReleaseVideoMemoryEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglClientStartIPC() {
    return 0;
}

int APS5_VABI sceSlimglCompositorAllocateIndex() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorCreateIndirectRenderTarget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorDeleteIndirectRenderTarget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorFlush() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorFlushWithRepeat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorGetCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorGetError() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorReleaseIndex() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorSetEndOfFrameCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorSetIndirectCanvasCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorSetIndirectCompositionCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSlimglCompositorSetIndirectRenderTargetConfigCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI sceSlimglCompositorSetInvisibleCanvasCommand() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglCompositorSetMemoryCommand() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglCompositorSetPostEventCommand() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglCompositorWaitEndOfRendering() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglCompositorWaitPostEvent() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglIPCEnd() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglIPCSetup() {
    return 0;
}

}

extern "C" {

int APS5_VABI libSceGLSlimVSHInventory_9iW854bwvmc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9iW854bwvmc", libSceGLSlimVSHInventory_9iW854bwvmc_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_DVNKmWX1jOU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DVNKmWX1jOU", libSceGLSlimVSHInventory_DVNKmWX1jOU_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_JjyNMbSAgLk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("JjyNMbSAgLk", libSceGLSlimVSHInventory_JjyNMbSAgLk_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_LLDLZ_minus_mQK_plus_Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LLDLZ-mQK+Q", libSceGLSlimVSHInventory_LLDLZ_minus_mQK_plus_Q_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_Yf0LATiXPsg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Yf0LATiXPsg", libSceGLSlimVSHInventory_Yf0LATiXPsg_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_ZLvm5korFRA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZLvm5korFRA", libSceGLSlimVSHInventory_ZLvm5korFRA_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_qSuVNKHENqs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qSuVNKHENqs", libSceGLSlimVSHInventory_qSuVNKHENqs_nid_no_patch);

}

extern "C" void APS5_VABI glDrawArrays_nid_postfix(GLenum mode, GLint first, GLsizei count) {
    NotImplemented_nid_no_patch(__func__);
}

extern "C" const unsigned char* APS5_VABI glGetString_nid_postfix(GLenum name) {
    NotImplemented_nid_no_patch(__func__);
}

extern "C" EGLSurface APS5_VABI eglCreateWindowSurface_nid_postfix(EGLDisplay display, EGLConfig config, void* window, const EGLint* attributes) {
    NotImplemented_nid_no_patch(__func__);
}
