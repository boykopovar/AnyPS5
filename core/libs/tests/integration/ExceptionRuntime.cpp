#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unwind.h>

extern "C" unsigned __cxa_uncaught_exceptions_nid_postfix();
extern "C" void* __cxa_current_primary_exception_nid_postfix();
extern "C" void __cxa_decrement_exception_refcount_nid_postfix(void*);
extern "C" void __cxa_rethrow_primary_exception_nid_postfix(void*);

extern "C" [[noreturn]] void _ZSt14_Xout_of_rangePKc_nid_postfix(const char*);
extern "C" [[noreturn]] void _ZSt13_Xrange_errorPKc_nid_postfix(const char*);
extern "C" [[noreturn]] void _ZNSt8__sce_v219_Xbad_function_callEv_nid_postfix();
extern "C" [[noreturn]] void __cxa_bad_cast_nid_postfix();
extern "C" [[noreturn]] void _ZSt19_Throw_bad_weak_ptrv_nid_postfix();
extern "C" [[noreturn]] void _ZNKSt9exception6_RaiseEv_nid_postfix(const void*);
extern "C" void* __cxa_vec_new3_nid_postfix(std::size_t, std::size_t, std::size_t, void(*)(void*), void(*)(void*), void*(*)(std::size_t), void(*)(void*, std::size_t));
extern "C" void __cxa_vec_delete3_nid_postfix(void*, std::size_t, std::size_t, void(*)(void*), void(*)(void*, std::size_t));
extern "C" _Unwind_Reason_Code _Unwind_Backtrace_nid_postfix(_Unwind_Trace_Fn, void*);
extern "C" _Unwind_Reason_Code _Unwind_ForcedUnwind_nid_postfix(_Unwind_Exception*, _Unwind_Stop_Fn, void*);
extern "C" std::uintptr_t _Unwind_GetIP_nid_postfix(_Unwind_Context*);
extern "C" void (*_ZSt13set_terminatePFvvE_nid_postfix(void(*)()))();

namespace ExceptionRuntimeTypes {

struct Base { virtual ~Base() = default; int value = 7; };
struct Other { virtual ~Other() = default; int padding = 9; };
struct Derived : Other, Base {};
struct Virtual : virtual Base {};
struct Left : Base {};
struct Right : Base {};
struct Repeated : Left, Right {};
struct LeftVirtual : virtual Base {};
struct RightVirtual : virtual Base {};
struct Diamond : LeftVirtual, RightVirtual {};
struct PrivateDerived : private Derived {
    Base* Source() { return this; }
    Derived* Target() { return this; }
};

} // namespace ExceptionRuntimeTypes

