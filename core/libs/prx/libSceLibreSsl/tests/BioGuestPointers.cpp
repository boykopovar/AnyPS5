#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

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
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using LibreSsl::Bio;
using LibreSsl::BioBuffer;
using LibreSsl::BioInfoCallback;
using LibreSsl::BioMethod;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL BIO guest pointer check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

int allocations = 0;

void* APS5_VABI Allocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    if (pointer) ++allocations;
    return pointer;
}

void APS5_VABI Free(void* pointer) {
    if (pointer) --allocations;
    std::free(pointer);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count ? count : 1, size ? size : 1);
    if (pointer) ++allocations;
    return pointer;
}

void* APS5_VABI Reallocate(void* pointer, std::size_t size) {
    void* moved = std::realloc(pointer, size ? size : 1);
    if (moved && !pointer) ++allocations;
    return moved;
}

void* APS5_VABI Align(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

void InitializeHeap() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(AllocateZeroed),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

int methodCalls = 0;
int callbackCalls = 0;
std::int64_t callbackResult = 1;

int APS5_VABI StubWrite(Bio*, const char*, int size) {
    ++methodCalls;
    return size;
}

int APS5_VABI StubPuts(Bio*, const char* source) {
    ++methodCalls;
    return source ? static_cast<int>(std::strlen(source)) : 0;
}

std::int64_t APS5_VABI StubControl(Bio*, int, std::int64_t argument, void*) {
    ++methodCalls;
    return argument + 10;
}

int APS5_VABI StubDestroy(Bio*) {
    ++methodCalls;
    return 1;
}

std::int64_t APS5_VABI StubCallbackControl(Bio*, int, BioInfoCallback) {
    ++methodCalls;
    return 1;
}

int alternateMethodCalls = 0;
const BioMethod* swappedMethod = nullptr;

int APS5_VABI AlternateWrite(Bio*, const char*, int size) {
    ++alternateMethodCalls;
    return size;
}

std::int64_t APS5_VABI StubBioCallback(Bio*, int, const char*, int, std::int64_t, std::int64_t) {
    ++callbackCalls;
    return callbackResult;
}

std::int64_t APS5_VABI InitializingCallback(Bio* bio, int operation, const char*, int, std::int64_t, std::int64_t result) {
    if (operation == 3) bio->initialized = 1;
    return result;
}

std::int64_t APS5_VABI ReconfiguringCallback(Bio* bio, int operation, const char*, int, std::int64_t, std::int64_t result) {
    if (operation == 3) {
        bio->initialized = 1;
        bio->method = swappedMethod;
    }
    return result;
}

void APS5_VABI DummyInfo(Bio*, int, const char*, int, std::int64_t, std::int64_t) {}

template <typename TCall>
void ExpectRuntimeError(TCall call, std::source_location location = std::source_location::current()) {
    methodCalls = 0;
    callbackCalls = 0;
    errno = E2BIG;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, location);
    Require(errno == E2BIG, location);
    Require(methodCalls == 0 && callbackCalls == 0, location);
}

struct GuardedPages {
    std::size_t pageSize = 0;
    std::uint8_t* base = nullptr;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
#else
        const long size = sysconf(_SC_PAGESIZE);
        pageSize = size > 0 ? static_cast<std::size_t>(size) : 4096u;
        void* mapped = mmap(nullptr, 2 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<std::uint8_t*>(mapped);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, 2 * pageSize);
#endif
    }

    void SetPage(std::size_t index, bool read, bool write) {
        auto* page = base + index * pageSize;
#ifdef _WIN32
        DWORD protect = PAGE_NOACCESS;
        if (read && write) protect = PAGE_READWRITE;
        else if (read) protect = PAGE_READONLY;
        DWORD previous = 0;
        Require(VirtualProtect(page, pageSize, protect, &previous) != 0);
#else
        int protect = PROT_NONE;
        if (read && write) protect = PROT_READ | PROT_WRITE;
        else if (read) protect = PROT_READ;
        Require(mprotect(page, pageSize, protect) == 0);
#endif
    }
};

