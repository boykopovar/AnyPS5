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

struct FakeCertificate {
    const void* const* vtable;
};

void* deleted = nullptr;
int deletions = 0;
FakeCertificate* selectedCertificate = nullptr;
std::int64_t selectedExpiration = 0;

const void* APS5_VABI IdentityCertificate(const void*) {
    return selectedCertificate;
}

std::int64_t APS5_VABI CertificateExpiration(const void*) {
    return selectedExpiration;
}

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

void TestHasExpiredDispatchesThroughIdentityAndCertificateVtables() {
    std::array<const void*, 8> certificateVtable{};
    certificateVtable[7] = reinterpret_cast<const void*>(&CertificateExpiration);
    FakeCertificate nativeCertificate{certificateVtable.data()};
    selectedCertificate = &nativeCertificate;

    std::array<const void*, 3> identityVtable{};
    identityVtable[2] = reinterpret_cast<const void*>(&IdentityCertificate);
    FakeIdentity identity{identityVtable.data()};
    WebRtcRtcCertificate certificate{1, &identity};

    selectedExpiration = 12;
    Require(!_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 11999));
    Require(_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 12000));
    Require(_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 12001));

    selectedExpiration = -1;
    Require(_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0));
}

}

int main() {
    TestDeletesTheIdentityThroughItsDeletingDestructor();
    TestWithoutIdentityDoesNothing();
    TestInvalidArgumentsThrow();
    TestHasExpiredDispatchesThroughIdentityAndCertificateVtables();
    return 0;
}
