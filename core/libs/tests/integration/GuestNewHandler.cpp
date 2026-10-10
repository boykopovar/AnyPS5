#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <new>
#include <string>

extern "C" {
using Handler = void (APS5_VABI*)();
Handler APS5_VABI _ZSt15set_new_handlerPFvvE_nid_postfix(Handler);
Handler APS5_VABI _ZSt15get_new_handlerv_nid_postfix();
void* APS5_VABI _Znwm_nid_postfix(std::size_t);
void* APS5_VABI _Znam_nid_postfix(std::size_t);
void* APS5_VABI _ZnwmRKSt9nothrow_t_nid_postfix(std::size_t, const void*) noexcept;
void* APS5_VABI _ZnamRKSt9nothrow_t_nid_postfix(std::size_t, const void*) noexcept;
}

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

alignas(16) std::array<std::byte, 64> storage{};
unsigned failuresLeft = 0;
unsigned allocations = 0;
unsigned handlerCalls = 0;
std::size_t lastSize = 0;
void* nestedAllocation = nullptr;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++allocations;
    lastSize = bytes;
    if (failuresLeft != 0) {
        --failuresLeft;
        return nullptr;
    }
    return storage.data();
}

void APS5_VABI Release(void*) {}
void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) { return Allocate(count * bytes); }
void* APS5_VABI Reallocate(void*, std::size_t bytes) { return Allocate(bytes); }
void* APS5_VABI Align(std::size_t, std::size_t bytes) { return Allocate(bytes); }
void* APS5_VABI Realign(void*, std::size_t bytes, std::size_t) { return Allocate(bytes); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = Allocate(bytes);
    return *pointer == nullptr ? 12 : 0;
}

void APS5_VABI CountingHandler() { ++handlerCalls; }
void APS5_VABI UninstallingHandler() {
    if (++handlerCalls == 3) _ZSt15set_new_handlerPFvvE_nid_postfix(nullptr);
}
void APS5_VABI ThrowingHandler() {
    ++handlerCalls;
    throw std::bad_alloc();
}
void APS5_VABI AllocatingHandler() {
    ++handlerCalls;
    failuresLeft = 0;
    nestedAllocation = _Znwm_nid_postfix(8);
}

void RegisterHeap() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&Allocate);
    api[1] = reinterpret_cast<void*>(&Release);
    api[2] = reinterpret_cast<void*>(&AllocateZeroed);
    api[3] = reinterpret_cast<void*>(&Reallocate);
    api[4] = reinterpret_cast<void*>(&Align);
    api[5] = reinterpret_cast<void*>(&Realign);
    api[6] = reinterpret_cast<void*>(&PosixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());
}

class HandlerFixture {
public:
    explicit HandlerFixture(Handler handler) {
        RegisterHeap();
        previous = _ZSt15set_new_handlerPFvvE_nid_postfix(handler);
        Reset(0);
    }

    ~HandlerFixture() {
        _ZSt15set_new_handlerPFvvE_nid_postfix(previous);
        Reset(0);
    }

    HandlerFixture(const HandlerFixture&) = delete;
    HandlerFixture& operator=(const HandlerFixture&) = delete;

    void Reset(unsigned failures) {
        failuresLeft = failures;
        allocations = 0;
        handlerCalls = 0;
        lastSize = 0;
        nestedAllocation = nullptr;
    }

private:
    Handler previous = nullptr;
};

void RequireCounts(unsigned expectedHandlerCalls, unsigned expectedAllocations, const char* message) {
    RequireEqual(handlerCalls, expectedHandlerCalls, std::string(message) + " handler calls");
    RequireEqual(allocations, expectedAllocations, std::string(message) + " allocations");
}

const Case defaultHandler{"GetNewHandler_Default_IsNull", [] {
    RequireEqual(_ZSt15get_new_handlerv_nid_postfix() == nullptr, true, "no handler installed");
}};

const Case setReturnsPrevious{"SetNewHandler_ReplaceHandler_ReturnsPreviousHandler", [] {
    HandlerFixture fixture(nullptr);
    RequireEqual(_ZSt15set_new_handlerPFvvE_nid_postfix(&CountingHandler) == nullptr, true, "first install returns null");
    RequireEqual(_ZSt15get_new_handlerv_nid_postfix() == &CountingHandler, true, "counting handler reported");
    RequireEqual(_ZSt15set_new_handlerPFvvE_nid_postfix(&UninstallingHandler) == &CountingHandler, true, "returns counting handler");
    RequireEqual(_ZSt15set_new_handlerPFvvE_nid_postfix(&ThrowingHandler) == &UninstallingHandler, true, "returns uninstalling handler");
    RequireEqual(_ZSt15set_new_handlerPFvvE_nid_postfix(&AllocatingHandler) == &ThrowingHandler, true, "returns throwing handler");
    RequireEqual(_ZSt15set_new_handlerPFvvE_nid_postfix(nullptr) == &AllocatingHandler, true, "returns allocating handler");
    RequireEqual(_ZSt15get_new_handlerv_nid_postfix() == nullptr, true, "handler removed");
}};

