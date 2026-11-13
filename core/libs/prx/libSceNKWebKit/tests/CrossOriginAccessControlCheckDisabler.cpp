#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceNKWebKit/include/CrossOriginAccessControlExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "CrossOriginAccessControlCheckDisabler check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static CrossOriginAccessControlCheckDisabler Garbage() {
    CrossOriginAccessControlCheckDisabler object;
    std::memset(static_cast<void*>(&object), 0xA5, sizeof(object));
    return object;
}

static void RequireVtable(const CrossOriginAccessControlCheckDisabler& object) {
    Require(object.vptr != nullptr);
    Require(object.vptr->completeDestructor == &_ZN7WebCore37CrossOriginAccessControlCheckDisablerD1Ev);
    Require(object.vptr->deletingDestructor == &_ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev);
    Require(object.vptr->crossOriginAccessControlCheckEnabled == &_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv);
}

static void TestSingleton() {
    auto& first = _ZN7WebCore37CrossOriginAccessControlCheckDisabler9singletonEv();
    auto& second = _ZN7WebCore37CrossOriginAccessControlCheckDisabler9singletonEv();
    Require(&first == &second);
    RequireVtable(first);
    Require(_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv(&first));
    _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(&first, false);
    Require(!_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv(&second));
    Require(!second.vptr->crossOriginAccessControlCheckEnabled(&second));
    _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(&first, true);
    Require(_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv(&second));
}

static void TestDefaultConstructors() {
    auto complete = Garbage();
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(&complete);
    RequireVtable(complete);
    Require(complete.accessControlCheckEnabled);
    auto base = Garbage();
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev(&base);
    RequireVtable(base);
    Require(base.accessControlCheckEnabled);
    Require(base.vptr == complete.vptr);
    Require(base.vptr == _ZN7WebCore37CrossOriginAccessControlCheckDisabler9singletonEv().vptr);
    complete.vptr->completeDestructor(&complete);
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerD2Ev(&base);
}

static void TestCopyConstructors() {
    CrossOriginAccessControlCheckDisabler source;
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(&source);
    _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(&source, false);
    auto complete = Garbage();
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1ERKS0_(&complete, &source);
    Require(complete.vptr == source.vptr);
    Require(!complete.accessControlCheckEnabled);
    auto base = Garbage();
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_(&base, &source);
    Require(base.vptr == source.vptr);
    Require(!base.vptr->crossOriginAccessControlCheckEnabled(&base));
    _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(&source, true);
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1ERKS0_(&complete, &source);
    Require(complete.accessControlCheckEnabled);
}

static void TestAssignment() {
    CrossOriginAccessControlCheckDisabler source;
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(&source);
    _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(&source, false);
    CrossOriginAccessControlCheckDisabler target;
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(&target);
    const auto* vptr = target.vptr;
    auto& result = _ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_(&target, &source);
    Require(&result == &target);
    Require(target.vptr == vptr);
    Require(!target.accessControlCheckEnabled);
    _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(&source, true);
    Require(&_ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_(&target, &source) == &target);
    Require(target.vptr->crossOriginAccessControlCheckEnabled(&target));
}

static void TestDeletingDestructor() {
    auto* object = static_cast<CrossOriginAccessControlCheckDisabler*>(_Znwm_nid_postfix(sizeof(CrossOriginAccessControlCheckDisabler)));
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(object);
    object->vptr->deletingDestructor(object);
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    TestSingleton();
    TestDefaultConstructors();
    TestCopyConstructors();
    TestAssignment();
    TestDeletingDestructor();
    std::puts("CrossOriginAccessControlCheckDisabler tests passed");
    return 0;
}
