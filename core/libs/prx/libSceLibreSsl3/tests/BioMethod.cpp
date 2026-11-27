#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/resource.h>
#endif
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIO_METHOD check failed at line %u\n", location.line());
    std::abort();
}

template <typename TAction>
void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

template <typename TAction>
void RequireInvalidArgumentPreservingErrno(TAction action, std::source_location location = std::source_location::current()) {
    errno = 71;
    RequireInvalidArgument(action, location);
    Require(errno == 71, location);
}

long PeakKilobytes() {
#ifdef __linux__
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
#else
    return 0;
#endif
}

const char Name[] = "test method";
int liveBlocks = 0;
int zeroedCallsBeforeFailure = -1;
int written = 0;
int destroyed = 0;
int payload = 0;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (zeroedCallsBeforeFailure == 0) {
        zeroedCallsBeforeFailure = -1;
        return nullptr;
    }
    if (zeroedCallsBeforeFailure > 0) --zeroedCallsBeforeFailure;
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

int APS5_VABI Write(LibreSsl::Bio*, const char*, int size) {
    written += size;
    return size;
}

int APS5_VABI Read(LibreSsl::Bio*, char*, int) { return 0; }

std::int64_t APS5_VABI Control(LibreSsl::Bio*, int command, std::int64_t argument, void*) { return command + argument; }

int APS5_VABI Create(LibreSsl::Bio* bio) {
    bio->initialized = 1;
    bio->data = &payload;
    return 1;
}

int APS5_VABI Destroy(LibreSsl::Bio*) {
    ++destroyed;
    return 1;
}

