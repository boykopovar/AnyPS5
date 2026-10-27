#include "prx/libc/include/Api.hpp"
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <typeinfo>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <optional>
#include <csignal>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

thread_local int destroyed = 0;
struct Guard {
    ~Guard() { assert(__cxa_uncaught_exceptions_nid_postfix() > 0); ++destroyed; }
};
thread_local int counted = 0;
struct Counted {
    ~Counted() { ++counted; }
};
struct Base { virtual ~Base() = default; int value = 7; };
struct Other { virtual ~Other() = default; int padding = 9; };
struct Derived : Other, Base {};
struct Virtual : virtual Base {};

[[gnu::noinline]] void ThrowInt() { Guard guard; throw 42; }
[[gnu::noinline]] void ThrowClass() { Guard guard; throw Derived(); }

struct Left : Base {};
struct Right : Base {};
struct Repeated : Left, Right {};
struct MixedRepeated : Left, private Right {};
struct LeftVirtual : virtual Base {};
struct RightVirtual : virtual Base {};
struct Diamond : LeftVirtual, RightVirtual {};
struct PrivateDerived : private Derived {
    Base* source() { return this; }
    Derived* target() { return this; }
};
[[gnu::noinline]] Derived* ToDerived(Base* p) { return dynamic_cast<Derived*>(p); }
[[gnu::noinline]] Repeated* ToRepeated(Base* p) { return dynamic_cast<Repeated*>(p); }
void CheckRtti() {
    Repeated repeated;
    Base* left = static_cast<Left*>(&repeated);
    assert(ToRepeated(left) == &repeated);
    assert(dynamic_cast<Right*>(left) == static_cast<Right*>(&repeated));
    PrivateDerived hidden;
    assert(ToDerived(hidden.source()) == hidden.target());
    try { throw static_cast<Repeated*>(nullptr); }
    catch (Base*) { assert(false); }
    catch (Repeated* p) { assert(p == nullptr); }
    try { throw MixedRepeated(); }
    catch (Base&) { assert(false); }
    catch (MixedRepeated&) {}
    try { throw static_cast<Diamond*>(nullptr); }
    catch (Base* p) { assert(p == nullptr); }
    try { throw nullptr; }
    catch (Base* p) { assert(p == nullptr); }
    int value = 1; int* pointer = &value;
    try { throw &pointer; }
    catch (const int**) { assert(false); }
    catch (int** p) { assert(p == &pointer); }
    try { throw &pointer; }
    catch (const int* const* p) { assert(**p == value); }
}
int foreignDeleted;
void CheckForeign() {
    auto* exception = new _Unwind_Exception {};
    exception->exception_class = 0x54455354464f5200;
    exception->exception_cleanup = [](_Unwind_Reason_Code reason, _Unwind_Exception* p) {
        assert(reason == _URC_FOREIGN_EXCEPTION_CAUGHT); ++foreignDeleted; delete p;
    };
    try {
        try { _Unwind_RaiseException_nid_postfix(exception); assert(false); }
        catch (...) { assert(__cxa_current_primary_exception_nid_postfix() == nullptr); throw; }
    } catch (...) {}
    assert(foreignDeleted == 1);
}

int staticAttempts = 0;
[[gnu::noinline]] int StaticValue() {
    static int value = [] { if (++staticAttempts == 1) throw 91; return 37; }();
    return value;
}
void CheckStaticInitialization() {
    try { StaticValue(); assert(false); } catch (int value) { assert(value == 91); }
    assert(StaticValue() == 37 && StaticValue() == 37 && staticAttempts == 2);
}

