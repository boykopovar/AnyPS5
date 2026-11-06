#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using NKWebKit::FilterAbi::BlendingContext;
using NKWebKit::FilterAbi::FilterOperationPtr;
using NKWebKit::FilterAbi::FilterOperationRef;
using NKWebKit::FilterAbi::FilterOperationVtable;
using NKWebKit::FilterAbi::FilterOperations;
using NKWebKit::FilterAbi::IntOutsets;
using NKWebKit::FilterAbi::SRGBAf;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::TextStream;

using WebCore::BasicColorMatrixFilterOperation;
using WebCore::BasicComponentTransferFilterOperation;
using WebCore::BlurFilterOperation;
using WebCore::Color;
using WebCore::DefaultFilterOperation;
using WebCore::DropShadowFilterOperation;
using WebCore::FilterOperation;
using WebCore::FilterOperationType;
using WebCore::IntPoint;
using WebCore::InvertLightnessFilterOperation;
using WebCore::Length;
using WebCore::LengthType;

using BlurCreateSignature = FilterOperationRef (APS5_VABI*)(Length*);
using BlurConstructorSignature = void (APS5_VABI*)(BlurFilterOperation*, Length*);
using BlurDestructorSignature = void (APS5_VABI*)(BlurFilterOperation*);
using BlurStdDeviationSignature = const Length* (APS5_VABI*)(const BlurFilterOperation*);
using BlurCloneSignature = FilterOperationRef (APS5_VABI*)(const BlurFilterOperation*);
using BlurEqualsSignature = bool (APS5_VABI*)(const BlurFilterOperation*, const FilterOperation*);
using DropShadowCreateSignature = FilterOperationRef (APS5_VABI*)(const IntPoint*, int, const Color*);
using DropShadowConstructorSignature = void (APS5_VABI*)(DropShadowFilterOperation*, const IntPoint*, int, const Color*);
using DropShadowDestructorSignature = void (APS5_VABI*)(DropShadowFilterOperation*);
using DropShadowStdDeviationSignature = int (APS5_VABI*)(const DropShadowFilterOperation*);
using DropShadowCloneSignature = FilterOperationRef (APS5_VABI*)(const DropShadowFilterOperation*);
using DropShadowColorSignature = const Color* (APS5_VABI*)(const DropShadowFilterOperation*);
using DropShadowLocationSignature = IntPoint (APS5_VABI*)(const DropShadowFilterOperation*);
using DropShadowEqualsSignature = bool (APS5_VABI*)(const DropShadowFilterOperation*, const FilterOperation*);
using InvertLightnessCreateSignature = FilterOperationRef (APS5_VABI*)();
using InvertLightnessConstructorSignature = void (APS5_VABI*)(InvertLightnessFilterOperation*);
using OperationPrinterSignature = TextStream* (APS5_VABI*)(TextStream*, const FilterOperation*);
using OperationsPrinterSignature = TextStream* (APS5_VABI*)(TextStream*, const FilterOperations*);
using DefaultRepresentedTypeSignature = FilterOperationType (APS5_VABI*)(const DefaultFilterOperation*);
using DefaultCloneSignature = FilterOperationRef (APS5_VABI*)(const DefaultFilterOperation*);
using DefaultEqualsSignature = bool (APS5_VABI*)(const DefaultFilterOperation*, const FilterOperation*);
using ColorMatrixPassthroughSignature = double (APS5_VABI*)(const BasicColorMatrixFilterOperation*);
using ColorMatrixCloneSignature = FilterOperationRef (APS5_VABI*)(const BasicColorMatrixFilterOperation*);
using ColorMatrixAmountSignature = double (APS5_VABI*)(const BasicColorMatrixFilterOperation*);
using ColorMatrixEqualsSignature = bool (APS5_VABI*)(const BasicColorMatrixFilterOperation*, const FilterOperation*);
using ComponentTransferAffectsOpacitySignature = bool (APS5_VABI*)(const BasicComponentTransferFilterOperation*);
using ComponentTransferPassthroughSignature = double (APS5_VABI*)(const BasicComponentTransferFilterOperation*);
using ComponentTransferCloneSignature = FilterOperationRef (APS5_VABI*)(const BasicComponentTransferFilterOperation*);
using ComponentTransferAmountSignature = double (APS5_VABI*)(const BasicComponentTransferFilterOperation*);
using ComponentTransferEqualsSignature = bool (APS5_VABI*)(const BasicComponentTransferFilterOperation*, const FilterOperation*);
using InvertLightnessCloneSignature = FilterOperationRef (APS5_VABI*)(const InvertLightnessFilterOperation*);
using InvertLightnessEqualsSignature = bool (APS5_VABI*)(const InvertLightnessFilterOperation*, const FilterOperation*);

static_assert(std::is_same_v<decltype(&_ZN7WebCore19BlurFilterOperation6createENS_6LengthE), BlurCreateSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore19BlurFilterOperationC1ENS_6LengthE), BlurConstructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore19BlurFilterOperationC2ENS_6LengthE), BlurConstructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore19BlurFilterOperationD0Ev), BlurDestructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore19BlurFilterOperationD1Ev), BlurDestructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore19BlurFilterOperationD2Ev), BlurDestructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore25DropShadowFilterOperation6createERKNS_8IntPointEiRKNS_5ColorE), DropShadowCreateSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore25DropShadowFilterOperationC1ERKNS_8IntPointEiRKNS_5ColorE), DropShadowConstructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore25DropShadowFilterOperationC2ERKNS_8IntPointEiRKNS_5ColorE), DropShadowConstructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore25DropShadowFilterOperationD0Ev), DropShadowDestructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore25DropShadowFilterOperationD1Ev), DropShadowDestructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore25DropShadowFilterOperationD2Ev), DropShadowDestructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore30InvertLightnessFilterOperation6createEv), InvertLightnessCreateSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore30InvertLightnessFilterOperationC1Ev), InvertLightnessConstructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore30InvertLightnessFilterOperationC2Ev), InvertLightnessConstructorSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCorelsERN3WTF10TextStreamERKNS_15FilterOperationE), OperationPrinterSignature>);
static_assert(std::is_same_v<decltype(&_ZN7WebCorelsERN3WTF10TextStreamERKNS_16FilterOperationsE), OperationsPrinterSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore19BlurFilterOperation12stdDeviationEv), BlurStdDeviationSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore19BlurFilterOperation5cloneEv), BlurCloneSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE), BlurEqualsSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore22DefaultFilterOperation15representedTypeEv), DefaultRepresentedTypeSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore22DefaultFilterOperation5cloneEv), DefaultCloneSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE), DefaultEqualsSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore25DropShadowFilterOperation12stdDeviationEv), DropShadowStdDeviationSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore25DropShadowFilterOperation1xEv), DropShadowStdDeviationSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore25DropShadowFilterOperation1yEv), DropShadowStdDeviationSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore25DropShadowFilterOperation5cloneEv), DropShadowCloneSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore25DropShadowFilterOperation5colorEv), DropShadowColorSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore25DropShadowFilterOperation8locationEv), DropShadowLocationSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE), DropShadowEqualsSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore30InvertLightnessFilterOperation5cloneEv), InvertLightnessCloneSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore30InvertLightnessFilterOperationeqERKNS_15FilterOperationE), InvertLightnessEqualsSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv), ColorMatrixPassthroughSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore31BasicColorMatrixFilterOperation5cloneEv), ColorMatrixCloneSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore31BasicColorMatrixFilterOperation6amountEv), ColorMatrixAmountSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE), ColorMatrixEqualsSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv), ComponentTransferAffectsOpacitySignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv), ComponentTransferPassthroughSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore37BasicComponentTransferFilterOperation5cloneEv), ComponentTransferCloneSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore37BasicComponentTransferFilterOperation6amountEv), ComponentTransferAmountSignature>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE), ComponentTransferEqualsSignature>);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "FilterOperation check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

