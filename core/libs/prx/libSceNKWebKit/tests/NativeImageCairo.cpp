#include "prx/libcairo/include/Surface.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NativeImageSizeFn = WebCore::IntSize (APS5_VABI*)(const NKWebKit::WtfAbi::RefPtr<CairoSurface>*);

static_assert(std::is_same_v<
    decltype(&_ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE),
    NativeImageSizeFn>);
static_assert(sizeof(WebCore::IntSize) == 8);
static_assert(sizeof(NKWebKit::WtfAbi::RefPtr<CairoSurface>) == 8);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "NativeImageCairo check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Fn>
void RequireThrowsPreservingErrno(Fn&& fn) {
    errno = EDOM;
    bool threw = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(errno == EDOM);
}

struct GuardedRegion {
    unsigned char* base{nullptr};
    std::size_t pageSize{4096};

    GuardedRegion() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD prev = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &prev) != 0);
#else
        long sz = sysconf(_SC_PAGESIZE);
        pageSize = sz > 0 ? static_cast<std::size_t>(sz) : 4096;
        void* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedRegion() {
        if (!base)
            return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 2) == 0);
#endif
    }
};

WebCore::IntSize CallNativeImageSize(const NKWebKit::WtfAbi::RefPtr<CairoSurface>& ref) {
    return _ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE(&ref);
}

void TestNullRefPtr() {
    const NKWebKit::WtfAbi::RefPtr<CairoSurface> nullRef{nullptr};
    errno = EDOM;
    const WebCore::IntSize size = CallNativeImageSize(nullRef);
    Require(errno == EDOM);
    Require(size.m_width == 0);
    Require(size.m_height == 0);
}

void TestImageSurfaces() {
    struct FormatCase {
        int format;
        int width;
        int height;
    };
    constexpr std::array<FormatCase, 6> cases{{
        {0, 0, 0},
        {0, 1, 1},
        {0, 13, 29},
        {1, 256, 128},
        {2, 17, 5},
        {3, 9, 41},
    }};
    for (const auto& entry : cases) {
        CairoSurface* surface = cairo_image_surface_create(entry.format, entry.width, entry.height);
        Require(surface != nullptr);
        Require(cairo_surface_status(surface) == 0);
        const NKWebKit::WtfAbi::RefPtr<CairoSurface> ref{surface};
        errno = EDOM;
        const WebCore::IntSize size = CallNativeImageSize(ref);
        Require(errno == EDOM);
        Require(size.m_width == entry.width);
        Require(size.m_height == entry.height);
        cairo_surface_destroy(surface);
    }
}

void TestImageForDataAndSimilarAndScaled() {
    constexpr int argb32 = 0;
    constexpr int width = 19;
    constexpr int height = 7;
    const int stride = cairo_format_stride_for_width(argb32, width);
    Require(stride >= width * 4);
    std::array<unsigned char, 4096> pixels{};
    CairoSurface* forData = cairo_image_surface_create_for_data(pixels.data(), argb32, width, height, stride);
    Require(forData != nullptr);
    Require(cairo_surface_status(forData) == 0);
    {
        const NKWebKit::WtfAbi::RefPtr<CairoSurface> ref{forData};
        const WebCore::IntSize size = CallNativeImageSize(ref);
        Require(size.m_width == width);
        Require(size.m_height == height);
    }

    cairo_surface_set_device_scale(forData, 2.0, 3.0);
    {
        const NKWebKit::WtfAbi::RefPtr<CairoSurface> ref{forData};
        const WebCore::IntSize size = CallNativeImageSize(ref);
        Require(size.m_width == width);
        Require(size.m_height == height);
    }

    constexpr int contentColorAlpha = 0x3000;
    CairoSurface* scaledSimilar = cairo_surface_create_similar(forData, contentColorAlpha, 31, 11);
    Require(scaledSimilar != nullptr);
    Require(cairo_surface_status(scaledSimilar) == 0);
    {
        const NKWebKit::WtfAbi::RefPtr<CairoSurface> ref{scaledSimilar};
        const WebCore::IntSize size = CallNativeImageSize(ref);
        Require(size.m_width == 62);
        Require(size.m_height == 33);
    }

    CairoSurface* unscaled = cairo_image_surface_create(argb32, 8, 8);
    Require(unscaled != nullptr);
    Require(cairo_surface_status(unscaled) == 0);
    CairoSurface* similar = cairo_surface_create_similar(unscaled, contentColorAlpha, 31, 11);
    Require(similar != nullptr);
    Require(cairo_surface_status(similar) == 0);
    {
        const NKWebKit::WtfAbi::RefPtr<CairoSurface> ref{similar};
        const WebCore::IntSize size = CallNativeImageSize(ref);
        Require(size.m_width == 31);
        Require(size.m_height == 11);
    }

    cairo_surface_destroy(similar);
    cairo_surface_destroy(unscaled);
    cairo_surface_destroy(scaledSimilar);
    cairo_surface_destroy(forData);
}

void TestErrorAndSubsurface() {
    CairoSurface* errorSurface = cairo_image_surface_create(0, -1, 10);
    Require(errorSurface != nullptr);
    Require(cairo_surface_status(errorSurface) != 0);
    {
        const NKWebKit::WtfAbi::RefPtr<CairoSurface> ref{errorSurface};
        const WebCore::IntSize size = CallNativeImageSize(ref);
        Require(size.m_width == 0);
        Require(size.m_height == 0);
    }
    cairo_surface_destroy(errorSurface);

    CairoSurface* backing = cairo_image_surface_create(0, 16, 16);
    Require(backing != nullptr);
    Require(cairo_surface_status(backing) == 0);
    CairoSurface* subsurface = cairo_surface_create_for_rectangle(backing, 1.0, 2.0, 5.0, 7.0);
    Require(subsurface != nullptr);
    Require(cairo_surface_status(subsurface) == 0);
    {
        const NKWebKit::WtfAbi::RefPtr<CairoSurface> ref{subsurface};
        const WebCore::IntSize size = CallNativeImageSize(ref);
        Require(size.m_width == 0);
        Require(size.m_height == 0);
    }
    cairo_surface_destroy(subsurface);
    cairo_surface_destroy(backing);
}

void TestGuestPointerValidation() {
    GuardedRegion guarded;
    const auto* unreadableRef = reinterpret_cast<const NKWebKit::WtfAbi::RefPtr<CairoSurface>*>(guarded.base + guarded.pageSize);
    auto* unreadableSurface = reinterpret_cast<CairoSurface*>(guarded.base + guarded.pageSize);
    const NKWebKit::WtfAbi::RefPtr<CairoSurface> badTargetRef{unreadableSurface};

    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE(nullptr));
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE(unreadableRef));
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(CallNativeImageSize(badTargetRef));
    });
}

}  // namespace

int main() {
    TestNullRefPtr();
    TestImageSurfaces();
    TestImageForDataAndSimilarAndScaled();
    TestErrorAndSubsurface();
    TestGuestPointerValidation();
    return 0;
}