namespace {

using namespace ExceptionRuntimeTypes;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

thread_local int destroyed = 0;
thread_local int guardsWithoutUncaught = 0;

struct Guard {
    ~Guard() {
        if (__cxa_uncaught_exceptions_nid_postfix() == 0) ++guardsWithoutUncaught;
        ++destroyed;
    }
};

class GuardCounters {
public:
    GuardCounters() { Reset(); }
    ~GuardCounters() { Reset(); }
    GuardCounters(const GuardCounters&) = delete;
    GuardCounters& operator=(const GuardCounters&) = delete;

private:
    static void Reset() {
        destroyed = 0;
        guardsWithoutUncaught = 0;
    }
};

[[gnu::noinline]] void ThrowInt() { Guard guard; throw 42; }
[[gnu::noinline]] void ThrowClass() { Guard guard; throw Derived(); }
[[gnu::noinline]] Derived* ToDerived(Base* pointer) { return dynamic_cast<Derived*>(pointer); }
[[gnu::noinline]] Repeated* ToRepeated(Base* pointer) { return dynamic_cast<Repeated*>(pointer); }

template<typename TError, typename TOperation>
bool ThrowsType(const TOperation& operation) {
    try {
        operation();
    } catch (const TError&) {
        return true;
    }
    return false;
}

template<typename TError, typename TOperation>
std::string MessageOf(const TOperation& operation, const char* description) {
    std::string message;
    bool caught = false;
    try {
        operation();
    } catch (const TError& error) {
        caught = true;
        message = error.what();
    }
    Require(caught, std::string(description) + " throws the expected type");
    return message;
}

int foreignCleanups = 0;
int foreignWrongReasons = 0;

int staticAttempts = 0;
[[gnu::noinline]] int StaticValue() {
    static int value = [] { if (++staticAttempts == 1) throw 91; return 37; }();
    return value;
}

int arrayConstructed = 0;
int arrayDestroyed = 0;
int arrayFreed = 0;
int arrayDestroyOrderErrors = 0;
int arrayFreeSizeErrors = 0;
bool failArray = false;

class ArrayCounters {
public:
    explicit ArrayCounters(bool fail) {
        Reset();
        failArray = fail;
    }
    ~ArrayCounters() { Reset(); }
    ArrayCounters(const ArrayCounters&) = delete;
    ArrayCounters& operator=(const ArrayCounters&) = delete;

private:
    static void Reset() {
        arrayConstructed = arrayDestroyed = arrayFreed = arrayDestroyOrderErrors = arrayFreeSizeErrors = 0;
        failArray = false;
    }
};

void ArrayConstruct(void* pointer) {
    if (failArray && arrayConstructed == 2) throw 73;
    *static_cast<int*>(pointer) = arrayConstructed++;
}

void ArrayDestroy(void* pointer) {
    if (*static_cast<int*>(pointer) != arrayConstructed - 1 - arrayDestroyed) ++arrayDestroyOrderErrors;
    ++arrayDestroyed;
}

void ArrayFree(void* pointer, std::size_t size) {
    if (size != 4 * sizeof(int) + sizeof(std::size_t)) ++arrayFreeSizeErrors;
    ++arrayFreed;
    std::free(pointer);
}

struct ThreadOutcome {
    int wrongValues = 0;
    int uncaughtAfterCatch = 0;
    int guardsWithoutUncaught = 0;
    int destroyed = 0;
};

void* ThreadTest(void* argument) {
    auto& outcome = *static_cast<ThreadOutcome*>(argument);
    destroyed = 0;
    guardsWithoutUncaught = 0;
    for (int index = 0; index < 100; ++index) {
        int caught = 0;
        try { ThrowInt(); } catch (int value) { caught = value; }
        if (caught != 42) ++outcome.wrongValues;
        if (__cxa_uncaught_exceptions_nid_postfix() != 0) ++outcome.uncaughtAfterCatch;
    }
    outcome.destroyed = destroyed;
    outcome.guardsWithoutUncaught = guardsWithoutUncaught;
    return nullptr;
}

[[gnu::noinline]] void UncaughtThrow() { throw 19; }
[[gnu::noinline]] void NoexceptThrow() noexcept { UncaughtThrow(); }

void* RunFunction(void* argument) {
    (*static_cast<void (**)()>(argument))();
    return nullptr;
}

[[noreturn]] void TerminateOnHandlerFreeThread(void (*function)()) {
    _ZSt13set_terminatePFvvE_nid_postfix([] { _exit(61); });
    pthread_t thread;
    if (pthread_create(&thread, nullptr, RunFunction, &function) != 0) _exit(66);
    pthread_join(thread, nullptr);
    _exit(62);
}

[[noreturn]] void UncaughtChild() { TerminateOnHandlerFreeThread(UncaughtThrow); }
[[noreturn]] void NoexceptChild() { TerminateOnHandlerFreeThread(NoexceptThrow); }

struct ForcedGuard { ~ForcedGuard() { ++destroyed; } };

[[gnu::noinline]] void ForceUnwind() {
    ForcedGuard guard;
    auto* exception = new _Unwind_Exception {};
    _Unwind_ForcedUnwind_nid_postfix(exception,
        [](int, _Unwind_Action actions, std::uint64_t, _Unwind_Exception*, _Unwind_Context*, void*) {
            if (actions & _UA_END_OF_STACK) _exit(destroyed == 1 ? 63 : 64);
            return _URC_NO_REASON;
        }, nullptr);
    _exit(65);
}

[[noreturn]] void ForcedUnwindChild() {
    destroyed = 0;
    ForceUnwind();
    _exit(67);
}

int ChildExitStatus(void (*body)()) {
    const pid_t child = fork();
    Require(child >= 0, "fork the child");
    if (child == 0) {
        body();
        _exit(68);
    }
    int status = 0;
    RequireEqual(waitpid(child, &status, 0), child, "wait for the child");
    Require(WIFEXITED(status), "child exited normally");
    return WEXITSTATUS(status);
}

struct BacktraceOutcome {
    int frames = 0;
    int zeroIps = 0;
};

const Case throwInt{"Throw_IntThroughCleanupFrame_CaughtAndGuardDestroyedDuringUnwind", [] {
    const GuardCounters counters;
    bool returned = false;
    int caught = 0;
    try { ThrowInt(); returned = true; } catch (int value) { caught = value; }
    Require(!returned, "ThrowInt threw");
    RequireEqual(caught, 42, "caught value");
    RequireEqual(destroyed, 1, "guards destroyed");
    RequireEqual(guardsWithoutUncaught, 0, "guards destroyed without an uncaught exception");
}};

const Case throwClass{"Throw_DerivedThroughCleanupFrame_CaughtAsBaseReference", [] {
    const GuardCounters counters;
    bool returned = false;
    int caught = 0;
    try { ThrowClass(); returned = true; } catch (const Base& value) { caught = value.value; }
    Require(!returned, "ThrowClass threw");
    RequireEqual(caught, 7, "caught base value");
    RequireEqual(destroyed, 1, "guards destroyed");
    RequireEqual(guardsWithoutUncaught, 0, "guards destroyed without an uncaught exception");
}};

const Case rethrow{"Rethrow_InsideHandler_ReachesOuterHandlerAndClearsUncaught", [] {
    int inner = 0;
    int outer = 0;
    try {
        try { throw 13; } catch (int value) { inner = value; throw; }
    } catch (int value) { outer = value; }
    RequireEqual(inner, 13, "inner handler value");
    RequireEqual(outer, 13, "outer handler value");
    RequireEqual(__cxa_uncaught_exceptions_nid_postfix(), 0u, "uncaught exceptions");
}};

const Case rethrowPrimary{"RethrowPrimaryException_RetainedException_RethrowsOriginalValue", [] {
    void* retained = nullptr;
    try { throw 27; } catch (...) { retained = __cxa_current_primary_exception_nid_postfix(); }
    Require(retained != nullptr, "primary exception retained");
    bool returned = false;
    int caught = 0;
    try { __cxa_rethrow_primary_exception_nid_postfix(retained); returned = true; } catch (int value) { caught = value; }
    __cxa_decrement_exception_refcount_nid_postfix(retained);
    Require(!returned, "rethrow threw");
    RequireEqual(caught, 27, "rethrown value");
}};

const Case throwPointer{"Throw_DerivedPointer_CaughtAsAdjustedBasePointer", [] {
    Derived object;
    Base* pointer = &object;
    Base* caught = nullptr;
    try { throw &object; } catch (Base* value) { caught = value; }
    Require(caught == pointer, "caught pointer is the base subobject");
}};

const Case downcast{"DynamicCast_BaseToDerived_ReturnsObject", [] {
    Derived object;
    Base* pointer = &object;
    Require(dynamic_cast<Derived*>(pointer) == &object, "downcast result");
}};

const Case crossCast{"DynamicCast_BaseToSiblingBase_ReturnsSiblingSubobject", [] {
    Derived object;
    Base* pointer = &object;
    Require(dynamic_cast<Other*>(pointer) == static_cast<Other*>(&object), "cross cast result");
}};

const Case virtualBase{"Throw_ClassWithVirtualBase_CaughtAsBaseReference", [] {
    int caught = 0;
    try { throw Virtual(); } catch (Base& value) { caught = value.value; }
    RequireEqual(caught, 7, "caught base value");
}};

const Case outOfRange{"XoutOfRange_Message_ThrowsLogicErrorWithMessage", [] {
    RequireEqual(MessageOf<std::logic_error>([] { _ZSt14_Xout_of_rangePKc_nid_postfix("test message"); }, "_Xout_of_range"),
        std::string("test message"), "what()");
}};

const Case rangeError{"XrangeError_Message_ThrowsRangeErrorWithMessage", [] {
    RequireEqual(MessageOf<std::range_error>([] { _ZSt13_Xrange_errorPKc_nid_postfix("range message"); }, "_Xrange_error"),
        std::string("range message"), "what()");
}};

const Case rangeErrorNull{"XrangeError_NullMessage_ThrowsRuntimeErrorWithEmptyMessage", [] {
    RequireEqual(MessageOf<std::runtime_error>([] { _ZSt13_Xrange_errorPKc_nid_postfix(nullptr); }, "_Xrange_error(nullptr)"),
        std::string(), "what()");
}};

const Case rangeErrorNotLogic{"XrangeError_Thrown_NotCaughtAsLogicError", [] {
    bool logic = false;
    bool range = false;
    try { _ZSt13_Xrange_errorPKc_nid_postfix("not a logic error"); }
    catch (const std::logic_error&) { logic = true; }
    catch (const std::range_error&) { range = true; }
    Require(!logic, "not caught as std::logic_error");
    Require(range, "caught as std::range_error");
}};

const Case badFunctionCall{"XbadFunctionCall_Called_ThrowsBadFunctionCall", [] {
    Require(ThrowsType<std::bad_function_call>([] { _ZNSt8__sce_v219_Xbad_function_callEv_nid_postfix(); }),
        "throws std::bad_function_call");
}};

const Case badCast{"CxaBadCast_Called_ThrowsStdExceptionWithMessage", [] {
    bool caught = false;
    bool hasMessage = false;
    try { __cxa_bad_cast_nid_postfix(); }
    catch (const std::exception& value) { caught = true; hasMessage = value.what() != nullptr; }
    Require(caught, "throws std::exception");
    Require(hasMessage, "what() is not null");
}};

const Case badWeakPtr{"ThrowBadWeakPtr_Called_ThrowsBadWeakPtrWithMessage", [] {
    RequireEqual(MessageOf<std::bad_weak_ptr>([] { _ZSt19_Throw_bad_weak_ptrv_nid_postfix(); }, "_Throw_bad_weak_ptr"),
        std::string("bad_weak_ptr"), "what()");
}};

const Case raiseNull{"ExceptionRaise_NullObject_ThrowsInvalidArgument", [] {
    Require(ThrowsType<std::invalid_argument>([] { _ZNKSt9exception6_RaiseEv_nid_postfix(nullptr); }),
        "throws std::invalid_argument");
}};

const Case raiseObject{"ExceptionRaise_Object_ThrowsStdException", [] {
    Derived object;
    bool caught = false;
    std::string message;
    try { _ZNKSt9exception6_RaiseEv_nid_postfix(&object); }
    catch (const std::exception& value) { caught = true; message = value.what(); }
    Require(caught, "throws std::exception");
    RequireEqual(message, std::string("std::exception"), "what()");
}};

const Case repeatedDowncast{"DynamicCast_RepeatedBaseThroughLeft_ReturnsMostDerived", [] {
    Repeated repeated;
    Base* left = static_cast<Left*>(&repeated);
    Require(ToRepeated(left) == &repeated, "downcast from the left base");
}};

const Case repeatedCrossCast{"DynamicCast_LeftBaseToRight_ReturnsRightSubobject", [] {
    Repeated repeated;
    Base* left = static_cast<Left*>(&repeated);
    Require(dynamic_cast<Right*>(left) == static_cast<Right*>(&repeated), "cross cast to the right base");
}};

const Case privateBase{"DynamicCast_BaseOfPrivateBase_ReturnsPrivateBaseObject", [] {
    PrivateDerived hidden;
    Require(ToDerived(hidden.Source()) == hidden.Target(), "downcast inside the private base");
}};

const Case ambiguousPointer{"Catch_NullPointerWithAmbiguousBase_SkipsBaseHandler", [] {
    int handler = 0;
    bool isNull = false;
    try { throw static_cast<Repeated*>(nullptr); }
    catch (Base*) { handler = 1; }
    catch (Repeated* pointer) { handler = 2; isNull = pointer == nullptr; }
    RequireEqual(handler, 2, "selected handler");
    Require(isNull, "caught pointer is null");
}};

const Case diamondPointer{"Catch_NullDiamondPointer_CaughtAsVirtualBasePointer", [] {
    bool caught = false;
    bool isNull = false;
    try { throw static_cast<Diamond*>(nullptr); }
    catch (Base* pointer) { caught = true; isNull = pointer == nullptr; }
    Require(caught, "caught as Base*");
    Require(isNull, "caught pointer is null");
}};

const Case nullPointer{"Catch_Nullptr_CaughtAsClassPointer", [] {
    bool caught = false;
    bool isNull = false;
    try { throw nullptr; }
    catch (Base* pointer) { caught = true; isNull = pointer == nullptr; }
    Require(caught, "caught as Base*");
    Require(isNull, "caught pointer is null");
}};

const Case pointerToPointer{"Catch_PointerToPointer_SkipsUnsafeConstConversionHandler", [] {
    int value = 1;
    int* pointer = &value;
    int handler = 0;
    bool same = false;
    try { throw &pointer; }
    catch (const int**) { handler = 1; }
    catch (int** caught) { handler = 2; same = caught == &pointer; }
    RequireEqual(handler, 2, "selected handler");
    Require(same, "caught pointer is the thrown pointer");
}};

const Case constPointerToPointer{"Catch_PointerToPointer_CaughtAsFullyConstQualified", [] {
    int value = 1;
    int* pointer = &value;
    bool caught = false;
    int pointee = 0;
    try { throw &pointer; }
    catch (const int* const* caughtPointer) { caught = true; pointee = **caughtPointer; }
    Require(caught, "caught as const int* const*");
    RequireEqual(pointee, value, "pointee");
}};

const Case foreign{"ForeignException_CaughtByCatchAll_HasNoPrimaryAndIsCleanedUpOnce", [] {
    foreignCleanups = 0;
    foreignWrongReasons = 0;
    auto* exception = new _Unwind_Exception {};
    exception->exception_class = 0x54455354464f5200;
    exception->exception_cleanup = [](_Unwind_Reason_Code reason, _Unwind_Exception* pointer) {
        if (reason != _URC_FOREIGN_EXCEPTION_CAUGHT) ++foreignWrongReasons;
        ++foreignCleanups;
        delete pointer;
    };
    bool returned = false;
    bool caughtInner = false;
    bool caughtOuter = false;
    bool primaryIsNull = false;
    try {
        try { _Unwind_RaiseException(exception); returned = true; }
        catch (...) { caughtInner = true; primaryIsNull = __cxa_current_primary_exception_nid_postfix() == nullptr; throw; }
    } catch (...) { caughtOuter = true; }
    Require(!returned, "raise did not return");
    Require(caughtInner, "caught by the inner catch-all");
    Require(primaryIsNull, "no primary exception for a foreign exception");
    Require(caughtOuter, "rethrown to the outer catch-all");
    RequireEqual(foreignCleanups, 1, "cleanup calls");
    RequireEqual(foreignWrongReasons, 0, "cleanup calls with a reason other than foreign caught");
}};

const Case staticInitialization{"StaticInitialization_FirstAttemptThrows_RetriedOnNextCall", [] {
    bool returned = false;
    int thrown = 0;
    try { StaticValue(); returned = true; } catch (int value) { thrown = value; }
    Require(!returned, "first initialization threw");
    RequireEqual(thrown, 91, "thrown value");
    RequireEqual(StaticValue(), 37, "second call value");
    RequireEqual(StaticValue(), 37, "third call value");
    RequireEqual(staticAttempts, 2, "initializer attempts");
}};

const Case arrayFailure{"VecNew3_ConstructorThrows_DestroysConstructedElementsAndFrees", [] {
    const ArrayCounters counters(true);
    bool returned = false;
    int thrown = 0;
    try {
        __cxa_vec_new3_nid_postfix(4, sizeof(int), sizeof(std::size_t), ArrayConstruct, ArrayDestroy, std::malloc, ArrayFree);
        returned = true;
    } catch (int value) { thrown = value; }
    Require(!returned, "vec_new3 threw");
    RequireEqual(thrown, 73, "thrown value");
    RequireEqual(arrayConstructed, 2, "elements constructed");
    RequireEqual(arrayDestroyed, 2, "elements destroyed");
    RequireEqual(arrayFreed, 1, "frees");
    RequireEqual(arrayDestroyOrderErrors, 0, "elements destroyed out of reverse order");
    RequireEqual(arrayFreeSizeErrors, 0, "frees with an unexpected size");
}};

const Case arraySuccess{"VecNew3AndDelete3_FourElements_ConstructsDestroysAndFreesAll", [] {
    const ArrayCounters counters(false);
    void* array = __cxa_vec_new3_nid_postfix(4, sizeof(int), sizeof(std::size_t), ArrayConstruct, ArrayDestroy, std::malloc, ArrayFree);
    __cxa_vec_delete3_nid_postfix(array, sizeof(int), sizeof(std::size_t), ArrayDestroy, ArrayFree);
    RequireEqual(arrayConstructed, 4, "elements constructed");
    RequireEqual(arrayDestroyed, 4, "elements destroyed");
    RequireEqual(arrayFreed, 1, "frees");
    RequireEqual(arrayDestroyOrderErrors, 0, "elements destroyed out of reverse order");
    RequireEqual(arrayFreeSizeErrors, 0, "frees with an unexpected size");
}};

const Case threads{"Throw_ConcurrentThreads_EachThreadCatchesAndCleansUp", [] {
    pthread_t handles[4];
    ThreadOutcome outcomes[4];
    bool created[4] {};
    for (int index = 0; index < 4; ++index) created[index] = pthread_create(&handles[index], nullptr, ThreadTest, &outcomes[index]) == 0;
    bool joined[4] {};
    for (int index = 0; index < 4; ++index) {
        if (created[index]) joined[index] = pthread_join(handles[index], nullptr) == 0;
    }
    for (int index = 0; index < 4; ++index) {
        const std::string thread = "thread " + std::to_string(index);
        Require(created[index], thread + " created");
        Require(joined[index], thread + " joined");
        RequireEqual(outcomes[index].wrongValues, 0, thread + " wrong caught values");
        RequireEqual(outcomes[index].uncaughtAfterCatch, 0, thread + " uncaught exceptions after catch");
        RequireEqual(outcomes[index].destroyed, 100, thread + " guards destroyed");
        RequireEqual(outcomes[index].guardsWithoutUncaught, 0, thread + " guards destroyed without an uncaught exception");
    }
}};

const Case terminateUncaught{"Terminate_ExceptionWithoutHandler_CallsTerminateHandler", [] {
    RequireEqual(ChildExitStatus(UncaughtChild), 61, "child exit status");
}};

const Case terminateNoexcept{"Terminate_ExceptionLeavingNoexceptFunction_CallsTerminateHandler", [] {
    RequireEqual(ChildExitStatus(NoexceptChild), 61, "child exit status");
}};

const Case forcedUnwind{"ForcedUnwind_ToEndOfStack_RunsCleanupAndCallsStopAtEnd", [] {
    RequireEqual(ChildExitStatus(ForcedUnwindChild), 63, "child exit status");
}};

const Case backtrace{"Backtrace_CurrentStack_VisitsFramesUntilEndOfStack", [] {
    BacktraceOutcome outcome;
    const auto result = _Unwind_Backtrace_nid_postfix([](_Unwind_Context* context, void* argument) {
        auto& state = *static_cast<BacktraceOutcome*>(argument);
        if (_Unwind_GetIP_nid_postfix(context) == 0) ++state.zeroIps;
        ++state.frames;
        return _URC_NO_REASON;
    }, &outcome);
    RequireEqual(result, _URC_END_OF_STACK, "backtrace result");
    Require(outcome.frames >= 2, "at least two frames, got " + std::to_string(outcome.frames));
    RequireEqual(outcome.zeroIps, 0, "frames with a zero instruction pointer");
}};

} // namespace