const FilterOperationVtable& Vtable(const FilterOperation* operation) {
    return *static_cast<const FilterOperationVtable*>(operation->m_vptr);
}

void ReleaseOperation(FilterOperation* operation) {
    if (operation->m_refCount.fetch_sub(1) == 1) Vtable(operation).deletingDestructor(operation);
}

Length MakeIntLength(LengthType type, int value, bool quirk = false) {
    Length length;
    length.m_type = type;
    length.m_hasQuirk = quirk;
    length.m_isFloat = false;
    length.m_intValue = value;
    return length;
}

Length MakeFloatLength(LengthType type, float value, bool quirk = false) {
    Length length;
    length.m_type = type;
    length.m_hasQuirk = quirk;
    length.m_isFloat = true;
    length.m_floatValue = value;
    return length;
}

Length MakeCalculatedLength(unsigned handle) {
    Length length;
    length.m_type = LengthType::Calculated;
    length.m_hasQuirk = false;
    length.m_isFloat = false;
    length.m_calculationValueHandle = handle;
    return length;
}

void FillShadow(DropShadowFilterOperation& operation, const IntPoint& location, int stdDeviation, const Color& color) {
    operation.m_type = FilterOperationType::DropShadow;
    operation.m_location = location;
    operation.m_stdDeviation = stdDeviation;
    operation.m_color = color;
}

template<typename T>
std::ptrdiff_t OffsetOf(const void* base, const T* member) {
    return reinterpret_cast<const char*>(member) - reinterpret_cast<const char*>(base);
}

StringBuilder NewBuilder() {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    return builder;
}

TextStream NewStream() {
    TextStream stream{NewBuilder()};
    stream.text = NewBuilder();
    stream.formattingFlags = 0;
    stream.multiLineMode = false;
    return stream;
}

std::u16string Units(const String& string) {
    if (!string.impl) return {};
    std::u16string result;
    for (unsigned i = 0; i < string.impl->length; ++i) {
        if (string.impl->hashAndFlags & 4) result += static_cast<const unsigned char*>(string.impl->data)[i];
        else result += static_cast<const char16_t*>(string.impl->data)[i];
    }
    return result;
}

std::u16string ReleaseStream(TextStream& stream) {
    return Units(_ZN3WTF10TextStream7releaseEv(&stream));
}

std::u16string Print(const FilterOperation* operation) {
    auto stream = NewStream();
    Require(_ZN7WebCorelsERN3WTF10TextStreamERKNS_15FilterOperationE(&stream, operation) == &stream);
    return ReleaseStream(stream);
}

FilterOperationRef CreateBlur(float value) {
    Length deviation = MakeFloatLength(LengthType::Fixed, value);
    return _ZN7WebCore19BlurFilterOperation6createENS_6LengthE(&deviation);
}

FilterOperationRef CreateShadow(const IntPoint& location, int stdDeviation, const Color& color) {
    return _ZN7WebCore25DropShadowFilterOperation6createERKNS_8IntPointEiRKNS_5ColorE(&location, stdDeviation, &color);
}

FilterOperationRef CreateColorMatrix(FilterOperationType type, double amount) {
    BasicColorMatrixFilterOperation source{};
    source.m_type = type;
    source.m_amount = amount;
    return _ZNK7WebCore31BasicColorMatrixFilterOperation5cloneEv(&source);
}

FilterOperationRef CreateComponentTransfer(FilterOperationType type, double amount) {
    BasicComponentTransferFilterOperation source{};
    source.m_type = type;
    source.m_amount = amount;
    return _ZNK7WebCore37BasicComponentTransferFilterOperation5cloneEv(&source);
}

FilterOperationRef CreateDefault(FilterOperationType representedType) {
    DefaultFilterOperation source{};
    source.m_type = FilterOperationType::Default;
    source.m_representedType = representedType;
    return _ZNK7WebCore22DefaultFilterOperation5cloneEv(&source);
}

void TestLayouts() {
    auto blur = CreateBlur(1.0f);
    auto* blurOperation = static_cast<const BlurFilterOperation*>(blur.Get());
    Require(OffsetOf(blurOperation, &blurOperation->m_vptr) == 0);
    Require(OffsetOf(blurOperation, &blurOperation->m_refCount) == 8);
    Require(OffsetOf(blurOperation, &blurOperation->m_type) == 12);
    Require(OffsetOf(blurOperation, &blurOperation->m_stdDeviation) == 16);

    auto defaultOperation = CreateDefault(FilterOperationType::None);
    auto* defaultValue = static_cast<const DefaultFilterOperation*>(defaultOperation.Get());
    Require(OffsetOf(defaultValue, &defaultValue->m_representedType) == 16);

    auto colorMatrix = CreateColorMatrix(FilterOperationType::Grayscale, 1);
    auto* colorMatrixValue = static_cast<const BasicColorMatrixFilterOperation*>(colorMatrix.Get());
    Require(OffsetOf(colorMatrixValue, &colorMatrixValue->m_amount) == 16);

    auto componentTransfer = CreateComponentTransfer(FilterOperationType::Invert, 1);
    auto* componentTransferValue = static_cast<const BasicComponentTransferFilterOperation*>(componentTransfer.Get());
    Require(OffsetOf(componentTransferValue, &componentTransferValue->m_amount) == 16);

    IntPoint location{0, 0};
    Color color{0x00000000FF0000FFULL};
    auto shadow = CreateShadow(location, 1, color);
    auto* shadowValue = static_cast<const DropShadowFilterOperation*>(shadow.Get());
    Require(OffsetOf(shadowValue, &shadowValue->m_location) == 16);
    Require(OffsetOf(shadowValue, &shadowValue->m_stdDeviation) == 24);
    Require(OffsetOf(shadowValue, &shadowValue->m_color) == 32);

    auto invertLightness = _ZN7WebCore30InvertLightnessFilterOperation6createEv();
    Require(sizeof(*invertLightness.Get()) == sizeof(FilterOperation));
    std::printf("FilterOperation layouts: member offsets match the reference header\n");
}

