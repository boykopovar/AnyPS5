#include "prx/libSceNKWebKit/include/InspectorBindingTraitsExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "InspectorBindingTraits check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Assertion = void (APS5_VABI*)(void*);

static const Assertion Assertions[] = {
    &_ZN9Inspector8Protocol13BindingTraitsINS0_8Debugger9CallFrameEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_8Debugger15FunctionDetailsEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_8Debugger8LocationEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_8Debugger5Scope4TypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_8Debugger5ScopeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime15CollectionEntryEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime12EntryPreviewEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime26InternalPropertyDescriptorEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime13ObjectPreview7SubtypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime13ObjectPreview4TypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime13ObjectPreviewEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime18PropertyDescriptorEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime15PropertyPreview7SubtypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime15PropertyPreview4TypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime15PropertyPreviewEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime12RemoteObject7SubtypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime12RemoteObject4TypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime12RemoteObjectEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_8Timeline9EventTypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
    &_ZN9Inspector8Protocol13BindingTraitsINS0_8Timeline13TimelineEventEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE,
};

int main() {
    unsigned char value[64];
    std::memset(value, 0x5a, sizeof(value));
    for (const Assertion assertion : Assertions) {
        assertion(nullptr);
        assertion(value);
        assertion(reinterpret_cast<void*>(std::uintptr_t{0x10}));
    }
    for (const unsigned char byte : value) Require(byte == 0x5a);
    Require(sizeof(Assertions) / sizeof(Assertions[0]) == 20);
    return 0;
}
