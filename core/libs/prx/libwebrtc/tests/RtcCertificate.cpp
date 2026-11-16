#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RTCCertificate check failed at line %u\n", location.line());
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

struct FakeIdentity {
    const void* const* vtable;
};

void* deleted = nullptr;
int deletions = 0;

void APS5_VABI DeleteIdentity(void* identity) {
    deleted = identity;
    ++deletions;
}

void APS5_VABI CompleteDestructor(void*) {
    Require(false);
}

void TestDeletesTheIdentityThroughItsDeletingDestructor() {
    const std::array<const void*, 3> vtable {reinterpret_cast<const void*>(&CompleteDestructor), reinterpret_cast<const void*>(&DeleteIdentity), nullptr};
    FakeIdentity identity {vtable.data()};
    WebRtcRtcCertificate certificate {1, &identity};
    _ZN3rtc14RTCCertificateD1Ev_nid_postfix(&certificate);
    Require(deletions == 1 && deleted == &identity);
    Require(certificate.refCount == 1);
}

void TestWithoutIdentityDoesNothing() {
    WebRtcRtcCertificate certificate {0, nullptr};
    const int before = deletions;
    _ZN3rtc14RTCCertificateD1Ev_nid_postfix(&certificate);
    Require(deletions == before);
}

void TestInvalidArgumentsThrow() {
    RequireInvalidArgument([] { _ZN3rtc14RTCCertificateD1Ev_nid_postfix(nullptr); });
    FakeIdentity withoutVtable {nullptr};
    WebRtcRtcCertificate certificate {1, &withoutVtable};
    RequireInvalidArgument([&] { _ZN3rtc14RTCCertificateD1Ev_nid_postfix(&certificate); });
    const std::array<const void*, 2> vtable {reinterpret_cast<const void*>(&CompleteDestructor), nullptr};
    FakeIdentity withoutDestructor {vtable.data()};
    certificate.identity = &withoutDestructor;
    RequireInvalidArgument([&] { _ZN3rtc14RTCCertificateD1Ev_nid_postfix(&certificate); });
}

}

int main() {
    TestDeletesTheIdentityThroughItsDeletingDestructor();
    TestWithoutIdentityDoesNothing();
    TestInvalidArgumentsThrow();
    return 0;
}
