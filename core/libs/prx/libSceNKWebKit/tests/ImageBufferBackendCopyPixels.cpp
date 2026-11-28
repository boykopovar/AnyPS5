#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using CopyImagePixelsFn = void (APS5_VABI*)(
    const WebCore::ImageBufferBackend*,
    WebCore::AlphaPremultiplication,
    WebCore::ColorFormat,
    unsigned,
    std::uint8_t*,
    WebCore::AlphaPremultiplication,
    WebCore::ColorFormat,
    unsigned,
    std::uint8_t*,
    const WebCore::IntSize*);

static_assert(std::is_same_v<
    decltype(&_ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE),
    CopyImagePixelsFn>);

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "ImageBufferBackend::copyImagePixels check failed at line %d\n", line);
    std::abort();
}

#define CHECK(cond) Require((cond), __LINE__)

template <typename Function>
bool ThrowsNotImplemented(Function fn) {
    try {
        fn();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find("not implemented") != std::string::npos;
    }
    return false;
}

std::array<std::uint8_t, 4> ExpectedPixel(
    WebCore::AlphaPremultiplication srcAlpha,
    WebCore::ColorFormat srcFormat,
    std::array<std::uint8_t, 4> src,
    WebCore::AlphaPremultiplication destAlpha,
    WebCore::ColorFormat destFormat) {
    const bool swap = srcFormat != destFormat;
    const auto ordered = [&](std::array<std::uint8_t, 4> px) -> std::array<std::uint8_t, 4> {
        if (!swap) return px;
        return {px[2], px[1], px[0], px[3]};
    };

    const std::uint8_t a = src[3];
    if (srcAlpha == destAlpha) {
        if (srcAlpha == WebCore::AlphaPremultiplication::Premultiplied) {
            if (a == 0) return {0, 0, 0, 0};
            return ordered(src);
        }
        return ordered(src);
    }

    if (a == 0) return {0, 0, 0, 0};
    if (a == 255) return ordered(src);

    const std::array<std::uint8_t, 4> base = ordered(src);
    if (destAlpha == WebCore::AlphaPremultiplication::Unpremultiplied) {
        return {
            static_cast<std::uint8_t>((base[0] * 255) / a),
            static_cast<std::uint8_t>((base[1] * 255) / a),
            static_cast<std::uint8_t>((base[2] * 255) / a),
            a};
    }
    return {
        static_cast<std::uint8_t>((base[0] * a + 254) / 255),
        static_cast<std::uint8_t>((base[1] * a + 254) / 255),
        static_cast<std::uint8_t>((base[2] * a + 254) / 255),
        a};
}

void TestAllFormatsAndTransitions() {
    const std::vector<std::array<std::uint8_t, 4>> samples = {
        {12, 34, 56, 0},
        {0, 1, 255, 1},
        {40, 80, 120, 128},
        {128, 255, 1, 128},
        {200, 100, 50, 200},
        {17, 99, 240, 255},
    };
    const std::array<WebCore::AlphaPremultiplication, 2> alphas = {
        WebCore::AlphaPremultiplication::Premultiplied,
        WebCore::AlphaPremultiplication::Unpremultiplied,
    };
    const std::array<WebCore::ColorFormat, 2> formats = {
        WebCore::ColorFormat::RGBA,
        WebCore::ColorFormat::BGRA,
    };

    const int width = static_cast<int>(samples.size());
    const WebCore::IntSize size{width, 1};
    std::vector<std::uint8_t> src(samples.size() * 4);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        std::memcpy(src.data() + i * 4, samples[i].data(), 4);
    }

    for (const auto srcAlpha : alphas) {
        for (const auto srcFormat : formats) {
            for (const auto destAlpha : alphas) {
                for (const auto destFormat : formats) {
                    std::vector<std::uint8_t> dest(samples.size() * 4, 0xA5);
                    errno = 42;
                    _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
                        nullptr, srcAlpha, srcFormat, static_cast<unsigned>(width * 4), src.data(),
                        destAlpha, destFormat, static_cast<unsigned>(width * 4), dest.data(), &size);
                    CHECK(errno == 42);
                    for (std::size_t i = 0; i < samples.size(); ++i) {
                        const auto expected = ExpectedPixel(srcAlpha, srcFormat, samples[i], destAlpha, destFormat);
                        CHECK(dest[i * 4 + 0] == expected[0]);
                        CHECK(dest[i * 4 + 1] == expected[1]);
                        CHECK(dest[i * 4 + 2] == expected[2]);
                        CHECK(dest[i * 4 + 3] == expected[3]);
                    }
                }
            }
        }
    }
}