void TestBlurConstruction() {
    Length argument = MakeFloatLength(LengthType::Fixed, 2.5f);
    auto created = _ZN7WebCore19BlurFilterOperation6createENS_6LengthE(&argument);
    Require(created.Get() != nullptr);
    Require(created.Get()->m_refCount == 1);
    Require(created.Get()->m_type == FilterOperationType::Blur);
    Require(argument.m_type == LengthType::Auto);
    auto* blur = static_cast<BlurFilterOperation*>(created.Get());
    Require(blur->m_stdDeviation.m_type == LengthType::Fixed);
    Require(blur->m_stdDeviation.m_isFloat);
    Require(blur->m_stdDeviation.m_floatValue == 2.5f);
    Require(_ZNK7WebCore19BlurFilterOperation12stdDeviationEv(blur) == &blur->m_stdDeviation);
    Require(Vtable(blur).clone != nullptr);
    Require(Vtable(blur).equals(blur, blur));

    BlurFilterOperation stack{};
    Length integer = MakeIntLength(LengthType::Fixed, 4);
    _ZN7WebCore19BlurFilterOperationC1ENS_6LengthE(&stack, &integer);
    Require(integer.m_type == LengthType::Auto);
    Require(stack.m_type == FilterOperationType::Blur);
    Require(stack.m_refCount == 1);
    Require(stack.m_stdDeviation.m_type == LengthType::Fixed);
    Require(!stack.m_stdDeviation.m_isFloat);
    Require(stack.m_stdDeviation.m_intValue == 4);
    _ZN7WebCore19BlurFilterOperationD1Ev(&stack);

    BlurFilterOperation copyConstructed{};
    Length relative = MakeIntLength(LengthType::Relative, -3);
    _ZN7WebCore19BlurFilterOperationC2ENS_6LengthE(&copyConstructed, &relative);
    Require(relative.m_type == LengthType::Auto);
    Require(copyConstructed.m_stdDeviation.m_type == LengthType::Relative);
    Require(copyConstructed.m_stdDeviation.m_intValue == -3);
    _ZN7WebCore19BlurFilterOperationD2Ev(&copyConstructed);

    Length calculated = MakeCalculatedLength(7);
    RequireThrows([&] { _ZN7WebCore19BlurFilterOperation6createENS_6LengthE(&calculated); });
    Require(calculated.m_type == LengthType::Calculated);
    Require(calculated.m_calculationValueHandle == 7);
    RequireThrows([&] { _ZN7WebCore19BlurFilterOperationC1ENS_6LengthE(&stack, &calculated); });
    Require(calculated.m_type == LengthType::Calculated);
    Require(calculated.m_calculationValueHandle == 7);

    Length undefined = MakeIntLength(LengthType::Undefined, 0);
    auto undefinedBlur = _ZN7WebCore19BlurFilterOperation6createENS_6LengthE(&undefined);
    auto* undefinedOperation = static_cast<BlurFilterOperation*>(undefinedBlur.Get());
    Require(undefinedOperation->m_stdDeviation.m_type == LengthType::Undefined);
    Require(undefinedOperation->m_stdDeviation.m_intValue == 0);
    Require(!undefinedOperation->m_stdDeviation.m_isFloat);
    std::printf("BlurFilterOperation construction: move, copy, calculated rejection passed\n");
}

void TestDropShadowConstruction() {
    IntPoint location{3, -4};
    Color color{0x00000000FF0000FFULL};
    auto created = CreateShadow(location, 5, color);
    Require(created.Get() != nullptr);
    Require(created.Get()->m_refCount == 1);
    Require(created.Get()->m_type == FilterOperationType::DropShadow);
    auto* shadow = static_cast<DropShadowFilterOperation*>(created.Get());
    Require(_ZNK7WebCore25DropShadowFilterOperation1xEv(shadow) == 3);
    Require(_ZNK7WebCore25DropShadowFilterOperation1yEv(shadow) == -4);
    IntPoint reported = _ZNK7WebCore25DropShadowFilterOperation8locationEv(shadow);
    Require(reported.m_x == 3 && reported.m_y == -4);
    Require(_ZNK7WebCore25DropShadowFilterOperation12stdDeviationEv(shadow) == 5);
    Require(_ZNK7WebCore25DropShadowFilterOperation5colorEv(shadow) == &shadow->m_color);
    Require(shadow->m_color.m_colorAndFlags == 0x00000000FF0000FFULL);

    DropShadowFilterOperation stack{};
    _ZN7WebCore25DropShadowFilterOperationC1ERKNS_8IntPointEiRKNS_5ColorE(&stack, &location, 6, &color);
    Require(stack.m_type == FilterOperationType::DropShadow);
    Require(stack.m_refCount == 1);
    Require(stack.m_location.m_x == 3 && stack.m_location.m_y == -4);
    Require(stack.m_stdDeviation == 6);
    Require(stack.m_color.m_colorAndFlags == color.m_colorAndFlags);
    _ZN7WebCore25DropShadowFilterOperationD1Ev(&stack);

    DropShadowFilterOperation copyConstructed{};
    _ZN7WebCore25DropShadowFilterOperationC2ERKNS_8IntPointEiRKNS_5ColorE(&copyConstructed, &location, 7, &color);
    Require(copyConstructed.m_stdDeviation == 7);
    _ZN7WebCore25DropShadowFilterOperationD2Ev(&copyConstructed);

    Color outOfLine{0x0008000000000000ULL};
    RequireThrows([&] { CreateShadow(location, 5, outOfLine); });
    RequireThrows([&] { _ZN7WebCore25DropShadowFilterOperationC1ERKNS_8IntPointEiRKNS_5ColorE(&stack, &location, 5, &outOfLine); });
    std::printf("DropShadowFilterOperation construction: location, deviation, color, out-of-line rejection passed\n");
}

