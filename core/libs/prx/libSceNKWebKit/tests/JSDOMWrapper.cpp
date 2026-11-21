#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::JSDOMWrapper;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "dom wrapper toWrapped check failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename Fn>
void ThrowsInvalidArgument(Fn&& fn, int testErrno, std::source_location location = std::source_location::current()) {
    errno = testErrno;
    bool caught = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught, location);
    Require(errno == testErrno, location);
}

JSDOMWrapper Cell(std::uint8_t type, void* wrapped) {
    JSDOMWrapper cell{};
    cell.cell.type = type;
    cell.wrapped = wrapped;
    return cell;
}

std::uint64_t Value(const JSDOMWrapper* cell) {
    return reinterpret_cast<std::uintptr_t>(cell);
}

void TestTypes() {
    int target = 0;
    for (unsigned type = 0; type < 256; ++type) {
        const JSDOMWrapper cell = Cell(static_cast<std::uint8_t>(type), &target);
        const std::uint64_t value = Value(&cell);
        Require((_ZN7WebCore10JSDocument9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, value) != nullptr) == (type == 0xf9));
        Require((_ZN7WebCore9JSElement9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, value) != nullptr) == (type == 0xf1));
        Require((_ZN7WebCore7JSEvent9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, value) != nullptr) == (type == 0xef));
        Require((_ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, value) != nullptr) == (type >= 0xf0));
    }
}

void TestReturnedPointers() {
    int documentTarget = 0, elementTarget = 0, eventTarget = 0, textTarget = 0;
    const JSDOMWrapper document = Cell(0xf9, &documentTarget);
    const JSDOMWrapper element = Cell(0xf1, &elementTarget);
    const JSDOMWrapper event = Cell(0xef, &eventTarget);
    const JSDOMWrapper text = Cell(0xf3, &textTarget);
    Require(_ZN7WebCore10JSDocument9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, Value(&document)) == static_cast<void*>(&documentTarget));
    Require(_ZN7WebCore9JSElement9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, Value(&element)) == static_cast<void*>(&elementTarget));
    Require(_ZN7WebCore7JSEvent9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, Value(&event)) == static_cast<void*>(&eventTarget));
    Require(_ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, Value(&document)) == static_cast<void*>(&documentTarget));
    Require(_ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, Value(&element)) == static_cast<void*>(&elementTarget));
    Require(_ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, Value(&text)) == static_cast<void*>(&textTarget));
    Require(_ZN7WebCore10JSDocument9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, Value(&element)) == nullptr);
    Require(_ZN7WebCore9JSElement9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, Value(&text)) == nullptr);
    Require(_ZN7WebCore7JSEvent9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, Value(&text)) == nullptr);
    Require(_ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, Value(&event)) == nullptr);
}

void TestNonCellValues() {
    const std::uint64_t values[] = {
        0xfffe000000000000ull,
        0xfffe000000000001ull,
        0xfffe0000deadbeefull,
        0x0000000000000002ull,
        0x000000000000000aull,
        0x0000000000000006ull,
        0x0002000000000000ull,
        0x0001000000000000ull,
        0xffff000000000000ull,
    };
    for (const std::uint64_t value : values) {
        const bool isNotCell = (value & 0xfffe000000000002ull) != 0;
        if (!isNotCell) continue;
        Require(_ZN7WebCore10JSDocument9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, value) == nullptr);
        Require(_ZN7WebCore9JSElement9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, value) == nullptr);
        Require(_ZN7WebCore7JSEvent9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, value) == nullptr);
        Require(_ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, value) == nullptr);
    }
    Require(_ZN7WebCore10JSDocument9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, 0xfffe000000000002ull) == nullptr);
    Require(_ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, 0x2) == nullptr);
}

void TestGuestPointerValidation() {
    ThrowsInvalidArgument([] { _ZN7WebCore10JSDocument9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, 0); }, EAGAIN);
    ThrowsInvalidArgument([] { _ZN7WebCore9JSElement9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, 0x8); }, EINVAL);
    ThrowsInvalidArgument([] { _ZN7WebCore7JSEvent9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, 0x10); }, ENOMEM);
    ThrowsInvalidArgument([] { _ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE(nullptr, 0x1000); }, EFAULT);
}

}

int main() {
    TestTypes();
    TestReturnedPointers();
    TestNonCellValues();
    TestGuestPointerValidation();
    return 0;
}
