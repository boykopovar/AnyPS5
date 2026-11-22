#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::OptionalBool;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore::Settings check failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename TAction>
void RequireThrowsInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    std::fprintf(stderr, "WebCore::Settings check failed at line %u: expected std::invalid_argument\n", location.line());
    std::abort();
}

alignas(8) unsigned char firstObject[8];
alignas(8) unsigned char secondObject[8];

OptionalBool Engaged(bool value) {
    return OptionalBool{ value, true };
}

OptionalBool Disengaged() {
    return OptionalBool{ false, false };
}

void TestDefaults() {
    Require(_ZNK7WebCore8Settings15isScriptEnabledEv(firstObject) == false);
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(firstObject) == true);
    Require(_ZNK7WebCore8Settings16showDebugBordersEv(firstObject) == false);
    Require(_ZNK7WebCore8Settings18showRepaintCounterEv(firstObject) == false);
    Require(_ZNK7WebCore8Settings18webSecurityEnabledEv(firstObject) == true);
    Require(_ZNK7WebCore8Settings23needsSiteSpecificQuirksEv(firstObject) == false);
    Require(_ZNK7WebCore8Settings23webRTCEncryptionEnabledEv(firstObject) == true);
    Require(_ZNK7WebCore8Settings26authorAndUserStylesEnabledEv(firstObject) == true);
    Require(_ZNK7WebCore8Settings28iceCandidateFilteringEnabledEv(firstObject) == true);
    std::printf("WebCore::Settings: getters return the WebCore defaults of the pinned Settings and WebPreferences yaml\n");
}

void TestBooleans() {
    _ZN7WebCore8Settings16setImagesEnabledEb(firstObject, false);
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(firstObject) == false);
    _ZN7WebCore8Settings16setImagesEnabledEb(firstObject, false);
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(firstObject) == false);
    _ZN7WebCore8Settings16setImagesEnabledEb(firstObject, true);
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(firstObject) == true);

    _ZN7WebCore8Settings19setShowDebugBordersEb(firstObject, true);
    Require(_ZNK7WebCore8Settings16showDebugBordersEv(firstObject) == true);
    _ZN7WebCore8Settings19setShowDebugBordersEb(firstObject, false);
    Require(_ZNK7WebCore8Settings16showDebugBordersEv(firstObject) == false);

    _ZN7WebCore8Settings21setShowRepaintCounterEb(firstObject, true);
    Require(_ZNK7WebCore8Settings18showRepaintCounterEv(firstObject) == true);
    _ZN7WebCore8Settings21setShowRepaintCounterEb(firstObject, true);
    Require(_ZNK7WebCore8Settings18showRepaintCounterEv(firstObject) == true);
    _ZN7WebCore8Settings21setShowRepaintCounterEb(firstObject, false);
    Require(_ZNK7WebCore8Settings18showRepaintCounterEv(firstObject) == false);

    _ZN7WebCore8Settings29setAuthorAndUserStylesEnabledEb(firstObject, false);
    Require(_ZNK7WebCore8Settings26authorAndUserStylesEnabledEv(firstObject) == false);
    _ZN7WebCore8Settings29setAuthorAndUserStylesEnabledEb(firstObject, true);
    Require(_ZNK7WebCore8Settings26authorAndUserStylesEnabledEv(firstObject) == true);
    _ZN7WebCore8Settings29setAuthorAndUserStylesEnabledEb(firstObject, true);
    Require(_ZNK7WebCore8Settings26authorAndUserStylesEnabledEv(firstObject) == true);

    _ZN7WebCore8Settings31setICECandidateFilteringEnabledEb(firstObject, false);
    Require(_ZNK7WebCore8Settings28iceCandidateFilteringEnabledEv(firstObject) == false);
    _ZN7WebCore8Settings31setICECandidateFilteringEnabledEb(firstObject, false);
    Require(_ZNK7WebCore8Settings28iceCandidateFilteringEnabledEv(firstObject) == false);
    _ZN7WebCore8Settings31setICECandidateFilteringEnabledEb(firstObject, true);
    Require(_ZNK7WebCore8Settings28iceCandidateFilteringEnabledEv(firstObject) == true);

    Require(_ZNK7WebCore8Settings15isScriptEnabledEv(firstObject) == false);
    Require(_ZNK7WebCore8Settings18webSecurityEnabledEv(firstObject) == true);
    Require(_ZNK7WebCore8Settings23needsSiteSpecificQuirksEv(firstObject) == false);
    Require(_ZNK7WebCore8Settings23webRTCEncryptionEnabledEv(firstObject) == true);
    std::printf("WebCore::Settings: boolean setters read back, repeat idempotently and leave the unrelated settings alone\n");
}

