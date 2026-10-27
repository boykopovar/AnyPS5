#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };
static_assert(sizeof(LibcException::ExceptionObject) == 16 && sizeof(LibcException::SystemErrorObject) == 32);

using GuestWhat = const char* (APS5_VABI *)(const LibcException::ExceptionObject*);
using Thrower = void (APS5_VABI *)(const void*);

extern "C" {
extern const TypeRecord _ZTISt9exception_nid_postfix;
extern const TypeRecord _ZTISt9bad_alloc_nid_postfix;
extern const TypeRecord _ZTISt12length_error_nid_postfix;
extern const TypeRecord _ZTISt12system_error_nid_postfix;
extern const TypeRecord _ZTINSt13__sce_cxxvers16_Error_exceptionE_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
const void* SceV2Caught = nullptr;
}

static unsigned failures = 0;

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

static const TypeRecord* TypeOf(const LibcException::ExceptionObject* object) {
    return static_cast<const TypeRecord*>(static_cast<const void* const*>(object->vtable)[-1]);
}

static const char* What(const LibcException::ExceptionObject* object) {
    return reinterpret_cast<GuestWhat>(const_cast<void*>(static_cast<const void* const*>(object->vtable)[2]))(object);
}

static TypeRecord Named(const char* name) {
    return {_ZTISt9exception_nid_postfix.vtable, name, nullptr};
}

static char inspected[256];
static std::size_t inspectBytes = sizeof(LibcException::ExceptionObject);
static LibcException::SystemErrorObject inspectedSystem {};

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* exception = static_cast<const LibcException::ExceptionObject*>(object);
    std::snprintf(inspected, sizeof(inspected), "%s|%s", TypeOf(exception)->name, What(exception));
    std::memcpy(&inspectedSystem, object, inspectBytes);
}

struct Case { const char* label; Thrower thrower; const void* argument; const void* catchType; const void* otherType; int selector; const char* expected; };

static void Run(const Case& test) {
    SceV2CatchType = test.catchType;
    SceV2OtherType = test.otherType;
    SceV2Selector = 0;
    inspected[0] = 0;
    SceV2Catch(test.thrower, test.argument);
    Require(SceV2Selector == test.selector, test.label);
    if (std::strcmp(inspected, test.expected) != 0) {
        std::fprintf(stderr, "  %s: got '%s', want '%s'\n", test.label, inspected, test.expected);
        ++failures;
    }
}

template<class TFunction>
static Thrower AsThrower(TFunction function) {
    return reinterpret_cast<Thrower>(function);
}

