#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libSceLibcInternal/include/Forwards.hpp"
#include "prx/libc/include/Shutdown.hpp"

extern "C" {

[[noreturn]] void abort_nid_postfix(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

void APS5_VABI _Assert_nid_disambig1_nid_postfix(const char* message, const char* location) {
    _Assert_nid_postfix(message, location);
}

int APS5_VABI _Atomic_compare_exchange_weak_4_nid_disambig1_nid_postfix(volatile unsigned int* target, unsigned int* expected, unsigned int desired, int successOrder, int failureOrder) {
    return _Atomic_compare_exchange_weak_4_nid_postfix(target, expected, desired, successOrder, failureOrder);
}

unsigned int APS5_VABI _Atomic_fetch_add_4_nid_disambig1_nid_postfix(volatile unsigned int* target, unsigned int value, int memoryOrder) {
    return _Atomic_fetch_add_4_nid_postfix(target, value, memoryOrder);
}

unsigned int APS5_VABI _Atomic_fetch_sub_4_nid_disambig1_nid_postfix(volatile unsigned int* target, unsigned int value, int memoryOrder) {
    return _Atomic_fetch_sub_4_nid_postfix(target, value, memoryOrder);
}

unsigned int APS5_VABI _Atomic_load_4_nid_disambig1_nid_postfix(volatile unsigned int* target, int memoryOrder) {
    return _Atomic_load_4_nid_postfix(target, memoryOrder);
}

int APS5_VABI _Cnd_broadcast_nid_disambig1_nid_postfix(void** handle) {
    return _Cnd_broadcast_nid_postfix(handle);
}

void APS5_VABI _Cnd_destroy_nid_disambig1_nid_postfix(void** handle) {
    _Cnd_destroy_nid_postfix(handle);
}

int APS5_VABI _Cnd_init_nid_disambig1_nid_postfix(void** handle) {
    return _Cnd_init_nid_postfix(handle);
}



int APS5_VABI _Cnd_signal_nid_disambig1_nid_postfix(void** handle) {
    return _Cnd_signal_nid_postfix(handle);
}

int APS5_VABI _Cnd_timedwait_nid_disambig1_nid_postfix(void** conditionHandle, void** mutexHandle, const Xtime* target) {
    return _Cnd_timedwait_nid_postfix(conditionHandle, mutexHandle, target);
}

int APS5_VABI _Cnd_wait_nid_disambig1_nid_postfix(void** conditionHandle, void** mutexHandle) {
    return _Cnd_wait_nid_postfix(conditionHandle, mutexHandle);
}

double APS5_VABI _Cosh_nid_disambig1_nid_postfix(double x, double y) {
    return _Cosh_nid_postfix(x, y);
}

void APS5_VABI _Exit_nid_disambig1_nid_postfix(int code) {
    _Exit_nid_postfix(code);
}

float APS5_VABI _FCosh_nid_disambig1_nid_postfix(float x, float y) {
    return _FCosh_nid_postfix(x, y);
}

short APS5_VABI _FDtest_nid_disambig1_nid_postfix(const float* value) {
    return _FDtest_nid_postfix(value);
}

short APS5_VABI _FExp_nid_disambig1_nid_postfix(float* value, float y, short offset) {
    return _FExp_nid_postfix(value, y, offset);
}

float APS5_VABI _FSinh_nid_disambig1_nid_postfix(float x, float y) {
    return _FSinh_nid_postfix(x, y);
}



const short* APS5_VABI _Getpctype_nid_disambig1_nid_postfix(void) {
    return _Getpctype_nid_postfix();
}

mbstate_t* APS5_VABI _Getpmbstate_nid_disambig1_nid_postfix(void) {
    return _Getpmbstate_nid_postfix();
}

const short* APS5_VABI _Getptolower_nid_disambig1_nid_postfix(void) {
    return _Getptolower_nid_postfix();
}

const short* APS5_VABI _Getptoupper_nid_disambig1_nid_postfix(void) {
    return _Getptoupper_nid_postfix();
}

mbstate_t* APS5_VABI _Getpwcstate_nid_disambig1_nid_postfix(void) {
    return _Getpwcstate_nid_postfix();
}

int APS5_VABI _Iswctype_nid_disambig1_nid_postfix(std::uint32_t c, std::uint64_t type) {
    return _Iswctype_nid_postfix(c, type);
}

void APS5_VABI _Lockfilelock_nid_disambig1_nid_postfix(FileStream* stream) {
    _Lockfilelock_nid_postfix(stream);
}

void APS5_VABI _Locksyslock_nid_disambig1_nid_postfix(void) {
    _Locksyslock_nid_postfix();
}

int APS5_VABI _Mbtowcx_nid_disambig1_nid_postfix(std::uint16_t* dst, const char* src, std::size_t count, mbstate_t* st) {
    return _Mbtowcx_nid_postfix(dst, src, count, st);
}

void APS5_VABI _Mtx_destroy_nid_disambig1_nid_postfix(void** handle) {
    _Mtx_destroy_nid_postfix(handle);
}

int APS5_VABI _Mtx_init_nid_disambig1_nid_postfix(void** handle, int type) {
    return _Mtx_init_nid_postfix(handle, type);
}

int APS5_VABI _Mtx_init_with_name_nid_disambig1_nid_postfix(void** handle, int type, const char* a2) {
    return _Mtx_init_with_name_nid_postfix(handle, type, a2);
}

int APS5_VABI _Mtx_lock_nid_disambig1_nid_postfix(void** handle) {
    return _Mtx_lock_nid_postfix(handle);
}

int APS5_VABI _Mtx_trylock_nid_disambig1_nid_postfix(void** handle) {
    return _Mtx_trylock_nid_postfix(handle);
}

int APS5_VABI _Mtx_unlock_nid_disambig1_nid_postfix(void** handle) {
    return _Mtx_unlock_nid_postfix(handle);
}

double APS5_VABI _Sinh_nid_disambig1_nid_postfix(double x, double y) {
    return _Sinh_nid_postfix(x, y);
}

std::uint64_t APS5_VABI _Stoul_nid_disambig1_nid_postfix(const char* str, char** endptr, int base) {
    return _Stoul_nid_postfix(str, endptr, base);
}

void* APS5_VABI _Thrd_id_nid_disambig1_nid_postfix(void) {
    return _Thrd_id_nid_postfix();
}

int APS5_VABI _Thrd_join_nid_disambig1_nid_postfix(void* thread, int* code) {
    return _Thrd_join_nid_postfix(thread, code);
}

int APS5_VABI _Thrd_sleep_nid_disambig1_nid_postfix(const Xtime* target, Xtime* a1) {
    return _Thrd_sleep_nid_postfix(target, a1);
}

int APS5_VABI _Thrd_yield_nid_disambig1_nid_postfix(void) {
    return _Thrd_yield_nid_postfix();
}

std::uint32_t APS5_VABI _Towctrans_nid_disambig1_nid_postfix(std::uint32_t c, std::uint64_t desc) {
    return _Towctrans_nid_postfix(c, desc);
}

void APS5_VABI _Unlockfilelock_nid_disambig1_nid_postfix(FileStream* stream) {
    _Unlockfilelock_nid_postfix(stream);
}

void APS5_VABI _Unlocksyslock_nid_disambig1_nid_postfix(void) {
    _Unlocksyslock_nid_postfix();
}

int APS5_VABI _Wctombx_nid_disambig1_nid_postfix(char* dst, std::uint16_t src, mbstate_t* st) {
    return _Wctombx_nid_postfix(dst, src, st);
}

int64_t APS5_VABI _Xtime_get_ticks_nid_disambig1_nid_postfix(void) {
    return _Xtime_get_ticks_nid_postfix();
}

void APS5_VABI _ZNKSt9exception6_RaiseEv_nid_disambig1_nid_postfix(const LibcException::ExceptionObject* self) {
    _ZNKSt9exception6_RaiseEv_nid_postfix(self);
}

void APS5_VABI _ZNKSt9exception8_DoraiseEv_nid_disambig1_nid_postfix(const LibcException::ExceptionObject* self) {
    _ZNKSt9exception8_DoraiseEv_nid_postfix(self);
}

void APS5_VABI _ZNSt13runtime_errorD1Ev_nid_disambig1_nid_postfix(LibcException::ExceptionObject* self) {
    _ZNSt13runtime_errorD1Ev_nid_postfix(self);
}

void APS5_VABI _ZNSt13runtime_errorD2Ev_nid_disambig1_nid_postfix(LibcException::ExceptionObject* self) {
    _ZNSt13runtime_errorD2Ev_nid_postfix(self);
}





void APS5_VABI _ZNSt4_Pad7_LaunchEPP7pthread_nid_disambig1_nid_postfix(GuestPad* pad, void** thread) {
    _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(pad, thread);
}

void APS5_VABI _ZNSt4_Pad8_ReleaseEv_nid_disambig1_nid_postfix(GuestPad* pad) {
    _ZNSt4_Pad8_ReleaseEv_nid_postfix(pad);
}

void APS5_VABI _ZNSt4_PadC2Ev_nid_disambig1_nid_postfix(GuestPad* pad) {
    _ZNSt4_PadC2Ev_nid_postfix(pad);
}



GuestLocale::Implementation* APS5_VABI _ZNSt6locale16_GetgloballocaleEv_nid_disambig1_nid_postfix(void) {
    return _ZNSt6locale16_GetgloballocaleEv_nid_postfix();
}

GuestLocale::Implementation* APS5_VABI _ZNSt6locale5_InitEv_nid_disambig1_nid_postfix(void) {
    return _ZNSt6locale5_InitEv_nid_postfix();
}

void APS5_VABI _ZNSt6locale5facet9_RegisterEv_nid_disambig1_nid_postfix(GuestLocale::Facet* self) {
    _ZNSt6locale5facet9_RegisterEv_nid_postfix(self);
}

GuestLocaleFacade* APS5_VABI _ZNSt6locale7classicEv_nid_disambig1_nid_postfix(void) {
    return _ZNSt6locale7classicEv_nid_postfix();
}

unsigned APS5_VABI _ZNSt6thread20hardware_concurrencyEv_nid_disambig1_nid_postfix(void) {
    return _ZNSt6thread20hardware_concurrencyEv_nid_postfix();
}

std::size_t APS5_VABI _ZNSt7codecvtIcc9_MbstatetE7_GetcatEPPKNSt6locale5facetEPKS2__nid_disambig1_nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* a1) {
    return _ZNSt7codecvtIcc9_MbstatetE7_GetcatEPPKNSt6locale5facetEPKS2__nid_postfix(facet, a1);
}

std::size_t APS5_VABI _ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_disambig1_nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    return _ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(facet, locale);
}

