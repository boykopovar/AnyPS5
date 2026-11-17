#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "Testing.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

class ProtectedPages {
public:
    explicit ProtectedPages(std::size_t count) : pageCount(count) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * pageCount, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        pageSize = static_cast<std::size_t>(page);
        base = static_cast<unsigned char*>(mmap(nullptr, pageSize * pageCount, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(base != nullptr && base != MAP_FAILED);
#endif
    }

    ~ProtectedPages() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * pageCount);
#endif
    }

    unsigned char* Page(std::size_t index) { return base + index * pageSize; }

    void ProtectReadOnly(std::size_t index) {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(Page(index), pageSize, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(Page(index), pageSize, PROT_READ) == 0);
#endif
    }

private:
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;
    std::size_t pageCount = 0;
};

template <typename TCall>
void Rejects(TCall call) {
    errno = EINVAL;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(errno == EINVAL);
}

void GuestPointers(LibreSslX509* issuer, LibreSslX509* subject, void* request, void* crl) {
    auto* badContext = reinterpret_cast<LibreSsl3X509V3Context*>(0x10);
    Rejects([&] { X509V3_set_ctx_nid_postfix(badContext, issuer, subject, request, crl, 3); });

    {
        ProtectedPages pages(1);
        auto* roContext = reinterpret_cast<LibreSsl3X509V3Context*>(pages.Page(0));
        roContext->flags = 9;
        roContext->issuerCertificate = issuer;
        pages.ProtectReadOnly(0);
        Rejects([&] { X509V3_set_ctx_nid_postfix(roContext, nullptr, subject, request, crl, 1); });
        Require(roContext->flags == 9 && roContext->issuerCertificate == issuer);
    }

    {
        ProtectedPages pages(2);
        auto* splitContext = reinterpret_cast<LibreSsl3X509V3Context*>(pages.Page(1) - offsetof(LibreSsl3X509V3Context, subjectCertificate));
        splitContext->flags = 77;
        splitContext->issuerCertificate = issuer;
        pages.ProtectReadOnly(1);
        Rejects([&] { X509V3_set_ctx_nid_postfix(splitContext, nullptr, subject, request, crl, 42); });
        Require(splitContext->flags == 77 && splitContext->issuerCertificate == issuer);
    }
}

}

int main() {
    LibreSslX509 issuer{};
    LibreSslX509 subject{};
    int request = 0;
    int crl = 0;
    int database = 0;
    LibreSsl3X509V3Context context{};
    context.database = &database;
    X509V3_set_ctx_nid_postfix(&context, &issuer, &subject, &request, &crl, 1);
    Require(context.flags == 1);
    Require(context.issuerCertificate == &issuer && context.subjectCertificate == &subject);
    Require(context.subjectRequest == &request && context.crl == &crl);
    Require(context.database == &database);
    X509V3_set_ctx_nid_postfix(&context, nullptr, &subject, nullptr, nullptr, 0);
    Require(context.flags == 0);
    Require(context.issuerCertificate == nullptr && context.subjectCertificate == &subject);
    Require(context.subjectRequest == nullptr && context.crl == nullptr);
    Require(context.database == &database);
    GuestPointers(&issuer, &subject, &request, &crl);
    std::puts("LibreSSL3 X509V3_set_ctx tests passed");
}
