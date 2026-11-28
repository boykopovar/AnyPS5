#include "prx/libSceFaceTracker/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

constexpr int kErrInvalid = static_cast<int>(0x812a0001);
constexpr int kErrNullContext = static_cast<int>(0x812a0002);
constexpr int kErrNotInitialized = static_cast<int>(0x812a0003);

constexpr std::size_t kContextSize = 0x2000;
constexpr std::size_t kInitFlag = 0x180c;

using RegisterCallback = decltype(&sceFaceTrackerRegisterStartTrackingCallback);

const RegisterCallback kRegistrations[5] = {
    sceFaceTrackerRegisterStartTrackingCallback,
    sceFaceTrackerRegisterFixUserIdCallback,
    sceFaceTrackerRegisterStopTrackingCallback,
    sceFaceTrackerRegisterLostTargetCallback,
    sceFaceTrackerRegisterRecoveryFromLostTargetCallback,
};

constexpr std::size_t kCallbackOffsets[5] = {0x1810, 0x1818, 0x1820, 0x1828, 0x1830};
constexpr std::size_t kArgumentOffsets[5] = {0x1838, 0x1840, 0x1848, 0x1850, 0x1858};

void* StoredPtr(const std::vector<std::uint8_t>& context, std::size_t off) {
    void* p;
    std::memcpy(&p, context.data() + off, sizeof(p));
    return p;
}

}

int main() {
    std::vector<std::uint8_t> markers(64, 0);
    void* callbacks[5];
    void* arguments[5];
    for (int i = 0; i < 5; ++i) {
        callbacks[i] = markers.data() + static_cast<std::size_t>(i) * 8;
        arguments[i] = markers.data() + 32 + static_cast<std::size_t>(i) * 8;
    }

    for (int i = 0; i < 5; ++i) {
        Require(kRegistrations[i](callbacks[i], arguments[i], nullptr) == kErrNullContext);
    }

    std::vector<std::uint8_t> context(kContextSize, 0);
    for (int i = 0; i < 5; ++i) {
        Require(kRegistrations[i](callbacks[i], arguments[i], context.data()) == kErrNotInitialized);
    }

    context[kInitFlag] = 1;
    for (int i = 0; i < 5; ++i) {
        Require(kRegistrations[i](callbacks[i], arguments[i], context.data()) == 0);
        Require(StoredPtr(context, kCallbackOffsets[i]) == callbacks[i]);
        Require(StoredPtr(context, kArgumentOffsets[i]) == arguments[i]);
    }

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            if (i == j) {
                continue;
            }
            Require(StoredPtr(context, kCallbackOffsets[j]) == callbacks[j]);
            Require(StoredPtr(context, kArgumentOffsets[j]) == arguments[j]);
        }
    }

    void* replacement = markers.data() + 56;
    void* replacementArg = markers.data() + 48;
    Require(sceFaceTrackerRegisterStartTrackingCallback(replacement, replacementArg, context.data()) == 0);
    Require(StoredPtr(context, 0x1810) == replacement);
    Require(StoredPtr(context, 0x1838) == replacementArg);
    Require(StoredPtr(context, 0x1818) == callbacks[1]);
    Require(StoredPtr(context, 0x1858) == arguments[4]);

#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    const std::size_t protectedBytes = ((kContextSize + pageSize - 1) / pageSize) * pageSize;
    auto* const readOnlyContext = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, protectedBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(readOnlyContext != nullptr);
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    const std::size_t pageSize = static_cast<std::size_t>(size);
    const std::size_t protectedBytes = ((kContextSize + pageSize - 1) / pageSize) * pageSize;
    auto* const readOnlyContext = static_cast<std::uint8_t*>(
        mmap(nullptr, protectedBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(readOnlyContext != MAP_FAILED);
#endif
    std::memset(readOnlyContext, 0, protectedBytes);
    readOnlyContext[kInitFlag] = 1;
#ifdef _WIN32
    DWORD oldProtection = 0;
    Require(VirtualProtect(readOnlyContext, protectedBytes, PAGE_READONLY, &oldProtection) != 0);
#else
    Require(mprotect(readOnlyContext, protectedBytes, PROT_READ) == 0);
#endif
    errno = 71;
    for (int i = 0; i < 5; ++i) {
        Require(kRegistrations[i](callbacks[i], arguments[i], readOnlyContext) == kErrInvalid);
        Require(errno == 71);
    }

    for (int i = 0; i < 5; ++i) {
        const std::size_t allocationBytes = pageSize * 3;
#ifdef _WIN32
        auto* const splitContextAllocation = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, allocationBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(splitContextAllocation != nullptr);
#else
        auto* const splitContextAllocation = static_cast<std::uint8_t*>(
            mmap(nullptr, allocationBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(splitContextAllocation != MAP_FAILED);
#endif
        const std::size_t baseOffset = pageSize - sizeof(void*) - (kCallbackOffsets[i] % pageSize);
        auto* const splitContext = splitContextAllocation + baseOffset;
        std::memset(splitContextAllocation, 0, allocationBytes);
        splitContext[kInitFlag] = 1;
        void* const previousCallback = markers.data() + 8;
        void* const previousArgument = markers.data() + 16;
        std::memcpy(splitContext + kCallbackOffsets[i], &previousCallback, sizeof(previousCallback));
        std::memcpy(splitContext + kArgumentOffsets[i], &previousArgument, sizeof(previousArgument));
#ifdef _WIN32
        DWORD secondPageProtection = 0;
        Require(VirtualProtect(splitContextAllocation + 2 * pageSize, pageSize, PAGE_READONLY,
                               &secondPageProtection) != 0);
#else
        Require(mprotect(splitContextAllocation + 2 * pageSize, pageSize, PROT_READ) == 0);
#endif
        errno = 71;
        Require(kRegistrations[i](callbacks[i], arguments[i], splitContext) == kErrInvalid);
        Require(errno == 71);
        void* storedCallback = nullptr;
        void* storedArgument = nullptr;
        std::memcpy(&storedCallback, splitContext + kCallbackOffsets[i], sizeof(storedCallback));
        std::memcpy(&storedArgument, splitContext + kArgumentOffsets[i], sizeof(storedArgument));
        Require(storedCallback == callbacks[i]);
        Require(storedArgument == previousArgument);
#ifdef _WIN32
        Require(VirtualFree(splitContextAllocation, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(splitContextAllocation, allocationBytes) == 0);
#endif
    }

#ifdef _WIN32
    Require(VirtualFree(readOnlyContext, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(readOnlyContext, protectedBytes) == 0);
#endif
    return 0;
}
