#include "prx/libSceNKWebKit/include/WebCoreProcessSwitchesExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

namespace {

using NKWebKit::WebCoreAbi::OptionalUInt64;
using NKWebKit::WebCoreAbi::ResourceLoadObserver;

constexpr unsigned CanAccessRawCookies = 1u << 0;
constexpr unsigned CanAccessCredentials = 1u << 1;
constexpr unsigned CanCommunicateWithWindowServer = 1u << 2;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore process switches check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

bool HasPrivilege(unsigned privilege) {
    return _ZN3WTF19hasProcessPrivilegeENS_16ProcessPrivilegeE(privilege);
}

void TestCookieApiPermission() {
    _ZN3WTF20setProcessPrivilegesENS_9OptionSetINS_16ProcessPrivilegeEEE({CanAccessCredentials | CanCommunicateWithWindowServer});
    Require(!_ZN7WebCore21NetworkStorageSession22processMayUseCookieAPIEv());
    Require(!HasPrivilege(CanAccessRawCookies));

    _ZN7WebCore21NetworkStorageSession27permitProcessToUseCookieAPIEb(true);
    Require(_ZN7WebCore21NetworkStorageSession22processMayUseCookieAPIEv());
    Require(HasPrivilege(CanAccessRawCookies));
    Require(HasPrivilege(CanAccessCredentials));
    Require(HasPrivilege(CanCommunicateWithWindowServer));

    _ZN7WebCore21NetworkStorageSession27permitProcessToUseCookieAPIEb(true);
    Require(_ZN7WebCore21NetworkStorageSession22processMayUseCookieAPIEv());
    Require(HasPrivilege(CanAccessRawCookies));

    _ZN7WebCore21NetworkStorageSession27permitProcessToUseCookieAPIEb(false);
    Require(!_ZN7WebCore21NetworkStorageSession22processMayUseCookieAPIEv());
    Require(!HasPrivilege(CanAccessRawCookies));
    Require(HasPrivilege(CanAccessCredentials));
    Require(HasPrivilege(CanCommunicateWithWindowServer));

    _ZN3WTF20setProcessPrivilegesENS_9OptionSetINS_16ProcessPrivilegeEEE({CanAccessRawCookies});
    Require(!_ZN7WebCore21NetworkStorageSession22processMayUseCookieAPIEv());
    _ZN7WebCore21NetworkStorageSession27permitProcessToUseCookieAPIEb(false);
    Require(!HasPrivilege(CanAccessRawCookies));
    _ZN3WTF20setProcessPrivilegesENS_9OptionSetINS_16ProcessPrivilegeEEE({0});
}

void TestSharedResourceLoadObserver() {
    alignas(8) static unsigned char storage[16];
    alignas(8) static unsigned char other[16];
    auto& observer = *reinterpret_cast<ResourceLoadObserver*>(storage);
    auto& second = *reinterpret_cast<ResourceLoadObserver*>(other);

    Require(_ZN7WebCore20ResourceLoadObserver14sharedIfExistsEv() == nullptr);
    _ZN7WebCore20ResourceLoadObserver9setSharedERS0_(observer);
    Require(_ZN7WebCore20ResourceLoadObserver14sharedIfExistsEv() == &observer);

    bool threw = false;
    try {
        _ZN7WebCore20ResourceLoadObserver9setSharedERS0_(second);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw);
    Require(_ZN7WebCore20ResourceLoadObserver14sharedIfExistsEv() == &observer);
}

void TestMediaEngineConfigurationMock() {
    Require(!NKWebKit::WebCoreAbi::MediaEngineConfigurationMockEnabled());
    _ZN7WebCore31MediaEngineConfigurationFactory10enableMockEv();
    Require(NKWebKit::WebCoreAbi::MediaEngineConfigurationMockEnabled());
    _ZN7WebCore31MediaEngineConfigurationFactory10enableMockEv();
    Require(NKWebKit::WebCoreAbi::MediaEngineConfigurationMockEnabled());
    _ZN7WebCore31MediaEngineConfigurationFactory11disableMockEv();
    Require(!NKWebKit::WebCoreAbi::MediaEngineConfigurationMockEnabled());
    _ZN7WebCore31MediaEngineConfigurationFactory11disableMockEv();
    Require(!NKWebKit::WebCoreAbi::MediaEngineConfigurationMockEnabled());
}

void TestPhysicalFootprint() {
    const OptionalUInt64 footprint = _ZN7WebCore18PerformanceLogging17physicalFootprintEv();
    Require(!footprint.hasValue);
    Require(footprint.value == 0);
}

}  // namespace

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    TestCookieApiPermission();
    TestSharedResourceLoadObserver();
    TestMediaEngineConfigurationMock();
    TestPhysicalFootprint();
    Require(NotImplementedCalls_nid_no_patch() == before);
    return 0;
}
