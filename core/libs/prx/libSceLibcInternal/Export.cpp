#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "SceTypes.hpp"
#include "prx/libSceLibcInternal/include/PendingExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" const char* __progname_nid_postfix;

extern "C" {

int APS5_VABI _ZNKSt8ios_base7failure8_DoraiseEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending4_005_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("EeBQ7253LkE", sceLibcInternalPending4_005_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_006_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("vbgCuYKySLI", sceLibcInternalPending4_006_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_007_nid_no_patch(const void* self, const void* name, const void* locale) {
    (void)self;
    (void)name;
    (void)locale;
    return -1;
}
APS5_EXPORT("HeBwePMtuFs", sceLibcInternalPending4_007_nid_no_patch);

void APS5_VABI sceLibcInternalPending4_008_nid_no_patch(const void* self, int catalog) {
    (void)self;
    (void)catalog;
}
APS5_EXPORT("rRmMX83UL1E", sceLibcInternalPending4_008_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_009_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Ea+awuQ5Bm8", sceLibcInternalPending4_009_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_010_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("TPq0HfoACeI", sceLibcInternalPending4_010_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_011_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("GGoH7e6SZSY", sceLibcInternalPending4_011_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_012_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("UM6rGQxnEMg", sceLibcInternalPending4_012_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_013_nid_no_patch(const void* self, const void* name, const void* locale) {
    (void)self;
    (void)name;
    (void)locale;
    return -1;
}
APS5_EXPORT("zSehLdHI0FA", sceLibcInternalPending4_013_nid_no_patch);

void APS5_VABI sceLibcInternalPending4_014_nid_no_patch(const void* self, int catalog) {
    (void)self;
    (void)catalog;
}
APS5_EXPORT("AjkxQBlsOOY", sceLibcInternalPending4_014_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_036_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("T85u2sPrKOo", sceLibcInternalPending4_036_nid_no_patch);

int APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending4_040_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("8PIh8BFpNYQ", sceLibcInternalPending4_040_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_042_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("xzYpD5d24aA", sceLibcInternalPending4_042_nid_no_patch);

int APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending4_045_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Vw03kdKZUN0", sceLibcInternalPending4_045_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_046_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("E7UermPZVcw", sceLibcInternalPending4_046_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_047_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("8raXTYQ11cg", sceLibcInternalPending4_047_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_052_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("SYCwZXKZQ08", sceLibcInternalPending4_052_nid_no_patch);

int APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending4_056_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("dP14OHWe4nI", sceLibcInternalPending4_056_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_058_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("hGlkh5YpcKw", sceLibcInternalPending4_058_nid_no_patch);

int APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending4_062_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("0MzJAexrlr4", sceLibcInternalPending4_062_nid_no_patch);

int APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending4_070_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("+i81FtUCarA", sceLibcInternalPending4_070_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_071_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Sc0lXhQG5Ko", sceLibcInternalPending4_071_nid_no_patch);

int APS5_VABI _ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPK2tmcc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewPK2tmcc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSs_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetmfldERS3_S5_bRSt8ios_basePc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


int APS5_VABI _ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSbIwS2_SaIwEE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetmfldERS3_S5_bRSt8ios_basePw_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


int APS5_VABI _ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_bRSt8ios_basecRKSs_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_bRSt8ios_basece_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE8_PutmfldES3_bRSt8ios_basecbSsc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


int APS5_VABI _ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_bRSt8ios_basewRKSbIwS2_SaIwEE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_bRSt8ios_basewe_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE8_PutmfldES3_bRSt8ios_basewbSbIwS2_SaIwEEw_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE5_CopyEmm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending4_103_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("6ZDv6ZusiFg", sceLibcInternalPending4_103_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_104_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("tJU-ttrsXsk", sceLibcInternalPending4_104_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_105_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("gVTWlvyBSIc", sceLibcInternalPending4_105_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_106_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("nk+0yTWvoRE", sceLibcInternalPending4_106_nid_no_patch);

int APS5_VABI _ZNSoD0Ev_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending4_108_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("HpCeP12cuNY", sceLibcInternalPending4_108_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_109_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("9HILqEoh24E", sceLibcInternalPending4_109_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_119_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("XD9FmX1mavU", sceLibcInternalPending4_119_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_120_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Yl10kSufa5k", sceLibcInternalPending4_120_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_123_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Wut42WAe7Rw", sceLibcInternalPending4_123_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_130_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("mU88GYCirhI", sceLibcInternalPending4_130_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_131_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("5afBJmEfUQI", sceLibcInternalPending4_131_nid_no_patch);

int APS5_VABI _ZNSt10moneypunctIcLb0EEC1ERKSt8_Locinfomb_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt10moneypunctIcLb0EEC1Em_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt10moneypunctIcLb0EEC2EPKcm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt10moneypunctIcLb0EEC2ERKSt8_Locinfomb_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt10moneypunctIcLb0EEC2Em_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt10moneypunctIcLb0EED0Ev_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending4_138_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("3cW6MrkCKt0", sceLibcInternalPending4_138_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_139_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("mbGmSOLXgN0", sceLibcInternalPending4_139_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_140_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("BuxsERsopss", sceLibcInternalPending4_140_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_141_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("9S960jA8tB0", sceLibcInternalPending4_141_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_142_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("TRn3cMU4mjY", sceLibcInternalPending4_142_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_143_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("nbTAoMwiO38", sceLibcInternalPending4_143_nid_no_patch);

int APS5_VABI _ZNSt10moneypunctIcLb1EEC2EPKcm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt10moneypunctIcLb1EEC2ERKSt8_Locinfomb_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt10moneypunctIcLb1EEC2Em_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt10moneypunctIcLb1EED0Ev_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt10moneypunctIcLb1EED1Ev_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt10moneypunctIcLb1EED2Ev_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending4_150_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("0OjBJZd9VeM", sceLibcInternalPending4_150_nid_no_patch);

}

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

void APS5_VABI setprogname_nid_postfix(const char* name) {
    if (name == nullptr) throw std::invalid_argument("setprogname: null name");
    const char* slash = std::strrchr(name, '/');
    __progname_nid_postfix = (slash != nullptr) ? slash + 1 : name;
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

int APS5_VABI _Closreg_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Daysto_nid_postfix(int year, int month) {
    static constexpr short DaysBeforeMonth[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    static constexpr short LeapDaysBeforeMonth[12] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
    if (month < 0 || month >= 12) throw std::out_of_range("_Daysto: the month index is outside the twelve-entry tables");
    const auto bits = static_cast<std::uint32_t>(year);
    std::uint32_t leapDays = 0;
    if (year > 0) leapDays = (bits - 1u) >> 2;
    else if (year <= -4) leapDays = ((4u - bits) >> 2) + 1u;
    const bool common = (bits & 3u) != 0 || year == 0;
    return static_cast<int>(leapDays + static_cast<std::uint32_t>(common ? DaysBeforeMonth[month] : LeapDaysBeforeMonth[month]));
}

int APS5_VABI _Deletegloballocale_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Dtento_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Erfc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FAtan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FDtento_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FErfc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FHypot_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

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

int APS5_VABI _FPow_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FQuad_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FQuadph_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FRint_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

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

float APS5_VABI _FXp_getw_nid_postfix(const float* parts, int count) {
    if (count == 0) return 0.0f;
    if (count == 1 || parts[0] == 0.0f || parts[1] == 0.0f) return parts[0];
    if (count == 2 || parts[2] == 0.0f) return parts[0] + parts[1];
    const float head = parts[0] + parts[1];
    float tail = parts[2];
    if (count >= 4) tail += parts[3];
    if (head - parts[0] == parts[1]) return head + tail;
    return parts[0] + (parts[1] + tail);
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

int APS5_VABI _Hypot_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Isdst_nid_postfix() {
    return -1;
}

int APS5_VABI _LAtan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LDtento_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LErfc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LHypot_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

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

int APS5_VABI _LPow_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LQuad_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LQuadph_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LRint_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

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

int APS5_VABI _Logpoly_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

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

int APS5_VABI _Nnl_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

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

int APS5_VABI _Pow_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

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

int APS5_VABI _Quad_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Quadph_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Readloc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
