#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::TiledBackingStoreAbi::TiledBackingStore;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "TiledBackingStore check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

TiledBackingStore MakeStore(float contentsScale) {
    TiledBackingStore store{};
    store.tileSize = WebCore::IntSize{512, 512};
    store.coverAreaMultiplier = 4.0f;
    store.trajectoryVector = WebCore::FloatPoint{3.0f, 3.0f};
    store.pendingTrajectoryVector = WebCore::FloatPoint{5.0f, 5.0f};
    store.visibleRect = WebCore::IntRect{{1, 2}, {3, 4}};
    store.coverRect = WebCore::IntRect{{5, 6}, {7, 8}};
    store.keepRect = WebCore::IntRect{{9, 10}, {11, 12}};
    store.rect = WebCore::IntRect{{13, 14}, {15, 16}};
    store.previousRect = WebCore::IntRect{{17, 18}, {19, 20}};
    store.contentsScale = contentsScale;
    store.pendingTileCreation = true;
    return store;
}

bool MapsTo(float contentsScale, int x, int y, int width, int height, int expectedX, int expectedY, int expectedWidth, int expectedHeight) {
    const TiledBackingStore store = MakeStore(contentsScale);
    const WebCore::IntRect input{{x, y}, {width, height}};
    const WebCore::IntRect mapped = _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(&store, &input);
    return mapped.m_location.m_x == expectedX && mapped.m_location.m_y == expectedY
        && mapped.m_size.m_width == expectedWidth && mapped.m_size.m_height == expectedHeight;
}

void TestScaleDown() {
    Require(MapsTo(2.0f, 10, 20, 30, 40, 5, 10, 15, 20));
    Require(MapsTo(2.0f, 3, 5, 7, 9, 1, 2, 4, 5));
    Require(MapsTo(2.0f, -3, -1, 1, 1, -2, -1, 1, 1));
    Require(MapsTo(2.0f, 0, 0, 0, 0, 0, 0, 0, 0));
}

void TestScaleUp() {
    Require(MapsTo(0.5f, 1, 2, 3, 4, 2, 4, 6, 8));
    Require(MapsTo(0.25f, -1, 0, 1, 2, -4, 0, 4, 8));
}

void TestFractionalScale() {
    Require(MapsTo(1.5f, 0, 0, 10, 10, 0, 0, 7, 7));
    Require(MapsTo(1.5f, 1, 1, 1, 1, 0, 0, 2, 2));
    Require(MapsTo(1.0f, 7, -7, 8, 9, 7, -7, 8, 9));
}

void TestSinglePrecision() {
    Require(MapsTo(1.0f, 16777217, 0, 1, 1, 16777216, 0, 0, 1));
}

void TestNullArguments() {
    const TiledBackingStore store = MakeStore(2.0f);
    const WebCore::IntRect input{{1, 2}, {3, 4}};
    Require(Throws([&] { _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(nullptr, &input); }));
    Require(Throws([&] { _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(&store, nullptr); }));
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    void ProtectReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(base, pageSize, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(base, pageSize, PROT_READ) == 0);
#endif
    }

    unsigned char* Readable() const { return base; }
    unsigned char* Writable() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template<typename TFunction>
void RequireNotImplemented(TFunction&& function) {
    const auto before = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(NotImplementedCalls_nid_no_patch() == before + 1);
}

void TestGuestPointers() {
    GuardedPages pages;
    const TiledBackingStore store = MakeStore(2.0f);
    const WebCore::IntRect input{{10, 20}, {30, 40}};

    std::memcpy(pages.Readable(), &store, sizeof(store));
    std::memcpy(pages.Readable() + 256, &input, sizeof(input));
    pages.ProtectReadOnly();

    const WebCore::IntRect mapped = _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(
        reinterpret_cast<const TiledBackingStore*>(pages.Readable()),
        reinterpret_cast<const WebCore::IntRect*>(pages.Readable() + 256));
    Require(mapped.m_location.m_x == 5 && mapped.m_location.m_y == 10
        && mapped.m_size.m_width == 15 && mapped.m_size.m_height == 20);

    RequireNotImplemented([&] {
        _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(
            reinterpret_cast<const TiledBackingStore*>(pages.Guard()), &input);
    });
    RequireNotImplemented([&] {
        _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(
            &store, reinterpret_cast<const WebCore::IntRect*>(pages.Guard()));
    });

    auto* splitStore = reinterpret_cast<const TiledBackingStore*>(pages.Guard() - (sizeof(TiledBackingStore) - 4));
    RequireNotImplemented([&] {
        _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(splitStore, &input);
    });

    auto* splitRect = reinterpret_cast<const WebCore::IntRect*>(pages.Guard() - (sizeof(WebCore::IntRect) - 4));
    RequireNotImplemented([&] {
        _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(&store, splitRect);
    });
}

}

int main() {
    TestScaleDown();
    TestScaleUp();
    TestFractionalScale();
    TestSinglePrecision();
    TestNullArguments();
    TestGuestPointers();
    return 0;
}