void TestVtables() {
    auto blur = CreateBlur(1.0f);
    auto blurClone = Vtable(blur.Get()).clone(blur.Get());
    Require(blurClone.Get() != blur.Get());
    Require(blurClone.Get()->m_refCount == 1);
    Require(blurClone.Get()->m_type == FilterOperationType::Blur);
    Require(static_cast<BlurFilterOperation*>(blurClone.Get())->m_stdDeviation.m_floatValue == 1.0f);

    IntPoint location{1, 2};
    Color color{0x00000000112233FFULL};
    auto shadow = CreateShadow(location, 3, color);
    auto shadowClone = Vtable(shadow.Get()).clone(shadow.Get());
    Require(shadowClone.Get() != shadow.Get());
    Require(shadowClone.Get()->m_type == FilterOperationType::DropShadow);
    Require(_ZNK7WebCore25DropShadowFilterOperation12stdDeviationEv(static_cast<const DropShadowFilterOperation*>(shadowClone.Get())) == 3);
    Require(Vtable(shadow.Get()).equals(shadow.Get(), shadow.Get()));

    auto invertLightness = _ZN7WebCore30InvertLightnessFilterOperation6createEv();
    Require(invertLightness.Get()->m_type == FilterOperationType::AppleInvertLightness);
    auto invertLightnessClone = Vtable(invertLightness.Get()).clone(invertLightness.Get());
    Require(invertLightnessClone.Get()->m_type == FilterOperationType::AppleInvertLightness);
    Require(Vtable(invertLightness.Get()).equals(invertLightness.Get(), invertLightness.Get()));

    auto defaultOperation = CreateDefault(FilterOperationType::Opacity);
    Require(defaultOperation.Get()->m_type == FilterOperationType::Default);
    auto defaultClone = Vtable(defaultOperation.Get()).clone(defaultOperation.Get());
    Require(defaultClone.Get()->m_type == FilterOperationType::Default);
    Require(_ZNK7WebCore22DefaultFilterOperation15representedTypeEv(static_cast<const DefaultFilterOperation*>(defaultClone.Get())) == FilterOperationType::Opacity);

    auto colorMatrix = CreateColorMatrix(FilterOperationType::Sepia, 0.25);
    auto colorMatrixClone = Vtable(colorMatrix.Get()).clone(colorMatrix.Get());
    Require(colorMatrixClone.Get()->m_type == FilterOperationType::Sepia);
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation6amountEv(static_cast<const BasicColorMatrixFilterOperation*>(colorMatrixClone.Get())) == 0.25);

    auto componentTransfer = CreateComponentTransfer(FilterOperationType::Brightness, 2.0);
    auto componentTransferClone = Vtable(componentTransfer.Get()).clone(componentTransfer.Get());
    Require(componentTransferClone.Get()->m_type == FilterOperationType::Brightness);
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation6amountEv(static_cast<const BasicComponentTransferFilterOperation*>(componentTransferClone.Get())) == 2.0);
    std::printf("FilterOperation vtables: clone and equality slots passed\n");
}

void TestClone() {
    auto blur = CreateBlur(2.5f);
    auto blurClone = _ZNK7WebCore19BlurFilterOperation5cloneEv(static_cast<const BlurFilterOperation*>(blur.Get()));
    Require(blurClone.Get() != blur.Get());
    Require(blurClone.Get()->m_refCount == 1);
    Require(blur.Get()->m_refCount == 1);
    Require(_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(static_cast<const BlurFilterOperation*>(blur.Get()), blurClone.Get()));
    Require(_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(static_cast<const BlurFilterOperation*>(blurClone.Get()), blur.Get()));

    IntPoint location{3, -4};
    Color color{0x00000000FF0000FFULL};
    auto shadow = CreateShadow(location, 5, color);
    auto shadowClone = _ZNK7WebCore25DropShadowFilterOperation5cloneEv(static_cast<const DropShadowFilterOperation*>(shadow.Get()));
    Require(shadowClone.Get() != shadow.Get());
    Require(shadowClone.Get()->m_refCount == 1);
    Require(shadow.Get()->m_refCount == 1);
    Require(_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(static_cast<const DropShadowFilterOperation*>(shadow.Get()), shadowClone.Get()));

    auto invertLightness = _ZN7WebCore30InvertLightnessFilterOperation6createEv();
    auto invertLightnessClone = _ZNK7WebCore30InvertLightnessFilterOperation5cloneEv(static_cast<const InvertLightnessFilterOperation*>(invertLightness.Get()));
    Require(invertLightnessClone.Get() != invertLightness.Get());
    Require(invertLightnessClone.Get()->m_refCount == 1);
    Require(_ZNK7WebCore30InvertLightnessFilterOperationeqERKNS_15FilterOperationE(static_cast<const InvertLightnessFilterOperation*>(invertLightness.Get()), invertLightnessClone.Get()));

    DefaultFilterOperation defaultSource{};
    defaultSource.m_type = FilterOperationType::Default;
    defaultSource.m_representedType = FilterOperationType::None;
    auto defaultClone = _ZNK7WebCore22DefaultFilterOperation5cloneEv(&defaultSource);
    Require(defaultClone.Get()->m_refCount == 1);
    Require(defaultSource.m_refCount == 1);
    Require(_ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE(&defaultSource, defaultClone.Get()));

    BasicColorMatrixFilterOperation colorMatrixSource{};
    colorMatrixSource.m_type = FilterOperationType::Grayscale;
    colorMatrixSource.m_amount = 0.5;
    auto colorMatrixClone = _ZNK7WebCore31BasicColorMatrixFilterOperation5cloneEv(&colorMatrixSource);
    Require(colorMatrixClone.Get()->m_refCount == 1);
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(&colorMatrixSource, colorMatrixClone.Get()));

    BasicComponentTransferFilterOperation componentTransferSource{};
    componentTransferSource.m_type = FilterOperationType::Opacity;
    componentTransferSource.m_amount = 0.5;
    auto componentTransferClone = _ZNK7WebCore37BasicComponentTransferFilterOperation5cloneEv(&componentTransferSource);
    Require(componentTransferClone.Get()->m_refCount == 1);
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE(&componentTransferSource, componentTransferClone.Get()));
    std::printf("FilterOperation clone: reference counts and equality passed\n");
}

