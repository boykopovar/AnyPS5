#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::uint32_t kOpGrayscale = 1;
constexpr std::uint32_t kOpSepia = 2;
constexpr std::uint32_t kOpSaturate = 3;
constexpr std::uint32_t kOpHueRotate = 4;
constexpr std::uint32_t kOpInvert = 5;
constexpr std::uint32_t kOpInvertLightness = 6;
constexpr std::uint32_t kOpOpacity = 7;
constexpr std::uint32_t kOpBrightness = 8;
constexpr std::uint32_t kOpContrast = 9;
constexpr std::uint32_t kOpNone = 14;

constexpr float kPiFloat = 3.14159265358979323846f;

using ColorMatrixCreateFn = WebCore::BasicFilterOperationRef<WebCore::BasicColorMatrixFilterOperation>(APS5_VABI*)(double, std::uint32_t);
using ColorMatrixCtorFn = void(APS5_VABI*)(WebCore::BasicColorMatrixFilterOperation*, double, std::uint32_t);
using ColorMatrixTransformFn = bool(APS5_VABI*)(const WebCore::BasicColorMatrixFilterOperation*, WebCore::SRGBA<float>&);
using ColorMatrixBlendFn = WebCore::BasicFilterOperationRef<WebCore::FilterOperation>(APS5_VABI*)(WebCore::BasicColorMatrixFilterOperation*, const WebCore::FilterOperation*, double, bool);

using ComponentTransferCreateFn = WebCore::BasicFilterOperationRef<WebCore::BasicComponentTransferFilterOperation>(APS5_VABI*)(double, std::uint32_t);
using ComponentTransferCtorFn = void(APS5_VABI*)(WebCore::BasicComponentTransferFilterOperation*, double, std::uint32_t);
using ComponentTransferTransformFn = bool(APS5_VABI*)(const WebCore::BasicComponentTransferFilterOperation*, WebCore::SRGBA<float>&);
using ComponentTransferBlendFn = WebCore::BasicFilterOperationRef<WebCore::FilterOperation>(APS5_VABI*)(WebCore::BasicComponentTransferFilterOperation*, const WebCore::FilterOperation*, double, bool);

static_assert(std::is_same_v<decltype(&_ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE), ColorMatrixCreateFn>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore31BasicColorMatrixFilterOperationC1EdNS_15FilterOperation13OperationTypeE), ColorMatrixCtorFn>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore31BasicColorMatrixFilterOperationC2EdNS_15FilterOperation13OperationTypeE), ColorMatrixCtorFn>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE), ColorMatrixTransformFn>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb), ColorMatrixBlendFn>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE), ComponentTransferCreateFn>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore37BasicComponentTransferFilterOperationC1EdNS_15FilterOperation13OperationTypeE), ComponentTransferCtorFn>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore37BasicComponentTransferFilterOperationC2EdNS_15FilterOperation13OperationTypeE), ComponentTransferCtorFn>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE), ComponentTransferTransformFn>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb), ComponentTransferBlendFn>);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "BasicFilterOperations check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

bool NearFloat(float actual, float expected, float tolerance = 1e-5f) {
    return std::fabs(actual - expected) <= tolerance;
}

bool NearDouble(double actual, double expected, double tolerance = 1e-12) {
    return std::fabs(actual - expected) <= tolerance;
}

bool NearRgba(const WebCore::SRGBA<float>& actual, float r, float g, float b, float a, float tolerance = 1e-5f) {
    return NearFloat(actual.red, r, tolerance)
        && NearFloat(actual.green, g, tolerance)
        && NearFloat(actual.blue, b, tolerance)
        && NearFloat(actual.alpha, a, tolerance);
}

std::uint32_t ReadTypeWord(const WebCore::FilterOperation& op) {
    std::uint32_t value = 0;
    std::memcpy(&value, &op.m_type, sizeof(value));
    return value;
}

