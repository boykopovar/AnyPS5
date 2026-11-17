#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "Testing.hpp"

#include <cerrno>
#include <cstddef>
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

namespace {

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

struct Fixture {
    unsigned char oidBytes[2] {0x2b, 0x0e};
    unsigned char differentBytes[2] {0x2b, 0x0f};
    unsigned char nameBytes[2] {0x20, 0x21};
    unsigned char keyBytes[2] {0x30, 0x31};
    unsigned char serialBytes[2] {0x01, 0x80};
    LibreSslAsn1Object oid {nullptr, nullptr, 0, 2, oidBytes, 0};
    LibreSslAsn1Object otherOid {nullptr, nullptr, 0, 2, differentBytes, 0};
    LibreSsl3X509Algor algorithm {&oid, nullptr};
    LibreSsl3Asn1Type parameter {5, nullptr};
    LibreSsl3X509Algor queryAlgorithm {&oid, &parameter};
    Asn1String name {2, 4, nameBytes, 0};
    Asn1String key {2, 4, keyBytes, 0};
    Asn1String serial {2, 2, serialBytes, 0};
    Asn1String querySerial = serial;
    OCSP_CERTID stored {&algorithm, &name, &key, &serial};
    OCSP_CERTID query {&queryAlgorithm, &name, &key, &querySerial};
    Asn1String revocation {}, current {}, next {}, sentinel {};
    unsigned char reasonByte = 6;
    Asn1String reasonValue {1, 10, &reasonByte, 0};
    LibreSslOcspRevokedInfo revoked {&revocation, &reasonValue};
    LibreSslOcspCertStatus certStatus {0, nullptr};
    LibreSslOcspCertStatus secondStatus {2, nullptr};
    LibreSslOcspSingleResponse single {&stored, &certStatus, &current, &next, nullptr};
    LibreSslOcspSingleResponse duplicate {&stored, &secondStatus, nullptr, nullptr, nullptr};
    char* entries[2] {reinterpret_cast<char*>(&single), reinterpret_cast<char*>(&duplicate)};
    LibreSslStack stack {1, entries, 0, 2, nullptr};
    LibreSslOcspResponseData data {nullptr, nullptr, nullptr, &stack, nullptr};
    LibreSslOcspBasicResponse response {&data, nullptr, nullptr, nullptr};

    int Find(int* status, int* reason, Asn1String** rev, Asn1String** now, Asn1String** after) {
        return OCSP_resp_find_status_nid_postfix(&response, &query, status, reason, rev, now, after);
    }

    void Miss(LibreSslOcspBasicResponse* bs) {
        int status = 88, reason = 99;
        Asn1String* rev = &sentinel;
        Asn1String* now = &sentinel;
        Asn1String* after = &sentinel;
        Require(OCSP_resp_find_status_nid_postfix(bs, &query, &status, &reason, &rev, &now, &after) == 0);
        Require(status == 88 && reason == 99);
        Require(rev == &sentinel && now == &sentinel && after == &sentinel);
    }

