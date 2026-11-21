#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RTCCertificate check failed at line %u\n", location.line());
    std::abort();
}

enum class Access { ReadWrite, ReadOnly, None };

struct GuardedPages {
    std::size_t pageSize = 0;
    std::size_t size = 0;
    unsigned char* base = nullptr;

    explicit GuardedPages(std::size_t bytes = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        size = (bytes + pageSize - 1) / pageSize * pageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        size = (bytes + pageSize - 1) / pageSize * pageSize;
        void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        base = mapped == MAP_FAILED ? nullptr : static_cast<unsigned char*>(mapped);
#endif
        Require(base != nullptr);
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    template<typename T> T* As(std::size_t offset = 0) const { return reinterpret_cast<T*>(base + offset); }

    void Protect(Access access, std::size_t offset = 0) const {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD mode = access == Access::ReadWrite ? PAGE_READWRITE : access == Access::ReadOnly ? PAGE_READONLY : PAGE_NOACCESS;
        Require(VirtualProtect(base + offset, size - offset, mode, &previous) != 0);
#else
        const int mode = access == Access::ReadWrite ? PROT_READ | PROT_WRITE : access == Access::ReadOnly ? PROT_READ : PROT_NONE;
        Require(mprotect(base + offset, size - offset, mode) == 0);
#endif
    }
};

template<typename TAction> bool ThrowsInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
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

void TestInaccessibleObjectsThrow() {
    const int before = deletions;
    GuardedPages hidden;
    hidden.Protect(Access::None);
    auto* hiddenCertificate = hidden.As<WebRtcRtcCertificate>();
    Require(ThrowsInvalidArgument([&] { _ZN3rtc14RTCCertificateD1Ev_nid_postfix(hiddenCertificate); }));
    Require(ThrowsInvalidArgument([&] { _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(hiddenCertificate, 0); }));
    WebRtcRtcCertificate certificate {1, hidden.As<void>()};
    Require(ThrowsInvalidArgument([&] { _ZN3rtc14RTCCertificateD1Ev_nid_postfix(&certificate); }));
    Require(ThrowsInvalidArgument([&] { _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0); }));
    FakeIdentity hiddenVtable {hidden.As<const void*>()};
    certificate.identity = &hiddenVtable;
    Require(ThrowsInvalidArgument([&] { _ZN3rtc14RTCCertificateD1Ev_nid_postfix(&certificate); }));
    Require(ThrowsInvalidArgument([&] { _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0); }));
    std::array<const void*, 3> identityVtable {};
    identityVtable[2] = reinterpret_cast<const void*>(&IdentityCertificate);
    FakeIdentity identity {identityVtable.data()};
    certificate.identity = &identity;
    selectedCertificate = hidden.As<FakeCertificate>();
    Require(ThrowsInvalidArgument([&] { _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0); }));
    Require(deletions == before);
}

int main() {
    TestDeletesTheIdentityThroughItsDeletingDestructor();
    TestWithoutIdentityDoesNothing();
    TestInvalidArgumentsThrow();
    TestHasExpiredDispatchesThroughIdentityAndCertificateVtables();
    TestInaccessibleObjectsThrow();
    return 0;
}