std::size_t APS5_VABI _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_disambig1_nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    return _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(facet, locale);
}

std::size_t APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_disambig1_nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* a1) {
    return _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(facet, a1);
}





void APS5_VABI _ZNSt8bad_castD1Ev_nid_disambig1_nid_postfix(LibcException::ExceptionObject* self) {
    _ZNSt8bad_castD1Ev_nid_postfix(self);
}

void APS5_VABI _ZNSt8ios_base7_AddstdEPS__nid_disambig1_nid_postfix(GuestLocale::IosBase* self) {
    _ZNSt8ios_base7_AddstdEPS__nid_postfix(self);
}

void APS5_VABI _ZNSt8ios_base7failureD1Ev_nid_disambig1_nid_postfix(LibcException::ExceptionObject* self) {
    _ZNSt8ios_base7failureD1Ev_nid_postfix(self);
}

std::size_t APS5_VABI _ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_disambig1_nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* a1) {
    return _ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(facet, a1);
}

std::size_t APS5_VABI _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_disambig1_nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* a1) {
    return _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(facet, a1);
}

void APS5_VABI _ZNSt9bad_allocD1Ev_nid_disambig1_nid_postfix(LibcException::ExceptionObject* self) {
    _ZNSt9bad_allocD1Ev_nid_postfix(self);
}

[[noreturn]] void APS5_VABI _ZSt11_Xbad_allocv_nid_disambig1_nid_postfix(void) {
    _ZSt11_Xbad_allocv_nid_postfix();
}

int APS5_VABI _ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_disambig1_nid_postfix(int* flag, int (APS5_VABI *callback)(void*, void*, void**), void* arg) {
    return _ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_postfix(flag, callback, arg);
}

void APS5_VABI _ZSt13_Xregex_errorNSt15regex_constants10error_typeE_nid_disambig1_nid_postfix(std::regex_constants::error_type code) {
    _ZSt13_Xregex_errorNSt15regex_constants10error_typeE_nid_postfix(code);
}

unsigned int APS5_VABI _ZSt14_Random_devicev_nid_disambig1_nid_postfix(void) {
    return _ZSt14_Random_devicev_nid_postfix();
}

void APS5_VABI _ZSt14_Throw_C_errori_nid_disambig1_nid_postfix(int code) {
    _ZSt14_Throw_C_errori_nid_postfix(code);
}

[[noreturn]] void APS5_VABI _ZSt14_Xlength_errorPKc_nid_disambig1_nid_postfix(const char* message) {
    _ZSt14_Xlength_errorPKc_nid_postfix(message);
}

[[noreturn]] void APS5_VABI _ZSt14_Xout_of_rangePKc_nid_disambig1_nid_postfix(const char* message) {
    _ZSt14_Xout_of_rangePKc_nid_postfix(message);
}





GuestNewHandler APS5_VABI _ZSt15get_new_handlerv_nid_disambig1_nid_postfix(void) {
    return _ZSt15get_new_handlerv_nid_postfix();
}

void APS5_VABI _ZSt16_Throw_Cpp_errori_nid_disambig1_nid_postfix(int code) {
    _ZSt16_Throw_Cpp_errori_nid_postfix(code);
}

const LibcException::CategoryObject* APS5_VABI _ZSt16generic_categoryv_nid_disambig1_nid_postfix(void) {
    return _ZSt16generic_categoryv_nid_postfix();
}

void APS5_VABI _ZSt18_Xinvalid_argumentPKc_nid_disambig1_nid_postfix(const char* message) {
    _ZSt18_Xinvalid_argumentPKc_nid_postfix(message);
}