static void CheckThrowers() {
    const TypeRecord logicError = Named("NSt8__sce_v211logic_errorE");
    const TypeRecord runtimeError = Named("NSt8__sce_v213runtime_errorE");
    const TypeRecord lengthError = Named("NSt8__sce_v212length_errorE");
    const TypeRecord systemError = Named("NSt8__sce_v212system_errorE");
    const TypeRecord unrelated = Named("NSt8__sce_v211range_errorE");
    const void* exception = &_ZTISt9exception_nid_postfix;
    const void* errorException = &_ZTINSt13__sce_cxxvers16_Error_exceptionE_nid_postfix;
    const Case cases[] {
        {"_Xlength_error", AsThrower(_ZNSt8__sce_v214_Xlength_errorEPKc_nid_postfix), "length", &logicError, &unrelated, 2,
            "NSt8__sce_v212length_errorE|length"},
        {"_Xlength_error is not std::length_error", AsThrower(_ZNSt8__sce_v214_Xlength_errorEPKc_nid_postfix), "length", &lengthError, &_ZTISt12length_error_nid_postfix, 2,
            "NSt8__sce_v212length_errorE|length"},
        {"_Xout_of_range", AsThrower(_ZNSt8__sce_v214_Xout_of_rangeEPKc_nid_postfix), "range", errorException, &runtimeError, 2,
            "NSt8__sce_v212out_of_rangeE|range"},
        {"_Xinvalid_argument", AsThrower(_ZNSt8__sce_v218_Xinvalid_argumentEPKc_nid_postfix), "argument", &logicError, &runtimeError, 2,
            "NSt8__sce_v216invalid_argumentE|argument"},
        {"_Xruntime_error", AsThrower(_ZNSt8__sce_v215_Xruntime_errorEPKc_nid_postfix), "runtime", &runtimeError, &logicError, 2,
            "NSt8__sce_v213runtime_errorE|runtime"},
        {"_Xoverflow_error", AsThrower(_ZNSt8__sce_v216_Xoverflow_errorEPKc_nid_postfix), nullptr, exception, &logicError, 2,
            "NSt8__sce_v214overflow_errorE|"},
        {"_Xbad_alloc", AsThrower(_ZNSt8__sce_v211_Xbad_allocEv_nid_postfix), nullptr, &_ZTISt9bad_alloc_nid_postfix, errorException, 2,
            "St9bad_alloc|std::bad_alloc"},
        {"_Xbad_function_call", AsThrower(_ZNSt8__sce_v219_Xbad_function_callEv_nid_postfix), nullptr, exception, errorException, 2,
            "NSt8__sce_v217bad_function_callE|bad function call"},
        {"_Throw(bad_optional_access)", AsThrower(_ZNSt8__sce_v26_ThrowERKNS_19bad_optional_accessE_nid_postfix), nullptr, exception, errorException, 2,
            "NSt8__sce_v219bad_optional_accessE|Bad optional access"},
        {"_Throw(bad_weak_ptr)", AsThrower(_ZNSt8__sce_v26_ThrowERKNS_12bad_weak_ptrE_nid_postfix), nullptr, exception, errorException, 2,
            "NSt8__sce_v212bad_weak_ptrE|bad_weak_ptr"},
        {"_Throw(bad_variant_access)", AsThrower(_ZNSt8__sce_v26_ThrowERKNS_18bad_variant_accessE_nid_postfix), nullptr, exception, errorException, 2,
            "NSt8__sce_v218bad_variant_accessE|bad variant access"},
        {"_Throw_Cpp_error", AsThrower(_ZNSt8__sce_v216_Throw_Cpp_errorEi_nid_postfix), reinterpret_cast<const void*>(1), &systemError, &_ZTISt12system_error_nid_postfix, 2,
            "NSt8__sce_v212system_errorE|invalid argument: invalid argument"},
    };
    inspectBytes = sizeof(void*);
    for (const auto& test : cases) Run(test);
    inspectBytes = sizeof(LibcException::ExceptionObject);
}

static void CheckCopies() {
    const TypeRecord runtimeError = Named("NSt8__sce_v213runtime_errorE");
    const TypeRecord systemError = Named("NSt8__sce_v212system_errorE");
    const TypeRecord logicError = Named("NSt8__sce_v211logic_errorE");

    LibcException::ExceptionObject range {};
    _ZNSt13__sce_cxxvers16_Error_exceptionC2EPKc_nid_postfix(&range, "range copy");
    Run({"_Throw(range_error)", AsThrower(_ZNSt8__sce_v26_ThrowERKNS_11range_errorE_nid_postfix), &range, &runtimeError, &logicError, 2,
        "NSt8__sce_v211range_errorE|range copy"});
    Require(inspectedSystem.base.message == range.message, "_Throw(range_error) shares the message");
    _ZNSt13__sce_cxxvers16_Error_exceptionD2Ev_nid_postfix(&range);

    LibcException::SystemErrorObject error {};
    _ZNSt13__sce_cxxvers16_Error_exceptionC2EPKc_nid_postfix(&error.base, "system copy");
    error.code = {0, 5, _ZSt16generic_categoryv_nid_postfix()};
    inspectBytes = sizeof(LibcException::SystemErrorObject);
    SceV2CatchType = &runtimeError;
    SceV2OtherType = &logicError;
    SceV2Catch(AsThrower(_ZNSt8__sce_v26_ThrowERKNS_12system_errorE_nid_postfix), &error);
    Require(std::strcmp(inspected, "NSt8__sce_v212system_errorE|system copy") == 0, "_Throw(system_error) type and message");
    Require(inspectedSystem.code.value == 5, "_Throw(system_error) copies the code value");
    Require(inspectedSystem.code.category == error.code.category, "_Throw(system_error) copies the category");

    SceV2CatchType = &systemError;
    SceV2Catch(AsThrower(_ZNSt8__sce_v28ios_base6_ThrowERKNS0_7failureE_nid_postfix), &error);
    Require(std::strcmp(inspected, "NSt8__sce_v28ios_base7failureE|system copy") == 0, "ios_base::_Throw(failure) type and message");
    Require(inspectedSystem.code.value == 5, "ios_base::_Throw(failure) copies the code");
    inspectBytes = sizeof(LibcException::ExceptionObject);
    _ZNSt13__sce_cxxvers16_Error_exceptionD2Ev_nid_postfix(&error.base);
}

