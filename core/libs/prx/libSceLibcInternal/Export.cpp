#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" const char* __progname_nid_postfix;

namespace {

struct ThreadDestructor {
    ThreadDestructorFunction function;
    void* object;
    void* dsoSymbol;
};

struct ThreadDestructorsTag {};

std::vector<ThreadDestructor>& ThreadDestructors() {
    return HostThreadLocal<std::vector<ThreadDestructor>, ThreadDestructorsTag>();
}

bool IsInLoadedImage(const void* address) {
    if (address == nullptr)
        return false;
#ifdef _WIN32
    HMODULE module = nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(address), &module) != 0;
#else
    Dl_info info{};
    return dladdr(address, &info) != 0;
#endif
}

void CallThreadDestructor(const ThreadDestructor& destructor) {
    const auto* function = reinterpret_cast<const void*>(destructor.function);
    if (!IsInLoadedImage(function)) {
        std::ostringstream message;
        message << "thread_local destructor " << function << " of dso " << destructor.dsoSymbol << " is not in a loaded image";
        throw std::runtime_error(message.str());
    }
    destructor.function(destructor.object);
}

void APS5_VABI RunThreadDestructors_nid_no_patch() {
    auto& destructors = ThreadDestructors();
    while (!destructors.empty()) {
        const ThreadDestructor destructor = destructors.back();
        destructors.pop_back();
        CallThreadDestructor(destructor);
    }
}

bool FindDsoModule(const void* dsoSymbol, KernelModule& handle) {
    ModuleInfoEx info{};
    info.st_size = sizeof(ModuleInfoEx);
    if (sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(dsoSymbol), 1, &info) != 0) {
        handle = 0;
        return false;
    }
    handle = info.id;
    return true;
}

bool ForceThreadDestructorPass(KernelModule handle) {
    auto& destructors = ThreadDestructors();
    bool found = false;
    for (std::size_t index = destructors.size(); index-- > 0;) {
        const ThreadDestructor destructor = destructors[index];
        KernelModule module = 0;
        const bool loaded = FindDsoModule(destructor.dsoSymbol, module);
        if (module != handle)
            continue;
        found = true;
        destructors.erase(destructors.begin() + static_cast<std::ptrdiff_t>(index));
        if (loaded && *static_cast<void* const*>(destructor.dsoSymbol) == destructor.dsoSymbol)
            CallThreadDestructor(destructor);
    }
    return found;
}

void RegisterThreadExitHook() {
    [[maybe_unused]] static const bool registered = [] {
        sceKernelSetThreadDtors(RunThreadDestructors_nid_no_patch);

        return true;
    }();
}

}

