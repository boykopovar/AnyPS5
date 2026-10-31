#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
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


int APS5_VABI sceLibcInternalUnknown_FHErahnajkw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("FHErahnajkw", sceLibcInternalUnknown_FHErahnajkw_nid_no_patch);

int APS5_VABI _Atqexit_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Atthreadexit_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _CStrftime_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _CStrxfrm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _CWcsxfrm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Call_onceEx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Closreg_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Daysto_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Deletegloballocale_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Do_call_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_vCQLavj_3CM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("vCQLavj-3CM", sceLibcInternalUnknown_vCQLavj_3CM_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_aQURHgjHo_w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("aQURHgjHo-w", sceLibcInternalUnknown_aQURHgjHo_w_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_us3bDnDzd70_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("us3bDnDzd70", sceLibcInternalUnknown_us3bDnDzd70_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_SoNnx4Ejxw8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("SoNnx4Ejxw8", sceLibcInternalUnknown_SoNnx4Ejxw8_nid_no_patch);

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

int APS5_VABI sceLibcInternalUnknown_OAE3YU396YQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("OAE3YU396YQ", sceLibcInternalUnknown_OAE3YU396YQ_nid_no_patch);

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

int APS5_VABI sceLibcInternalUnknown_y9Ur6T0A0p8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("y9Ur6T0A0p8", sceLibcInternalUnknown_y9Ur6T0A0p8_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_PDQbEFcV4h0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("PDQbEFcV4h0", sceLibcInternalUnknown_PDQbEFcV4h0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_TLvAYmLtdVw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("TLvAYmLtdVw", sceLibcInternalUnknown_TLvAYmLtdVw_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_fQ_SWrQUQBg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("fQ+SWrQUQBg", sceLibcInternalUnknown_fQ_SWrQUQBg_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_iBrTJkDlQv8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("iBrTJkDlQv8", sceLibcInternalUnknown_iBrTJkDlQv8_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_odPHnVL_rFg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("odPHnVL-rFg", sceLibcInternalUnknown_odPHnVL_rFg_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_1EyHxzcz6AM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("1EyHxzcz6AM", sceLibcInternalUnknown_1EyHxzcz6AM_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_1b_IhPTX0nk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("1b+IhPTX0nk", sceLibcInternalUnknown_1b_IhPTX0nk_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_e1y7KVAySrY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("e1y7KVAySrY", sceLibcInternalUnknown_e1y7KVAySrY_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_OVqW4uElSrc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("OVqW4uElSrc", sceLibcInternalUnknown_OVqW4uElSrc_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_7GgGIxmwA6I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("7GgGIxmwA6I", sceLibcInternalUnknown_7GgGIxmwA6I_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_DVZmEd0ipSg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("DVZmEd0ipSg", sceLibcInternalUnknown_DVZmEd0ipSg_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_W_lrIwAQVUk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("W+lrIwAQVUk", sceLibcInternalUnknown_W_lrIwAQVUk_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_o1B4dkvesMc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("o1B4dkvesMc", sceLibcInternalUnknown_o1B4dkvesMc_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_ikHTMeh60B0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ikHTMeh60B0", sceLibcInternalUnknown_ikHTMeh60B0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_5zWUVRtR8xg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("5zWUVRtR8xg", sceLibcInternalUnknown_5zWUVRtR8xg_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_pNWIpeE5Wv4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("pNWIpeE5Wv4", sceLibcInternalUnknown_pNWIpeE5Wv4_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_HD9vSXqj6zI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("HD9vSXqj6zI", sceLibcInternalUnknown_HD9vSXqj6zI_nid_no_patch);

int APS5_VABI _Fac_tidy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Fail_s_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Fgpos_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Fofind_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Fofree_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Fopen_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Foprep_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Frprep_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Fspos_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Fwprep_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Genld_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Gentime_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getcloc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getctyptab_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getdst_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Geterrno_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getfld_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getfloat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getgloballocale_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getint_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getpcostate_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getpwcostate_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getpwctrtab_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getpwctytab_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getstr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Gettime_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getzone_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_wUa_oPQvFZ0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("wUa+oPQvFZ0", sceLibcInternalUnknown_wUa_oPQvFZ0_nid_no_patch);

int APS5_VABI _Isdst_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_2Aw366Jn07s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("2Aw366Jn07s", sceLibcInternalUnknown_2Aw366Jn07s_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_PEU_SAfo5_0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("PEU-SAfo5+0", sceLibcInternalUnknown_PEU_SAfo5_0_nid_no_patch);

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

int APS5_VABI sceLibcInternalUnknown_niPixjs0l2w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("niPixjs0l2w", sceLibcInternalUnknown_niPixjs0l2w_nid_no_patch);

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

int APS5_VABI sceLibcInternalUnknown_fXCPTDS0tD0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("fXCPTDS0tD0", sceLibcInternalUnknown_fXCPTDS0tD0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_CbRhl_RoYEM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("CbRhl+RoYEM", sceLibcInternalUnknown_CbRhl_RoYEM_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_fJmOr59K8QY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("fJmOr59K8QY", sceLibcInternalUnknown_fJmOr59K8QY_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_g3dK2qlzwnM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("g3dK2qlzwnM", sceLibcInternalUnknown_g3dK2qlzwnM_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_mM4OblD9xWM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("mM4OblD9xWM", sceLibcInternalUnknown_mM4OblD9xWM_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_jq4Srfnz9K8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("jq4Srfnz9K8", sceLibcInternalUnknown_jq4Srfnz9K8_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_QEr_PxGUoic_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("QEr-PxGUoic", sceLibcInternalUnknown_QEr_PxGUoic_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_BuP7bDPGEcQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("BuP7bDPGEcQ", sceLibcInternalUnknown_BuP7bDPGEcQ_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_TnublTBYXTI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("TnublTBYXTI", sceLibcInternalUnknown_TnublTBYXTI_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_FAreWopdBvE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("FAreWopdBvE", sceLibcInternalUnknown_FAreWopdBvE_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_7O_vjsHecbY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("7O-vjsHecbY", sceLibcInternalUnknown_7O_vjsHecbY_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_wlEdSSqaz_E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("wlEdSSqaz+E", sceLibcInternalUnknown_wlEdSSqaz_E_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_riets0BFHMY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("riets0BFHMY", sceLibcInternalUnknown_riets0BFHMY_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_LbLiLA2biaI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("LbLiLA2biaI", sceLibcInternalUnknown_LbLiLA2biaI_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_hjDoZ6qbkmQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("hjDoZ6qbkmQ", sceLibcInternalUnknown_hjDoZ6qbkmQ_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_kHVXc_AWbMU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("kHVXc-AWbMU", sceLibcInternalUnknown_kHVXc_AWbMU_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_IPjwywTNR8w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("IPjwywTNR8w", sceLibcInternalUnknown_IPjwywTNR8w_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_YCVxOE0lHOI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("YCVxOE0lHOI", sceLibcInternalUnknown_YCVxOE0lHOI_nid_no_patch);

int APS5_VABI _Ldtob_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Litob_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Lock_spin_lock_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Locsum_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Locterm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Locvar_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_bqcStLRGIXw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("bqcStLRGIXw", sceLibcInternalUnknown_bqcStLRGIXw_nid_no_patch);

int APS5_VABI _Makeloc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Makestab_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Makewct_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Mbtowc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

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

int APS5_VABI _Printf_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Putfld_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Putstr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Puttxt_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_TQPr_IeNIS0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("TQPr-IeNIS0", sceLibcInternalUnknown_TQPr_IeNIS0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_VecRKuKdXdo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("VecRKuKdXdo", sceLibcInternalUnknown_VecRKuKdXdo_nid_no_patch);

int APS5_VABI _Readloc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