void TestStridesPaddingZeroStrideAndInPlace() {
    std::vector<std::uint8_t> src = {
        10, 20, 30, 255,  1, 0, 255, 1,  0xEE, 0xEE, 0xEE,
        99, 88, 77, 0,    64, 128, 192, 128,  0xDD, 0xDD, 0xDD,
    };
    const unsigned srcStride = 11;
    const unsigned destStride = 14;
    std::vector<std::uint8_t> dest(2 * destStride + 8, 0x5A);
    const WebCore::IntSize size{2, 2};

    _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
        reinterpret_cast<const WebCore::ImageBufferBackend*>(0x1234),
        WebCore::AlphaPremultiplication::Unpremultiplied, WebCore::ColorFormat::RGBA, srcStride, src.data(),
        WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::BGRA, destStride, dest.data() + 4, &size);

    for (int i = 0; i < 4; ++i) CHECK(dest[i] == 0x5A);
    for (int i = 0; i < 6; ++i) CHECK(dest[4 + 8 + i] == 0x5A);
    for (int i = 0; i < 10; ++i) CHECK(dest[4 + destStride + 8 + i] == 0x5A);

    const std::array<std::uint8_t, 4> p00{30, 20, 10, 255};
    const std::array<std::uint8_t, 4> p01{1, 0, 1, 1};
    const std::array<std::uint8_t, 4> p10{0, 0, 0, 0};
    const std::array<std::uint8_t, 4> p11{97, 65, 33, 128};
    CHECK(std::memcmp(dest.data() + 4, p00.data(), 4) == 0);
    CHECK(std::memcmp(dest.data() + 8, p01.data(), 4) == 0);
    CHECK(std::memcmp(dest.data() + 4 + destStride, p10.data(), 4) == 0);
    CHECK(std::memcmp(dest.data() + 4 + destStride + 4, p11.data(), 4) == 0);

    std::vector<std::uint8_t> oneRow = {10, 20, 30, 255, 40, 50, 60, 0};
    std::vector<std::uint8_t> repeated(16, 0x33);
    _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
        nullptr,
        WebCore::AlphaPremultiplication::Unpremultiplied, WebCore::ColorFormat::RGBA, 0, oneRow.data(),
        WebCore::AlphaPremultiplication::Unpremultiplied, WebCore::ColorFormat::BGRA, 8, repeated.data(), &size);
    const std::array<std::uint8_t, 8> expectedRepeated = {30, 20, 10, 255, 60, 50, 40, 0};
    CHECK(std::memcmp(repeated.data(), expectedRepeated.data(), 8) == 0);
    CHECK(std::memcmp(repeated.data() + 8, expectedRepeated.data(), 8) == 0);

    std::vector<std::uint8_t> inPlace = {12, 34, 56, 0, 100, 150, 200, 255};
    const WebCore::IntSize rowSize{2, 1};
    _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
        nullptr,
        WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, inPlace.data(),
        WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::BGRA, 8, inPlace.data(), &rowSize);
    const std::array<std::uint8_t, 8> expectedInPlace = {0, 0, 0, 0, 200, 150, 100, 255};
    CHECK(std::memcmp(inPlace.data(), expectedInPlace.data(), 8) == 0);
}

void TestEmptyDimensions() {
    const std::array<WebCore::IntSize, 5> emptySizes = {
        WebCore::IntSize{0, 0},
        WebCore::IntSize{0, 4},
        WebCore::IntSize{4, 0},
        WebCore::IntSize{4, -2},
        WebCore::IntSize{-2, 0},
    };
    for (const auto& size : emptySizes) {
        errno = 77;
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr,
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 0, nullptr,
            WebCore::AlphaPremultiplication::Unpremultiplied, WebCore::ColorFormat::BGRA, 0, nullptr, &size);
        CHECK(errno == 77);
    }
}