bool APS5_VABI _ZSt18uncaught_exceptionv_nid_disambig1_nid_postfix(void) {
    return _ZSt18uncaught_exceptionv_nid_postfix();
}

void APS5_VABI _ZSt19_Xbad_function_callv_nid_disambig1_nid_postfix(void) {
    _ZSt19_Xbad_function_callv_nid_postfix();
}

StreamManipulator APS5_VABI _ZSt4setwi_nid_disambig1_nid_postfix(std::int32_t width) {
    return _ZSt4setwi_nid_postfix(width);
}

void APS5_VABI _ZdaPv_nid_disambig1_nid_postfix(void* pointer) {
    _ZdaPv_nid_postfix(pointer);
}

void APS5_VABI _ZdlPv_nid_disambig1_nid_postfix(void* pointer) {
    _ZdlPv_nid_postfix(pointer);
}

void APS5_VABI _ZdlPvSt11align_val_t_nid_disambig1_nid_postfix(void* pointer, std::size_t alignment) {
    _ZdlPvSt11align_val_t_nid_postfix(pointer, alignment);
}

void* APS5_VABI _Znam_nid_disambig1_nid_postfix(std::size_t size) {
    return _Znam_nid_postfix(size);
}

void* APS5_VABI _Znwm_nid_disambig1_nid_postfix(std::size_t size) {
    return _Znwm_nid_postfix(size);
}

int APS5_VABI __cxa_atexit_nid_disambig1_nid_postfix(void (APS5_VABI *func)(void*), void* arg, void* dsoHandle) {
    return __cxa_atexit_nid_postfix(func, arg, dsoHandle);
}

void APS5_VABI __cxa_decrement_exception_refcount_nid_disambig1_nid_postfix(void* object) {
    __cxa_decrement_exception_refcount_nid_postfix(object);
}

void APS5_VABI __cxa_end_catch_nid_disambig1_nid_postfix(void) {
    __cxa_end_catch_nid_postfix();
}

void APS5_VABI __cxa_guard_abort_nid_disambig1_nid_postfix(std::uint64_t* guardObject) {
    __cxa_guard_abort_nid_postfix(guardObject);
}

int APS5_VABI __cxa_guard_acquire_nid_disambig1_nid_postfix(std::uint64_t* guardObject) {
    return __cxa_guard_acquire_nid_postfix(guardObject);
}

void APS5_VABI __cxa_guard_release_nid_disambig1_nid_postfix(std::uint64_t* guardObject) {
    __cxa_guard_release_nid_postfix(guardObject);
}

void APS5_VABI __cxa_increment_exception_refcount_nid_disambig1_nid_postfix(void* object) {
    __cxa_increment_exception_refcount_nid_postfix(object);
}

void APS5_VABI __cxa_pure_virtual_nid_disambig1_nid_postfix(void) {
    __cxa_pure_virtual_nid_postfix();
}

void APS5_VABI __cxa_rethrow_nid_disambig1_nid_postfix(void) {
    __cxa_rethrow_nid_postfix();
}

void APS5_VABI __cxa_rethrow_primary_exception_nid_disambig1_nid_postfix(void* object) {
    __cxa_rethrow_primary_exception_nid_postfix(object);
}

__int128 APS5_VABI __divti3_nid_disambig1_nid_postfix(__int128 dividend, __int128 divisor) {
    return __divti3_nid_postfix(dividend, divisor);
}

Int128 APS5_VABI __fixdfti_nid_disambig1_nid_postfix(double a) {
    return __fixdfti_nid_postfix(a);
}

int APS5_VABI __isfinite_nid_disambig1_nid_postfix(double x) {
    return __isfinite_nid_postfix(x);
}

int APS5_VABI __isfinitef_nid_disambig1_nid_postfix(float x) {
    return __isfinitef_nid_postfix(x);
}

int APS5_VABI __isfinitel_nid_disambig1_nid_postfix(long double x) {
    return __isfinitel_nid_postfix(x);
}

int APS5_VABI __isinf_nid_disambig1_nid_postfix(double x) {
    return __isinf_nid_postfix(x);
}

int APS5_VABI __isinff_nid_disambig1_nid_postfix(float x) {
    return __isinff_nid_postfix(x);
}

int APS5_VABI __isnan_nid_disambig1_nid_postfix(double x) {
    return __isnan_nid_postfix(x);
}

int APS5_VABI __isnanf_nid_disambig1_nid_postfix(float x) {
    return __isnanf_nid_postfix(x);
}

int APS5_VABI __isnormal_nid_disambig1_nid_postfix(double x) {
    return __isnormal_nid_postfix(x);
}

__int128 APS5_VABI __modti3_nid_disambig1_nid_postfix(__int128 dividend, __int128 divisor) {
    return __modti3_nid_postfix(dividend, divisor);
}

double APS5_VABI __powidf2_nid_disambig1_nid_postfix(double base, int exponent) {
    return __powidf2_nid_postfix(base, exponent);
}

float APS5_VABI __powisf2_nid_disambig1_nid_postfix(float base, int exponent) {
    return __powisf2_nid_postfix(base, exponent);
}

int APS5_VABI __signbit_nid_disambig1_nid_postfix(double x) {
    return __signbit_nid_postfix(x);
}

int APS5_VABI __signbitf_nid_disambig1_nid_postfix(float x) {
    return __signbitf_nid_postfix(x);
}

unsigned __int128 APS5_VABI __udivti3_nid_disambig1_nid_postfix(unsigned __int128 dividend, unsigned __int128 divisor) {
    return __udivti3_nid_postfix(dividend, divisor);
}

void APS5_VABI _init_env_nid_disambig1_nid_postfix(void) {
    _init_env_nid_postfix();
}

[[noreturn]] void APS5_VABI abort_nid_disambig1_nid_postfix(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    abort_nid_postfix(a0, a1, a2, a3, a4, a5);
}

double APS5_VABI acos_nid_disambig1_nid_postfix(double x) {
    return acos_nid_postfix(x);
}

float APS5_VABI acosf_nid_disambig1_nid_postfix(float x) {
    return acosf_nid_postfix(x);
}

double APS5_VABI acosh_nid_disambig1_nid_postfix(double x) {
    return acosh_nid_postfix(x);
}

void* APS5_VABI aligned_alloc_nid_disambig1_nid_postfix(size_t alignment, size_t size) {
    return aligned_alloc_nid_postfix(alignment, size);
}

double APS5_VABI asin_nid_disambig1_nid_postfix(double x) {
    return asin_nid_postfix(x);
}

float APS5_VABI asinf_nid_disambig1_nid_postfix(float x) {
    return asinf_nid_postfix(x);
}

double APS5_VABI asinh_nid_disambig1_nid_postfix(double x) {
    return asinh_nid_postfix(x);
}

float APS5_VABI asinhf_nid_disambig1_nid_postfix(float x) {
    return asinhf_nid_postfix(x);
}

double APS5_VABI atan_nid_disambig1_nid_postfix(double x) {
    return atan_nid_postfix(x);
}