void TestAsn1ValuesBounds() {
    {
        constexpr std::size_t largeLength = 1100000;
        std::vector<char> largeText(largeLength + 1, 'a');
        for (std::size_t index = 0; index < largeLength; ++index)
            largeText[index] = static_cast<char>('a' + (index % 26));
        largeText[largeLength] = '\0';
        Asn1String large{0, 4, nullptr, 0};
        errno = 71;
        Require(ASN1_STRING_set_nid_postfix(&large, largeText.data(), -1) == 1);
        Require(errno == 71 && large.length == static_cast<int>(largeLength) && large.data != nullptr);
        Require(std::memcmp(large.data, largeText.data(), largeLength) == 0 && large.data[largeLength] == 0);
        free_nid_postfix(large.data);
        Require(liveBlocks == 0);
    }
    {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const std::size_t page = info.dwPageSize;
        auto* pages = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(pages != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(pages + page, page, PAGE_NOACCESS, &previous) != 0);
#else
        const std::size_t page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        auto* pages = static_cast<unsigned char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(pages != MAP_FAILED);
        Require(mprotect(pages + page, page, PROT_NONE) == 0);
#endif
        constexpr std::size_t tailLength = 13;
        auto* tail = reinterpret_cast<char*>(pages + page - (tailLength + 1));
        std::memset(tail, 'z', tailLength);
        tail[tailLength] = '\0';
        Asn1String pageEnd{0, 4, nullptr, 0};
        errno = 71;
        Require(ASN1_STRING_set_nid_postfix(&pageEnd, tail, -1) == 1);
        Require(errno == 71 && pageEnd.length == static_cast<int>(tailLength) && pageEnd.data != nullptr);
        Require(std::memcmp(pageEnd.data, tail, tailLength) == 0 && pageEnd.data[tailLength] == 0);
        free_nid_postfix(pageEnd.data);

        pageEnd = {0, 4, nullptr, 0};
        Require(ASN1_STRING_set_nid_postfix(&pageEnd, "kept", 4) == 1);
        auto* const keptData = pageEnd.data;
        tail[tailLength] = 'z';
        errno = 71;
        Require(ASN1_STRING_set_nid_postfix(&pageEnd, tail, -1) == 0);
        Require(errno == 71 && pageEnd.data == keptData && pageEnd.length == 4);
        errno = 71;
        Require(ASN1_STRING_set_nid_postfix(&pageEnd, reinterpret_cast<const void*>(std::uintptr_t{0x10}), -1) == 0);
        Require(errno == 71 && pageEnd.data == keptData && pageEnd.length == 4);
        Require(ASN1_STRING_set_nid_postfix(&pageEnd, "kept", -2) == 0);
        Require(pageEnd.data == nullptr && pageEnd.length == 0 && pageEnd.type == 4 && liveBlocks == 0);
        Require(ASN1_STRING_set_nid_postfix(&pageEnd, "kept", 4) == 1);
        Require(ASN1_STRING_set_nid_postfix(&pageEnd, reinterpret_cast<const void*>(std::uintptr_t{0x10}), 3) == 0);
        Require(pageEnd.data == nullptr && pageEnd.length == 0 && pageEnd.type == 4 && liveBlocks == 0);

        unsigned char borrowedBuf[5] = {'b', 'o', 'r', 'r', 0};
        Asn1String borrowed{4, 12, borrowedBuf, 0x10 | 0x24};
        Require(ASN1_STRING_set_nid_postfix(&borrowed, "next", 4) == 1);
        Require(borrowed.type == 12 && borrowed.flags == 0x24 && borrowed.length == 4);
        Require(borrowed.data != borrowedBuf && std::memcmp(borrowed.data, "next", 5) == 0);
        Require(std::memcmp(borrowedBuf, "borr", 5) == 0);
        free_nid_postfix(borrowed.data);
        Require(liveBlocks == 0);
#ifdef _WIN32
        VirtualFree(pages, 0, MEM_RELEASE);
#else
        munmap(pages, page * 2);
#endif
    }
    {
        auto* const unmapped = reinterpret_cast<Asn1String*>(std::uintptr_t{0x10});
        auto* const unmappedData = reinterpret_cast<unsigned char*>(std::uintptr_t{0x10});
        RequireInvalidArgumentPreservingErrno([&] { ASN1_STRING_set_nid_postfix(unmapped, "abc", 3); });
        RequireInvalidArgumentPreservingErrno([&] { ASN1_INTEGER_set_nid_postfix(unmapped, 42); });
        Asn1String badString{4, 4, unmappedData, 0};
        RequireInvalidArgumentPreservingErrno([&] { ASN1_STRING_set_nid_postfix(&badString, "abc", 3); });
        Require(badString.data == unmappedData && badString.length == 4);
        Asn1String badInteger{4, 2, unmappedData, 0};
        RequireInvalidArgumentPreservingErrno([&] { ASN1_INTEGER_set_nid_postfix(&badInteger, 42); });
        Require(badInteger.data == unmappedData && badInteger.length == 4);
        Require(liveBlocks == 0);
    }
    {
        auto* const unmappedData = reinterpret_cast<unsigned char*>(std::uintptr_t{0x10});
        const long rssBefore = PeakKilobytes();
        Asn1String hugeCmpA{0x7fffffff, 4, unmappedData, 0};
        Asn1String hugeCmpB{0x7fffffff, 4, unmappedData, 0};
        RequireInvalidArgumentPreservingErrno([&] { ASN1_OCTET_STRING_cmp_nid_postfix(&hugeCmpA, &hugeCmpB); });
        auto* hugeInt = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
        Require(hugeInt != nullptr);
        *hugeInt = {0x7fffffff, 2, unmappedData, 0};
        RequireInvalidArgumentPreservingErrno([&] { ASN1_INTEGER_free_nid_postfix(hugeInt); });
        Require(hugeInt->data == unmappedData && hugeInt->length == 0x7fffffff);
        free_nid_postfix(hugeInt);
        const long rssAfter = PeakKilobytes();
        Require(rssAfter - rssBefore < 64 * 1024);

        constexpr int multiChunkLength = 9000;
        std::vector<unsigned char> firstBytes(multiChunkLength, 0x3C);
        std::vector<unsigned char> secondBytes(multiChunkLength, 0x3C);
        Asn1String multiA{multiChunkLength, 4, firstBytes.data(), 0};
        Asn1String multiB{multiChunkLength, 4, secondBytes.data(), 0};
        errno = 71;
        Require(ASN1_OCTET_STRING_cmp_nid_postfix(&multiA, &multiB) == 0 && errno == 71);
        secondBytes[5000] = 0x5E;
        errno = 71;
        Require(ASN1_OCTET_STRING_cmp_nid_postfix(&multiA, &multiB) < 0 && errno == 71);
        errno = 71;
        Require(ASN1_OCTET_STRING_cmp_nid_postfix(&multiB, &multiA) > 0 && errno == 71);
        Require(liveBlocks == 0);
    }
}

}

