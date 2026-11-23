#include "prx/libSceNKWebKit/include/TrapBodyExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#ifndef _WIN32
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#endif

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "TrapBodies check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Entry {
    const char* name;
    void (APS5_VABI *function)();
};

static const Entry entries[] = {
    {"_ZN3JSC10Identifier27checkCurrentAtomStringTableERNS_2VME", &_ZN3JSC10Identifier27checkCurrentAtomStringTableERNS_2VME},
    {"_ZN3JSC14JSRunLoopTimerD0Ev", &_ZN3JSC14JSRunLoopTimerD0Ev},
    {"_ZN3JSC14SourceProviderD0Ev", &_ZN3JSC14SourceProviderD0Ev},
    {"_ZN3JSC17MarkingConstraintD0Ev", &_ZN3JSC17MarkingConstraintD0Ev},
    {"_ZN3JSC18GCActivityCallbackD0Ev", &_ZN3JSC18GCActivityCallbackD0Ev},
    {"_ZN3JSC18GCActivityCallbackD1Ev", &_ZN3JSC18GCActivityCallbackD1Ev},
    {"_ZN3JSC20TypedArrayControllerD0Ev", &_ZN3JSC20TypedArrayControllerD0Ev},
    {"_ZN3JSC2VM10ClientDataD0Ev", &_ZN3JSC2VM10ClientDataD0Ev},
    {"_ZN3JSC8Bindings8InstanceD0Ev", &_ZN3JSC8Bindings8InstanceD0Ev},
    {"_ZN3WTF15AutomaticThreadD0Ev", &_ZN3WTF15AutomaticThreadD0Ev},
    {"_ZN3WTF18FunctionDispatcherD0Ev", &_ZN3WTF18FunctionDispatcherD0Ev},
    {"_ZN7WebCore11DisplayList8RecorderD0Ev", &_ZN7WebCore11DisplayList8RecorderD0Ev},
    {"_ZN7WebCore11ImageBufferD0Ev", &_ZN7WebCore11ImageBufferD0Ev},
    {"_ZN7WebCore11ImageBufferD1Ev", &_ZN7WebCore11ImageBufferD1Ev},
    {"_ZN7WebCore13GraphicsLayerD0Ev", &_ZN7WebCore13GraphicsLayerD0Ev},
    {"_ZN7WebCore14LoaderStrategyD0Ev", &_ZN7WebCore14LoaderStrategyD0Ev},
    {"_ZN7WebCore14ScrollableAreaD0Ev", &_ZN7WebCore14ScrollableAreaD0Ev},
    {"_ZN7WebCore15ActiveDOMObjectD0Ev", &_ZN7WebCore15ActiveDOMObjectD0Ev},
    {"_ZN7WebCore15XPathNSResolverD0Ev", &_ZN7WebCore15XPathNSResolverD0Ev},
    {"_ZN7WebCore16VisitedLinkStoreD0Ev", &_ZN7WebCore16VisitedLinkStoreD0Ev},
    {"_ZN7WebCore17FrameLoaderClientD0Ev", &_ZN7WebCore17FrameLoaderClientD0Ev},
    {"_ZN7WebCore17FrameLoaderClientD1Ev", &_ZN7WebCore17FrameLoaderClientD1Ev},
    {"_ZN7WebCore18ImageBufferBackendD0Ev", &_ZN7WebCore18ImageBufferBackendD0Ev},
    {"_ZN7WebCore18PluginInfoProviderD0Ev", &_ZN7WebCore18PluginInfoProviderD0Ev},
    {"_ZN7WebCore19UserContentProviderD0Ev", &_ZN7WebCore19UserContentProviderD0Ev},
    {"_ZN7WebCore20ResourceHandleClientD0Ev", &_ZN7WebCore20ResourceHandleClientD0Ev},
    {"_ZN7WebCore21DisplayRefreshMonitorD0Ev", &_ZN7WebCore21DisplayRefreshMonitorD0Ev},
    {"_ZN7WebCore24StorageNamespaceProviderD0Ev", &_ZN7WebCore24StorageNamespaceProviderD0Ev},
    {"_ZN7WebCore27ContentSecurityPolicyClientD0Ev", &_ZN7WebCore27ContentSecurityPolicyClientD0Ev},
    {"_ZN7WebCore27ContentSecurityPolicyClientD1Ev", &_ZN7WebCore27ContentSecurityPolicyClientD1Ev},
    {"_ZN7WebCore28InspectorFrontendClientLocalD0Ev", &_ZN7WebCore28InspectorFrontendClientLocalD0Ev},
    {"_ZN7WebCore6WidgetD0Ev", &_ZN7WebCore6WidgetD0Ev},
    {"_ZN7WebCore9TimerBaseD0Ev", &_ZN7WebCore9TimerBaseD0Ev},
    {"_ZN9Inspector15InspectorTargetD0Ev", &_ZN9Inspector15InspectorTargetD0Ev},
    {"_ZN9Inspector15InspectorTargetD1Ev", &_ZN9Inspector15InspectorTargetD1Ev},
    {"_ZN9Inspector15RemoteInspector6ClientD0Ev", &_ZN9Inspector15RemoteInspector6ClientD0Ev},
    {"_ZN9Inspector19InspectorAuditAgentD0Ev", &_ZN9Inspector19InspectorAuditAgentD0Ev},
    {"_ZN9Inspector20InjectedScriptModuleD0Ev", &_ZN9Inspector20InjectedScriptModuleD0Ev},
    {"_ZN9Inspector21InspectorRuntimeAgentD0Ev", &_ZN9Inspector21InspectorRuntimeAgentD0Ev},
    {"_ZN9Inspector22InspectorDebuggerAgentD0Ev", &_ZN9Inspector22InspectorDebuggerAgentD0Ev},
    {"_ZN9Inspector22RemoteAutomationTargetD0Ev", &_ZN9Inspector22RemoteAutomationTargetD0Ev},
    {"_ZN9Inspector22RemoteInspectionTargetD1Ev", &_ZN9Inspector22RemoteInspectionTargetD1Ev},
    {"_ZN9Inspector24RemoteControllableTargetD0Ev", &_ZN9Inspector24RemoteControllableTargetD0Ev},
    {"_ZThn16_N9Inspector19InspectorAuditAgentD0Ev", &_ZThn16_N9Inspector19InspectorAuditAgentD0Ev},
    {"_ZThn16_N9Inspector21InspectorRuntimeAgentD0Ev", &_ZThn16_N9Inspector21InspectorRuntimeAgentD0Ev},
    {"_ZThn16_N9Inspector22InspectorDebuggerAgentD0Ev", &_ZThn16_N9Inspector22InspectorDebuggerAgentD0Ev},
    {"_ZThn24_N9Inspector22InspectorDebuggerAgentD0Ev", &_ZThn24_N9Inspector22InspectorDebuggerAgentD0Ev},
};

#ifndef _WIN32
static int Outcome(void (APS5_VABI *function)()) {
    const pid_t child = ::fork();
    Require(child >= 0);
    if (child == 0) {
#ifdef __linux__
        if (::prctl(PR_SET_DUMPABLE, 0) != 0) ::_exit(71);
#endif
        if (!std::freopen("/dev/null", "w", stderr)) ::_exit(72);
        try {
            function();
        } catch (...) {
            ::_exit(73);
        }
        ::_exit(74);
    }
    int status = 0;
    Require(::waitpid(child, &status, 0) == child);
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT ? 0 : 1;
}
#endif

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    std::set<std::string> names;
    std::set<void*> addresses;
    for (const auto& entry : entries) {
        Require(entry.function != nullptr);
        Require(names.insert(entry.name).second);
        Require(addresses.insert(reinterpret_cast<void*>(entry.function)).second);
#ifndef _WIN32
        Require(Outcome(entry.function) == 0);
#endif
    }
    Require(names.size() == sizeof(entries) / sizeof(entries[0]));
    Require(NotImplementedCalls_nid_no_patch() == before);
    return 0;
}