template<typename Fn>
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

    void MakeFirstPageReadOnly() {
#ifdef _WIN32
        DWORD prev = 0;
        Require(VirtualProtect(base, pageSize, PAGE_READONLY, &prev) != 0);
#else
        Require(mprotect(base, pageSize, PROT_READ) == 0);
#endif
    }

    void MakeFirstPageReadWrite() {
#ifdef _WIN32
        DWORD prev = 0;
        Require(VirtualProtect(base, pageSize, PAGE_READWRITE, &prev) != 0);
#else
        Require(mprotect(base, pageSize, PROT_READ | PROT_WRITE) == 0);
#endif
    }
};

void TestColorMatrixConstructionAndVtable() {
    WebCore::BasicColorMatrixFilterOperation c1{};
    WebCore::BasicColorMatrixFilterOperation c2{};
    std::memset(&c1, 0xAA, sizeof(c1));
    std::memset(&c2, 0x55, sizeof(c2));

    _ZN7WebCore31BasicColorMatrixFilterOperationC1EdNS_15FilterOperation13OperationTypeE(&c1, 0.75, kOpGrayscale);
    _ZN7WebCore31BasicColorMatrixFilterOperationC2EdNS_15FilterOperation13OperationTypeE(&c2, 0.75, kOpGrayscale);
    Require(std::memcmp(&c1, &c2, sizeof(c1)) == 0);
    Require(c1.m_vptr != nullptr);
    Require(c1.m_refCount.load() == 1);
    Require(ReadTypeWord(c1) == kOpGrayscale);
    Require(c1.m_amount == 0.75);

    auto created = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.75, kOpGrayscale);
    Require(created.get() != nullptr);
    Require(created->m_vptr == c1.m_vptr);
    Require(created->m_refCount.load() == 1);
    Require(ReadTypeWord(*created) == kOpGrayscale);
    Require(created->m_amount == 0.75);

    using CloneFn = WebCore::BasicFilterOperationRef<WebCore::FilterOperation>(APS5_VABI*)(const WebCore::FilterOperation*);
    using EqualsFn = bool(APS5_VABI*)(const WebCore::FilterOperation*, const WebCore::FilterOperation&);
    using BoolQueryFn = bool(APS5_VABI*)(const WebCore::FilterOperation*);
    using InverseColorFn = bool(APS5_VABI*)(const WebCore::FilterOperation*, WebCore::SRGBA<float>&);

    auto** vtable = static_cast<void**>(created->m_vptr);
    auto cloned = reinterpret_cast<CloneFn>(vtable[2])(created.get());
    Require(cloned.get() != nullptr && cloned.get() != created.get());
    Require(cloned->m_refCount.load() == 1);
    Require(reinterpret_cast<EqualsFn>(vtable[3])(created.get(), *cloned));
    Require(!reinterpret_cast<BoolQueryFn>(vtable[7])(created.get()));
    Require(!reinterpret_cast<BoolQueryFn>(vtable[8])(created.get()));
    Require(!reinterpret_cast<BoolQueryFn>(vtable[9])(created.get()));

    WebCore::SRGBA<float> probe{0.2f, 0.4f, 0.6f, 0.8f};
    Require(!reinterpret_cast<InverseColorFn>(vtable[6])(created.get(), probe));
    Require(NearRgba(probe, 0.2f, 0.4f, 0.6f, 0.8f));
}