void TestEquality() {
    BasicColorMatrixFilterOperation gray{};
    gray.m_type = FilterOperationType::Grayscale;
    gray.m_amount = 0.5;
    BasicColorMatrixFilterOperation sepia{};
    sepia.m_type = FilterOperationType::Sepia;
    sepia.m_amount = 0.5;
    Require(!_ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(&gray, &sepia));
    Require(!_ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(&sepia, &gray));

    BasicColorMatrixFilterOperation grayOther{};
    grayOther.m_type = FilterOperationType::Grayscale;
    grayOther.m_amount = 0.25;
    Require(!_ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(&gray, &grayOther));

    BasicColorMatrixFilterOperation graySame{};
    graySame.m_type = FilterOperationType::Grayscale;
    graySame.m_amount = 0.5;
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(&gray, &graySame));

    const double nan = std::numeric_limits<double>::quiet_NaN();
    BasicColorMatrixFilterOperation nanFirst{};
    nanFirst.m_type = FilterOperationType::Grayscale;
    nanFirst.m_amount = nan;
    BasicColorMatrixFilterOperation nanSecond{};
    nanSecond.m_type = FilterOperationType::Grayscale;
    nanSecond.m_amount = nan;
    Require(!_ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(&nanFirst, &nanSecond));

    BasicComponentTransferFilterOperation opacity{};
    opacity.m_type = FilterOperationType::Opacity;
    opacity.m_amount = 1;
    BasicComponentTransferFilterOperation brightness{};
    brightness.m_type = FilterOperationType::Brightness;
    brightness.m_amount = 1;
    Require(!_ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE(&opacity, &brightness));

    BlurFilterOperation undefinedFirst{};
    undefinedFirst.m_type = FilterOperationType::Blur;
    undefinedFirst.m_stdDeviation = MakeIntLength(LengthType::Undefined, 3);
    BlurFilterOperation undefinedSecond{};
    undefinedSecond.m_type = FilterOperationType::Blur;
    undefinedSecond.m_stdDeviation = MakeIntLength(LengthType::Undefined, 9);
    Require(_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(&undefinedFirst, &undefinedSecond));

    BlurFilterOperation integer{};
    integer.m_type = FilterOperationType::Blur;
    integer.m_stdDeviation = MakeIntLength(LengthType::Fixed, 3);
    BlurFilterOperation floating{};
    floating.m_type = FilterOperationType::Blur;
    floating.m_stdDeviation = MakeFloatLength(LengthType::Fixed, 3.0f);
    Require(_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(&integer, &floating));
    Require(_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(&floating, &integer));

    BlurFilterOperation quirked{};
    quirked.m_type = FilterOperationType::Blur;
    quirked.m_stdDeviation = MakeIntLength(LengthType::Fixed, 3, true);
    Require(!_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(&integer, &quirked));

    BlurFilterOperation percent{};
    percent.m_type = FilterOperationType::Blur;
    percent.m_stdDeviation = MakeIntLength(LengthType::Percent, 3);
    Require(!_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(&integer, &percent));

    IntPoint location{3, -4};
    Color color{0x00000000FF0000FFULL};
    DropShadowFilterOperation shadow{};
    FillShadow(shadow, location, 5, color);

    DropShadowFilterOperation otherLocation{};
    FillShadow(otherLocation, IntPoint{4, -4}, 5, color);
    Require(!_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(&shadow, &otherLocation));

    DropShadowFilterOperation otherDeviation{};
    FillShadow(otherDeviation, location, 6, color);
    Require(!_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(&shadow, &otherDeviation));

    DropShadowFilterOperation otherColor{};
    FillShadow(otherColor, location, 5, Color{0x0000000000FF00FFULL});
    Require(!_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(&shadow, &otherColor));

    DropShadowFilterOperation sameShadow{};
    FillShadow(sameShadow, location, 5, color);
    Require(_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(&shadow, &sameShadow));
    std::printf("FilterOperation equality: types, amounts, NaN, Length and color rules passed\n");
}

void TestPassthroughAmounts() {
    BasicColorMatrixFilterOperation colorMatrix{};
    colorMatrix.m_type = FilterOperationType::Grayscale;
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(&colorMatrix) == 0);
    colorMatrix.m_type = FilterOperationType::Sepia;
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(&colorMatrix) == 0);
    colorMatrix.m_type = FilterOperationType::HueRotate;
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(&colorMatrix) == 0);
    colorMatrix.m_type = FilterOperationType::Saturate;
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(&colorMatrix) == 1);
    colorMatrix.m_type = FilterOperationType::Opacity;
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(&colorMatrix) == 0);

    BasicComponentTransferFilterOperation componentTransfer{};
    componentTransfer.m_type = FilterOperationType::Opacity;
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(&componentTransfer) == 1);
    componentTransfer.m_type = FilterOperationType::Invert;
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(&componentTransfer) == 0);
    componentTransfer.m_type = FilterOperationType::Contrast;
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(&componentTransfer) == 1);
    componentTransfer.m_type = FilterOperationType::Brightness;
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(&componentTransfer) == 1);
    componentTransfer.m_type = FilterOperationType::Grayscale;
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(&componentTransfer) == 0);

    componentTransfer.m_type = FilterOperationType::Opacity;
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(&componentTransfer));
    componentTransfer.m_type = FilterOperationType::Invert;
    Require(!_ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(&componentTransfer));
    componentTransfer.m_type = FilterOperationType::Brightness;
    Require(!_ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(&componentTransfer));
    componentTransfer.m_type = FilterOperationType::Contrast;
    Require(!_ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(&componentTransfer));
    std::printf("FilterOperation accessors: passthrough amounts and affectsOpacity passed\n");
}