double APS5_VABI atan2_nid_disambig1_nid_postfix(double y, double x) {
    return atan2_nid_postfix(y, x);
}

float APS5_VABI atan2f_nid_disambig1_nid_postfix(float y, float x) {
    return atan2f_nid_postfix(y, x);
}

float APS5_VABI atanf_nid_disambig1_nid_postfix(float x) {
    return atanf_nid_postfix(x);
}

double APS5_VABI atanh_nid_disambig1_nid_postfix(double x) {
    return atanh_nid_postfix(x);
}

int APS5_VABI atexit_nid_disambig1_nid_postfix(GuestExitCallback func) {
    return atexit_nid_postfix(func);
}

double APS5_VABI atof_nid_disambig1_nid_postfix(const char* str) {
    return atof_nid_postfix(str);
}

int APS5_VABI bcmp_nid_disambig1_nid_postfix(const void* s1, const void* s2, size_t n) {
    return bcmp_nid_postfix(s1, s2, n);
}

void* APS5_VABI calloc_nid_disambig1_nid_postfix(size_t count, size_t size) {
    return calloc_nid_postfix(count, size);
}

double APS5_VABI cbrt_nid_disambig1_nid_postfix(double x) {
    return cbrt_nid_postfix(x);
}

float APS5_VABI cbrtf_nid_disambig1_nid_postfix(float x) {
    return cbrtf_nid_postfix(x);
}

int64_t APS5_VABI clock_nid_disambig1_nid_postfix(void) {
    return clock_nid_postfix();
}



double APS5_VABI cos_nid_disambig1_nid_postfix(double x) {
    return cos_nid_postfix(x);
}

float APS5_VABI cosf_nid_disambig1_nid_postfix(float x) {
    return cosf_nid_postfix(x);
}

double APS5_VABI cosh_nid_disambig1_nid_postfix(double x) {
    return cosh_nid_postfix(x);
}

double APS5_VABI difftime_nid_disambig1_nid_postfix(int64_t time1, int64_t time0) {
    return difftime_nid_postfix(time1, time0);
}

std::div_t APS5_VABI div_nid_disambig1_nid_postfix(int numerator, int denominator) {
    return div_nid_postfix(numerator, denominator);
}

void APS5_VABI exit_nid_disambig1_nid_postfix(int code) {
    LibcExit_nid_no_patch(code);
}

double APS5_VABI exp_nid_disambig1_nid_postfix(double x) {
    return exp_nid_postfix(x);
}

double APS5_VABI exp2_nid_disambig1_nid_postfix(double x) {
    return exp2_nid_postfix(x);
}

float APS5_VABI exp2f_nid_disambig1_nid_postfix(float x) {
    return exp2f_nid_postfix(x);
}

float APS5_VABI expf_nid_disambig1_nid_postfix(float x) {
    return expf_nid_postfix(x);
}

double APS5_VABI expm1_nid_disambig1_nid_postfix(double x) {
    return expm1_nid_postfix(x);
}

double APS5_VABI fabs_nid_disambig1_nid_postfix(double x) {
    return fabs_nid_postfix(x);
}

int APS5_VABI fclose_nid_disambig1_nid_postfix(FileStream* stream) {
    return fclose_nid_postfix(stream);
}



int APS5_VABI feof_nid_disambig1_nid_postfix(FileStream* stream) {
    return feof_nid_postfix(stream);
}

int APS5_VABI ferror_nid_disambig1_nid_postfix(FileStream* stream) {
    return ferror_nid_postfix(stream);
}

int APS5_VABI fgetc_nid_disambig1_nid_postfix(FileStream* stream) {
    return fgetc_nid_postfix(stream);
}

int APS5_VABI fgetpos_nid_disambig1_nid_postfix(FileStream* stream, std::int64_t* position) {
    return fgetpos_nid_postfix(stream, position);
}

char* APS5_VABI fgets_nid_disambig1_nid_postfix(char* buffer, int size, FileStream* stream) {
    return fgets_nid_postfix(buffer, size, stream);
}

double APS5_VABI fmod_nid_disambig1_nid_postfix(double x, double y) {
    return fmod_nid_postfix(x, y);
}

float APS5_VABI fmodf_nid_disambig1_nid_postfix(float x, float y) {
    return fmodf_nid_postfix(x, y);
}

FileStream* APS5_VABI fopen_nid_disambig1_nid_postfix(const char* filename, const char* mode) {
    return fopen_nid_postfix(filename, mode);
}

int APS5_VABI fopen_s_nid_disambig1_nid_postfix(FileStream** result, const char* filename, const char* mode) {
    return fopen_s_nid_postfix(result, filename, mode);
}



int APS5_VABI fprintf_nid_disambig1_nid_postfix(FileStream* stream, const char* format, std::uint64_t v0, std::uint64_t v1) {
    return fprintf_nid_postfix(stream, format, v0, v1);
}

int APS5_VABI fputc_nid_disambig1_nid_postfix(int value, FileStream* stream) {
    return fputc_nid_postfix(value, stream);
}



size_t APS5_VABI fread_nid_disambig1_nid_postfix(void* buffer, size_t size, size_t count, FileStream* stream) {
    return fread_nid_postfix(buffer, size, count, stream);
}

void APS5_VABI free_nid_disambig1_nid_postfix(void* ptr) {
    free_nid_postfix(ptr);
}

double APS5_VABI frexp_nid_disambig1_nid_postfix(double x, int* exponent) {
    return frexp_nid_postfix(x, exponent);
}

float APS5_VABI frexpf_nid_disambig1_nid_postfix(float x, int* exponent) {
    return frexpf_nid_postfix(x, exponent);
}

long double APS5_VABI frexpl_nid_disambig1_nid_postfix(long double x, int* exponent) {
    return frexpl_nid_postfix(x, exponent);
}

int APS5_VABI fscanf_nid_disambig1_nid_postfix(FileStream* stream, const char* format, std::uint64_t v0, std::uint64_t v1) {
    return fscanf_nid_postfix(stream, format, v0, v1);
}

int APS5_VABI fseek_nid_disambig1_nid_postfix(FileStream* stream, std::int64_t offset, int origin) {
    return fseek_nid_postfix(stream, offset, origin);
}

int APS5_VABI fsetpos_nid_disambig1_nid_postfix(FileStream* stream, const std::int64_t* position) {
    return fsetpos_nid_postfix(stream, position);
}

std::int64_t APS5_VABI ftell_nid_disambig1_nid_postfix(FileStream* stream) {
    return ftell_nid_postfix(stream);
}

size_t APS5_VABI fwrite_nid_disambig1_nid_postfix(const void* buffer, size_t size, size_t count, FileStream* stream) {
    return fwrite_nid_postfix(buffer, size, count, stream);
}

GuestTm* APS5_VABI gmtime_nid_disambig1_nid_postfix(const int64_t* timer) {
    return gmtime_nid_postfix(timer);
}

GuestTm* APS5_VABI gmtime_s_nid_disambig1_nid_postfix(const int64_t* timer, GuestTm* result) {
    return gmtime_s_nid_postfix(timer, result);
}

