#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc KeyParams check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

WebRtcKeyParams Filled() {
    WebRtcKeyParams params;
    std::memset(&params, 0xA5, sizeof(params));
    return params;
}

bool Unchanged(const WebRtcKeyParams& params) {
    WebRtcKeyParams filled = Filled();
    return std::memcmp(&params, &filled, sizeof(params)) == 0;
}

void TestEcdsa() {
    WebRtcKeyParams params = Filled();
    _ZN3rtc9KeyParamsC1ENS_7KeyTypeE_nid_postfix(&params, WebRtcKeyTypeEcdsa);
    Require(params.type == WebRtcKeyTypeEcdsa);
    Require(params.curve == WebRtcEcNistP256);
    std::uint32_t publicExponent = 0;
    std::memcpy(&publicExponent, reinterpret_cast<const unsigned char*>(&params) + 8, sizeof(publicExponent));
    Require(publicExponent == 0xA5A5A5A5u);
}

void TestUnknownTypesThrow() {
    for (std::int32_t type : {2, 3, -1, 0x7fffffff}) {
        WebRtcKeyParams params = Filled();
        RequireThrows<std::invalid_argument>([&] { _ZN3rtc9KeyParamsC1ENS_7KeyTypeE_nid_postfix(&params, type); });
        Require(Unchanged(params));
    }
    RequireThrows<std::invalid_argument>([] { _ZN3rtc9KeyParamsC1ENS_7KeyTypeE_nid_postfix(nullptr, WebRtcKeyTypeEcdsa); });
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        address_ = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
        Require(address_ != nullptr);
#else
        void* mapped = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        address_ = mapped;
#endif
    }
    ~GuardedPage() {
        if (!address_)
            return;
#ifdef _WIN32
        VirtualFree(address_, 0, MEM_RELEASE);
#else
        munmap(address_, 4096);
#endif
    }
    template<typename T>
    T* As() const {
        return static_cast<T*>(address_);
    }

private:
    void* address_ = nullptr;
};

void TestGuardedParamsThrow() {
    GuardedPage guard;
    RequireThrows<std::invalid_argument>([&] { _ZN3rtc9KeyParamsC1ENS_7KeyTypeE_nid_postfix(guard.As<WebRtcKeyParams>(), WebRtcKeyTypeEcdsa); });
}

}

int main() {
    TestEcdsa();
    TestUnknownTypesThrow();
    TestGuardedParamsThrow();
    return 0;
}