int arrayConstructed, arrayDestroyed, arrayFreed;
bool failArray;
void ArrayConstruct(void* pointer) {
    if (failArray && arrayConstructed == 2) throw 73;
    *static_cast<int*>(pointer) = arrayConstructed++;
}
void ArrayDestroy(void* pointer) {
    assert(*static_cast<int*>(pointer) == arrayConstructed - 1 - arrayDestroyed);
    ++arrayDestroyed;
}
void ArrayFree(void* pointer, std::size_t size) {
    assert(size == 4 * sizeof(int) + sizeof(std::size_t));
    ++arrayFreed;
    std::free(pointer);
}
void CheckArrays() {
    failArray = true;
    try {
        __cxa_vec_new3_nid_postfix(4, sizeof(int), sizeof(std::size_t), ArrayConstruct, ArrayDestroy, std::malloc, ArrayFree);

        assert(false);
    } catch (int value) { assert(value == 73); }
    assert(arrayConstructed == 2 && arrayDestroyed == 2 && arrayFreed == 1);
    arrayConstructed = arrayDestroyed = arrayFreed = 0;
    failArray = false;
    void* array = __cxa_vec_new3_nid_postfix(4, sizeof(int), sizeof(std::size_t), ArrayConstruct, ArrayDestroy, std::malloc, ArrayFree);
    __cxa_vec_delete3_nid_postfix(array, sizeof(int), sizeof(std::size_t), ArrayDestroy, ArrayFree);

    assert(arrayConstructed == 4 && arrayDestroyed == 4 && arrayFreed == 1);
}
void* ThreadTest(void*) {
    for (int i = 0; i < 100; ++i) {
        try { ThrowInt(); } catch (int value) { assert(value == 42); }
        assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
    }
    assert(destroyed == 100);
    return nullptr;
}
void CheckThreads() {
    pthread_t threads[4];
    for (auto& thread : threads) assert(pthread_create(&thread, nullptr, ThreadTest, nullptr) == 0);
    for (auto& thread : threads) assert(pthread_join(thread, nullptr) == 0);
}
[[gnu::noinline]] void UncaughtThrow() { throw 19; }
[[gnu::noinline]] void NoexceptThrow() noexcept { UncaughtThrow(); }
void CheckTerminate(void (*function)()) {
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        _ZSt13set_terminatePFvvE_nid_postfix([] { _exit(61); });
        function();
        _exit(62);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 61);
}
void CheckDefaultTerminateMessage() {
    const std::string message = std::string(3000, 'w') + " end of message";
    int channel[2];
    assert(pipe(channel) == 0);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(channel[0]);
        dup2(channel[1], 2);
#ifdef __linux__
        assert(prctl(PR_SET_DUMPABLE, 0) == 0);
#endif
        throw std::runtime_error(message);
    }
    close(channel[1]);
    std::string output;
    char buffer[4096];
    for (ssize_t count; (count = read(channel[0], buffer, sizeof(buffer))) > 0;) output.append(buffer, static_cast<std::size_t>(count));
    close(channel[0]);
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    assert(output.find("  what():  " + message + "\n") != std::string::npos);
}
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
void CheckForcedUnwind() {
    pid_t child = fork(); assert(child >= 0);
    if (child == 0) { destroyed = 0; ForceUnwind(); }
    int status; assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 63);
}