void TestIdentitySlots() {
    auto saturateOne = CreateColorMatrix(FilterOperationType::Saturate, 1);
    Require(Vtable(saturateOne.Get()).isIdentity(saturateOne.Get()));
    auto saturateHalf = CreateColorMatrix(FilterOperationType::Saturate, 0.5);
    Require(!Vtable(saturateHalf.Get()).isIdentity(saturateHalf.Get()));
    auto grayscaleZero = CreateColorMatrix(FilterOperationType::Grayscale, 0);
    Require(Vtable(grayscaleZero.Get()).isIdentity(grayscaleZero.Get()));

    auto invertZero = CreateComponentTransfer(FilterOperationType::Invert, 0);
    Require(Vtable(invertZero.Get()).isIdentity(invertZero.Get()));
    auto opacityOne = CreateComponentTransfer(FilterOperationType::Opacity, 1);
    Require(Vtable(opacityOne.Get()).isIdentity(opacityOne.Get()));
    auto opacityHalf = CreateComponentTransfer(FilterOperationType::Opacity, 0.5);
    Require(!Vtable(opacityHalf.Get()).isIdentity(opacityHalf.Get()));

    IntPoint origin{0, 0};
    Color color{0x00000000FF0000FFULL};
    auto shadowOrigin = CreateShadow(origin, 0, color);
    Require(Vtable(shadowOrigin.Get()).isIdentity(shadowOrigin.Get()));
    auto shadowNegative = CreateShadow(origin, -1, color);
    Require(Vtable(shadowNegative.Get()).isIdentity(shadowNegative.Get()));
    IntPoint offset{1, 0};
    auto shadowOffset = CreateShadow(offset, 0, color);
    Require(!Vtable(shadowOffset.Get()).isIdentity(shadowOffset.Get()));
    std::printf("FilterOperation isIdentity slots: color matrix, component transfer, drop shadow passed\n");
}

void TestConstantSlots() {
    auto blur = CreateBlur(1.0f);
    Require(Vtable(blur.Get()).affectsOpacity(blur.Get()) == (_ZNK7WebCore19BlurFilterOperation14affectsOpacityEv() != 0));
    Require(Vtable(blur.Get()).movesPixels(blur.Get()) == (_ZNK7WebCore19BlurFilterOperation11movesPixelsEv() != 0));
    Require(!Vtable(blur.Get()).shouldBeRestrictedBySecurityOrigin(blur.Get()));
    Require(!Vtable(blur.Get()).transformColor(blur.Get(), nullptr));
    Require(!Vtable(blur.Get()).inverseTransformColor(blur.Get(), nullptr));

    IntPoint location{0, 0};
    Color color{0x00000000FF0000FFULL};
    auto shadow = CreateShadow(location, 1, color);
    Require(Vtable(shadow.Get()).affectsOpacity(shadow.Get()) == (_ZNK7WebCore25DropShadowFilterOperation14affectsOpacityEv() != 0));
    Require(Vtable(shadow.Get()).movesPixels(shadow.Get()) == (_ZNK7WebCore25DropShadowFilterOperation11movesPixelsEv() != 0));
    Require(!Vtable(shadow.Get()).shouldBeRestrictedBySecurityOrigin(shadow.Get()));

    auto colorMatrix = CreateColorMatrix(FilterOperationType::Grayscale, 1);
    Require(!Vtable(colorMatrix.Get()).affectsOpacity(colorMatrix.Get()));
    Require(!Vtable(colorMatrix.Get()).movesPixels(colorMatrix.Get()));
    Require(!Vtable(colorMatrix.Get()).shouldBeRestrictedBySecurityOrigin(colorMatrix.Get()));
    IntOutsets outsets = Vtable(colorMatrix.Get()).outsets(colorMatrix.Get());
    Require(outsets.m_sides[0] == 0 && outsets.m_sides[1] == 0 && outsets.m_sides[2] == 0 && outsets.m_sides[3] == 0);

    auto componentTransfer = CreateComponentTransfer(FilterOperationType::Opacity, 1);
    Require(Vtable(componentTransfer.Get()).affectsOpacity(componentTransfer.Get()));
    auto componentTransferInvert = CreateComponentTransfer(FilterOperationType::Invert, 1);
    Require(!Vtable(componentTransferInvert.Get()).affectsOpacity(componentTransferInvert.Get()));

    auto invertLightness = _ZN7WebCore30InvertLightnessFilterOperation6createEv();
    Require(!Vtable(invertLightness.Get()).affectsOpacity(invertLightness.Get()));
    Require(!Vtable(invertLightness.Get()).movesPixels(invertLightness.Get()));
    Require(!Vtable(invertLightness.Get()).shouldBeRestrictedBySecurityOrigin(invertLightness.Get()));

    auto defaultOperation = CreateDefault(FilterOperationType::None);
    Require(!Vtable(defaultOperation.Get()).affectsOpacity(defaultOperation.Get()));
    Require(!Vtable(defaultOperation.Get()).movesPixels(defaultOperation.Get()));
    Require(!Vtable(defaultOperation.Get()).isIdentity(defaultOperation.Get()));
    IntOutsets defaultOutsets = Vtable(defaultOperation.Get()).outsets(defaultOperation.Get());
    Require(defaultOutsets.m_sides[0] == 0 && defaultOutsets.m_sides[1] == 0 && defaultOutsets.m_sides[2] == 0 && defaultOutsets.m_sides[3] == 0);
    std::printf("FilterOperation constant slots: opacity, pixels, security origin and outsets passed\n");
}

void TestBlendSlot() {
    auto defaultOperation = CreateDefault(FilterOperationType::Grayscale);
    auto noBlend = Vtable(defaultOperation.Get()).blend(defaultOperation.Get(), nullptr, nullptr, false);
    Require(noBlend.Get() == nullptr);

    auto invertLightness = _ZN7WebCore30InvertLightnessFilterOperation6createEv();
    auto fromNull = Vtable(invertLightness.Get()).blend(invertLightness.Get(), nullptr, nullptr, false);
    Require(fromNull.Get() != nullptr);
    Require(fromNull.Get() != invertLightness.Get());
    Require(fromNull.Get()->m_type == FilterOperationType::AppleInvertLightness);
    Require(fromNull.Get()->m_refCount == 1);

    BasicColorMatrixFilterOperation grayscale{};
    grayscale.m_type = FilterOperationType::Grayscale;
    auto fromOther = Vtable(invertLightness.Get()).blend(invertLightness.Get(), &grayscale, nullptr, false);
    Require(fromOther.Get() == invertLightness.Get());
    Require(invertLightness.Get()->m_refCount == 2);

    BlendingContext context{};
    RequireThrows([&] { Vtable(invertLightness.Get()).transformColor(invertLightness.Get(), nullptr); });
    RequireThrows([&] { Vtable(invertLightness.Get()).inverseTransformColor(invertLightness.Get(), nullptr); });

    auto colorMatrix = CreateColorMatrix(FilterOperationType::Grayscale, 1);
    RequireThrows([&] { Vtable(colorMatrix.Get()).blend(colorMatrix.Get(), nullptr, &context, false); });
    RequireThrows([&] { Vtable(colorMatrix.Get()).transformColor(colorMatrix.Get(), nullptr); });

    auto componentTransfer = CreateComponentTransfer(FilterOperationType::Invert, 1);
    RequireThrows([&] { Vtable(componentTransfer.Get()).blend(componentTransfer.Get(), nullptr, &context, false); });
    RequireThrows([&] { Vtable(componentTransfer.Get()).transformColor(componentTransfer.Get(), nullptr); });

    auto blur = CreateBlur(1.0f);
    RequireThrows([&] { Vtable(blur.Get()).blend(blur.Get(), nullptr, &context, false); });
    RequireThrows([&] { Vtable(blur.Get()).isIdentity(blur.Get()); });
    RequireThrows([&] { Vtable(blur.Get()).outsets(blur.Get()); });

    IntPoint location{0, 0};
    Color color{0x00000000FF0000FFULL};
    auto shadow = CreateShadow(location, 1, color);
    RequireThrows([&] { Vtable(shadow.Get()).blend(shadow.Get(), nullptr, &context, false); });
    RequireThrows([&] { Vtable(shadow.Get()).outsets(shadow.Get()); });
    std::printf("FilterOperation blend and throwing slots: null, this and NotImplemented passed\n");
}

