#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL OCSP response check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

template<typename TAction> static void RequireNotImplemented(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::runtime_error&) { return; }
    Require(false, location);
}

class ReadOnlyPage {
public:
    explicit ReadOnlyPage(const void* initial = nullptr, std::size_t size = 0) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        _size = info.dwPageSize != 0 ? info.dwPageSize : 4096u;
        _page = VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(_page != nullptr);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(_page, _size, PAGE_READONLY, &oldProtect) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        _size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096u;
        _page = mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(_page != MAP_FAILED);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        Require(mprotect(_page, _size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!_page) return;
#ifdef _WIN32
        VirtualFree(_page, 0, MEM_RELEASE);
#else
        munmap(_page, _size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    template<typename T> T* As() const { return static_cast<T*>(_page); }

private:
    void* _page = nullptr;
    std::size_t _size = 0;
};

constexpr int Enumerated = 10;
constexpr int NegativeEnumerated = 0x10A;
constexpr int Integer = 2;

static int Status(Asn1String* value) {
    LibreSslOcspResponse response {value, nullptr};
    return OCSP_response_status_nid_postfix(&response);
}

static void TestResponseStatus() {
    unsigned char zero[] {0};
    unsigned char three[] {3};
    unsigned char six[] {0, 6};
    unsigned char wide[] {0x01, 0x02, 0x03, 0x04, 0x05};
    unsigned char full[] {0x80, 0, 0, 0, 0, 0, 0, 0};
    unsigned char nine[] {0, 0, 0, 0, 0, 0, 0, 0, 1};
    Asn1String value {1, Enumerated, zero, 0};
    Require(Status(&value) == 0);
    value = {1, Enumerated, three, 0};
    Require(Status(&value) == 3);
    value = {2, Enumerated, six, 0};
    Require(Status(&value) == 6);
    value = {1, NegativeEnumerated, three, 0};
    Require(Status(&value) == -3);
    value = {5, Enumerated, wide, 0};
    Require(Status(&value) == 0x02030405);
    value = {8, Enumerated, full, 0};
    Require(Status(&value) == 0);
    value = {9, Enumerated, nine, 0};
    Require(Status(&value) == -1);
    value = {0, Enumerated, three, 0};
    Require(Status(&value) == 0);
    value = {1, Enumerated, nullptr, 0};
    Require(Status(&value) == 0);
    value = {1, Integer, three, 0};
    Require(Status(&value) == -1);
    value = {9, Integer, nullptr, 0};
    Require(Status(&value) == -1);
    Require(Status(nullptr) == 0);
    RequireThrows([] { OCSP_response_status_nid_postfix(nullptr); });
}

static void TestBasicResponseFree() {
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(1, 2, 3);
    const auto error = ERR_peek_error_nid_postfix();
    OCSP_BASICRESP_free_nid_postfix(nullptr);
    Require(ERR_peek_error_nid_postfix() == error);
}

static void TestGet1Basic() {
    ERR_clear_error_nid_postfix();
    RequireThrows([] { OCSP_response_get1_basic_nid_postfix(nullptr); });
    Require(ERR_peek_error_nid_postfix() == 0);

    LibreSslOcspResponse response {};
    std::array<unsigned char, sizeof(response)> before {};
    std::memcpy(before.data(), &response, sizeof(response));
    LibreSslPutError_nid_no_patch(5, 6, 7);
    constexpr std::uint64_t sentinelBeforeError = (std::uint64_t{5} << 24) | (std::uint64_t{6} << 12) | 7;
    Require(OCSP_response_get1_basic_nid_postfix(&response) == nullptr);
    Require(std::memcmp(before.data(), &response, sizeof(response)) == 0);
    constexpr std::uint64_t noResponseData = (std::uint64_t{39} << 24) | (std::uint64_t{0xfff} << 12) | 108;
    Require(ERR_peek_error_nid_postfix() == sentinelBeforeError);
    Require(LibreSslPeekLastError_nid_no_patch() == noResponseData);
    Require(ERR_get_error_nid_postfix() == sentinelBeforeError);
    Require(ERR_get_error_nid_postfix() == noResponseData);
    Require(ERR_get_error_nid_postfix() == 0);
}

static void TestSingleStatus() {
    Asn1String revoked {}, thisUpdate {}, nextUpdate {}, other {};
    unsigned char keyCompromise[] {1};
    Asn1String reasonValue {1, Enumerated, keyCompromise, 0};
    LibreSslOcspRevokedInfo revokedInfo {&revoked, &reasonValue};
    LibreSslOcspCertStatus status {0, nullptr};
    LibreSslOcspSingleResponse single {nullptr, &status, &thisUpdate, &nextUpdate, nullptr};
    int reason = 77;
    Asn1String* revocationTime = &other;
    Asn1String* thisResult = nullptr;
    Asn1String* nextResult = nullptr;

    Require(OCSP_single_get0_status_nid_postfix(nullptr, &reason, &revocationTime, &thisResult, &nextResult) == -1);
    Require(reason == 77 && revocationTime == &other && thisResult == nullptr && nextResult == nullptr);

    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, &revocationTime, &thisResult, &nextResult) == 0);
    Require(reason == 77 && revocationTime == &other && thisResult == &thisUpdate && nextResult == &nextUpdate);