double APS5_VABI hypot_nid_disambig1_nid_postfix(double x, double y) {
    return hypot_nid_postfix(x, y);
}

float APS5_VABI hypotf_nid_disambig1_nid_postfix(float x, float y) {
    return hypotf_nid_postfix(x, y);
}

double APS5_VABI ldexp_nid_disambig1_nid_postfix(double x, int exponent) {
    return ldexp_nid_postfix(x, exponent);
}

float APS5_VABI ldexpf_nid_disambig1_nid_postfix(float x, int exp) {
    return ldexpf_nid_postfix(x, exp);
}

std::int64_t APS5_VABI llroundf_nid_disambig1_nid_postfix(float x) {
    return llroundf_nid_postfix(x);
}

SonyLconv* APS5_VABI localeconv_nid_disambig1_nid_postfix(void) {
    return localeconv_nid_postfix();
}

GuestTm* APS5_VABI localtime_nid_disambig1_nid_postfix(const int64_t* timer) {
    return localtime_nid_postfix(timer);
}

double APS5_VABI log_nid_disambig1_nid_postfix(double x) {
    return log_nid_postfix(x);
}

double APS5_VABI log10_nid_disambig1_nid_postfix(double x) {
    return log10_nid_postfix(x);
}

float APS5_VABI log10f_nid_disambig1_nid_postfix(float x) {
    return log10f_nid_postfix(x);
}

double APS5_VABI log1p_nid_disambig1_nid_postfix(double x) {
    return log1p_nid_postfix(x);
}

double APS5_VABI log2_nid_disambig1_nid_postfix(double x) {
    return log2_nid_postfix(x);
}

float APS5_VABI log2f_nid_disambig1_nid_postfix(float x) {
    return log2f_nid_postfix(x);
}

double APS5_VABI logb_nid_disambig1_nid_postfix(double x) {
    return logb_nid_postfix(x);
}

float APS5_VABI logbf_nid_disambig1_nid_postfix(float x) {
    return logbf_nid_postfix(x);
}

float APS5_VABI logf_nid_disambig1_nid_postfix(float x) {
    return logf_nid_postfix(x);
}

void APS5_VABI longjmp_nid_disambig1_nid_postfix(std::uint64_t* buffer, int value) {
    longjmp_nid_postfix(buffer, value);
}

std::int64_t APS5_VABI lround_nid_disambig1_nid_postfix(double x) {
    return lround_nid_postfix(x);
}

std::int64_t APS5_VABI lroundf_nid_disambig1_nid_postfix(float x) {
    return lroundf_nid_postfix(x);
}

void* APS5_VABI malloc_nid_disambig1_nid_postfix(size_t size) {
    return malloc_nid_postfix(size);
}

size_t APS5_VABI malloc_usable_size_nid_disambig1_nid_postfix(void* pointer) {
    return malloc_usable_size_nid_postfix(pointer);
}

std::size_t APS5_VABI mbsrtowcs_nid_disambig1_nid_postfix(std::uint16_t* destination, const char** source, std::size_t capacity, void* state) {
    return mbsrtowcs_nid_postfix(destination, source, capacity, state);
}

std::size_t APS5_VABI mbstowcs_nid_disambig1_nid_postfix(std::uint16_t* dest, const char* src, std::size_t size) {
    return mbstowcs_nid_postfix(dest, src, size);
}

int APS5_VABI mbstowcs_s_nid_disambig1_nid_postfix(std::size_t* result, std::uint16_t* destination, std::size_t capacity, const char* source, std::size_t limit) {
    return mbstowcs_s_nid_postfix(result, destination, capacity, source, limit);
}

void* APS5_VABI memalign_nid_disambig1_nid_postfix(size_t alignment, size_t size) {
    return memalign_nid_postfix(alignment, size);
}

const void* APS5_VABI memchr_nid_disambig1_nid_postfix(const void* s, int c, size_t n) {
    return memchr_nid_postfix(s, c, n);
}

int APS5_VABI memcmp_nid_disambig1_nid_postfix(const void* s1, const void* s2, size_t n) {
    return memcmp_nid_postfix(s1, s2, n);
}

void* APS5_VABI memcpy_nid_disambig1_nid_postfix(void* dest, const void* src, size_t n) {
    return memcpy_nid_postfix(dest, src, n);
}

int APS5_VABI memcpy_s_nid_disambig1_nid_postfix(void* dest, size_t destsz, const void* src, size_t count) {
    return memcpy_s_nid_postfix(dest, destsz, src, count);
}

void* APS5_VABI memmove_nid_disambig1_nid_postfix(void* dest, const void* src, size_t n) {
    return memmove_nid_postfix(dest, src, n);
}

void* APS5_VABI memset_nid_disambig1_nid_postfix(void* s, int c, size_t n) {
    return memset_nid_postfix(s, c, n);
}

int APS5_VABI memset_s_nid_disambig1_nid_postfix(void* dest, size_t destsz, int value, size_t count) {
    return memset_s_nid_postfix(dest, destsz, value, count);
}

int64_t APS5_VABI mktime_nid_disambig1_nid_postfix(GuestTm* timeptr) {
    return mktime_nid_postfix(timeptr);
}

double APS5_VABI modf_nid_disambig1_nid_postfix(double x, double* integral) {
    return modf_nid_postfix(x, integral);
}

float APS5_VABI modff_nid_disambig1_nid_postfix(float x, float* integral) {
    return modff_nid_postfix(x, integral);
}

float APS5_VABI nanf_nid_disambig1_nid_postfix(const char* tagp) {
    return nanf_nid_postfix(tagp);
}

double APS5_VABI nextafter_nid_disambig1_nid_postfix(double x, double y) {
    return nextafter_nid_postfix(x, y);
}

float APS5_VABI nextafterf_nid_disambig1_nid_postfix(float x, float y) {
    return nextafterf_nid_postfix(x, y);
}









void APS5_VABI perror_nid_disambig1_nid_postfix(const char* prefix) {
    perror_nid_postfix(prefix);
}

int APS5_VABI posix_memalign_nid_disambig1_nid_postfix(void** pointer, size_t alignment, size_t size) {
    return posix_memalign_nid_postfix(pointer, alignment, size);
}

double APS5_VABI pow_nid_disambig1_nid_postfix(double base, double exp) {
    return pow_nid_postfix(base, exp);
}

float APS5_VABI powf_nid_disambig1_nid_postfix(float base, float exp) {
    return powf_nid_postfix(base, exp);
}

int APS5_VABI printf_nid_disambig1_nid_postfix(const char* format, std::uint64_t v0, std::uint64_t v1) {
    return printf_nid_postfix(format, v0, v1);
}

int APS5_VABI putchar_nid_disambig1_nid_postfix(int value) {
    return putchar_nid_postfix(value);
}

int APS5_VABI puts_nid_disambig1_nid_postfix(const char* s) {
    return puts_nid_postfix(s);
}

void APS5_VABI qsort_nid_disambig1_nid_postfix(void* base, size_t count, size_t size, int (APS5_VABI *compare)(const void*, const void*)) {
    qsort_nid_postfix(base, count, size, compare);
}

void APS5_VABI quick_exit_nid_disambig1_nid_postfix(int status) {
    quick_exit_nid_postfix(status);
}