void TestDestruction() {
    ReleaseOperation(CreateBlur(1.0f).Leak());
    ReleaseOperation(CreateColorMatrix(FilterOperationType::Sepia, 1).Leak());
    ReleaseOperation(CreateComponentTransfer(FilterOperationType::Contrast, 1).Leak());
    ReleaseOperation(_ZN7WebCore30InvertLightnessFilterOperation6createEv().Leak());
    ReleaseOperation(CreateDefault(FilterOperationType::None).Leak());
    IntPoint location{0, 0};
    Color color{0x00000000FF0000FFULL};
    ReleaseOperation(CreateShadow(location, 1, color).Leak());
    std::printf("FilterOperation destruction: deleting destructors freed every class\n");
}

void TestPrinting() {
    FilterOperation reference{};
    reference.m_type = FilterOperationType::Reference;
    Require(Print(&reference) == u"reference");

    BasicColorMatrixFilterOperation grayscale{};
    grayscale.m_type = FilterOperationType::Grayscale;
    grayscale.m_amount = 0.5;
    Require(Print(&grayscale) == u"grayscale(0.50)");

    grayscale.m_type = FilterOperationType::Sepia;
    Require(Print(&grayscale) == u"sepia(0.50)");
    grayscale.m_type = FilterOperationType::Saturate;
    grayscale.m_amount = 2;
    Require(Print(&grayscale) == u"saturate(2.00)");
    grayscale.m_type = FilterOperationType::HueRotate;
    grayscale.m_amount = 90;
    Require(Print(&grayscale) == u"hue-rotate(90.00)");

    BasicComponentTransferFilterOperation componentTransfer{};
    componentTransfer.m_type = FilterOperationType::Invert;
    componentTransfer.m_amount = 0.5;
    Require(Print(&componentTransfer) == u"invert(0.50)");
    componentTransfer.m_type = FilterOperationType::Opacity;
    componentTransfer.m_amount = 0.75;
    Require(Print(&componentTransfer) == u"opacity(0.75)");
    componentTransfer.m_type = FilterOperationType::Brightness;
    componentTransfer.m_amount = 1.5;
    Require(Print(&componentTransfer) == u"brightness(1.50)");
    componentTransfer.m_type = FilterOperationType::Contrast;
    componentTransfer.m_amount = 0.25;
    Require(Print(&componentTransfer) == u"contrast(0.25)");

    InvertLightnessFilterOperation invertLightness{};
    invertLightness.m_type = FilterOperationType::AppleInvertLightness;
    Require(Print(&invertLightness) == u"apple-invert-lightness()");

    BlurFilterOperation blur{};
    blur.m_type = FilterOperationType::Blur;
    blur.m_stdDeviation = MakeFloatLength(LengthType::Fixed, 2.5f);
    Require(Print(&blur) == u"blur(2.50)");
    blur.m_stdDeviation = MakeIntLength(LengthType::Fixed, 5);
    Require(Print(&blur) == u"blur(5.00)");

    FilterOperation passthrough{};
    passthrough.m_type = FilterOperationType::Passthrough;
    Require(Print(&passthrough) == u"passthrough");

    DefaultFilterOperation defaultOperation{};
    defaultOperation.m_type = FilterOperationType::Default;
    defaultOperation.m_representedType = static_cast<FilterOperationType>(13);
    Require(Print(&defaultOperation) == u"default type=13");

    FilterOperation none{};
    none.m_type = FilterOperationType::None;
    Require(Print(&none) == u"none");

    IntPoint location{3, -4};
    Color color{0x00000000FF0000FFULL};
    auto shadow = CreateShadow(location, 5, color);
    auto stream = NewStream();
    RequireThrows([&] { _ZN7WebCorelsERN3WTF10TextStreamERKNS_15FilterOperationE(&stream, shadow.Get()); });
    Require(ReleaseStream(stream).empty());

    BasicColorMatrixFilterOperation listedGrayscale{};
    listedGrayscale.m_type = FilterOperationType::Grayscale;
    listedGrayscale.m_amount = 1;
    FilterOperation* entries[3] = {&listedGrayscale, nullptr, &invertLightness};
    FilterOperations operations{entries, 3, 3};
    auto operationsStream = NewStream();
    Require(_ZN7WebCorelsERN3WTF10TextStreamERKNS_16FilterOperationsE(&operationsStream, &operations) == &operationsStream);
    Require(ReleaseStream(operationsStream) == u"grayscale(1.00) (null) apple-invert-lightness()");

    FilterOperations empty{nullptr, 0, 0};
    auto emptyStream = NewStream();
    Require(_ZN7WebCorelsERN3WTF10TextStreamERKNS_16FilterOperationsE(&emptyStream, &empty) == &emptyStream);
    Require(ReleaseStream(emptyStream).empty());

    DropShadowFilterOperation listedShadow{};
    FillShadow(listedShadow, IntPoint{0, 0}, 1, Color{0x00000000FF0000FFULL});
    FilterOperation* dropShadowEntries[2] = {&listedGrayscale, &listedShadow};
    FilterOperations dropShadowList{dropShadowEntries, 2, 2};
    auto dropShadowListStream = NewStream();
    RequireThrows([&] { _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FilterOperationsE(&dropShadowListStream, &dropShadowList); });
    Require(ReleaseStream(dropShadowListStream) == u"grayscale(1.00) ");
    std::printf("FilterOperation printing: every type, drop-shadow throw, FilterOperations list passed\n");
}