void TestInvalidArgumentsAndFaultSafety() {
    std::array<std::uint8_t, 8> src = {10, 20, 30, 255, 40, 50, 60, 255};
    std::array<std::uint8_t, 8> dest = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    const auto destBefore = dest;
    const WebCore::IntSize validSize{2, 1};
    const WebCore::IntSize negWidthSize{-1, 2};
    auto* const unmapped = reinterpret_cast<std::uint8_t*>(std::uintptr_t{0x10});
    const auto* const unmappedSize = reinterpret_cast<const WebCore::IntSize*>(std::uintptr_t{0x10});

    errno = 91;
    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, src.data(),
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, dest.data(), nullptr);
    }));
    CHECK(errno == 91);

    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, src.data(),
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, dest.data(), unmappedSize);
    }));

    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, src.data(),
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, dest.data(), &negWidthSize);
    }));

    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, static_cast<WebCore::AlphaPremultiplication>(2), WebCore::ColorFormat::RGBA, 8, src.data(),
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, dest.data(), &validSize);
    }));
    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, static_cast<WebCore::ColorFormat>(2), 8, src.data(),
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, dest.data(), &validSize);
    }));
    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, src.data(),
            static_cast<WebCore::AlphaPremultiplication>(99), WebCore::ColorFormat::RGBA, 8, dest.data(), &validSize);
    }));
    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, src.data(),
            WebCore::AlphaPremultiplication::Premultiplied, static_cast<WebCore::ColorFormat>(99), 8, dest.data(), &validSize);
    }));

    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, nullptr,
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, dest.data(), &validSize);
    }));
    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, src.data(),
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, nullptr, &validSize);
    }));
    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, unmapped,
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, dest.data(), &validSize);
    }));
    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, src.data(),
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 8, unmapped, &validSize);
    }));
    CHECK(dest == destBefore);

    constexpr std::size_t pageSize = 4096;
#ifdef _WIN32
    auto* const twoPages = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    CHECK(twoPages != nullptr);
    std::memset(twoPages, 0x5B, pageSize * 2);
    DWORD oldProtect = 0;
    CHECK(VirtualProtect(twoPages + pageSize, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
    auto* const twoPages = static_cast<std::uint8_t*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0));
    CHECK(twoPages != MAP_FAILED);
    std::memset(twoPages, 0x5B, pageSize * 2);
    CHECK(mprotect(twoPages + pageSize, pageSize, PROT_READ) == 0);
#endif

    const std::array<std::uint8_t, 8> twoRowSrc = {1, 2, 3, 255, 4, 5, 6, 255};
    const WebCore::IntSize twoRowSize{1, 2};
    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 4, const_cast<std::uint8_t*>(twoRowSrc.data()),
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, static_cast<unsigned>(pageSize), twoPages, &twoRowSize);
    }));
    for (std::size_t i = 0; i < 4; ++i) CHECK(twoPages[i] == 0x5B);

#ifdef _WIN32
    CHECK(VirtualProtect(twoPages + pageSize, pageSize, PAGE_NOACCESS, &oldProtect) != 0);
#else
    CHECK(mprotect(twoPages + pageSize, pageSize, PROT_NONE) == 0);
#endif
    std::array<std::uint8_t, 8> guardedDest = {0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C};
    CHECK(ThrowsNotImplemented([&] {
        _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
            nullptr, WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, static_cast<unsigned>(pageSize), twoPages,
            WebCore::AlphaPremultiplication::Premultiplied, WebCore::ColorFormat::RGBA, 4, guardedDest.data(), &twoRowSize);
    }));
    for (const auto byte : guardedDest) CHECK(byte == 0x6C);

#ifdef _WIN32
    CHECK(VirtualFree(twoPages, 0, MEM_RELEASE) != 0);
#else
    CHECK(munmap(twoPages, pageSize * 2) == 0);
#endif
}

} // namespace

int main() {
    TestAllFormatsAndTransitions();
    TestStridesPaddingZeroStrideAndInPlace();
    TestEmptyDimensions();
    TestInvalidArgumentsAndFaultSafety();
    std::puts("ImageBufferBackend copyImagePixels tests passed");
    return 0;
}
