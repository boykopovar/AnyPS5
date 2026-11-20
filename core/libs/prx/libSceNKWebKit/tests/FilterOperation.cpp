#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libSceNKWebKit FilterOperation check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static_assert(sizeof(WTF::Ref<WebCore::DefaultFilterOperation>) == 8);
static_assert(sizeof(WTF::RefPtr<WebCore::FilterOperation>) == 8);
static_assert(!std::is_trivially_destructible_v<WTF::Ref<WebCore::DefaultFilterOperation>>);
static_assert(!std::is_trivially_destructible_v<WTF::RefPtr<WebCore::FilterOperation>>);
static_assert(!std::is_trivially_destructible_v<WTF::Ref<WebCore::FilterOperation>>);

bool Near(float actual, float expected, float tolerance = 1e-5f) {
    return std::fabs(actual - expected) <= tolerance;
}

bool NearColor(const WebCore::SRGBA<float>& color, float red, float green, float blue, float alpha) {
    return Near(color.red, red) && Near(color.green, green) && Near(color.blue, blue) && Near(color.alpha, alpha);
}

struct Outsets {
    std::int32_t sides[4];
};

struct FilterOperationVtable {
    void (APS5_VABI* completeDestructor)(void*);
    void (APS5_VABI* deletingDestructor)(void*);
    WTF::Ref<WebCore::FilterOperation> (APS5_VABI* clone)(const void*);
    bool (APS5_VABI* equals)(const void*, const WebCore::FilterOperation&);
    WTF::RefPtr<WebCore::FilterOperation> (APS5_VABI* blend)(void*, const WebCore::FilterOperation*, double, bool);
    bool (APS5_VABI* transformColor)(const void*, WebCore::SRGBA<float>&);
    bool (APS5_VABI* inverseTransformColor)(const void*, WebCore::SRGBA<float>&);
    bool (APS5_VABI* isIdentity)(const void*);
    Outsets (APS5_VABI* outsets)(const void*);
    bool (APS5_VABI* affectsOpacity)(const void*);
    bool (APS5_VABI* movesPixels)(const void*);
    bool (APS5_VABI* shouldBeRestrictedBySecurityOrigin)(const void*);
};

const FilterOperationVtable& VtableOf(const void* object) {
    return **reinterpret_cast<const FilterOperationVtable* const*>(object);
}