void TestNullPointers() {
    RequireThrows([] { _ZN7WebCore19BlurFilterOperation6createENS_6LengthE(nullptr); });
    RequireThrows([] { _ZN7WebCore19BlurFilterOperationC1ENS_6LengthE(nullptr, nullptr); });
    RequireThrows([] { _ZN7WebCore19BlurFilterOperationC2ENS_6LengthE(nullptr, nullptr); });
    RequireThrows([] { _ZN7WebCore19BlurFilterOperationD0Ev(nullptr); });
    RequireThrows([] { _ZN7WebCore19BlurFilterOperationD1Ev(nullptr); });
    RequireThrows([] { _ZN7WebCore19BlurFilterOperationD2Ev(nullptr); });
    RequireThrows([] { _ZN7WebCore25DropShadowFilterOperation6createERKNS_8IntPointEiRKNS_5ColorE(nullptr, 0, nullptr); });
    RequireThrows([] { _ZN7WebCore25DropShadowFilterOperationC1ERKNS_8IntPointEiRKNS_5ColorE(nullptr, nullptr, 0, nullptr); });
    RequireThrows([] { _ZN7WebCore25DropShadowFilterOperationC2ERKNS_8IntPointEiRKNS_5ColorE(nullptr, nullptr, 0, nullptr); });
    RequireThrows([] { _ZN7WebCore25DropShadowFilterOperationD0Ev(nullptr); });
    RequireThrows([] { _ZN7WebCore25DropShadowFilterOperationD1Ev(nullptr); });
    RequireThrows([] { _ZN7WebCore25DropShadowFilterOperationD2Ev(nullptr); });
    RequireThrows([] { _ZN7WebCore30InvertLightnessFilterOperationC1Ev(nullptr); });
    RequireThrows([] { _ZN7WebCore30InvertLightnessFilterOperationC2Ev(nullptr); });
    RequireThrows([] { _ZN7WebCorelsERN3WTF10TextStreamERKNS_15FilterOperationE(nullptr, nullptr); });
    RequireThrows([] { _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FilterOperationsE(nullptr, nullptr); });
    RequireThrows([] { _ZNK7WebCore19BlurFilterOperation12stdDeviationEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore19BlurFilterOperation5cloneEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(nullptr, nullptr); });
    RequireThrows([] { _ZNK7WebCore22DefaultFilterOperation15representedTypeEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore22DefaultFilterOperation5cloneEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE(nullptr, nullptr); });
    RequireThrows([] { _ZNK7WebCore25DropShadowFilterOperation12stdDeviationEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore25DropShadowFilterOperation1xEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore25DropShadowFilterOperation1yEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore25DropShadowFilterOperation5cloneEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore25DropShadowFilterOperation5colorEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore25DropShadowFilterOperation8locationEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(nullptr, nullptr); });
    RequireThrows([] { _ZNK7WebCore30InvertLightnessFilterOperation5cloneEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore30InvertLightnessFilterOperationeqERKNS_15FilterOperationE(nullptr, nullptr); });
    RequireThrows([] { _ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore31BasicColorMatrixFilterOperation5cloneEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore31BasicColorMatrixFilterOperation6amountEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(nullptr, nullptr); });
    RequireThrows([] { _ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore37BasicComponentTransferFilterOperation5cloneEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore37BasicComponentTransferFilterOperation6amountEv(nullptr); });
    RequireThrows([] { _ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE(nullptr, nullptr); });

    BlurFilterOperation blur{};
    RequireThrows([&] { _ZN7WebCore19BlurFilterOperationC1ENS_6LengthE(&blur, nullptr); });
    RequireThrows([&] { _ZN7WebCore19BlurFilterOperationC2ENS_6LengthE(&blur, nullptr); });

    IntPoint location{0, 0};
    Color color{0x00000000FF0000FFULL};
    DropShadowFilterOperation shadow{};
    RequireThrows([&] { _ZN7WebCore25DropShadowFilterOperation6createERKNS_8IntPointEiRKNS_5ColorE(&location, 0, nullptr); });
    RequireThrows([&] { _ZN7WebCore25DropShadowFilterOperationC1ERKNS_8IntPointEiRKNS_5ColorE(&shadow, nullptr, 0, &color); });
    RequireThrows([&] { _ZN7WebCore25DropShadowFilterOperationC1ERKNS_8IntPointEiRKNS_5ColorE(&shadow, &location, 0, nullptr); });
    RequireThrows([&] { _ZN7WebCore25DropShadowFilterOperationC2ERKNS_8IntPointEiRKNS_5ColorE(&shadow, nullptr, 0, &color); });
    RequireThrows([&] { _ZN7WebCore25DropShadowFilterOperationC2ERKNS_8IntPointEiRKNS_5ColorE(&shadow, &location, 0, nullptr); });

    DefaultFilterOperation defaultOperation{};
    defaultOperation.m_type = FilterOperationType::Default;
    InvertLightnessFilterOperation invertLightness{};
    invertLightness.m_type = FilterOperationType::AppleInvertLightness;
    BasicColorMatrixFilterOperation colorMatrix{};
    colorMatrix.m_type = FilterOperationType::Grayscale;
    BasicComponentTransferFilterOperation componentTransfer{};
    componentTransfer.m_type = FilterOperationType::Invert;
    RequireThrows([&] { _ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(&blur, nullptr); });
    RequireThrows([&] { _ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE(&defaultOperation, nullptr); });
    RequireThrows([&] { _ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(&shadow, nullptr); });
    RequireThrows([&] { _ZNK7WebCore30InvertLightnessFilterOperationeqERKNS_15FilterOperationE(&invertLightness, nullptr); });
    RequireThrows([&] { _ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(&colorMatrix, nullptr); });
    RequireThrows([&] { _ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE(&componentTransfer, nullptr); });

    auto validStream = NewStream();
    FilterOperations validOperations{nullptr, 0, 0};
    RequireThrows([&] { _ZN7WebCorelsERN3WTF10TextStreamERKNS_15FilterOperationE(&validStream, nullptr); });
    RequireThrows([&] { _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FilterOperationsE(&validStream, nullptr); });
    RequireThrows([&] { _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FilterOperationsE(nullptr, &validOperations); });

    std::printf("FilterOperation null pointers: every pointer argument threw, including later ones\n");
}

}  // namespace

int main() {
    TestLayouts();
    TestBlurConstruction();
    TestDropShadowConstruction();
    TestVtables();
    TestClone();
    TestEquality();
    TestPassthroughAmounts();
    TestIdentitySlots();
    TestConstantSlots();
    TestBlendSlot();
    TestDestruction();
    TestPrinting();
    TestNullPointers();
    return 0;
}