    status = {2, nullptr};
    single.nextUpdate = nullptr;
    nextResult = &other;
    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, &revocationTime, &thisResult, &nextResult) == 2);
    Require(reason == 77 && revocationTime == &other && nextResult == nullptr);

    status = {1, &revokedInfo};
    single.nextUpdate = &nextUpdate;
    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, &revocationTime, &thisResult, &nextResult) == 1);
    Require(reason == 1 && revocationTime == &revoked && thisResult == &thisUpdate && nextResult == &nextUpdate);

    revokedInfo.revocationReason = nullptr;
    reason = 77;
    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, nullptr, nullptr, nullptr) == 1);
    Require(reason == -1);

    Asn1String wrongType {1, Integer, keyCompromise, 0};
    revokedInfo.revocationReason = &wrongType;
    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, nullptr, nullptr, nullptr) == 1);
    Require(reason == -1);

    Require(OCSP_single_get0_status_nid_postfix(&single, nullptr, nullptr, nullptr, nullptr) == 1);

    status = {1, nullptr};
    Require(OCSP_single_get0_status_nid_postfix(&single, nullptr, nullptr, &thisResult, nullptr) == 1);
    RequireThrows([&] { OCSP_single_get0_status_nid_postfix(&single, &reason, nullptr, nullptr, nullptr); });
    RequireThrows([&] { OCSP_single_get0_status_nid_postfix(&single, nullptr, &revocationTime, nullptr, nullptr); });

    status = {5, nullptr};
    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, &revocationTime, nullptr, nullptr) == 5);

    single.certStatus = nullptr;
    RequireThrows([&] { OCSP_single_get0_status_nid_postfix(&single, nullptr, nullptr, nullptr, nullptr); });
}