int main() {
    InitializeHeap();
    {
        auto* method = BIO_meth_new_nid_postfix(0x400 | 25, Name);
        Require(method && method->type == (0x400 | 25) && method->name == Name);
        Require(!method->write && !method->read && !method->puts && !method->gets && !method->control);
        Require(!method->create && !method->destroy && !method->callbackControl);
        Require(liveBlocks == 1);
        free_nid_postfix(method);
        zeroedCallsBeforeFailure = 0;
        Require(BIO_meth_new_nid_postfix(1, Name) == nullptr && liveBlocks == 0);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(2, nullptr);
        Require(method && liveBlocks == 1);
        BIO_meth_free_nid_postfix(method);
        Require(liveBlocks == 0);
        BIO_meth_free_nid_postfix(nullptr);
        Require(liveBlocks == 0);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(3, Name);
        Require(BIO_meth_set_write_nid_postfix(method, &Write) == 1 && method->write == &Write);
        auto* bio = BIO_new_nid_postfix(method);
        Require(bio && bio->method == method);
        bio->initialized = 1;
        Require(BIO_write_nid_postfix(bio, "abc", 3) == 3 && written == 3 && bio->bytesWritten == 3);
        Require(BIO_free_nid_postfix(bio) == 1);
        Require(BIO_meth_set_write_nid_postfix(method, nullptr) == 1 && method->write == nullptr);
        BIO_meth_free_nid_postfix(method);
        Require(liveBlocks == 0);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(4, Name);
        Require(BIO_meth_set_read_nid_postfix(method, &Read) == 1 && method->read == &Read && !method->write);
        BIO_meth_free_nid_postfix(method);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(5, Name);
        Require(BIO_meth_set_ctrl_nid_postfix(method, &Control) == 1 && method->control == &Control);
        auto* bio = BIO_new_nid_postfix(method);
        Require(bio && BIO_ctrl_nid_postfix(bio, 7, 9, nullptr) == 16);
        Require(BIO_free_nid_postfix(bio) == 1);
        BIO_meth_free_nid_postfix(method);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(6, Name);
        Require(BIO_meth_set_create_nid_postfix(method, &Create) == 1 && method->create == &Create);
        auto* bio = BIO_new_nid_postfix(method);
        Require(bio && bio->initialized == 1 && bio->data == &payload);
        Require(BIO_free_nid_postfix(bio) == 1);
        BIO_meth_free_nid_postfix(method);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(7, Name);
        Require(BIO_meth_set_destroy_nid_postfix(method, &Destroy) == 1 && method->destroy == &Destroy);
        auto* bio = BIO_new_nid_postfix(method);
        Require(bio && destroyed == 0);
        Require(BIO_free_nid_postfix(bio) == 1 && destroyed == 1);
        BIO_meth_free_nid_postfix(method);
        Require(liveBlocks == 0);
    }
    {
        errno = 71;
        auto* const unmapped = reinterpret_cast<LibreSsl::BioMethod*>(0x10);
        RequireInvalidArgument([&] { BIO_meth_set_write_nid_postfix(unmapped, &Write); });
        RequireInvalidArgument([&] { BIO_meth_set_read_nid_postfix(unmapped, &Read); });
        RequireInvalidArgument([&] { BIO_meth_set_ctrl_nid_postfix(unmapped, &Control); });
        RequireInvalidArgument([&] { BIO_meth_set_create_nid_postfix(unmapped, &Create); });
        RequireInvalidArgument([&] { BIO_meth_set_destroy_nid_postfix(unmapped, &Destroy); });
        Require(errno == 71 && liveBlocks == 0);
    }
    TestAsn1ValuesBounds();
}