int APS5_VABI rand_nid_disambig1_nid_postfix(void) {
    return rand_nid_postfix();
}

void* APS5_VABI realloc_nid_disambig1_nid_postfix(void* ptr, size_t newSize) {
    return realloc_nid_postfix(ptr, newSize);
}

void* APS5_VABI reallocalign_nid_disambig1_nid_postfix(void* ptr, size_t size, size_t alignment) {
    return reallocalign_nid_postfix(ptr, size, alignment);
}

double APS5_VABI remainder_nid_disambig1_nid_postfix(double x, double y) {
    return remainder_nid_postfix(x, y);
}

float APS5_VABI remainderf_nid_disambig1_nid_postfix(float x, float y) {
    return remainderf_nid_postfix(x, y);
}

double APS5_VABI round_nid_disambig1_nid_postfix(double x) {
    return round_nid_postfix(x);
}

float APS5_VABI roundf_nid_disambig1_nid_postfix(float x) {
    return roundf_nid_postfix(x);
}

double APS5_VABI scalbn_nid_disambig1_nid_postfix(double x, int exponent) {
    return scalbn_nid_postfix(x, exponent);
}

float APS5_VABI scalbnf_nid_disambig1_nid_postfix(float x, int exponent) {
    return scalbnf_nid_postfix(x, exponent);
}

void APS5_VABI sceLibcInternalBacktraceForGame_nid_disambig1_nid_postfix(const char* heapName) {
    sceLibcInternalBacktraceForGame_nid_postfix(heapName);
}

void APS5_VABI sceLibcInternalHeapErrorReportForGame_nid_disambig1_nid_postfix(void* heap, void* block, std::uint32_t error) {
    sceLibcInternalHeapErrorReportForGame_nid_postfix(heap, block, error);
}

void* APS5_VABI sceLibcMspaceCalloc_nid_disambig1_nid_postfix(void* handle, std::size_t count, std::size_t size) {
    return sceLibcMspaceCalloc_nid_postfix(handle, count, size);
}

int APS5_VABI sceLibcMspaceDestroy_nid_disambig1_nid_postfix(void* handle) {
    return sceLibcMspaceDestroy_nid_postfix(handle);
}

void APS5_VABI sceLibcMspaceFree_nid_disambig1_nid_postfix(void* handle, void* pointer) {
    sceLibcMspaceFree_nid_postfix(handle, pointer);
}

void* APS5_VABI sceLibcMspaceMalloc_nid_disambig1_nid_postfix(void* handle, std::size_t size) {
    return sceLibcMspaceMalloc_nid_postfix(handle, size);
}

int APS5_VABI sceLibcMspaceMallocStats_nid_disambig1_nid_postfix(void* handle, MallocManagedSize* stats) {
    return sceLibcMspaceMallocStats_nid_postfix(handle, stats);
}

int APS5_VABI sceLibcMspaceMallocStatsFast_nid_disambig1_nid_postfix(void* handle, MallocManagedSize* stats) {
    return sceLibcMspaceMallocStatsFast_nid_postfix(handle, stats);
}

std::size_t APS5_VABI sceLibcMspaceMallocUsableSize_nid_disambig1_nid_postfix(const void* pointer) {
    return sceLibcMspaceMallocUsableSize_nid_postfix(pointer);
}

void* APS5_VABI sceLibcMspaceMemalign_nid_disambig1_nid_postfix(void* handle, std::size_t alignment, std::size_t size) {
    return sceLibcMspaceMemalign_nid_postfix(handle, alignment, size);
}

void* APS5_VABI sceLibcMspaceRealloc_nid_disambig1_nid_postfix(void* handle, void* pointer, std::size_t size) {
    return sceLibcMspaceRealloc_nid_postfix(handle, pointer, size);
}

void* APS5_VABI sceLibcMspaceReallocalign_nid_disambig1_nid_postfix(void* handle, void* pointer, std::size_t alignment, std::size_t size) {
    return sceLibcMspaceReallocalign_nid_postfix(handle, pointer, alignment, size);
}

const char* APS5_VABI setlocale_nid_disambig1_nid_postfix(int a0, const char* locale) {
    return setlocale_nid_postfix(a0, locale);
}

int APS5_VABI setvbuf_nid_disambig1_nid_postfix(FileStream* stream, char* buffer, int mode, std::size_t size) {
    return setvbuf_nid_postfix(stream, buffer, mode, size);
}

double APS5_VABI sin_nid_disambig1_nid_postfix(double x) {
    return sin_nid_postfix(x);
}

void APS5_VABI sincos_nid_disambig1_nid_postfix(double x, double* sinp, double* cosp) {
    sincos_nid_postfix(x, sinp, cosp);
}

void APS5_VABI sincosf_nid_disambig1_nid_postfix(float x, float* sinp, float* cosp) {
    sincosf_nid_postfix(x, sinp, cosp);
}

float APS5_VABI sinf_nid_disambig1_nid_postfix(float x) {
    return sinf_nid_postfix(x);
}

double APS5_VABI sinh_nid_disambig1_nid_postfix(double x) {
    return sinh_nid_postfix(x);
}

float APS5_VABI sinhf_nid_disambig1_nid_postfix(float x) {
    return sinhf_nid_postfix(x);
}

int APS5_VABI snprintf_nid_disambig1_nid_postfix(char* buffer, size_t size, const char* format, std::uint64_t v0, std::uint64_t v1) {
    return snprintf_nid_postfix(buffer, size, format, v0, v1);
}

int APS5_VABI snprintf_s_nid_disambig1_nid_postfix(char* buffer, size_t size, const char* format, std::uint64_t v0, std::uint64_t v1) {
    return snprintf_s_nid_postfix(buffer, size, format, v0, v1);
}

int APS5_VABI sprintf_nid_disambig1_nid_postfix(char* buffer, const char* format, std::uint64_t v0, std::uint64_t v1) {
    return sprintf_nid_postfix(buffer, format, v0, v1);
}

int APS5_VABI sprintf_s_nid_disambig1_nid_postfix(char* buffer, size_t size, const char* format, std::uint64_t v0, std::uint64_t v1) {
    return sprintf_s_nid_postfix(buffer, size, format, v0, v1);
}



void APS5_VABI srand_nid_disambig1_nid_postfix(unsigned int seed) {
    srand_nid_postfix(seed);
}

int APS5_VABI sscanf_nid_disambig1_nid_postfix(const char* input, const char* format, std::uint64_t v0, std::uint64_t v1) {
    return sscanf_nid_postfix(input, format, v0, v1);
}

int APS5_VABI sscanf_s_nid_disambig1_nid_postfix(const char* buffer, const char* format, std::uint64_t v0, std::uint64_t v1) {
    return sscanf_s_nid_postfix(buffer, format, v0, v1);
}

int APS5_VABI strcasecmp_nid_disambig1_nid_postfix(const char* s1, const char* s2) {
    return strcasecmp_nid_postfix(s1, s2);
}

char* APS5_VABI strcat_nid_disambig1_nid_postfix(char* dest, const char* src) {
    return strcat_nid_postfix(dest, src);
}

