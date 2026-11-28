#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "currentUserGesture guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename TAction>
static bool Rejected(TAction action) {
    errno = 71;
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    return rejected && errno == 71;
}

static WebCore::UserGestureToken* Sentinel() {
    return reinterpret_cast<WebCore::UserGestureToken*>(static_cast<std::uintptr_t>(0x5A5A5A5A5A5A5A5Aull));
}

static unsigned char* MapReadOnlyPage(WebCore::UserGestureToken* value) {
#ifdef _WIN32
    auto* page = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(page != nullptr);
    std::memcpy(page, &value, sizeof(value));
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    void* mapped = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* page = static_cast<unsigned char*>(mapped);
    std::memcpy(page, &value, sizeof(value));
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
    return page;
}

static void UnmapPage(unsigned char* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

int main() {
    Require(Rejected([] { _ZN7WebCore20UserGestureIndicator18currentUserGestureEv(nullptr); }));

    auto** unmapped = reinterpret_cast<WebCore::UserGestureToken**>(std::uintptr_t{0x10});
    Require(Rejected([&] { _ZN7WebCore20UserGestureIndicator18currentUserGestureEv(unmapped); }));

    unsigned char* page = MapReadOnlyPage(Sentinel());
    auto** readOnly = reinterpret_cast<WebCore::UserGestureToken**>(page);
    Require(Rejected([&] { _ZN7WebCore20UserGestureIndicator18currentUserGestureEv(readOnly); }));
    Require(*readOnly == Sentinel());
    UnmapPage(page);

    _ZN3WTF20initializeMainThreadEv();
    Require(Rejected([&] { _ZN7WebCore20UserGestureIndicator18currentUserGestureEv(unmapped); }));

    WebCore::UserGestureToken* slot = Sentinel();
    errno = 71;
    Require(_ZN7WebCore20UserGestureIndicator18currentUserGestureEv(&slot) == &slot);
    Require(slot == nullptr && errno == 71);

    std::puts("currentUserGesture guest pointer tests passed");
}
