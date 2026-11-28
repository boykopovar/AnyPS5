#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/GuestWords.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "SceTypes.hpp"
#include "prx/libSceLibcInternal/include/PendingExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" const char* __progname_nid_postfix;

namespace {

template<typename T, typename Evaluate>
T ErfcTail(T x, Evaluate evaluate, const char* caller) {
    constexpr auto tailStart = static_cast<T>(1.523L);
    if (!std::isfinite(x) || x < tailStart) NotImplemented_nid_no_patch(caller);
    return evaluate(x);
}

template<typename Slot>
Slot MessagesSlot(const void* self, Slot GuestLocale::MessagesVtable::*member, const char* function) {
    const auto* facet = static_cast<const GuestLocale::Facet*>(self);
    if (facet == nullptr || facet->vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null facet");
    const Slot slot = reinterpret_cast<const GuestLocale::MessagesVtable*>(facet->vtable)->*member;
    if (slot == nullptr) throw std::invalid_argument(std::string(function) + ": empty vtable slot");
    return slot;
}

}

extern "C" {

void* APS5_VABI _ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix(void* result, const void* self, int catalog, int set, int message, const void* fallback) {
    return MessagesSlot(self, &GuestLocale::MessagesVtable::get, "messages<char>::get")(static_cast<GuestLocale::String*>(result), static_cast<const GuestLocale::Facet*>(self), catalog, set, message, static_cast<const GuestLocale::String*>(fallback));
}

int APS5_VABI _ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix(const void* self, const void* name, const void* locale) {
    return MessagesSlot(self, &GuestLocale::MessagesVtable::open, "messages<char>::open")(static_cast<const GuestLocale::Facet*>(self), static_cast<const GuestLocale::String*>(name), locale);
}

void APS5_VABI sceLibcInternalPending4_005_nid_no_patch(const void* self, int catalog) {
    MessagesSlot(self, &GuestLocale::MessagesVtable::close, "messages<char>::close")(static_cast<const GuestLocale::Facet*>(self), catalog);
}
APS5_EXPORT("EeBQ7253LkE", sceLibcInternalPending4_005_nid_no_patch);

void* APS5_VABI sceLibcInternalPending4_006_nid_no_patch(void* result, const void* self, int catalog, int set, int message, const void* fallback) {
    (void)self;
    (void)catalog;
    (void)set;
    (void)message;
    return LibcCopyString_nid_no_patch(static_cast<GuestLocale::String*>(result), static_cast<const GuestLocale::String*>(fallback));
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

void* APS5_VABI sceLibcInternalPending4_009_nid_no_patch(void* result, const void* self, int catalog, int set, int message, const void* fallback) {
    return MessagesSlot(self, &GuestLocale::MessagesVtable::get, "messages<wchar_t>::get")(static_cast<GuestLocale::String*>(result), static_cast<const GuestLocale::Facet*>(self), catalog, set, message, static_cast<const GuestLocale::String*>(fallback));
}
APS5_EXPORT("Ea+awuQ5Bm8", sceLibcInternalPending4_009_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_010_nid_no_patch(const void* self, const void* name, const void* locale) {
    return MessagesSlot(self, &GuestLocale::MessagesVtable::open, "messages<wchar_t>::open")(static_cast<const GuestLocale::Facet*>(self), static_cast<const GuestLocale::String*>(name), locale);
}
APS5_EXPORT("TPq0HfoACeI", sceLibcInternalPending4_010_nid_no_patch);

void APS5_VABI sceLibcInternalPending4_011_nid_no_patch(const void* self, int catalog) {
    MessagesSlot(self, &GuestLocale::MessagesVtable::close, "messages<wchar_t>::close")(static_cast<const GuestLocale::Facet*>(self), catalog);
}
APS5_EXPORT("GGoH7e6SZSY", sceLibcInternalPending4_011_nid_no_patch);

void* APS5_VABI sceLibcInternalPending4_012_nid_no_patch(void* result, const void* self, int catalog, int set, int message, const void* fallback) {
    (void)self;
    (void)catalog;
    (void)set;
    (void)message;
    return LibcCopyWideString_nid_no_patch(static_cast<GuestLocale::String*>(result), static_cast<const GuestLocale::String*>(fallback));
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

GuestLocale::OutputIterator APS5_VABI sceLibcInternalTimePutDoPut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const GuestTm* time, char format, char modifier) {
    return LibcTimePutDoPut_nid_no_patch(self, out, stream, fill, time, format, modifier);
}
APS5_EXPORT("+i81FtUCarA", sceLibcInternalTimePutDoPut_nid_no_patch);

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

[[noreturn]] void APS5_VABI sceLibcInternalPending4_123_nid_no_patch(const char* message, const GuestLocale::String* path1,
    const GuestLocale::String* path2, LibcException::ErrorCode code) {
    FilesystemErrorTwoPaths_nid_no_patch(message, path1, path2, code);
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

void APS5_VABI _ZNSt10moneypunctIcLb0EED0Ev_nid_postfix(void* self) {
    _ZdlPv_nid_postfix(self);
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

void APS5_VABI _ZNSt10moneypunctIcLb1EED0Ev_nid_postfix(void* self) {
    _ZdlPv_nid_postfix(self);
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

struct ThreadExitCallbacksTag {};

std::vector<GuestExitCallback>& ThreadExitCallbacks() {
    return HostThreadLocal<std::vector<GuestExitCallback>, ThreadExitCallbacksTag>();
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
    auto& callbacks = ThreadExitCallbacks();
    while (!destructors.empty() || !callbacks.empty()) {
        while (!destructors.empty()) {
            const ThreadDestructor destructor = destructors.back();
            destructors.pop_back();
            CallThreadDestructor(destructor);
        }
        if (!callbacks.empty()) {
            const auto callback = callbacks.back();
            callbacks.pop_back();
            callback();
        }
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

struct GentimeTm {
    std::int32_t sec;
    std::int32_t min;
    std::int32_t hour;
    std::int32_t mday;
    std::int32_t mon;
    std::int32_t year;
    std::int32_t wday;
    std::int32_t yday;
    std::int32_t isdst;
};

struct GentimeText {
    const char* text;
    int length;
};

GentimeText GentimeField(const char* guest, int index) {
    std::string copy;
    if (CopyGuestString_nid_no_patch(guest, 0x10000, copy) != 0) throw std::invalid_argument("_Gentime: an unreadable time information string");
    if (copy.empty()) return {guest, 0};
    const char separator = copy[0];
    std::size_t start = 1;
    for (; index > 0; --index) {
        const std::size_t next = copy.find(separator, start);
        if (next == std::string::npos) return {guest + copy.size(), 0};
        start = next + 1;
    }
    const std::size_t end = std::min(copy.find(separator, start), copy.size());
    return {guest + start, static_cast<int>(end - start)};
}

int GentimeDigits(char* text, int value, int digits) {
    if (value < 0) value = 0;
    for (int index = digits - 1; index >= 0; --index, value /= 10) text[index] = static_cast<char>('0' + value % 10);
    return digits;
}

bool GentimeLongIsoYear(int year) {
    const auto weekdayOfDecember31 = [](int y) { return (y + y / 4 - y / 100 + y / 400) % 7; };
    return weekdayOfDecember31(year) == 4 || weekdayOfDecember31(year - 1) == 3;
}

void GentimeIsoWeek(const GentimeTm& time, int* week, int* isoYear) {
    const int weekday = time.wday == 0 ? 7 : time.wday;
    int year = time.year + 1900;
    int number = (time.yday + 1 - weekday + 10) / 7;
    if (number < 1) {
        --year;
        number = GentimeLongIsoYear(year) ? 53 : 52;
    } else if (number == 53 && !GentimeLongIsoYear(year)) {
        ++year;
        number = 1;
    }
    *week = number;
    *isoYear = year;
}

std::int64_t GentimeSeconds(const GentimeTm& time) {
    std::int64_t year = static_cast<std::int64_t>(time.year) + 1900 + time.mon / 12;
    std::int64_t month = time.mon % 12;
    if (month < 0) {
        month += 12;
        --year;
    }
    year -= month < 2;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t yearOfEra = year - era * 400;
    const std::int64_t shifted = (month + 10) % 12;
    const std::int64_t dayOfYear = (153 * shifted + 2) / 5;
    const std::int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    const std::int64_t days = era * 146097 + dayOfEra - 719468 + time.mday - 1;
    return days * 86400 + static_cast<std::int64_t>(time.hour) * 3600 + static_cast<std::int64_t>(time.min) * 60 + time.sec;
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
    std::string copied;
    if (CopyGuestString_nid_no_patch(name, 65536, copied) != 0) throw std::invalid_argument("setprogname: unreadable name");
    const std::size_t slash = copied.rfind('/');
    __progname_nid_postfix = slash != std::string::npos ? name + slash + 1 : name;
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


int APS5_VABI _Atqexit_nid_postfix(void (APS5_VABI *func)()) {
    if (func == nullptr)
        return 0;
    return at_quick_exit_nid_postfix(func);
}

int APS5_VABI _Atthreadexit_nid_postfix(void (APS5_VABI *func)()) {
    if (func == nullptr)
        return 0;
    RegisterThreadExitHook();
    ThreadExitCallbacks().push_back(func);
    return 0;
}

size_t APS5_VABI _CStrftime_nid_postfix(char* destination, size_t count, const char* format, const GuestTm* time) {
    return strftime_nid_postfix(destination, count, format, time);
}

size_t APS5_VABI _CStrxfrm_nid_postfix(char* destination, const char* source, size_t count) {
    size_t result = 0;
    result = strxfrm_nid_postfix(destination, source, count);
    return result;
}

size_t APS5_VABI _CWcsxfrm_nid_postfix(char16_t* destination, const char16_t* source, size_t count) {
    size_t result = 0;
    result = wcsxfrm_nid_postfix(destination, source, count);
    return result;
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

double APS5_VABI _Erfc_nid_postfix(double x) {
    return ErfcTail(x, erfc_nid_postfix, __func__);
}

float APS5_VABI _FErfc_nid_postfix(float x) {
    return ErfcTail(x, erfcf_nid_postfix, __func__);
}

int APS5_VABI _FQuad_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _FQuadph_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

float APS5_VABI _FXp_getw_nid_postfix(const float* parts, int count) {
    if (count == 0) return 0.0f;
    const float first = ExtendedPrecisionWords::ReadWord(parts, 0, "_FXp_getw");
    if (count == 1 || first == 0.0f) return first;
    const float second = ExtendedPrecisionWords::ReadWord(parts, 1, "_FXp_getw");
    if (second == 0.0f) return first;
    if (count == 2) return first + second;
    const float third = ExtendedPrecisionWords::ReadWord(parts, 2, "_FXp_getw");
    if (third == 0.0f) return first + second;
    const float head = first + second;
    float tail = third;
    if (count >= 4) tail += ExtendedPrecisionWords::ReadWord(parts, 3, "_FXp_getw");
    if (head - first == second) return head + tail;
    return first + (second + tail);
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

const char* APS5_VABI _Gentime_nid_postfix(const void* time, const void* info, char qualifier, char code, int* length, char* buffer) {
    (void)qualifier;
    static constexpr char Empty[] = "";
    static constexpr char Zone[] = ":";
    static constexpr char DateFormat[] = "%m/%d/%y";
    static constexpr char IsoDateFormat[] = "%Y-%m-%d";
    static constexpr char HourMinuteFormat[] = "%H:%M";
    static constexpr char TimeFormat[] = "%H:%M:%S";
    GentimeTm local{};
    GuestLocale::TimeInfo names{};
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&local, time, sizeof(local)) && GuestCopyIn_nid_no_patch(&names, info, sizeof(names));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument("_Gentime: an unreadable time or time information");
    char text[8]{};
    GentimeText result{buffer, 0};
    bool expand = false;
    bool terminated = false;
    const auto digits = [&](int value, int count) { result.length = GentimeDigits(text, value, count); };
    switch (code) {
    case 'a': result = GentimeField(names.days, 2 * local.wday); break;
    case 'A': result = GentimeField(names.days, 2 * local.wday + 1); break;
    case 'b':
    case 'h': result = GentimeField(names.months, 2 * local.mon); break;
    case 'B': result = GentimeField(names.months, 2 * local.mon + 1); break;
    case 'p': result = GentimeField(names.amPm, local.hour < 12 ? 0 : 1); break;
    case 'c': result = GentimeField(names.formats, 0), expand = true; break;
    case 'x': result = GentimeField(names.formats, 1), expand = true; break;
    case 'X': result = GentimeField(names.formats, 2), expand = true; break;
    case 'r': result = GentimeField(names.formats, 3), expand = true; break;
    case 'D': result = {DateFormat, sizeof(DateFormat) - 1}, expand = true; break;
    case 'F': result = {IsoDateFormat, sizeof(IsoDateFormat) - 1}, expand = true; break;
    case 'R': result = {HourMinuteFormat, sizeof(HourMinuteFormat) - 1}, expand = true; break;
    case 'T': result = {TimeFormat, sizeof(TimeFormat) - 1}, expand = true; break;
    case 'C': digits(19 + local.year / 100, 2); break;
    case 'd': digits(local.mday, 2); break;
    case 'e':
        digits(local.mday, 2);
        if (text[0] == '0') text[0] = ' ';
        break;
    case 'H': digits(local.hour, 2); break;
    case 'I': digits((local.hour + 11) % 12 + 1, 2); break;
    case 'j': digits(local.yday + 1, 3); break;
    case 'm': digits(local.mon + 1, 2); break;
    case 'M': digits(local.min, 2); break;
    case 'S': digits(local.sec, 2); break;
    case 'u': digits(local.wday == 0 ? 7 : local.wday, 1); break;
    case 'w': digits(local.wday, 1); break;
    case 'y': digits((local.year + 1900) % 100, 2); break;
    case 'Y': digits(local.year + 1900, 4); break;
    case 'U': digits((local.yday - local.wday + 7) / 7, 2); break;
    case 'W': digits((local.yday - (local.wday + 6) % 7 + 7) / 7, 2); break;
    case 'V':
    case 'G':
    case 'g': {
        int week = 0;
        int isoYear = 0;
        GentimeIsoWeek(local, &week, &isoYear);
        if (code == 'V') digits(week, 2);
        else if (code == 'G') digits(isoYear, 4);
        else digits(isoYear % 100, 2);
        break;
    }
    case 'n': text[0] = '\n', result.length = 1; break;
    case 't': text[0] = '\t', result.length = 1; break;
    case '%': text[0] = '%', result.length = 1; break;
    case 'Z': result = GentimeField(Zone, local.isdst > 0 ? 1 : 0); break;
    case 'z': {
        std::int64_t utc = 0;
        KernelTimesec zone{};
        std::int32_t daylight = 0;
        if (sceKernelConvertLocaltimeToUtc(GentimeSeconds(local), local.isdst, &utc, &zone, &daylight) < 0) return Empty;
        const std::int32_t offset = static_cast<std::int32_t>(zone.west_sec) + (local.isdst > 0 ? daylight : 0);
        const int hours = offset / 3600;
        result.length = std::min(std::snprintf(text, 6, "%+03d%02d", hours, std::abs(offset / 60) % 60), 5);
        terminated = true;
        break;
    }
    default:
        text[0] = '%';
        text[1] = code;
        result.length = 2;
        break;
    }
    if (result.text == buffer && result.length > 0) {
        const bool written = GuestCopyOut_nid_no_patch(buffer, text, static_cast<std::size_t>(result.length) + (terminated ? 1 : 0));
        errno = savedErrno;
        if (!written) throw std::invalid_argument("_Gentime: an unwritable buffer");
    }
    const int reported = expand ? -result.length : result.length;
    const bool stored = GuestCopyOut_nid_no_patch(length, &reported, sizeof(reported));
    errno = savedErrno;
    if (!stored) throw std::invalid_argument("_Gentime: an unwritable length");
    return result.text;
}

int APS5_VABI _Getcloc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

const char* APS5_VABI _Getdst_nid_postfix(const char* s) {
    if (s == nullptr) {
        return nullptr;
    }
    const char* const begin = s;
    const auto readByte = [&](const char* p) -> char {
        if (p - begin >= 0x10000) throw std::invalid_argument("_Getdst: an unterminated rule string");
        char value;
        const int savedErrno = errno;
        const bool read = GuestCopyIn_nid_no_patch(&value, p, sizeof(value));
        errno = savedErrno;
        if (!read) throw std::invalid_argument("_Getdst: an unreadable rule string");
        return value;
    };
    if (readByte(s) == '\0') {
        return nullptr;
    }
    const char sep = readByte(s);
    ++s;
    if (readByte(s) == '\0') {
        return nullptr;
    }

    struct Rule {
        std::uint8_t suffix;
        std::uint8_t hour;
        std::uint8_t day;
        std::uint8_t month;
        std::uint8_t year;
    };

    std::vector<Rule> rules;
    std::uint8_t lastYear = 0;

    while (true) {
        const char head = readByte(s);
        if (head == '\0') {
            break;
        }
        if (head == sep) {
            return nullptr;
        }

        std::uint8_t ruleYear = lastYear;
        if (head == '(') {
            ++s;
            int yearVal = 0;
            int digits = 0;
            while (true) {
                const char digit = readByte(s);
                if (digit < '0' || digit > '9') {
                    break;
                }
                yearVal = yearVal * 10 + (digit - '0');
                ++digits;
                ++s;
                if (digits > 4) {
                    return nullptr;
                }
            }
            if (digits != 4 || readByte(s) != ')' || yearVal < 1900) {
                return nullptr;
            }
            ++s;
            ruleYear = static_cast<std::uint8_t>((yearVal - 1900) & 0xff);
            lastYear = ruleYear;
        }

        int dateDigits = 0;
        char dateBuf[7] = {};
        while (true) {
            const char digit = readByte(s);
            if (digit < '0' || digit > '9') {
                break;
            }
            if (dateDigits < 6) {
                dateBuf[dateDigits] = digit;
            }
            ++dateDigits;
            ++s;
        }

        if (dateDigits != 4 && dateDigits != 6) {
            return nullptr;
        }

        int mm = (dateBuf[0] - '0') * 10 + (dateBuf[1] - '0');
        int dd = (dateBuf[2] - '0') * 10 + (dateBuf[3] - '0');
        int hh = 0;
        if (dateDigits == 6) {
            hh = (dateBuf[4] - '0') * 10 + (dateBuf[5] - '0');
        }

        if (mm < 1 || mm > 12) {
            return nullptr;
        }
        if (dd < 1 || dd > 99) {
            return nullptr;
        }

        std::uint8_t suffixByte = 0;
        const char sign = readByte(s);
        if (sign == '+' || sign == '-') {
            ++s;
            const char weight = readByte(s);
            if (weight < '0' || weight > '6') {
                return nullptr;
            }
            int w = weight - '0';
            ++s;
            int v = (w == 0) ? 7 : w;
            if (sign == '+') {
                suffixByte = static_cast<std::uint8_t>(v + 7);
            } else {
                suffixByte = static_cast<std::uint8_t>(v);
            }
        }

        const char stop = readByte(s);
        if (stop != '\0' && stop != sep) {
            return nullptr;
        }

        Rule r;
        r.suffix = suffixByte;
        r.hour = static_cast<std::uint8_t>(hh);
        r.day = static_cast<std::uint8_t>(dd - 1);
        r.month = static_cast<std::uint8_t>(mm - 1);
        r.year = ruleYear;
        rules.push_back(r);

        if (stop == sep) {
            ++s;
            if (readByte(s) == '\0') {
                return nullptr;
            }
        }
    }

    if (rules.empty()) {
        return nullptr;
    }

    std::size_t totalBytes = (rules.size() + 1) * 5;
    char* buf = static_cast<char*>(std::malloc(totalBytes));
    if (buf == nullptr) {
        return nullptr;
    }

    for (std::size_t i = 0; i < rules.size(); ++i) {
        buf[i * 5 + 0] = static_cast<char>(rules[i].suffix);
        buf[i * 5 + 1] = static_cast<char>(rules[i].hour);
        buf[i * 5 + 2] = static_cast<char>(rules[i].day);
        buf[i * 5 + 3] = static_cast<char>(rules[i].month);
        buf[i * 5 + 4] = static_cast<char>(rules[i].year);
    }

    std::size_t termOffset = rules.size() * 5;
    buf[termOffset + 0] = static_cast<char>(0xff);
    buf[termOffset + 1] = 0;
    buf[termOffset + 2] = 0;
    buf[termOffset + 3] = 0;
    buf[termOffset + 4] = static_cast<char>(lastYear);

    return buf;
}

int APS5_VABI _Getfld_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getfloat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getint_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Getstr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

const char* APS5_VABI _Gettime_nid_postfix(const char* s, int n, int* len) {
    if (s == nullptr) {
        if (len != nullptr) *len = 0;
        return "";
    }
    const char sep = *s;
    if (sep == '\0' || n < 0) {
        while (*s != '\0') ++s;
        if (len != nullptr) *len = 0;
        return s;
    }
    ++s;
    for (; 0 < n; --n) {
        while (*s != '\0' && *s != sep) {
            ++s;
        }
        if (*s == '\0') {
            if (len != nullptr) *len = 0;
            return s;
        }
        ++s;
    }
    const char* start = s;
    while (*s != '\0' && *s != sep) {
        ++s;
    }
    if (len != nullptr) {
        *len = static_cast<int>(s - start);
    }
    return start;
}

const char* APS5_VABI _Getzone_nid_postfix() {
    return ":";
}

int APS5_VABI _Isdst_nid_postfix() {
    return -1;
}

long double APS5_VABI _LErfc_nid_postfix(long double x) {
    return ErfcTail(x, erfcl_nid_postfix, __func__);
}

int APS5_VABI _LQuad_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _LQuadph_nid_postfix() {
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

void APS5_VABI _Lock_spin_lock_nid_postfix(volatile char* lock, int mode) {
    if (lock == nullptr) return;
    while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE) != 0) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_pause();
#endif
    }
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

int APS5_VABI _Nnl_nid_postfix() {
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