template<typename Function> bool ThrowsNotImplemented(Function function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestDefaultCreate() {
    auto operation = _ZN7WebCore22DefaultFilterOperation6createENS_15FilterOperation13OperationTypeE(
        static_cast<std::uint32_t>(WebCore::FilterOperationType::Blur));
    auto* blur = operation.get();
    Require(blur != nullptr);
    Require(_ZNK7WebCore22DefaultFilterOperation15representedTypeEv(blur) ==
            static_cast<std::uint32_t>(WebCore::FilterOperationType::Blur));
    Require(blur->m_type == WebCore::FilterOperationType::Default);
    Require(blur->m_refCount.load() == 1);
    Require(blur->m_vptr != nullptr);

    const auto& vtable = VtableOf(blur);
    WebCore::SRGBA<float> color{0.25f, 0.5f, 0.75f, 0.5f};
    Require(!vtable.transformColor(blur, color));
    Require(color.red == 0.25f && color.green == 0.5f && color.blue == 0.75f && color.alpha == 0.5f);
    Require(!vtable.inverseTransformColor(blur, color));
    Require(color.red == 0.25f && color.green == 0.5f && color.blue == 0.75f && color.alpha == 0.5f);
    Require(!vtable.isIdentity(blur));
    Require(!vtable.affectsOpacity(blur));
    Require(!vtable.movesPixels(blur));
    Require(!vtable.shouldBeRestrictedBySecurityOrigin(blur));
    Outsets outsets = vtable.outsets(blur);
    Require(outsets.sides[0] == 0 && outsets.sides[1] == 0 && outsets.sides[2] == 0 && outsets.sides[3] == 0);
    Require(!vtable.blend(blur, nullptr, 0.5, false).get());
    Require(ThrowsNotImplemented([&] { vtable.clone(blur); }));
    Require(ThrowsNotImplemented([&] { vtable.equals(blur, *blur); }));
}

void TestDefaultRepresentedTypes() {
    const std::uint32_t types[] = {
        static_cast<std::uint32_t>(WebCore::FilterOperationType::Reference),
        static_cast<std::uint32_t>(WebCore::FilterOperationType::Invert),
        static_cast<std::uint32_t>(WebCore::FilterOperationType::AppleInvertLightness),
        static_cast<std::uint32_t>(WebCore::FilterOperationType::None),
        300,
    };
    for (std::uint32_t type : types) {
        auto operation = _ZN7WebCore22DefaultFilterOperation6createENS_15FilterOperation13OperationTypeE(type);
        auto* operationPtr = operation.get();
        Require(operationPtr != nullptr);
        Require(_ZNK7WebCore22DefaultFilterOperation15representedTypeEv(operationPtr) == type);
        Require(operationPtr->m_type == WebCore::FilterOperationType::Default);
        Require(operationPtr->m_refCount.load() == 1);
    }
}

void TestDefaultConstructors() {
    alignas(8) std::byte second[24];
    auto* baseObject = reinterpret_cast<WebCore::DefaultFilterOperation*>(second);
    _ZN7WebCore22DefaultFilterOperationC2ENS_15FilterOperation13OperationTypeE(
        baseObject, static_cast<std::uint32_t>(WebCore::FilterOperationType::HueRotate));
    Require(_ZNK7WebCore22DefaultFilterOperation15representedTypeEv(baseObject) ==
            static_cast<std::uint32_t>(WebCore::FilterOperationType::HueRotate));
    Require(baseObject->m_type == WebCore::FilterOperationType::Default);
    Require(baseObject->m_refCount.load() == 1);
    WebCore::SRGBA<float> untouched{};
    Require(VtableOf(baseObject).transformColor(baseObject, untouched) == false);

    alignas(8) std::byte complete[24];
    auto* completeObject = reinterpret_cast<WebCore::DefaultFilterOperation*>(complete);
    _ZN7WebCore22DefaultFilterOperationC1ENS_15FilterOperation13OperationTypeE(
        completeObject, static_cast<std::uint32_t>(WebCore::FilterOperationType::Opacity));
    Require(_ZNK7WebCore22DefaultFilterOperation15representedTypeEv(completeObject) ==
            static_cast<std::uint32_t>(WebCore::FilterOperationType::Opacity));
    Require(completeObject->m_type == WebCore::FilterOperationType::Default);
    Require(completeObject->m_refCount.load() == 1);
    WebCore::SRGBA<float> completeUntouched{};
    Require(VtableOf(completeObject).transformColor(completeObject, completeUntouched) == false);

    bool threw = false;
    try {
        _ZN7WebCore22DefaultFilterOperationC1ENS_15FilterOperation13OperationTypeE(nullptr, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        _ZN7WebCore22DefaultFilterOperationC2ENS_15FilterOperation13OperationTypeE(nullptr, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

WebCore::InvertLightnessFilterOperation* FakeInvertLightness() {
    auto* fake = new WebCore::InvertLightnessFilterOperation();
    fake->m_vptr = nullptr;
    fake->m_type = WebCore::FilterOperationType::AppleInvertLightness;
    return fake;
}

void TestInvertLightnessBlend() {
    auto* self = FakeInvertLightness();
    auto defaultOperation = _ZN7WebCore22DefaultFilterOperation6createENS_15FilterOperation13OperationTypeE(
        static_cast<std::uint32_t>(WebCore::FilterOperationType::Grayscale));
    auto* defaultOperationPtr = defaultOperation.get();
    auto* other = FakeInvertLightness();

    {
        auto blended = _ZN7WebCore30InvertLightnessFilterOperation5blendEPKNS_15FilterOperationEdb(self, nullptr, 0.5, false);
        Require(blended.get() != nullptr);
        Require(blended.get() != self);
        Require(blended->m_type == WebCore::FilterOperationType::AppleInvertLightness);
        Require(blended->m_refCount.load() == 1);
    }

    {
        auto blended = _ZN7WebCore30InvertLightnessFilterOperation5blendEPKNS_15FilterOperationEdb(self, self, 0.0, true);
        Require(blended.get() != nullptr);
        Require(blended.get() != self);
        Require(blended->m_type == WebCore::FilterOperationType::AppleInvertLightness);
        Require(blended->m_refCount.load() == 1);
    }

    {
        auto blended = _ZN7WebCore30InvertLightnessFilterOperation5blendEPKNS_15FilterOperationEdb(self, other, 1.0, false);
        Require(blended.get() != nullptr);
        Require(blended.get() != self);
        Require(blended->m_type == WebCore::FilterOperationType::AppleInvertLightness);
        Require(blended->m_refCount.load() == 1);
    }

    {
        auto blended = _ZN7WebCore30InvertLightnessFilterOperation5blendEPKNS_15FilterOperationEdb(self, defaultOperationPtr, 0.25, true);
        Require(blended.get() == self);
        Require(self->m_refCount.load() == 2);
    }
    Require(self->m_refCount.load() == 1);

    bool threw = false;
    try {
        _ZN7WebCore30InvertLightnessFilterOperation5blendEPKNS_15FilterOperationEdb(nullptr, nullptr, 0.0, false);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    delete other;
    delete self;
}

void TestInvertLightnessTransformColor() {
    auto* self = FakeInvertLightness();

    WebCore::SRGBA<float> white{1.0f, 1.0f, 1.0f, 1.0f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(self, white));
    Require(NearColor(white, 0.2f, 0.2f, 0.199f, 1.0f));

    WebCore::SRGBA<float> black{0.0f, 0.0f, 0.0f, 1.0f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(self, black));
    Require(NearColor(black, 1.0f, 1.0f, 1.0f, 1.0f));

    WebCore::SRGBA<float> red{1.0f, 0.0f, 0.0f, 1.0f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(self, red));
    Require(NearColor(red, 0.97f, 0.17f, 0.169f, 1.0f));

    WebCore::SRGBA<float> extended{1.5f, 0.0f, 0.0f, 1.0f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(self, extended));
    Require(NearColor(extended, 0.955f, 0.0f, 0.0f, 1.0f));

    WebCore::SRGBA<float> alphaChannel{0.0f, 0.0f, 0.0f, 0.25f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(self, alphaChannel));
    Require(NearColor(alphaChannel, 1.0f, 1.0f, 1.0f, 0.25f));

    WebCore::SRGBA<float> highAlpha{0.0f, 0.0f, 0.0f, 2.0f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(self, highAlpha));
    Require(NearColor(highAlpha, 1.0f, 1.0f, 1.0f, 1.0f));

    WebCore::SRGBA<float> lowAlpha{0.0f, 0.0f, 0.0f, -1.0f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(self, lowAlpha));
    Require(NearColor(lowAlpha, 1.0f, 1.0f, 1.0f, 0.0f));

    WebCore::SRGBA<float> nanRed{std::nanf(""), 0.0f, 0.0f, 1.0f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(self, nanRed));
    Require(NearColor(nanRed, 1.0f, 1.0f, 1.0f, 1.0f));

    bool threw = false;
    WebCore::SRGBA<float> color{};
    try {
        _ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(nullptr, color);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void TestInvertLightnessInverseTransformColor() {
    auto* self = FakeInvertLightness();

    WebCore::SRGBA<float> white{1.0f, 1.0f, 1.0f, 1.0f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation21inverseTransformColorERNS_5SRGBAIfEE(self, white));
    Require(NearColor(white, 0.0f, 0.0f, 0.0f, 1.0f));

    WebCore::SRGBA<float> black{0.0f, 0.0f, 0.0f, 1.0f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation21inverseTransformColorERNS_5SRGBAIfEE(self, black));
    Require(NearColor(black, 1.0f, 1.0f, 1.0f, 1.0f));

    WebCore::SRGBA<float> red{1.0f, 0.0f, 0.0f, 1.0f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation21inverseTransformColorERNS_5SRGBAIfEE(self, red));
    Require(NearColor(red, 1.0f, 0.0f, 0.0f, 1.0f));

    WebCore::SRGBA<float> alphaChannel{0.0f, 0.0f, 0.0f, 0.5f};
    Require(_ZNK7WebCore30InvertLightnessFilterOperation21inverseTransformColorERNS_5SRGBAIfEE(self, alphaChannel));
    Require(NearColor(alphaChannel, 1.0f, 1.0f, 1.0f, 0.5f));

    bool threw = false;
    WebCore::SRGBA<float> color{};
    try {
        _ZNK7WebCore30InvertLightnessFilterOperation21inverseTransformColorERNS_5SRGBAIfEE(nullptr, color);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void TestInvertLightnessVtable() {
    auto* self = FakeInvertLightness();
    auto blendedHolder = _ZN7WebCore30InvertLightnessFilterOperation5blendEPKNS_15FilterOperationEdb(self, nullptr, 0.5, false);
    auto* blended = blendedHolder.get();
    Require(blended != nullptr);
    Require(blended->m_vptr != nullptr);

    const auto& vtable = VtableOf(blended);
    WebCore::SRGBA<float> white{1.0f, 1.0f, 1.0f, 1.0f};
    Require(vtable.transformColor(blended, white));
    Require(NearColor(white, 0.2f, 0.2f, 0.199f, 1.0f));

    WebCore::SRGBA<float> black{0.0f, 0.0f, 0.0f, 1.0f};
    Require(vtable.inverseTransformColor(blended, black));
    Require(NearColor(black, 1.0f, 1.0f, 1.0f, 1.0f));

    Require(!vtable.isIdentity(blended));
    Require(!vtable.affectsOpacity(blended));
    Require(!vtable.movesPixels(blended));
    Require(!vtable.shouldBeRestrictedBySecurityOrigin(blended));
    Outsets outsets = vtable.outsets(blended);
    Require(outsets.sides[0] == 0 && outsets.sides[1] == 0 && outsets.sides[2] == 0 && outsets.sides[3] == 0);
    {
        auto fromSameType = vtable.blend(blended, self, 0.5, false);
        Require(fromSameType.get() != nullptr);
        Require(fromSameType.get() != blended);
        Require(fromSameType->m_refCount.load() == 1);
    }
    {
        auto fromNull = vtable.blend(blended, nullptr, 0.5, false);
        Require(fromNull.get() != nullptr);
        Require(fromNull.get() != blended);
        Require(fromNull->m_refCount.load() == 1);
    }
    {
        auto defaultOperation = _ZN7WebCore22DefaultFilterOperation6createENS_15FilterOperation13OperationTypeE(0);
        auto returnedThis = vtable.blend(blended, defaultOperation.get(), 0.5, false);
        Require(returnedThis.get() == blended);
        Require(blended->m_refCount.load() == 2);
    }
    Require(blended->m_refCount.load() == 1);
    Require(ThrowsNotImplemented([&] { vtable.clone(blended); }));
    Require(ThrowsNotImplemented([&] { vtable.equals(blended, *blended); }));

    delete self;
}

}

int main() {
    TestDefaultCreate();
    TestDefaultRepresentedTypes();
    TestDefaultConstructors();
    TestInvertLightnessBlend();
    TestInvertLightnessTransformColor();
    TestInvertLightnessInverseTransformColor();
    TestInvertLightnessVtable();
    return 0;
}