int main() {
    try { ThrowInt(); assert(false); } catch (int value) { assert(value == 42); }
    assert(destroyed == 1);
    try { ThrowClass(); assert(false); } catch (const Base& value) { assert(value.value == 7); }
    assert(destroyed == 2);
    try {
        try { throw 13; } catch (int value) { assert(value == 13); throw; }
    } catch (int value) { assert(value == 13); }
    assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
    void* retained = nullptr;
    try { throw 27; } catch (...) { retained = __cxa_current_primary_exception_nid_postfix(); }
    try { __cxa_rethrow_primary_exception_nid_postfix(retained); assert(false); }
    catch (int value) { assert(value == 27); }
    __cxa_decrement_exception_refcount_nid_postfix(retained);

    {
        auto made = std::make_exception_ptr(Counted());
        try { std::rethrow_exception(made); assert(false); } catch (const Counted&) {}
    }
    assert(counted == 2);
    Derived object;
    Base* pointer = &object;
    try { throw &object; } catch (Base* value) { assert(value == pointer); }
    assert(dynamic_cast<Derived*>(pointer) == &object);
    assert(dynamic_cast<Other*>(pointer) == static_cast<Other*>(&object));
    try { throw Virtual(); } catch (Base& value) { assert(value.value == 7); }
    try { _ZSt14_Xout_of_rangePKc_nid_postfix("test message"); }
    catch (const std::logic_error& value) { assert(std::strcmp(value.what(), "test message") == 0); }
    try { _ZSt13_Xrange_errorPKc_nid_postfix("range message"); }
    catch (const std::range_error& value) { if (std::strcmp(value.what(), "range message") != 0) std::abort(); }
    try { _ZSt13_Xrange_errorPKc_nid_postfix(nullptr); }
    catch (const std::runtime_error& value) { if (std::strcmp(value.what(), "") != 0) std::abort(); }
    try { _ZSt13_Xrange_errorPKc_nid_postfix("not a logic error"); }
    catch (const std::logic_error&) { std::abort(); }
    catch (const std::range_error&) {}
    bool badFunctionCall = false;
    try { _ZNSt8__sce_v219_Xbad_function_callEv_nid_postfix(); }
    catch (const std::bad_function_call&) { std::abort(); }
    catch (const std::exception& value) {
        if (std::strcmp(typeid(value).name(), "NSt8__sce_v217bad_function_callE") != 0) std::abort();
        if (std::strcmp(value.what(), "bad function call") != 0) std::abort();
        badFunctionCall = true;
    }
    if (!badFunctionCall) std::abort();
    bool plainBadFunctionCall = false;
    try { _ZSt19_Xbad_function_callv_nid_postfix(); }
    catch (const std::bad_function_call&) { plainBadFunctionCall = true; }
    if (!plainBadFunctionCall) std::abort();
    try { __cxa_bad_cast_nid_postfix(); }
    catch (const std::exception& value) { assert(value.what() != nullptr); }
    try { _ZSt19_Throw_bad_weak_ptrv_nid_postfix(); std::abort(); }
    catch (const std::bad_weak_ptr& value) { if (std::strcmp(value.what(), "bad_weak_ptr") != 0) std::abort(); }
    try { _ZSt26__throw_bad_variant_accessv_nid_postfix(); std::abort(); }
    catch (const std::bad_variant_access& value) { if (std::strcmp(value.what(), "bad_variant_access") != 0) std::abort(); }
    try { _ZSt26_Throw_bad_optional_accessv_nid_postfix(); std::abort(); }
    catch (const std::bad_optional_access& value) { if (std::strcmp(value.what(), "bad_optional_access") != 0) std::abort(); }
    try { _ZNKSt9exception6_RaiseEv_nid_postfix(nullptr); assert(false); }
    catch (const std::invalid_argument&) {}
    LibcException::ExceptionObject exceptionObject {};
    _ZNSt9exceptionC1Ev_nid_postfix(&exceptionObject);
    try { _ZNKSt9exception6_RaiseEv_nid_postfix(&exceptionObject); assert(false); }
    catch (const std::exception& value) { assert(std::strcmp(value.what(), "std::exception") == 0); }
    _ZNSt9exceptionD1Ev_nid_postfix(&exceptionObject);
    try { _ZNKSt8bad_cast8_DoraiseEv_nid_postfix(nullptr); assert(false); }
    catch (const std::invalid_argument&) {}
    _ZNSt8bad_castC1Ev_nid_postfix(&exceptionObject);
    try { _ZNKSt8bad_cast8_DoraiseEv_nid_postfix(&exceptionObject); assert(false); }
    catch (const std::bad_cast&) {}
    _ZNSt8bad_castD1Ev_nid_postfix(&exceptionObject);
    CheckRtti();
    CheckForeign();
    CheckStaticInitialization();
    CheckArrays();
    CheckThreads();
    CheckTerminate(UncaughtThrow);
    CheckTerminate(NoexceptThrow);
    CheckDefaultTerminateMessage();
    CheckForcedUnwind();
    int frames = 0;
    auto backtrace = _Unwind_Backtrace_nid_postfix([](_Unwind_Context* context, void* argument) {
        assert(_Unwind_GetIP_nid_postfix(context) != 0);
        ++*static_cast<int*>(argument);
        return _URC_NO_REASON;
    }, &frames);
    assert(backtrace == _URC_END_OF_STACK && frames >= 2);
    frames = 0;
    backtrace = _Unwind_Backtrace_nid_postfix([](_Unwind_Context* context, void* argument) {
        const auto* slots = *reinterpret_cast<const std::uintptr_t* const*>(context);
        const auto validRegister = reinterpret_cast<bool (*)(_Unwind_Context*, int)>(slots[2]);
        const auto getRegister = reinterpret_cast<std::uintptr_t (*)(_Unwind_Context*, int)>(slots[3]);
        const auto setRegister = reinterpret_cast<void (*)(_Unwind_Context*, int, std::uintptr_t)>(slots[4]);
        const auto validFloatRegister = reinterpret_cast<bool (*)(_Unwind_Context*, int)>(slots[5]);
        const auto isSignalFrame = reinterpret_cast<bool (*)(_Unwind_Context*)>(slots[11]);
        assert(validRegister(context, -1) && validRegister(context, -2) && validRegister(context, 16) && !validRegister(context, 17));
        assert(getRegister(context, -1) == _Unwind_GetIP_nid_postfix(context) && getRegister(context, -1) != 0);
        assert(getRegister(context, -2) == _Unwind_GetGR_nid_postfix(context, 7));
        const auto rbx = getRegister(context, 3);
        setRegister(context, 3, rbx + 1);
        assert(_Unwind_GetGR_nid_postfix(context, 3) == rbx + 1);
        setRegister(context, 3, rbx);
        assert(!validFloatRegister(context, 17) && !isSignalFrame(context));
        ++*static_cast<int*>(argument);
        return _URC_NO_REASON;
    }, &frames);
    assert(backtrace == _URC_END_OF_STACK && frames >= 2);
    std::puts("exception runtime tests passed");
}
