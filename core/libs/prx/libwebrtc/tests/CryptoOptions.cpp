#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    std::fprintf(stderr, "libwebrtc CryptoOptions check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

bool Same(const WebRtcCryptoOptions& left, const WebRtcCryptoOptions& right) {
    return std::memcmp(&left, &right, sizeof(WebRtcCryptoOptions)) == 0;
}

void TestEveryCombination() {
    for (unsigned bits = 0; bits < 32; ++bits) {
        WebRtcCryptoOptions source {{(bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0, (bits & 8) != 0}, {(bits & 16) != 0}};
        WebRtcCryptoOptions copy;
        std::memset(&copy, 0xA5, sizeof(copy));
        _ZN6webrtc13CryptoOptionsC1ERKS0__nid_postfix(&copy, &source);
        Require(Same(copy, source));
    }
}

void TestSelfCopy() {
    WebRtcCryptoOptions options {{false, true, false, true}, {true}};
    WebRtcCryptoOptions expected = options;
    _ZN6webrtc13CryptoOptionsC1ERKS0__nid_postfix(&options, &options);
    Require(Same(options, expected));
}

void TestNullArgumentsThrow() {
    WebRtcCryptoOptions options {{true, false, true, true}, {false}};
    WebRtcCryptoOptions expected = options;
    RequireInvalidArgument([&] { _ZN6webrtc13CryptoOptionsC1ERKS0__nid_postfix(nullptr, &options); });
    RequireInvalidArgument([&] { _ZN6webrtc13CryptoOptionsC1ERKS0__nid_postfix(&options, nullptr); });
    Require(Same(options, expected));
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

void TestGuardedArgumentsThrow() {
    GuardedPage guard;
    WebRtcCryptoOptions options {{true, false, true, true}, {false}};
    WebRtcCryptoOptions expected = options;
    RequireInvalidArgument([&] { _ZN6webrtc13CryptoOptionsC1ERKS0__nid_postfix(guard.As<WebRtcCryptoOptions>(), &options); });
    RequireInvalidArgument([&] { _ZN6webrtc13CryptoOptionsC1ERKS0__nid_postfix(&options, guard.As<const WebRtcCryptoOptions>()); });
    Require(Same(options, expected));
}

}

int main() {
    TestEveryCombination();
    TestSelfCopy();
    TestNullArgumentsThrow();
    TestGuardedArgumentsThrow();
    return 0;
}