static void CheckErrorException() {
    LibcException::ExceptionObject empty {};
    _ZNSt13__sce_cxxvers16_Error_exceptionC1EPKc_nid_postfix(&empty, nullptr);
    Require(std::strcmp(_ZNKSt13__sce_cxxvers16_Error_exception4whatEv_nid_postfix(&empty), "") == 0, "_Error_exception(nullptr) has an empty message");
    Require(std::strcmp(TypeOf(&empty)->name, "NSt13__sce_cxxvers16_Error_exceptionE") == 0, "_Error_exception type name");
    Require(TypeOf(&empty)->base == &_ZTISt9exception_nid_postfix, "_Error_exception derives from std::exception");

    LibcException::ExceptionObject text {};
    _ZNSt13__sce_cxxvers16_Error_exceptionC2EPKc_nid_postfix(&text, "text");
    LibcException::ExceptionObject copy {};
    _ZNSt13__sce_cxxvers16_Error_exceptionC1ERKS0__nid_postfix(&copy, &text);
    Require(std::strcmp(_ZNKSt13__sce_cxxvers16_Error_exception4whatEv_nid_postfix(&copy), "text") == 0, "_Error_exception copy keeps the message");
    _ZNSt13__sce_cxxvers16_Error_exceptionD2Ev_nid_postfix(&text);
    Require(std::strcmp(_ZNKSt13__sce_cxxvers16_Error_exception4whatEv_nid_postfix(&copy), "text") == 0, "_Error_exception copy outlives the source");
    _ZNSt13__sce_cxxvers16_Error_exceptionD1Ev_nid_postfix(&copy);
    _ZNSt13__sce_cxxvers16_Error_exceptionD1Ev_nid_postfix(&empty);
}

static void CheckSyserrorMap() {
    Require(std::strcmp(_ZNSt8__sce_v213_Syserror_mapEi_nid_postfix(0), "success") == 0, "_Syserror_map(0)");
    Require(_ZSt13_Syserror_mapi_nid_postfix(0) == nullptr, "unversioned _Syserror_map(0) stays null");
    Require(std::strcmp(_ZNSt8__sce_v213_Syserror_mapEi_nid_postfix(2), _ZSt13_Syserror_mapi_nid_postfix(2)) == 0, "_Syserror_map(2) matches the unversioned text");
    Require(std::strcmp(_ZNSt8__sce_v213_Syserror_mapEi_nid_postfix(99999), "unknown error") == 0, "_Syserror_map(unknown)");
    Require(_ZSt13_Syserror_mapi_nid_postfix(99999) == nullptr, "unversioned _Syserror_map(unknown) stays null");
}

static void CheckTicks() {
    const auto before = _Xtime_get_ticks_nid_postfix();
    const auto ticks = _ZNSt8__sce_v216_Xtime_get_ticksEv_nid_postfix();
    const auto after = _Xtime_get_ticks_nid_postfix();
    Require(before <= ticks && ticks <= after, "_Xtime_get_ticks is the unversioned clock");
}

int main() {
    CheckErrorException();
    CheckThrowers();
    CheckCopies();
    CheckSyserrorMap();
    CheckTicks();
    if (failures) return 1;
    std::puts("guest std::__sce_v2 exceptions: PASS");
    return 0;
}
