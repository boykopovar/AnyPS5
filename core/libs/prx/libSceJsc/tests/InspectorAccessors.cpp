#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using Jsc::InspectorAbi::AsyncCallIdentifier;
using Jsc::InspectorAbi::InjectedScriptHost;
using Jsc::InspectorAbi::OptionalString;
using Jsc::InspectorAbi::RemoteTarget;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Inspector accessors check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename F> bool Throws(F function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestAsyncCallIdentifier() {
    const AsyncCallIdentifier timer = _ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi(0, 7);
    Require(timer.type == 0 && timer.callbackId == 7);
    const AsyncCallIdentifier frame = _ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi(3, -1);
    Require(frame.type == 3 && frame.callbackId == -1);
    const AsyncCallIdentifier large = _ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi(5, INT32_MIN);
    Require(large.type == 5 && large.callbackId == INT32_MIN);
}

void TestRemoteTargets() {
    RemoteTarget target{reinterpret_cast<const void*>(0x1000), 42, false};
    Require(!_ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv(&target));
    Require(_ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv(&target));
    target.flag = true;
    Require(_ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv(&target));
    Require(!_ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv(&target));
    target.identifier = 0;
    Require(_ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv(&target));
    Require(Throws([] { _ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv(nullptr); }));
    Require(Throws([] { _ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv(nullptr); }));
}

void TestSavedResultAlias() {
    int impl = 0;
    InjectedScriptHost host{reinterpret_cast<const void*>(0x2000), 1, nullptr, {true, &impl}};
    const OptionalString* alias = _ZNK9Inspector18InjectedScriptHost16savedResultAliasEv(&host);
    Require(alias == &host.savedResultAlias);
    Require(reinterpret_cast<const char*>(alias) - reinterpret_cast<const char*>(&host) == 24);
    Require(alias->engaged && alias->impl == &impl);
    host.savedResultAlias = {false, nullptr};
    Require(!_ZNK9Inspector18InjectedScriptHost16savedResultAliasEv(&host)->engaged);
    Require(Throws([] { _ZNK9Inspector18InjectedScriptHost16savedResultAliasEv(nullptr); }));
}

} // namespace

int main() {
    TestAsyncCallIdentifier();
    TestRemoteTargets();
    TestSavedResultAlias();
    std::puts("Inspector accessors tests passed");
    return 0;
}