void CheckBioGuestPointers() {
    GuardedPages pages;
    const std::size_t page = pages.pageSize;

    const BioMethod customMethod{0x7f, "custom", StubWrite, nullptr, StubPuts, nullptr, StubControl, nullptr, StubDestroy, StubCallbackControl};
    Bio validBio{};
    validBio.method = &customMethod;
    validBio.initialized = 1;
    validBio.shutdown = 1;
    validBio.references = 1;

    BioBuffer validBuffer{};
    LibreSslSslInternal validInternal{};
    LibreSslSsl validSsl{};
    validSsl.internal = &validInternal;

    std::memcpy(pages.base, &customMethod, sizeof(customMethod));
    std::memcpy(pages.base + 256, &validBio, sizeof(validBio));
    std::memcpy(pages.base + 512, &validBuffer, sizeof(validBuffer));
    std::memcpy(pages.base + 640, &validInternal, sizeof(validInternal));
    std::memcpy(pages.base + 1280, &validSsl, sizeof(validSsl));
    pages.SetPage(0, true, false);
    pages.SetPage(1, false, false);

    const auto* roMethod = reinterpret_cast<const BioMethod*>(pages.base);
    auto* roBio = reinterpret_cast<Bio*>(pages.base + 256);
    auto* roBuffer = reinterpret_cast<BioBuffer*>(pages.base + 512);
    auto* roInternal = reinterpret_cast<LibreSslSslInternal*>(pages.base + 640);
    auto* roSsl = reinterpret_cast<LibreSslSsl*>(pages.base + 1280);
    auto* noAccess = pages.base + page;
    auto* unmapped = reinterpret_cast<void*>(std::uintptr_t{1});

    bool invalidMethodThrew = false;
    try {
        BIO_new_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        invalidMethodThrew = true;
    }
    Require(invalidMethodThrew && allocations == 0);

    ExpectRuntimeError([&] { BIO_new_nid_postfix(reinterpret_cast<const BioMethod*>(unmapped)); });
    Require(allocations == 0);
    ExpectRuntimeError([&] { BIO_new_nid_postfix(reinterpret_cast<const BioMethod*>(noAccess)); });
    Require(allocations == 0);

    Bio* createdRo = BIO_new_nid_postfix(roMethod);
    Require(createdRo != nullptr && allocations == 1);
    createdRo->initialized = 1;
    methodCalls = 0;
    Require(BIO_ctrl_nid_postfix(createdRo, 1, 5, nullptr) == 15 && methodCalls == 1);
    Require(BIO_write_nid_postfix(createdRo, "abcd", 4) == 4 && createdRo->bytesWritten == 4 && methodCalls == 2);
    Require(BIO_puts_nid_postfix(createdRo, "xyz") == 3 && createdRo->bytesWritten == 7 && methodCalls == 3);
    Require(BIO_free_nid_postfix(createdRo) == 1 && methodCalls == 4 && allocations == 0);

    Require(BIO_free_nid_postfix(nullptr) == 0);
    Require(BIO_ctrl_nid_postfix(nullptr, 1, 0, nullptr) == 0);
    Require(BIO_write_nid_postfix(reinterpret_cast<Bio*>(unmapped), "abc", -1) == 0);
    Require(BIO_puts_nid_postfix(nullptr, "abc") == -2);

    std::array<unsigned char, sizeof(Bio)> roSnapshot{};
    std::memcpy(roSnapshot.data(), roBio, sizeof(Bio));
    for (Bio* badBio : {reinterpret_cast<Bio*>(unmapped), reinterpret_cast<Bio*>(noAccess), roBio}) {
        ExpectRuntimeError([&] { BIO_free_nid_postfix(badBio); });
        ExpectRuntimeError([&] { BIO_ctrl_nid_postfix(badBio, 8, 0, nullptr); });
        ExpectRuntimeError([&] { BIO_write_nid_postfix(badBio, "abc", 3); });
        ExpectRuntimeError([&] { BIO_puts_nid_postfix(badBio, "abc"); });
    }
    Require(std::memcmp(roBio, roSnapshot.data(), sizeof(Bio)) == 0);

    Bio* bio = BIO_new_nid_postfix(roMethod);
    Require(bio != nullptr && allocations == 1);
    bio->initialized = 1;
    for (const BioMethod* badMethod : {reinterpret_cast<const BioMethod*>(unmapped), reinterpret_cast<const BioMethod*>(noAccess)}) {
        bio->method = badMethod;
        ExpectRuntimeError([&] { BIO_free_nid_postfix(bio); });
        Require(bio->references == 1 && allocations == 1);
        ExpectRuntimeError([&] { BIO_ctrl_nid_postfix(bio, 1, 0, nullptr); });
        ExpectRuntimeError([&] { BIO_write_nid_postfix(bio, "a", 1); });
        ExpectRuntimeError([&] { BIO_puts_nid_postfix(bio, "a"); });

        bio->callback = StubBioCallback;
        callbackResult = 1;
        ExpectRuntimeError([&] { BIO_ctrl_nid_postfix(bio, 1, 0, nullptr); });
        ExpectRuntimeError([&] { BIO_write_nid_postfix(bio, "a", 1); });
        ExpectRuntimeError([&] { BIO_puts_nid_postfix(bio, "a"); });
        bio->callback = nullptr;
    }

    bio->callback = StubBioCallback;
    callbackResult = 0;
    bio->method = reinterpret_cast<const BioMethod*>(unmapped);
    callbackCalls = 0;
    Require(BIO_free_nid_postfix(bio) == 0 && callbackCalls == 1 && bio->references == 0 && allocations == 1);

    bio->references = 1;
    callbackResult = 1;
    callbackCalls = 0;
    errno = E2BIG;
    bool threwAfterCallback = false;
    try {
        BIO_free_nid_postfix(bio);
    } catch (const std::runtime_error&) {
        threwAfterCallback = true;
    }
    Require(threwAfterCallback && errno == E2BIG && callbackCalls == 1 && allocations == 1);

    bio->callback = nullptr;
    bio->method = roMethod;
    bio->references = 2;
    methodCalls = 0;
    Require(BIO_free_nid_postfix(bio) == 1 && methodCalls == 0 && bio->references == 1 && allocations == 1);
    Require(BIO_free_nid_postfix(bio) == 1 && methodCalls == 1 && allocations == 0);

    pages.SetPage(0, true, true);
    pages.SetPage(1, true, true);
    auto* straddleMethod = reinterpret_cast<BioMethod*>(pages.base + page - 16);
    std::memcpy(straddleMethod, &customMethod, sizeof(customMethod));
    pages.SetPage(0, true, false);
    pages.SetPage(1, false, false);
    ExpectRuntimeError([&] { BIO_new_nid_postfix(straddleMethod); });
    Require(allocations == 0);
    Bio* straddleMethodOwner = BIO_new_nid_postfix(&customMethod);
    straddleMethodOwner->initialized = 1;
    straddleMethodOwner->method = straddleMethod;
    ExpectRuntimeError([&] { BIO_free_nid_postfix(straddleMethodOwner); });
    ExpectRuntimeError([&] { BIO_ctrl_nid_postfix(straddleMethodOwner, 1, 0, nullptr); });
    ExpectRuntimeError([&] { BIO_write_nid_postfix(straddleMethodOwner, "a", 1); });
    ExpectRuntimeError([&] { BIO_puts_nid_postfix(straddleMethodOwner, "a"); });
    straddleMethodOwner->method = &customMethod;
    Require(BIO_free_nid_postfix(straddleMethodOwner) == 1 && allocations == 0);

    pages.SetPage(0, true, true);
    pages.SetPage(1, true, true);
    auto* straddleBio = reinterpret_cast<Bio*>(pages.base + page - 16);
    std::memcpy(straddleBio, &validBio, sizeof(validBio));
    std::array<unsigned char, 16> straddlePrefix{};
    std::memcpy(straddlePrefix.data(), straddleBio, 16);
    pages.SetPage(1, true, false);
    ExpectRuntimeError([&] { BIO_free_nid_postfix(straddleBio); });
    ExpectRuntimeError([&] { BIO_ctrl_nid_postfix(straddleBio, 1, 0, nullptr); });
    ExpectRuntimeError([&] { BIO_write_nid_postfix(straddleBio, "a", 1); });
    ExpectRuntimeError([&] { BIO_puts_nid_postfix(straddleBio, "a"); });
    Require(std::memcmp(straddleBio, straddlePrefix.data(), 16) == 0);

    pages.SetPage(1, false, false);
    ExpectRuntimeError([&] { BIO_free_nid_postfix(straddleBio); });
    ExpectRuntimeError([&] { BIO_ctrl_nid_postfix(straddleBio, 1, 0, nullptr); });
    ExpectRuntimeError([&] { BIO_write_nid_postfix(straddleBio, "a", 1); });
    ExpectRuntimeError([&] { BIO_puts_nid_postfix(straddleBio, "a"); });
    Require(std::memcmp(straddleBio, straddlePrefix.data(), 16) == 0);

    char readOut[8]{};
    const BioMethod* memMethod = BIO_s_mem_nid_postfix();
    ExpectRuntimeError([&] { memMethod->read(reinterpret_cast<Bio*>(noAccess), readOut, 4); });
    ExpectRuntimeError([&] { memMethod->write(reinterpret_cast<Bio*>(noAccess), "a", 1); });
    ExpectRuntimeError([&] { memMethod->control(reinterpret_cast<Bio*>(noAccess), 2, 0, nullptr); });

    pages.SetPage(0, true, false);
    Bio* memBio = BIO_new_nid_postfix(memMethod);
    Require(memBio != nullptr && memBio->data != nullptr);
    void* realMemData = memBio->data;
    for (void* badBuffer : {static_cast<void*>(noAccess), static_cast<void*>(roBuffer)}) {
        memBio->data = badBuffer;
        ExpectRuntimeError([&] { BIO_ctrl_nid_postfix(memBio, 2, 0, nullptr); });
        ExpectRuntimeError([&] { BIO_write_nid_postfix(memBio, "a", 1); });
        ExpectRuntimeError([&] { memMethod->read(memBio, readOut, 4); });
    }
    memBio->data = realMemData;
    Require(BIO_free_nid_postfix(memBio) == 1 && allocations == 0);

    const BioMethod* sslMethod = BIO_f_ssl_nid_postfix();
    Bio* sslBio = BIO_new_nid_postfix(sslMethod);
    Require(sslBio != nullptr && sslBio->data != nullptr);
    void** attachedSsl = static_cast<void**>(sslBio->data);

    *attachedSsl = noAccess;
    ExpectRuntimeError([&] { sslMethod->callbackControl(sslBio, 14, DummyInfo); });

    LibreSslSsl sslCopy{};
    sslCopy.internal = roInternal;
    *attachedSsl = &sslCopy;
    ExpectRuntimeError([&] { sslMethod->callbackControl(sslBio, 14, DummyInfo); });

    sslCopy.internal = &validInternal;
    sslCopy.rbio = roBio;
    ExpectRuntimeError([&] { sslMethod->callbackControl(sslBio, 15, DummyInfo); });

    Bio* forward = BIO_new_nid_postfix(roMethod);
    Require(forward != nullptr);
    forward->method = reinterpret_cast<const BioMethod*>(noAccess);
    sslCopy.rbio = forward;
    ExpectRuntimeError([&] { sslMethod->callbackControl(sslBio, 15, DummyInfo); });
    forward->method = roMethod;
    methodCalls = 0;
    Require(sslMethod->callbackControl(sslBio, 15, DummyInfo) == 1 && methodCalls == 1);

    validInternal.infoCallback = nullptr;
    *attachedSsl = roSsl;
    Require(sslMethod->callbackControl(sslBio, 14, DummyInfo) == 1 && validInternal.infoCallback == reinterpret_cast<void*>(DummyInfo));

    pages.SetPage(0, true, true);
    pages.SetPage(1, true, true);
    auto* straddleSsl = reinterpret_cast<LibreSslSsl*>(pages.base + page - 16);
    std::memcpy(straddleSsl, &validSsl, sizeof(validSsl));
    pages.SetPage(0, true, false);
    pages.SetPage(1, false, false);
    *attachedSsl = straddleSsl;
    ExpectRuntimeError([&] { sslMethod->callbackControl(sslBio, 14, DummyInfo); });

    *attachedSsl = nullptr;
    Require(BIO_free_nid_postfix(forward) == 1 && BIO_free_nid_postfix(sslBio) == 1 && allocations == 0);

    {
        const BioMethod probeMethod{0x7f, "probe", StubWrite, nullptr, StubPuts, nullptr, StubControl, nullptr, StubDestroy, StubCallbackControl};
        Bio probe{};
        probe.method = &probeMethod;
        probe.references = 1;
        probe.callback = InitializingCallback;
        methodCalls = 0;
        Require(BIO_write_nid_postfix(&probe, "abc", 3) == 3 && methodCalls == 1 && probe.bytesWritten == 3);
    }

    {
        const BioMethod replacementMethod{0x7f, "replacement", AlternateWrite, nullptr, StubPuts, nullptr, StubControl, nullptr, StubDestroy, StubCallbackControl};
        Bio swapBio{};
        swapBio.method = &customMethod;
        swapBio.references = 1;
        swapBio.callback = ReconfiguringCallback;
        swappedMethod = &replacementMethod;
        methodCalls = 0;
        alternateMethodCalls = 0;
        Require(BIO_write_nid_postfix(&swapBio, "abcd", 4) == 4 && methodCalls == 0 && alternateMethodCalls == 1 && swapBio.bytesWritten == 4);

        swapBio.initialized = 0;
        swapBio.method = &customMethod;
        swappedMethod = reinterpret_cast<const BioMethod*>(noAccess);
        alternateMethodCalls = 0;
        ExpectRuntimeError([&] { BIO_write_nid_postfix(&swapBio, "efgh", 4); });
        Require(alternateMethodCalls == 0 && swapBio.bytesWritten == 4);
        swappedMethod = nullptr;
    }
}

}

int main() {
    InitializeHeap();
    CheckBioGuestPointers();
    std::puts("LibreSSL BIO guest pointers: ok");
}