void TestColorMatrixTransformColor() {
    const WebCore::SRGBA<float> base{0.25f, 0.5f, 0.75f, 0.4f};

    {
        auto op0 = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.0, kOpGrayscale);
        WebCore::SRGBA<float> c = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(op0.get(), c));
        Require(NearRgba(c, 0.25f, 0.5f, 0.75f, 0.4f));

        auto op1 = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(1.0, kOpGrayscale);
        c = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(op1.get(), c));
        const float lum = 0.2126f * 0.25f + 0.7152f * 0.5f + 0.0722f * 0.75f;
        Require(NearRgba(c, lum, lum, lum, 0.4f));

        auto opOver = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(2.5, kOpGrayscale);
        WebCore::SRGBA<float> cOver = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(opOver.get(), cOver));
        Require(NearRgba(cOver, lum, lum, lum, 0.4f));

        auto opNeg = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(-1.0, kOpGrayscale);
        WebCore::SRGBA<float> cNeg = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(opNeg.get(), cNeg));
        Require(NearRgba(cNeg, 0.25f, 0.5f, 0.75f, 0.4f));
    }

    {
        auto op1 = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(1.0, kOpSepia);
        WebCore::SRGBA<float> c = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(op1.get(), c));
        Require(NearRgba(c,
            0.393f * 0.25f + 0.769f * 0.5f + 0.189f * 0.75f,
            0.349f * 0.25f + 0.686f * 0.5f + 0.168f * 0.75f,
            0.272f * 0.25f + 0.534f * 0.5f + 0.131f * 0.75f,
            0.4f));

        auto op0 = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.0, kOpSepia);
        c = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(op0.get(), c));
        Require(NearRgba(c, 0.25f, 0.5f, 0.75f, 0.4f));
    }

    {
        auto op1 = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(1.0, kOpSaturate);
        WebCore::SRGBA<float> c = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(op1.get(), c));
        Require(NearRgba(c, 0.25f, 0.5f, 0.75f, 0.4f));

        auto op0 = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.0, kOpSaturate);
        c = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(op0.get(), c));
        const float sat0 = 0.213f * 0.25f + 0.715f * 0.5f + 0.072f * 0.75f;
        Require(NearRgba(c, sat0, sat0, sat0, 0.4f));

        auto op2 = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(2.0, kOpSaturate);
        c = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(op2.get(), c));
        Require(NearRgba(c,
            (0.213f + 0.787f * 2.0f) * 0.25f + (0.715f - 0.715f * 2.0f) * 0.5f + (0.072f - 0.072f * 2.0f) * 0.75f,
            (0.213f - 0.213f * 2.0f) * 0.25f + (0.715f + 0.285f * 2.0f) * 0.5f + (0.072f - 0.072f * 2.0f) * 0.75f,
            (0.213f - 0.213f * 2.0f) * 0.25f + (0.715f - 0.715f * 2.0f) * 0.5f + (0.072f + 0.928f * 2.0f) * 0.75f,
            0.4f));
    }

    {
        auto op0 = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.0, kOpHueRotate);
        WebCore::SRGBA<float> c = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(op0.get(), c));
        Require(NearRgba(c, 0.25f, 0.5f, 0.75f, 0.4f));

        auto op90 = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(90.0, kOpHueRotate);
        c = base;
        Require(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(op90.get(), c));
        const float rad = 90.0f * kPiFloat / 180.0f;
        const float ch = std::cos(rad);
        const float sh = std::sin(rad);
        const float er = (0.213f + ch * 0.787f - sh * 0.213f) * 0.25f
            + (0.715f - ch * 0.715f - sh * 0.715f) * 0.5f
            + (0.072f - ch * 0.072f + sh * 0.928f) * 0.75f;
        const float eg = (0.213f - ch * 0.213f + sh * 0.143f) * 0.25f
            + (0.715f + ch * 0.285f + sh * 0.140f) * 0.5f
            + (0.072f - ch * 0.072f - sh * 0.283f) * 0.75f;
        const float eb = (0.213f - ch * 0.213f - sh * 0.787f) * 0.25f
            + (0.715f - ch * 0.715f + sh * 0.715f) * 0.5f
            + (0.072f + ch * 0.928f + sh * 0.072f) * 0.75f;
        Require(NearRgba(c, er, eg, eb, 0.4f));
    }

    {
        auto opBad = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.5, kOpOpacity);
        WebCore::SRGBA<float> c = base;
        Require(!_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(opBad.get(), c));
        Require(std::memcmp(&c, &base, sizeof(c)) == 0);

        auto opHigh = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.5, 0x101u);
        Require(!_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(opHigh.get(), c));
        Require(std::memcmp(&c, &base, sizeof(c)) == 0);
    }
}