extern "C" {

int APS5_VABI sceLibcMspaceSetMallocCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcPafMspaceReportMemoryBlocks() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcHeapSetAddressRangeCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcPafMspaceTrim() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcHeapMutexCalloc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcInternalSetMallocCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcHeapSetTraceMarker() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcPafMspaceCheckMemoryBounds() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcHeapUnsetTraceMarker() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcHeapGetAddressRanges() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcHeapMutexFree() {
    NotImplemented_nid_no_patch(__func__);
}

int Need_sceLibcInternal_nid_postfix = 1;

const char* APS5_VABI getprogname_nid_postfix() {
    return __progname_nid_postfix;
}

const char* APS5_VABI _getprogname_nid_postfix() {
    return getprogname_nid_postfix();
}

void APS5_VABI __cxa_finalize_nid_postfix(void* dsoHandle) {
    CxaFinalize_nid_no_patch(dsoHandle);
}

void APS5_VABI sceLibcHeapGetTraceInfo_nid_postfix(Info* info) {
    LibcHeapTraceInfo_nid_no_patch(info);
}

int APS5_VABI _sceLibcInternalThreadAtexit_nid_postfix(ThreadDestructorFunction destructor, void* object, void* dsoSymbol) {
    RegisterThreadExitHook();
    ThreadDestructors().push_back({destructor, object, dsoSymbol});
    return 0;
}

void APS5_VABI _sceLibcInternalThreadDtors_nid_postfix() {
    RunThreadDestructors_nid_no_patch();
}

int APS5_VABI _sceLibcInternalForceTlsDestructor_nid_postfix(KernelModule handle) {
    for (int pass = 0; pass < 4; ++pass) {
        if (!ForceThreadDestructorPass(handle))
            break;
    }
    return 0;
}

unsigned int APS5_VABI GuestAlarm_nid_no_patch(unsigned int seconds);

unsigned int APS5_VABI alarm_nid_postfix(unsigned int seconds) {
    return GuestAlarm_nid_no_patch(seconds);
}


int APS5_VABI _Atan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_M6nCy6H8Hs4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("M6nCy6H8Hs4", sceLibcInternalUnknown_M6nCy6H8Hs4_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_IHiK3lL7CvI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("IHiK3lL7CvI", sceLibcInternalUnknown_IHiK3lL7CvI_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_ykNF6P3ZsdA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ykNF6P3ZsdA", sceLibcInternalUnknown_ykNF6P3ZsdA_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_we_vQBAugV8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("we-vQBAugV8", sceLibcInternalUnknown_we_vQBAugV8_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_6aEXAPYpaEA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("6aEXAPYpaEA", sceLibcInternalUnknown_6aEXAPYpaEA_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_G1kDk_5L6dU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("G1kDk+5L6dU", sceLibcInternalUnknown_G1kDk_5L6dU_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_mgNGxmJltOY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("mgNGxmJltOY", sceLibcInternalUnknown_mgNGxmJltOY_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_ZlsoRa7pcuI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ZlsoRa7pcuI", sceLibcInternalUnknown_ZlsoRa7pcuI_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_H0FQnSWp1es_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("H0FQnSWp1es", sceLibcInternalUnknown_H0FQnSWp1es_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_4QwxZ3U0OK0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("4QwxZ3U0OK0", sceLibcInternalUnknown_4QwxZ3U0OK0_nid_no_patch);

int APS5_VABI _Dtento_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_aQURHgjHo_w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("aQURHgjHo-w", sceLibcInternalUnknown_aQURHgjHo_w_nid_no_patch);

int APS5_VABI _FAtan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FDtento_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_qwR1gtp_WS0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("qwR1gtp-WS0", sceLibcInternalUnknown_qwR1gtp_WS0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_DS03EjPDtFo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("DS03EjPDtFo", sceLibcInternalUnknown_DS03EjPDtFo_nid_no_patch);

int APS5_VABI _FLogpoly_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_cfpRP3h9F6o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("cfpRP3h9F6o", sceLibcInternalUnknown_cfpRP3h9F6o_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_IdWhZ0SM7JA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("IdWhZ0SM7JA", sceLibcInternalUnknown_IdWhZ0SM7JA_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_A98W3Iad6xE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("A98W3Iad6xE", sceLibcInternalUnknown_A98W3Iad6xE_nid_no_patch);

int APS5_VABI _FQuad_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FQuadph_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_TLvAYmLtdVw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("TLvAYmLtdVw", sceLibcInternalUnknown_TLvAYmLtdVw_nid_no_patch);

int APS5_VABI _FSincos_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FTan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FTgamma_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_addh_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_addx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_getw_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_invx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_ldexpx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_movx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_mulh_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_mulx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_setn_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_setw_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_sqrtx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FXp_subx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_chzmnjxxVtk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("chzmnjxxVtk", sceLibcInternalUnknown_chzmnjxxVtk_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_D_fkILS7EK0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("D+fkILS7EK0", sceLibcInternalUnknown_D_fkILS7EK0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_dYJJbxnyb74_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("dYJJbxnyb74", sceLibcInternalUnknown_dYJJbxnyb74_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_xGT4Mc55ViQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("xGT4Mc55ViQ", sceLibcInternalUnknown_xGT4Mc55ViQ_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_jVDuvE3s5Bs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("jVDuvE3s5Bs", sceLibcInternalUnknown_jVDuvE3s5Bs_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_sQL8D_jio7U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("sQL8D-jio7U", sceLibcInternalUnknown_sQL8D_jio7U_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_dREVnZkAKRE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("dREVnZkAKRE", sceLibcInternalUnknown_dREVnZkAKRE_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_9s3P_LCvWP8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("9s3P+LCvWP8", sceLibcInternalUnknown_9s3P_LCvWP8_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_A_Y3xfrWLLo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("A+Y3xfrWLLo", sceLibcInternalUnknown_A_Y3xfrWLLo_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_4F9pQjbh8R8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("4F9pQjbh8R8", sceLibcInternalUnknown_4F9pQjbh8R8_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_vakoyx9nkqo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("vakoyx9nkqo", sceLibcInternalUnknown_vakoyx9nkqo_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_bRN9BzEkm4o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("bRN9BzEkm4o", sceLibcInternalUnknown_bRN9BzEkm4o_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_2MfMa3456FI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("2MfMa3456FI", sceLibcInternalUnknown_2MfMa3456FI_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_i1N28hWcD_4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("i1N28hWcD-4", sceLibcInternalUnknown_i1N28hWcD_4_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_Jcu0Wl1_XbE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Jcu0Wl1-XbE", sceLibcInternalUnknown_Jcu0Wl1_XbE_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_M1xC101lsIU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("M1xC101lsIU", sceLibcInternalUnknown_M1xC101lsIU_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_bItEABINEm0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("bItEABINEm0", sceLibcInternalUnknown_bItEABINEm0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_7iFNNuNyXxw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("7iFNNuNyXxw", sceLibcInternalUnknown_7iFNNuNyXxw_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_8Jr4cvRM6EM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("8Jr4cvRM6EM", sceLibcInternalUnknown_8Jr4cvRM6EM_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_PWmDp8ZTS9k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("PWmDp8ZTS9k", sceLibcInternalUnknown_PWmDp8ZTS9k_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_bF4eWOM5ouo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("bF4eWOM5ouo", sceLibcInternalUnknown_bF4eWOM5ouo_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_hzsdjKbFD7g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("hzsdjKbFD7g", sceLibcInternalUnknown_hzsdjKbFD7g_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_RLdcWoBjmT4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("RLdcWoBjmT4", sceLibcInternalUnknown_RLdcWoBjmT4_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_uF8hDs1CqWI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("uF8hDs1CqWI", sceLibcInternalUnknown_uF8hDs1CqWI_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_i2yN6xBwooo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("i2yN6xBwooo", sceLibcInternalUnknown_i2yN6xBwooo_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_g8ozp2Zrsj8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("g8ozp2Zrsj8", sceLibcInternalUnknown_g8ozp2Zrsj8_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_Wz9CvcF5jn4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Wz9CvcF5jn4", sceLibcInternalUnknown_Wz9CvcF5jn4_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_wUa_oPQvFZ0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("wUa+oPQvFZ0", sceLibcInternalUnknown_wUa_oPQvFZ0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_Sb26PiOiFtE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Sb26PiOiFtE", sceLibcInternalUnknown_Sb26PiOiFtE_nid_no_patch);

int APS5_VABI _LAtan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LDtento_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_PG4HVe4X_Oc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("PG4HVe4X+Oc", sceLibcInternalUnknown_PG4HVe4X_Oc_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_IRUFZywbTT0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("IRUFZywbTT0", sceLibcInternalUnknown_IRUFZywbTT0_nid_no_patch);

int APS5_VABI _LLogpoly_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_W_T_amhWrkA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("W-T-amhWrkA", sceLibcInternalUnknown_W_T_amhWrkA_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_gso5X06CFkI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("gso5X06CFkI", sceLibcInternalUnknown_gso5X06CFkI_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_3Z8jD_HTKr8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("3Z8jD-HTKr8", sceLibcInternalUnknown_3Z8jD_HTKr8_nid_no_patch);

int APS5_VABI _LQuad_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LQuadph_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_fJmOr59K8QY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("fJmOr59K8QY", sceLibcInternalUnknown_fJmOr59K8QY_nid_no_patch);

int APS5_VABI _LSincos_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LTan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LTgamma_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_addh_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_addx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_getw_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_invx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_ldexpx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_movx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_mulh_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_mulx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_setn_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_setw_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_sqrtx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LXp_subx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_3BbBNPjfkYI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("3BbBNPjfkYI", sceLibcInternalUnknown_3BbBNPjfkYI_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown__GxvfSLhoAo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("+GxvfSLhoAo", sceLibcInternalUnknown__GxvfSLhoAo_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_Cv_8x__GS9A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Cv-8x++GS9A", sceLibcInternalUnknown_Cv_8x__GS9A_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_sYz_SxY8H6M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("sYz-SxY8H6M", sceLibcInternalUnknown_sYz_SxY8H6M_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_4ifo9QGrO5w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("4ifo9QGrO5w", sceLibcInternalUnknown_4ifo9QGrO5w_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_ElU3kEY8q_I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ElU3kEY8q+I", sceLibcInternalUnknown_ElU3kEY8q_I_nid_no_patch);

int APS5_VABI _Logpoly_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_tTDqwhYbUUo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("tTDqwhYbUUo", sceLibcInternalUnknown_tTDqwhYbUUo_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_AnTtuRUE1cE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("AnTtuRUE1cE", sceLibcInternalUnknown_AnTtuRUE1cE_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_QalEczzSNqc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("QalEczzSNqc", sceLibcInternalUnknown_QalEczzSNqc_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_sij3OtJpHFc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("sij3OtJpHFc", sceLibcInternalUnknown_sij3OtJpHFc_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_LaPaA6mYA38_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("LaPaA6mYA38", sceLibcInternalUnknown_LaPaA6mYA38_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_z7STeF6abuU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("z7STeF6abuU", sceLibcInternalUnknown_z7STeF6abuU_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_pE4Ot3CffW0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("pE4Ot3CffW0", sceLibcInternalUnknown_pE4Ot3CffW0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_cMwgSSmpE5o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("cMwgSSmpE5o", sceLibcInternalUnknown_cMwgSSmpE5o_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_Ss3108pBuZY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Ss3108pBuZY", sceLibcInternalUnknown_Ss3108pBuZY_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_1hGmBh83dL8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("1hGmBh83dL8", sceLibcInternalUnknown_1hGmBh83dL8_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_hrv1BgM68kU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("hrv1BgM68kU", sceLibcInternalUnknown_hrv1BgM68kU_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_ECh_p_LRG6Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ECh+p-LRG6Y", sceLibcInternalUnknown_ECh_p_LRG6Y_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_FModQzwn1_4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("FModQzwn1-4", sceLibcInternalUnknown_FModQzwn1_4_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_rnxaQ_2hSMI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("rnxaQ+2hSMI", sceLibcInternalUnknown_rnxaQ_2hSMI_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_Fot_m6M2oKE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Fot-m6M2oKE", sceLibcInternalUnknown_Fot_m6M2oKE_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_ijAqq39g4dI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ijAqq39g4dI", sceLibcInternalUnknown_ijAqq39g4dI_nid_no_patch);

int APS5_VABI _Quad_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Quadph_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_MxZ4Lc35Rig_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("MxZ4Lc35Rig", sceLibcInternalUnknown_MxZ4Lc35Rig_nid_no_patch);
}