    void Match(int expected, int expectedReason) {
        for (unsigned mask = 0; mask < 32; ++mask) {
            int status = 88, reason = 99;
            Asn1String* rev = &sentinel;
            Asn1String* now = &sentinel;
            Asn1String* after = &sentinel;
            Require(Find(mask & 1 ? &status : nullptr, mask & 2 ? &reason : nullptr,
                mask & 4 ? &rev : nullptr, mask & 8 ? &now : nullptr, mask & 16 ? &after : nullptr) == 1);
            Require(status == (mask & 1 ? expected : 88));
            Require(reason == ((mask & 2) && expected == 1 ? expectedReason : 99));
            Require(rev == ((mask & 4) && expected == 1 ? &revocation : &sentinel));
            Require(now == (mask & 8 ? &current : &sentinel));
            Require(after == (mask & 16 ? single.nextUpdate : &sentinel));
        }
    }
};

void TestFind() {
    Fixture f;
    f.Miss(nullptr);
    f.data.responses = nullptr;
    f.Miss(&f.response);
    f.data.responses = &f.stack;
    f.stack.num = 0;
    f.Miss(&f.response);
    f.stack.num = 1;
    f.Match(0, 99);
    f.queryAlgorithm.algorithm = &f.otherOid;
    f.Miss(&f.response);
    f.queryAlgorithm.algorithm = &f.oid;
    Asn1String different {2, 4, f.differentBytes, 0};
    f.query.issuerNameHash = &different;
    f.Miss(&f.response);
    f.query.issuerNameHash = &f.name;
    f.query.issuerKeyHash = &different;
    f.Miss(&f.response);
    f.query.issuerKeyHash = &f.key;
    f.querySerial.data = f.differentBytes;
    f.Miss(&f.response);
    f.querySerial.data = f.serialBytes;
    f.querySerial.length = 1;
    f.Miss(&f.response);
    f.querySerial.length = 2;
    f.querySerial.type = 0x102;
    f.Miss(&f.response);
    f.serial.type = 0x102;
    f.Match(0, 99);
    f.querySerial.data = f.differentBytes;
    f.Miss(&f.response);
    f.querySerial.data = f.serialBytes;
    f.querySerial.type = 2;
    f.Miss(&f.response);
    f.serial.type = 2;
    f.stack.num = 2;
    f.Match(0, 99);
    f.certStatus.type = 1;
    f.certStatus.value = &f.revoked;
    f.Match(1, 6);
    f.revoked.revocationReason = nullptr;
    f.Match(1, -1);
    f.certStatus.type = 2;
    f.Match(2, 99);
    f.single.nextUpdate = nullptr;
    f.Match(2, 99);
    f.response.tbsResponseData = nullptr;
    bool threw = false;
    try { f.Find(nullptr, nullptr, nullptr, nullptr, nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

void TestGuestPointers() {
    Fixture f;
    f.certStatus.type = 1;
    f.certStatus.value = &f.revoked;

    int status = 88;
    int reason = 99;
    Asn1String* rev = &f.sentinel;
    Asn1String* now = &f.sentinel;
    Asn1String* after = &f.sentinel;
    ReadOnlyPage roInt(&status, sizeof(status));
    ReadOnlyPage roPtr(&rev, sizeof(rev));

    errno = 73;
    ExpectErr<std::runtime_error>([&] {
        OCSP_resp_find_status_nid_postfix(reinterpret_cast<LibreSslOcspBasicResponse*>(0x10), &f.query, &status, &reason, &rev, &now, &after);
    });
    Require(errno == 73 && status == 88 && reason == 99 && rev == &f.sentinel && now == &f.sentinel && after == &f.sentinel);

    LibreSslOcspBasicResponse badTbsBs {reinterpret_cast<LibreSslOcspResponseData*>(0x10), nullptr, nullptr, nullptr};
    errno = 73;
    ExpectErr<std::runtime_error>([&] {
        OCSP_resp_find_status_nid_postfix(&badTbsBs, &f.query, &status, &reason, &rev, &now, &after);
    });
    Require(errno == 73);

    errno = 73;
    ExpectErr<std::runtime_error>([&] {
        OCSP_resp_find_status_nid_postfix(&f.response, reinterpret_cast<OCSP_CERTID*>(0x10), &status, &reason, &rev, &now, &after);
    });
    Require(errno == 73);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { f.Find(&status, &reason, roPtr.As<Asn1String*>(), &now, &after); });
    Require(errno == 73 && status == 88 && reason == 99 && now == &f.sentinel && after == &f.sentinel);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { f.Find(&status, roInt.As<int>(), &rev, &now, &after); });
    Require(errno == 73 && status == 88 && rev == &f.sentinel && now == &f.sentinel && after == &f.sentinel);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { f.Find(&status, &reason, &rev, roPtr.As<Asn1String*>(), &after); });
    Require(errno == 73 && status == 88 && reason == 99 && rev == &f.sentinel && after == &f.sentinel);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { f.Find(&status, &reason, &rev, &now, roPtr.As<Asn1String*>()); });
    Require(errno == 73 && status == 88 && reason == 99 && rev == &f.sentinel && now == &f.sentinel);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { f.Find(roInt.As<int>(), &reason, &rev, &now, &after); });
    Require(errno == 73 && reason == 99 && rev == &f.sentinel && now == &f.sentinel && after == &f.sentinel);

    f.certStatus.type = 0;
    f.certStatus.value = nullptr;
    errno = 73;
    Require(f.Find(&status, roInt.As<int>(), roPtr.As<Asn1String*>(), &now, &after) == 1);
    Require(errno == 73 && status == 0 && now == &f.current && after == &f.next);
}

}

int main() {
    TestFind();
    TestGuestPointers();
}