static void TestGuestPointers() {
    errno = 73;
    RequireNotImplemented([] { OCSP_response_status_nid_postfix(reinterpret_cast<const LibreSslOcspResponse*>(0x10)); });
    Require(errno == 73);
    LibreSslOcspResponse badStatusResponse {reinterpret_cast<Asn1String*>(0x10), nullptr};
    errno = 73;
    RequireNotImplemented([&] { OCSP_response_status_nid_postfix(&badStatusResponse); });
    Require(errno == 73);
    Asn1String badDataValue {1, Enumerated, reinterpret_cast<unsigned char*>(0x10), 0};
    LibreSslOcspResponse badDataResponse {&badDataValue, nullptr};
    errno = 73;
    RequireNotImplemented([&] { OCSP_response_status_nid_postfix(&badDataResponse); });
    Require(errno == 73);

    Asn1String revoked {}, thisUpdate {}, nextUpdate {}, other {};
    unsigned char keyCompromise[] {1};
    Asn1String reasonValue {1, Enumerated, keyCompromise, 0};
    LibreSslOcspRevokedInfo revokedInfo {&revoked, &reasonValue};
    LibreSslOcspCertStatus certStatus {1, &revokedInfo};
    LibreSslOcspSingleResponse single {nullptr, &certStatus, &thisUpdate, &nextUpdate, nullptr};

    int reason = 77;
    Asn1String* revocationTime = &other;
    Asn1String* thisResult = &other;
    Asn1String* nextResult = &other;
    ReadOnlyPage roPtr(&thisResult, sizeof(thisResult));
    ReadOnlyPage roInt(&reason, sizeof(reason));

    errno = 73;
    RequireNotImplemented([&] { OCSP_single_get0_status_nid_postfix(reinterpret_cast<const LibreSslOcspSingleResponse*>(0x10),
        &reason, &revocationTime, &thisResult, &nextResult); });
    Require(errno == 73 && reason == 77 && revocationTime == &other && thisResult == &other && nextResult == &other);

    LibreSslOcspSingleResponse badStatusSingle {nullptr, reinterpret_cast<LibreSslOcspCertStatus*>(0x10), &thisUpdate, &nextUpdate, nullptr};
    errno = 73;
    RequireNotImplemented([&] { OCSP_single_get0_status_nid_postfix(&badStatusSingle, &reason, &revocationTime, &thisResult, &nextResult); });
    Require(errno == 73 && reason == 77 && revocationTime == &other && thisResult == &other && nextResult == &other);

    LibreSslOcspCertStatus badRevokedStatus {1, reinterpret_cast<void*>(0x10)};
    LibreSslOcspSingleResponse badRevokedSingle {nullptr, &badRevokedStatus, &thisUpdate, &nextUpdate, nullptr};
    errno = 73;
    RequireNotImplemented([&] { OCSP_single_get0_status_nid_postfix(&badRevokedSingle, &reason, &revocationTime, &thisResult, &nextResult); });
    Require(errno == 73 && reason == 77 && revocationTime == &other && thisResult == &other && nextResult == &other);

    LibreSslOcspRevokedInfo badReasonRevoked {&revoked, reinterpret_cast<Asn1String*>(0x10)};
    LibreSslOcspCertStatus badReasonStatus {1, &badReasonRevoked};
    LibreSslOcspSingleResponse badReasonSingle {nullptr, &badReasonStatus, &thisUpdate, &nextUpdate, nullptr};
    errno = 73;
    RequireNotImplemented([&] { OCSP_single_get0_status_nid_postfix(&badReasonSingle, &reason, &revocationTime, &thisResult, &nextResult); });
    Require(errno == 73 && reason == 77 && revocationTime == &other && thisResult == &other && nextResult == &other);

    errno = 73;
    RequireNotImplemented([&] { OCSP_single_get0_status_nid_postfix(&single, &reason, roPtr.As<Asn1String*>(), &thisResult, &nextResult); });
    Require(errno == 73 && reason == 77 && thisResult == &other && nextResult == &other);

    errno = 73;
    RequireNotImplemented([&] { OCSP_single_get0_status_nid_postfix(&single, roInt.As<int>(), &revocationTime, &thisResult, &nextResult); });
    Require(errno == 73 && revocationTime == &other && thisResult == &other && nextResult == &other);

    errno = 73;
    RequireNotImplemented([&] { OCSP_single_get0_status_nid_postfix(&single, &reason, &revocationTime, roPtr.As<Asn1String*>(), &nextResult); });
    Require(errno == 73 && reason == 77 && revocationTime == &other && nextResult == &other);

    errno = 73;
    RequireNotImplemented([&] { OCSP_single_get0_status_nid_postfix(&single, &reason, &revocationTime, &thisResult, roPtr.As<Asn1String*>()); });
    Require(errno == 73 && reason == 77 && revocationTime == &other && thisResult == &other);
}

int main() {
    TestGet1Basic();
    TestResponseStatus();
    TestBasicResponseFree();
    TestSingleStatus();
    TestGuestPointers();
    std::puts("libSceLibreSsl OCSP response tests passed");
}