void TestColorMatrixBlend() {
    auto toGray = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.8, kOpGrayscale);
    auto fromGray = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.2, kOpGrayscale);

    {
        auto blended = _ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(
            toGray.get(), nullptr, 0.25, false);
        Require(blended.get() != nullptr && blended.get() != toGray.get());
        auto* asColorMatrix = static_cast<WebCore::BasicColorMatrixFilterOperation*>(blended.get());
        Require(ReadTypeWord(*asColorMatrix) == kOpGrayscale);
        Require(NearDouble(asColorMatrix->m_amount, 0.2));
    }

    {
        auto blended = _ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(
            toGray.get(), fromGray.get(), 0.5, false);
        Require(blended.get() != nullptr && blended.get() != toGray.get());
        auto* asColorMatrix = static_cast<WebCore::BasicColorMatrixFilterOperation*>(blended.get());
        Require(NearDouble(asColorMatrix->m_amount, 0.5));
    }

    {
        auto blended = _ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(
            toGray.get(), fromGray.get(), 0.25, true);
        auto* asColorMatrix = static_cast<WebCore::BasicColorMatrixFilterOperation*>(blended.get());
        Require(NearDouble(asColorMatrix->m_amount, 0.6));
    }

    {
        auto sat = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(3.0, kOpSaturate);
        auto fromNull = _ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(
            sat.get(), nullptr, 0.5, false);
        Require(NearDouble(static_cast<WebCore::BasicColorMatrixFilterOperation*>(fromNull.get())->m_amount, 2.0));

        auto toPass = _ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(
            sat.get(), nullptr, 0.25, true);
        Require(NearDouble(static_cast<WebCore::BasicColorMatrixFilterOperation*>(toPass.get())->m_amount, 2.5));
    }

    {
        GuardedRegion guarded;
        auto* shortOp = reinterpret_cast<WebCore::FilterOperation*>(guarded.base + guarded.pageSize - sizeof(WebCore::FilterOperation));
        std::memset(shortOp, 0, sizeof(WebCore::FilterOperation));
        const std::uint32_t otherType = kOpInvertLightness;
        std::memcpy(&shortOp->m_type, &otherType, sizeof(otherType));

        Require(toGray->m_refCount.load() == 1);
        {
            auto mismatched = _ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(
                toGray.get(), shortOp, 0.5, false);
            Require(mismatched.get() == toGray.get());
            Require(toGray->m_refCount.load() == 2);
        }
        Require(toGray->m_refCount.load() == 1);
    }
}

void TestComponentTransferConstructionAndVtable() {
    WebCore::BasicComponentTransferFilterOperation c1{};
    WebCore::BasicComponentTransferFilterOperation c2{};
    std::memset(&c1, 0xCC, sizeof(c1));
    std::memset(&c2, 0x33, sizeof(c2));

    _ZN7WebCore37BasicComponentTransferFilterOperationC1EdNS_15FilterOperation13OperationTypeE(&c1, 0.5, kOpOpacity);
    _ZN7WebCore37BasicComponentTransferFilterOperationC2EdNS_15FilterOperation13OperationTypeE(&c2, 0.5, kOpOpacity);
    Require(std::memcmp(&c1, &c2, sizeof(c1)) == 0);
    Require(c1.m_vptr != nullptr);
    Require(c1.m_refCount.load() == 1);
    Require(ReadTypeWord(c1) == kOpOpacity);
    Require(c1.m_amount == 0.5);

    auto opOpacity = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.5, kOpOpacity);
    auto opInvert = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.5, kOpInvert);

    using CloneFn = WebCore::BasicFilterOperationRef<WebCore::FilterOperation>(APS5_VABI*)(const WebCore::FilterOperation*);
    using EqualsFn = bool(APS5_VABI*)(const WebCore::FilterOperation*, const WebCore::FilterOperation&);
    using BoolQueryFn = bool(APS5_VABI*)(const WebCore::FilterOperation*);

    auto** vtable = static_cast<void**>(opOpacity->m_vptr);
    auto cloned = reinterpret_cast<CloneFn>(vtable[2])(opOpacity.get());
    Require(cloned.get() != nullptr && cloned.get() != opOpacity.get());
    Require(reinterpret_cast<EqualsFn>(vtable[3])(opOpacity.get(), *cloned));
    Require(!reinterpret_cast<EqualsFn>(vtable[3])(opOpacity.get(), *opInvert));
    Require(reinterpret_cast<BoolQueryFn>(vtable[7])(opOpacity.get()));
    Require(!reinterpret_cast<BoolQueryFn>(vtable[7])(opInvert.get()));
    Require(!reinterpret_cast<BoolQueryFn>(vtable[8])(opOpacity.get()));
    Require(!reinterpret_cast<BoolQueryFn>(vtable[9])(opOpacity.get()));
}