const char* APS5_VABI strchr_nid_disambig1_nid_postfix(const char* s, int c) {
    return strchr_nid_postfix(s, c);
}

int APS5_VABI strcmp_nid_disambig1_nid_postfix(const char* s1, const char* s2) {
    return strcmp_nid_postfix(s1, s2);
}

char* APS5_VABI strcpy_nid_disambig1_nid_postfix(char* dest, const char* src) {
    return strcpy_nid_postfix(dest, src);
}

int APS5_VABI strcpy_s_nid_disambig1_nid_postfix(char* dest, size_t destsz, const char* src) {
    return strcpy_s_nid_postfix(dest, destsz, src);
}

std::size_t APS5_VABI strcspn_nid_disambig1_nid_postfix(const char* text, const char* reject) {
    return strcspn_nid_postfix(text, reject);
}

int APS5_VABI strerror_r_nid_disambig1_nid_postfix(int error, char* buffer, std::size_t length) {
    return strerror_r_nid_postfix(error, buffer, length);
}

size_t APS5_VABI strftime_nid_disambig1_nid_postfix(char* str, size_t count, const char* format, const GuestTm* timeptr) {
    return strftime_nid_postfix(str, count, format, timeptr);
}

std::size_t APS5_VABI strlcat_nid_disambig1_nid_postfix(char* destination, const char* source, std::size_t capacity) {
    return strlcat_nid_postfix(destination, source, capacity);
}

size_t APS5_VABI strlcpy_nid_disambig1_nid_postfix(char* dest, const char* src, size_t size) {
    return strlcpy_nid_postfix(dest, src, size);
}

size_t APS5_VABI strlen_nid_disambig1_nid_postfix(const char* s) {
    return strlen_nid_postfix(s);
}

int APS5_VABI strncasecmp_nid_disambig1_nid_postfix(const char* s1, const char* s2, size_t n) {
    return strncasecmp_nid_postfix(s1, s2, n);
}

char* APS5_VABI strncat_nid_disambig1_nid_postfix(char* destination, const char* source, std::size_t limit) {
    return strncat_nid_postfix(destination, source, limit);
}

int APS5_VABI strncat_s_nid_disambig1_nid_postfix(char* dest, size_t destsz, const char* src, size_t count) {
    return strncat_s_nid_postfix(dest, destsz, src, count);
}

int APS5_VABI strncmp_nid_disambig1_nid_postfix(const char* s1, const char* s2, size_t n) {
    return strncmp_nid_postfix(s1, s2, n);
}

char* APS5_VABI strncpy_nid_disambig1_nid_postfix(char* dest, const char* src, size_t count) {
    return strncpy_nid_postfix(dest, src, count);
}

int APS5_VABI strncpy_s_nid_disambig1_nid_postfix(char* dest, size_t destsz, const char* src, size_t count) {
    return strncpy_s_nid_postfix(dest, destsz, src, count);
}

std::size_t APS5_VABI strnlen_nid_disambig1_nid_postfix(const char* text, std::size_t limit) {
    return strnlen_nid_postfix(text, limit);
}

std::size_t APS5_VABI strnlen_s_nid_disambig1_nid_postfix(const char* text, std::size_t limit) {
    return strnlen_s_nid_postfix(text, limit);
}

char* APS5_VABI strpbrk_nid_disambig1_nid_postfix(const char* text, const char* accept) {
    return strpbrk_nid_postfix(text, accept);
}

char* APS5_VABI strrchr_nid_disambig1_nid_postfix(const char* s, int c) {
    return strrchr_nid_postfix(s, c);
}

char* APS5_VABI strsep_nid_disambig1_nid_postfix(char** text, const char* delimiters) {
    return strsep_nid_postfix(text, delimiters);
}

size_t APS5_VABI strspn_nid_disambig1_nid_postfix(const char* s, const char* accept) {
    return strspn_nid_postfix(s, accept);
}

char* APS5_VABI strstr_nid_disambig1_nid_postfix(const char* haystack, const char* needle) {
    return strstr_nid_postfix(haystack, needle);
}

double APS5_VABI strtod_nid_disambig1_nid_postfix(const char* str, char** endptr) {
    return strtod_nid_postfix(str, endptr);
}

float APS5_VABI strtof_nid_disambig1_nid_postfix(const char* str, char** endptr) {
    return strtof_nid_postfix(str, endptr);
}

char* APS5_VABI strtok_nid_disambig1_nid_postfix(char* text, const char* delimiters) {
    return strtok_nid_postfix(text, delimiters);
}

char* APS5_VABI strtok_r_nid_disambig1_nid_postfix(char* text, const char* delimiters, char** state) {
    return strtok_r_nid_postfix(text, delimiters, state);
}

std::int64_t APS5_VABI strtol_nid_disambig1_nid_postfix(const char* str, char** endptr, int base) {
    return strtol_nid_postfix(str, endptr, base);
}

long long APS5_VABI strtoll_nid_disambig1_nid_postfix(const char* str, char** endptr, int base) {
    return strtoll_nid_postfix(str, endptr, base);
}

unsigned long long APS5_VABI strtoull_nid_disambig1_nid_postfix(const char* str, char** endptr, int base) {
    return strtoull_nid_postfix(str, endptr, base);
}

int APS5_VABI swprintf_nid_disambig1_nid_postfix(char16_t* buffer, std::size_t size, const char16_t* format, std::uint64_t v0, std::uint64_t v1) {
    return swprintf_nid_postfix(buffer, size, format, v0, v1);
}

double APS5_VABI tan_nid_disambig1_nid_postfix(double x) {
    return tan_nid_postfix(x);
}

float APS5_VABI tanf_nid_disambig1_nid_postfix(float x) {
    return tanf_nid_postfix(x);
}

double APS5_VABI tanh_nid_disambig1_nid_postfix(double x) {
    return tanh_nid_postfix(x);
}

float APS5_VABI tanhf_nid_disambig1_nid_postfix(float x) {
    return tanhf_nid_postfix(x);
}

int64_t APS5_VABI time_nid_disambig1_nid_postfix(int64_t* timer) {
    return time_nid_postfix(timer);
}

int APS5_VABI ungetc_nid_disambig1_nid_postfix(int value, FileStream* stream) {
    return ungetc_nid_postfix(value, stream);
}

int APS5_VABI vsnprintf_nid_disambig1_nid_postfix(char* str, size_t size, const char* format, VaList* c) {
    return vsnprintf_nid_postfix(str, size, format, c);
}

int APS5_VABI vsnprintf_s_nid_disambig1_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    return vsnprintf_s_nid_postfix(buffer, size, format, args);
}

int APS5_VABI vsnwprintf_s_nid_disambig1_nid_postfix(char16_t* buffer, std::size_t size, const char16_t* format, VaList* args) {
    return vsnwprintf_s_nid_postfix(buffer, size, format, args);
}

int APS5_VABI vsprintf_nid_disambig1_nid_postfix(char* str, const char* format, VaList* args) {
    return vsprintf_nid_postfix(str, format, args);
}