void TestObjectsAreIndependent() {
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(secondObject) == true);
    _ZN7WebCore8Settings16setImagesEnabledEb(secondObject, false);
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(secondObject) == false);
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(firstObject) == true);
    _ZN7WebCore8Settings16setImagesEnabledEb(firstObject, true);
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(firstObject) == true);
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(secondObject) == false);
    std::printf("WebCore::Settings: two settings objects keep separate values\n");
}

void TestInspectorOverrides() {
    _ZN7WebCore8Settings16setImagesEnabledEb(firstObject, true);
    _ZN7WebCore8Settings33setImagesEnabledInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Engaged(false));
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(firstObject) == false);
    _ZN7WebCore8Settings33setImagesEnabledInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Disengaged());
    Require(_ZNK7WebCore8Settings16areImagesEnabledEv(firstObject) == true);

    _ZN7WebCore8Settings19setShowDebugBordersEb(firstObject, false);
    _ZN7WebCore8Settings36setShowDebugBordersInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Engaged(true));
    Require(_ZNK7WebCore8Settings16showDebugBordersEv(firstObject) == true);
    _ZN7WebCore8Settings36setShowDebugBordersInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Engaged(true));
    Require(_ZNK7WebCore8Settings16showDebugBordersEv(firstObject) == true);
    _ZN7WebCore8Settings36setShowDebugBordersInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Disengaged());
    Require(_ZNK7WebCore8Settings16showDebugBordersEv(firstObject) == false);

    _ZN7WebCore8Settings21setShowRepaintCounterEb(firstObject, true);
    _ZN7WebCore8Settings38setShowRepaintCounterInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Engaged(false));
    Require(_ZNK7WebCore8Settings18showRepaintCounterEv(firstObject) == false);
    _ZN7WebCore8Settings38setShowRepaintCounterInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Disengaged());
    Require(_ZNK7WebCore8Settings18showRepaintCounterEv(firstObject) == true);

    _ZN7WebCore8Settings29setAuthorAndUserStylesEnabledEb(firstObject, true);
    _ZN7WebCore8Settings46setAuthorAndUserStylesEnabledInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Engaged(false));
    Require(_ZNK7WebCore8Settings26authorAndUserStylesEnabledEv(firstObject) == false);
    _ZN7WebCore8Settings46setAuthorAndUserStylesEnabledInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Disengaged());
    Require(_ZNK7WebCore8Settings26authorAndUserStylesEnabledEv(firstObject) == true);

    _ZN7WebCore8Settings31setICECandidateFilteringEnabledEb(firstObject, false);
    _ZN7WebCore8Settings48setICECandidateFilteringEnabledInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Engaged(true));
    Require(_ZNK7WebCore8Settings28iceCandidateFilteringEnabledEv(firstObject) == true);
    _ZN7WebCore8Settings48setICECandidateFilteringEnabledInspectorOverrideEN3WTF8OptionalIbEE(firstObject, Disengaged());
    Require(_ZNK7WebCore8Settings28iceCandidateFilteringEnabledEv(firstObject) == false);
    std::printf("WebCore::Settings: inspector overrides replace the stored value and fall back when disengaged\n");
}

void TestNullObject() {
    RequireThrowsInvalidArgument([] { _ZNK7WebCore8Settings16areImagesEnabledEv(nullptr); });
    RequireThrowsInvalidArgument([] { _ZN7WebCore8Settings16setImagesEnabledEb(nullptr, true); });
    RequireThrowsInvalidArgument([] { _ZN7WebCore8Settings33setImagesEnabledInspectorOverrideEN3WTF8OptionalIbEE(nullptr, Disengaged()); });
    RequireThrowsInvalidArgument([] { _ZNK7WebCore8Settings15isScriptEnabledEv(nullptr); });
    std::printf("WebCore::Settings: a null settings object throws std::invalid_argument\n");
}

void* APS5_VABI HeapAllocate(std::size_t size) { return std::malloc(size == 0 ? 1 : size); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t size) {
    return std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
}
void* APS5_VABI HeapReallocate(void* pointer, std::size_t size) {
    return std::realloc(pointer, size == 0 ? 1 : size);
}
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

}

int main() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(HeapAllocate),
        reinterpret_cast<void*>(HeapFree),
        reinterpret_cast<void*>(HeapCallocate),
        reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign),
        reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    TestDefaults();
    TestBooleans();
    TestObjectsAreIndependent();
    TestInspectorOverrides();
    TestNullObject();
    return 0;
}