void TestComponentTransferTransformColor() {
    const WebCore::SRGBA<float> base{0.2f, 0.5f, 0.9f, 0.8f};

    {
        auto op = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.5, kOpOpacity);
        WebCore::SRGBA<float> c = base;
        Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(op.get(), c));
        Require(NearRgba(c, 0.2f, 0.5f, 0.9f, 0.4f));
    }

    {
        auto op0 = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.0, kOpInvert);
        WebCore::SRGBA<float> c = base;
        Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(op0.get(), c));
        Require(NearRgba(c, 0.2f, 0.5f, 0.9f, 0.8f));

        auto opHalf = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.5, kOpInvert);
        c = base;
        Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(opHalf.get(), c));
        Require(NearRgba(c, 0.5f, 0.5f, 0.5f, 0.8f));

        auto op1 = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(1.0, kOpInvert);
        c = base;
        Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(op1.get(), c));
        Require(NearRgba(c, 0.8f, 0.5f, 0.1f, 0.8f));
    }

    {
        auto op0 = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.0, kOpContrast);
        WebCore::SRGBA<float> c = base;
        Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(op0.get(), c));
        Require(NearRgba(c, 0.5f, 0.5f, 0.5f, 0.8f));

        auto op1 = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(1.0, kOpContrast);
        c = base;
        Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(op1.get(), c));
        Require(NearRgba(c, 0.2f, 0.5f, 0.9f, 0.8f));

        auto op2 = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(2.0, kOpContrast);
        c = base;
        Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(op2.get(), c));
        Require(NearRgba(c, 0.0f, 0.5f, 1.0f, 0.8f));
    }

    {
        auto op2 = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(2.0, kOpBrightness);
        WebCore::SRGBA<float> c = base;
        Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(op2.get(), c));
        Require(NearRgba(c, 0.4f, 1.0f, 1.8f, 0.8f));

        auto opNeg = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(-1.0, kOpBrightness);
        c = base;
        Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(opNeg.get(), c));
        Require(NearRgba(c, 0.0f, 0.0f, 0.0f, 0.8f));
    }

    {
        auto opBad = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.5, kOpGrayscale);
        WebCore::SRGBA<float> c = base;
        Require(!_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(opBad.get(), c));
        Require(std::memcmp(&c, &base, sizeof(c)) == 0);

        auto opNone = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.5, kOpNone);
        Require(!_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(opNone.get(), c));
        Require(std::memcmp(&c, &base, sizeof(c)) == 0);
    }
}

void TestComponentTransferBlend() {
    auto opOpacity = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.2, kOpOpacity);
    auto fromOpacity = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.6, kOpOpacity);
    auto opInvert = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.8, kOpInvert);

    {
        auto fromNull = _ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb(
            opOpacity.get(), nullptr, 0.5, false);
        Require(NearDouble(static_cast<WebCore::BasicComponentTransferFilterOperation*>(fromNull.get())->m_amount, 0.6));

        auto fromOp = _ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb(
            opOpacity.get(), fromOpacity.get(), 0.25, false);
        Require(NearDouble(static_cast<WebCore::BasicComponentTransferFilterOperation*>(fromOp.get())->m_amount, 0.5));

        auto toPass = _ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb(
            opOpacity.get(), fromOpacity.get(), 0.5, true);
        Require(NearDouble(static_cast<WebCore::BasicComponentTransferFilterOperation*>(toPass.get())->m_amount, 0.6));

        auto invFromNull = _ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb(
            opInvert.get(), nullptr, 0.25, false);
        Require(NearDouble(static_cast<WebCore::BasicComponentTransferFilterOperation*>(invFromNull.get())->m_amount, 0.2));

        auto invToPass = _ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb(
            opInvert.get(), nullptr, 0.25, true);
        Require(NearDouble(static_cast<WebCore::BasicComponentTransferFilterOperation*>(invToPass.get())->m_amount, 0.6));
    }

    {
        Require(opOpacity->m_refCount.load() == 1);
        {
            auto mismatched = _ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb(
                opOpacity.get(), opInvert.get(), 0.5, false);
            Require(mismatched.get() == opOpacity.get());
            Require(opOpacity->m_refCount.load() == 2);
        }
        Require(opOpacity->m_refCount.load() == 1);
    }
}