int APS5_VABI vsprintf_s_nid_disambig1_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    return vsprintf_s_nid_postfix(buffer, size, format, args);
}

int APS5_VABI vswprintf_nid_disambig1_nid_postfix(char16_t* buffer, std::size_t size, const char16_t* format, VaList* args) {
    return vswprintf_nid_postfix(buffer, size, format, args);
}

char16_t* APS5_VABI wcscat_nid_disambig1_nid_postfix(char16_t* dest, const char16_t* src) {
    return wcscat_nid_postfix(dest, src);
}

const char16_t* APS5_VABI wcschr_nid_disambig1_nid_postfix(const char16_t* s, char16_t c) {
    return wcschr_nid_postfix(s, c);
}

int APS5_VABI wcscmp_nid_disambig1_nid_postfix(const char16_t* s1, const char16_t* s2) {
    return wcscmp_nid_postfix(s1, s2);
}

char16_t* APS5_VABI wcscpy_nid_disambig1_nid_postfix(char16_t* dest, const char16_t* src) {
    return wcscpy_nid_postfix(dest, src);
}

int APS5_VABI wcscpy_s_nid_disambig1_nid_postfix(std::uint16_t* dest, std::size_t size, const std::uint16_t* src) {
    return wcscpy_s_nid_postfix(dest, size, src);
}

size_t APS5_VABI wcslen_nid_disambig1_nid_postfix(const char16_t* s) {
    return wcslen_nid_postfix(s);
}

int APS5_VABI wcsncat_s_nid_disambig1_nid_postfix(char16_t* dest, size_t destsz, const char16_t* src, size_t count) {
    return wcsncat_s_nid_postfix(dest, destsz, src, count);
}

int APS5_VABI wcsncmp_nid_disambig1_nid_postfix(const char16_t* s1, const char16_t* s2, size_t n) {
    return wcsncmp_nid_postfix(s1, s2, n);
}

char16_t* APS5_VABI wcsncpy_nid_disambig1_nid_postfix(char16_t* dest, const char16_t* src, size_t n) {
    return wcsncpy_nid_postfix(dest, src, n);
}

int APS5_VABI wcsncpy_s_nid_disambig1_nid_postfix(char16_t* dest, size_t destsz, const char16_t* src, size_t count) {
    return wcsncpy_s_nid_postfix(dest, destsz, src, count);
}

size_t APS5_VABI wcsnlen_s_nid_disambig1_nid_postfix(const char16_t* s, size_t maxlen) {
    return wcsnlen_s_nid_postfix(s, maxlen);
}

const char16_t* APS5_VABI wcsrchr_nid_disambig1_nid_postfix(const char16_t* s, char16_t c) {
    return wcsrchr_nid_postfix(s, c);
}

const char16_t* APS5_VABI wcsstr_nid_disambig1_nid_postfix(const char16_t* haystack, const char16_t* needle) {
    return wcsstr_nid_postfix(haystack, needle);
}

double APS5_VABI wcstod_nid_disambig1_nid_postfix(const char16_t* str, char16_t** endptr) {
    return wcstod_nid_postfix(str, endptr);
}

float APS5_VABI wcstof_nid_disambig1_nid_postfix(const char16_t* str, char16_t** endptr) {
    return wcstof_nid_postfix(str, endptr);
}

long long APS5_VABI wcstol_nid_disambig1_nid_postfix(const char16_t* str, char16_t** endptr, int base) {
    return wcstol_nid_postfix(str, endptr, base);
}

long long APS5_VABI wcstoll_nid_disambig1_nid_postfix(const char16_t* str, char16_t** endptr, int base) {
    return wcstoll_nid_postfix(str, endptr, base);
}

std::size_t APS5_VABI wcstombs_nid_disambig1_nid_postfix(char* destination, const std::uint16_t* source, std::size_t capacity) {
    return wcstombs_nid_postfix(destination, source, capacity);
}

unsigned long long APS5_VABI wcstoull_nid_disambig1_nid_postfix(const char16_t* str, char16_t** endptr, int base) {
    return wcstoull_nid_postfix(str, endptr, base);
}

const char16_t* APS5_VABI wmemchr_nid_disambig1_nid_postfix(const char16_t* s, char16_t c, size_t n) {
    return wmemchr_nid_postfix(s, c, n);
}

int APS5_VABI wmemcmp_nid_disambig1_nid_postfix(const char16_t* s1, const char16_t* s2, size_t n) {
    return wmemcmp_nid_postfix(s1, s2, n);
}

char16_t* APS5_VABI wmemcpy_nid_disambig1_nid_postfix(char16_t* dest, const char16_t* src, size_t n) {
    return wmemcpy_nid_postfix(dest, src, n);
}

char16_t* APS5_VABI wmemmove_nid_disambig1_nid_postfix(char16_t* dest, const char16_t* src, size_t n) {
    return wmemmove_nid_postfix(dest, src, n);
}

char16_t* APS5_VABI wmemset_nid_disambig1_nid_postfix(char16_t* s, char16_t c, size_t n) {
    return wmemset_nid_postfix(s, c, n);
}

void* APS5_VABI sceLibcMspaceCreate_nid_disambig1_nid_postfix(const char* name, void* base, std::size_t size, unsigned flags) {
    return sceLibcMspaceCreate_nid_postfix(name, base, size, flags);
}

void* APS5_VABI __cxa_begin_catch_nid_disambig1_nid_postfix(void* a0) {
    return __cxa_begin_catch_nid_postfix(a0);
}

void* APS5_VABI bsearch_nid_disambig1_nid_postfix(const void* key, const void* base, size_t count, size_t size, int (APS5_VABI *compare)(const void*, const void*)) {
    return bsearch_nid_postfix(key, base, count, size, compare);
}

_Unwind_Reason_Code APS5_VABI __gxx_personality_v0_nid_disambig1_nid_postfix(int version, _Unwind_Action actions, std::uint64_t exceptionClass, _Unwind_Exception* exception, _Unwind_Context* context) {
    return __gxx_personality_v0_nid_postfix(version, actions, exceptionClass, exception, context);
}

void APS5_VABI _Unwind_Resume_nid_disambig1_nid_postfix(_Unwind_Exception* a0) {
    _Unwind_Resume_nid_postfix(a0);
}

GuestTm* APS5_VABI localtime_s_nid_disambig1_nid_postfix(const int64_t* timer, GuestTm* result) {
    return localtime_s_nid_postfix(timer, result);
}



void* APS5_VABI __dynamic_cast_nid_disambig1_nid_postfix(const void* source, const __cxxabiv1::__class_type_info* sourceType, const __cxxabiv1::__class_type_info* destinationType, std::ptrdiff_t a3) {
    return __dynamic_cast_nid_postfix(source, sourceType, destinationType, a3);
}

void APS5_VABI __cxa_free_exception_nid_disambig1_nid_postfix(void* a0) {
    __cxa_free_exception_nid_postfix(a0);
}

int APS5_VABI sceLibcMspacePosixMemalign_nid_disambig1_nid_postfix(void* handle, void** result, std::size_t alignment, std::size_t size) {
    return sceLibcMspacePosixMemalign_nid_postfix(handle, result, alignment, size);
}

}
