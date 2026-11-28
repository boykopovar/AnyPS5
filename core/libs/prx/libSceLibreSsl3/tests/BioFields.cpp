#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

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

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIO field check failed at line %u\n", location.line());
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

    LibreSsl::Bio* Set(const LibreSsl::Bio& value) {
        Require(sizeof(value) <= bytes);
        std::memcpy(data, &value, sizeof(value));
        return static_cast<LibreSsl::Bio*>(data);
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

template <typename TCall>
void Rejects(TCall call, std::source_location location = std::source_location::current()) {
    errno = EINVAL;
    bool threw = false;
    try { call(); } catch (const std::runtime_error&) { threw = true; }
    Require(threw && errno == EINVAL, location);
}

void GuestPointers() {
    auto* unmapped = reinterpret_cast<LibreSsl::Bio*>(0x10);
    int payload = 0;
    Rejects([&] { BIO_get_data_nid_postfix(unmapped); });
    Rejects([&] { BIO_set_data_nid_postfix(unmapped, &payload); });
    Rejects([&] { BIO_set_init_nid_postfix(unmapped, 1); });
    Rejects([&] { BIO_get_shutdown_nid_postfix(unmapped); });
    Rejects([&] { BIO_set_shutdown_nid_postfix(unmapped, 1); });
    Rejects([&] { BIO_set_flags_nid_postfix(unmapped, 1); });
    Rejects([&] { BIO_clear_flags_nid_postfix(unmapped, 1); });

    LibreSsl::Bio initial{};
    initial.data = &payload;
    initial.initialized = 1;
    initial.shutdown = 1;
    initial.flags = 0x5;
    ReadOnlyPage page;
    LibreSsl::Bio* readOnly = page.Set(initial);
    page.Protect();
    errno = EINVAL;
    Require(BIO_get_data_nid_postfix(readOnly) == &payload && BIO_get_shutdown_nid_postfix(readOnly) == 1 && errno == EINVAL);
    Rejects([&] { BIO_set_data_nid_postfix(readOnly, nullptr); });
    Rejects([&] { BIO_set_init_nid_postfix(readOnly, 0); });
    Rejects([&] { BIO_set_shutdown_nid_postfix(readOnly, 0); });
    Rejects([&] { BIO_set_flags_nid_postfix(readOnly, 0x2); });
    Rejects([&] { BIO_clear_flags_nid_postfix(readOnly, 0x1); });
    Require(std::memcmp(readOnly, &initial, sizeof(initial)) == 0);
}

}

int main() {
    LibreSsl::Bio bio{};
    int payload = 0;
    bio.data = &payload;
    Require(BIO_get_data_nid_postfix(&bio) == &payload);
    BIO_set_data_nid_postfix(&bio, nullptr);
    Require(bio.data == nullptr);
    BIO_set_data_nid_postfix(&bio, &payload);
    Require(bio.data == &payload);
    BIO_set_init_nid_postfix(&bio, 1);
    Require(bio.initialized == 1);
    BIO_set_init_nid_postfix(&bio, 0);
    Require(bio.initialized == 0 && bio.data == &payload);
    bio.shutdown = 1;
    Require(BIO_get_shutdown_nid_postfix(&bio) == 1);
    bio.shutdown = 0;
    Require(BIO_get_shutdown_nid_postfix(&bio) == 0);
    BIO_set_shutdown_nid_postfix(&bio, 1);
    Require(bio.shutdown == 1 && bio.initialized == 0);
    BIO_set_shutdown_nid_postfix(&bio, 0);
    Require(bio.shutdown == 0);
    bio.flags = 0x1;
    BIO_set_flags_nid_postfix(&bio, 0x6);
    Require(bio.flags == 0x7);
    BIO_clear_flags_nid_postfix(&bio, 0x3);
    Require(bio.flags == 0x4);
    BIO_clear_flags_nid_postfix(&bio, 0x8);
    Require(bio.flags == 0x4);
    GuestPointers();
}