void TestGuestPointerValidation() {
    GuardedRegion guarded;
    auto* unreadable = reinterpret_cast<WebCore::BasicColorMatrixFilterOperation*>(guarded.base + guarded.pageSize);
    auto* unreadableTransfer = reinterpret_cast<WebCore::BasicComponentTransferFilterOperation*>(guarded.base + guarded.pageSize);
    auto* unreadableColor = reinterpret_cast<WebCore::SRGBA<float>*>(guarded.base + guarded.pageSize);

    auto validColorMatrix = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.5, kOpGrayscale);
    auto validTransfer = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(0.5, kOpOpacity);
    WebCore::SRGBA<float> color{0.2f, 0.4f, 0.6f, 0.8f};

    RequireThrowsPreservingErrno([&] {
        _ZN7WebCore31BasicColorMatrixFilterOperationC1EdNS_15FilterOperation13OperationTypeE(nullptr, 0.5, kOpGrayscale);
    });
    RequireThrowsPreservingErrno([&] {
        _ZN7WebCore31BasicColorMatrixFilterOperationC2EdNS_15FilterOperation13OperationTypeE(unreadable, 0.5, kOpGrayscale);
    });
    RequireThrowsPreservingErrno([&] {
        _ZN7WebCore37BasicComponentTransferFilterOperationC1EdNS_15FilterOperation13OperationTypeE(nullptr, 0.5, kOpOpacity);
    });
    RequireThrowsPreservingErrno([&] {
        _ZN7WebCore37BasicComponentTransferFilterOperationC2EdNS_15FilterOperation13OperationTypeE(unreadableTransfer, 0.5, kOpOpacity);
    });

    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(nullptr, color));
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(unreadable, color));
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(validColorMatrix.get(), *unreadableColor));
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(nullptr, color));
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(validTransfer.get(), *unreadableColor));
    });

    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(nullptr, nullptr, 0.5, false));
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(validColorMatrix.get(), unreadable, 0.5, false));
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb(nullptr, nullptr, 0.5, false));
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb(validTransfer.get(), unreadableTransfer, 0.5, false));
    });

    auto* roMatrix = reinterpret_cast<WebCore::BasicColorMatrixFilterOperation*>(guarded.base);
    _ZN7WebCore31BasicColorMatrixFilterOperationC1EdNS_15FilterOperation13OperationTypeE(roMatrix, 0.5, kOpGrayscale);
    auto* roColor = reinterpret_cast<WebCore::SRGBA<float>*>(guarded.base + 64);
    *roColor = color;
    guarded.MakeFirstPageReadOnly();

    RequireThrowsPreservingErrno([&] {
        _ZN7WebCore31BasicColorMatrixFilterOperationC1EdNS_15FilterOperation13OperationTypeE(roMatrix, 0.25, kOpSepia);
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(validColorMatrix.get(), *roColor));
    });
    RequireThrowsPreservingErrno([&] {
        static_cast<void>(_ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(roMatrix, validTransfer.get(), 0.5, false));
    });
    guarded.MakeFirstPageReadWrite();
}

} // namespace

int main() {
    TestColorMatrixConstructionAndVtable();
    TestColorMatrixTransformColor();
    TestColorMatrixBlend();
    TestComponentTransferConstructionAndVtable();
    TestComponentTransferTransformColor();
    TestComponentTransferBlend();
    TestGuestPointerValidation();
    return 0;
}
