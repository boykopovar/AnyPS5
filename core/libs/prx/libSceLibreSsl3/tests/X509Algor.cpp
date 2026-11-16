#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cerrno>
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

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_ALGOR_get0 check failed at line %u\n", location.line());
    std::abort();
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(data != nullptr);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        bytes = static_cast<std::size_t>(page);
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != nullptr && data != MAP_FAILED);
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes);
#endif
    }

    template <typename T>
    T* Set(const T& value) {
        Require(sizeof(value) <= bytes);
        std::memcpy(data, &value, sizeof(value));
        return static_cast<T*>(data);
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(data, bytes, PROT_READ) == 0);
#endif
    }

private:
    void* data = nullptr;
    std::size_t bytes = 0;
};

template <typename TException, typename TCall>
void Rejects(TCall call) {
    errno = 71;
    bool rejected = false;
    try {
        call();
    } catch (const TException&) {
        rejected = errno == 71;
    }
    Require(rejected);
}

int object = 0;
int sentinel = 0;

}

int main() {
    char octets[] = "params";
    LibreSsl3Asn1Type parameter{4, octets};
    LibreSsl3X509Algor algor{&object, &parameter};

    const void* outObject = &sentinel;
    int outType = 99;
    const void* outValue = &sentinel;
    X509_ALGOR_get0_nid_postfix(&outObject, &outType, &outValue, &algor);
    Require(outObject == &object && outType == 4 && outValue == octets);

    LibreSsl3X509Algor bare{&object, nullptr};
    outObject = &sentinel;
    outType = 99;
    outValue = &sentinel;
    X509_ALGOR_get0_nid_postfix(&outObject, &outType, &outValue, &bare);
    Require(outObject == &object && outType == LibreSsl3Asn1Undefined && outValue == nullptr);

    outObject = &sentinel;
    outValue = &sentinel;
    X509_ALGOR_get0_nid_postfix(&outObject, nullptr, &outValue, &algor);
    Require(outObject == &object && outValue == nullptr);

    outType = 99;
    outValue = &sentinel;
    X509_ALGOR_get0_nid_postfix(nullptr, &outType, &outValue, &algor);
    Require(outType == 4 && outValue == octets);

    outType = 99;
    X509_ALGOR_get0_nid_postfix(nullptr, &outType, nullptr, &algor);
    Require(outType == 4);

    outValue = &sentinel;
    X509_ALGOR_get0_nid_postfix(nullptr, nullptr, &outValue, nullptr);
    Require(outValue == nullptr);
    X509_ALGOR_get0_nid_postfix(nullptr, nullptr, nullptr, nullptr);

    const auto* badAlgor = reinterpret_cast<const LibreSsl3X509Algor*>(0x10);
    LibreSsl3X509Algor badParameter{&object, reinterpret_cast<LibreSsl3Asn1Type*>(0x10)};
    auto* badPtrOut = reinterpret_cast<const void**>(0x10);
    auto* badIntOut = reinterpret_cast<int*>(0x10);

    ReadOnlyPage roObjectPage;
    const void** roObject = roObjectPage.Set<const void*>(&sentinel);
    roObjectPage.Protect();
    ReadOnlyPage roTypePage;
    int* roType = roTypePage.Set(77);
    roTypePage.Protect();
    ReadOnlyPage roValuePage;
    const void** roValue = roValuePage.Set<const void*>(&sentinel);
    roValuePage.Protect();

    outObject = &sentinel;
    outType = 99;
    outValue = &sentinel;
    Rejects<std::invalid_argument>([&] { X509_ALGOR_get0_nid_postfix(&outObject, &outType, &outValue, nullptr); });
    Rejects<std::invalid_argument>([&] { X509_ALGOR_get0_nid_postfix(nullptr, &outType, &outValue, nullptr); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(&outObject, &outType, &outValue, badAlgor); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(nullptr, &outType, &outValue, badAlgor); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(&outObject, &outType, &outValue, &badParameter); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(badPtrOut, &outType, &outValue, &algor); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(&outObject, badIntOut, &outValue, &algor); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(&outObject, &outType, badPtrOut, &algor); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(nullptr, nullptr, badPtrOut, &algor); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(roObject, &outType, &outValue, &algor); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(&outObject, roType, &outValue, &algor); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(&outObject, &outType, roValue, &algor); });
    Rejects<std::runtime_error>([&] { X509_ALGOR_get0_nid_postfix(nullptr, nullptr, roValue, nullptr); });
    Require(outObject == &sentinel && outType == 99 && outValue == &sentinel);
    Require(*roObject == &sentinel && *roType == 77 && *roValue == &sentinel);
}