const Case throwingNewWithoutHandler{"New_FailureWithoutHandler_ThrowsBadAlloc", [] {
    HandlerFixture fixture(nullptr);
    fixture.Reset(1);
    RequireThrows<std::bad_alloc>([] { _Znwm_nid_postfix(8); }, "operator new");
    RequireCounts(0, 1, "operator new");
    fixture.Reset(1);
    RequireThrows<std::bad_alloc>([] { _Znam_nid_postfix(8); }, "operator new[]");
    RequireCounts(0, 1, "operator new[]");
}};

const Case nothrowNewWithoutHandler{"NothrowNew_FailureWithoutHandler_ReturnsNull", [] {
    HandlerFixture fixture(nullptr);
    fixture.Reset(1);
    RequireEqual(_ZnwmRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr, true, "nothrow operator new result");
    RequireEqual(handlerCalls, 0u, "nothrow operator new handler calls");
    fixture.Reset(1);
    RequireEqual(_ZnamRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr, true, "nothrow operator new[] result");
    RequireEqual(handlerCalls, 0u, "nothrow operator new[] handler calls");
}};

const Case throwingNewRetries{"New_TransientFailureWithHandler_RetriesUntilAllocated", [] {
    HandlerFixture fixture(&CountingHandler);
    fixture.Reset(2);
    RequireEqual(_Znwm_nid_postfix(8) == storage.data(), true, "operator new result");
    RequireCounts(2, 3, "operator new");
    fixture.Reset(3);
    RequireEqual(_Znam_nid_postfix(8) == storage.data(), true, "operator new[] result");
    RequireCounts(3, 4, "operator new[]");
}};

const Case nothrowNewRetries{"NothrowNew_TransientFailureWithHandler_RetriesUntilAllocated", [] {
    HandlerFixture fixture(&CountingHandler);
    fixture.Reset(1);
    RequireEqual(_ZnwmRKSt9nothrow_t_nid_postfix(8, nullptr) == storage.data(), true, "nothrow operator new result");
    RequireCounts(1, 2, "nothrow operator new");
    fixture.Reset(1);
    RequireEqual(_ZnamRKSt9nothrow_t_nid_postfix(8, nullptr) == storage.data(), true, "nothrow operator new[] result");
    RequireCounts(1, 2, "nothrow operator new[]");
}};

const Case immediateSuccess{"New_ImmediateSuccess_DoesNotCallHandler", [] {
    HandlerFixture fixture(&CountingHandler);
    RequireEqual(_Znwm_nid_postfix(8) == storage.data(), true, "operator new result");
    RequireCounts(0, 1, "operator new");
}};

const Case zeroSize{"New_ZeroSize_AllocatesOneByte", [] {
    HandlerFixture fixture(&CountingHandler);
    fixture.Reset(1);
    RequireEqual(_Znwm_nid_postfix(0) == storage.data(), true, "operator new result");
    RequireEqual(lastSize, std::size_t{1}, "requested size");
    RequireEqual(handlerCalls, 1u, "handler calls");
}};

const Case uninstallingHandler{"New_HandlerUninstallsItself_ThrowsBadAlloc", [] {
    HandlerFixture fixture(&UninstallingHandler);
    fixture.Reset(100);
    RequireThrows<std::bad_alloc>([] { _Znwm_nid_postfix(8); }, "operator new");
    RequireCounts(3, 4, "operator new");
    RequireEqual(_ZSt15get_new_handlerv_nid_postfix() == nullptr, true, "handler uninstalled");
}};

const Case throwingHandlerNew{"New_ThrowingHandler_PropagatesBadAlloc", [] {
    HandlerFixture fixture(&ThrowingHandler);
    fixture.Reset(1);
    RequireThrows<std::bad_alloc>([] { _Znwm_nid_postfix(8); }, "operator new");
    RequireCounts(1, 1, "operator new");
}};

const Case throwingHandlerNothrow{"NothrowNew_ThrowingHandler_ReturnsNull", [] {
    HandlerFixture fixture(&ThrowingHandler);
    fixture.Reset(1);
    RequireEqual(_ZnwmRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr, true, "nothrow operator new result");
    RequireCounts(1, 1, "nothrow operator new");
    fixture.Reset(1);
    RequireEqual(_ZnamRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr, true, "nothrow operator new[] result");
    RequireCounts(1, 1, "nothrow operator new[]");
}};

const Case allocatingHandler{"New_HandlerThatAllocates_RetriesAndSucceeds", [] {
    HandlerFixture fixture(&AllocatingHandler);
    fixture.Reset(1);
    RequireEqual(_Znwm_nid_postfix(16) == storage.data(), true, "operator new result");
    RequireEqual(handlerCalls, 1u, "handler calls");
    RequireEqual(nestedAllocation == storage.data(), true, "allocation inside the handler");
}};

} // namespace
