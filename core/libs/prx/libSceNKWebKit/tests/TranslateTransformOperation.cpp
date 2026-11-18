#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;
using WebCore::Length;
using WebCore::LengthType;
using OpType = WebCore::TransformOperation::Type;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "TranslateTransformOperation check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Function> bool ThrowsInvalid(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<typename Function> bool ThrowsNotImplemented(Function function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

using Create3 = WebCore::TransformOperationRef (APS5_VABI*)(const Length&, const Length&, const Length&, OpType);
using Create2 = WebCore::TransformOperationRef (APS5_VABI*)(const Length&, const Length&, OpType);
using Ctor3 = void (APS5_VABI*)(WebCore::TranslateTransformOperation*, const Length&, const Length&, const Length&, OpType);

constexpr Create3 CreateFixed = _ZN7WebCore27TranslateTransformOperation6createERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE;
constexpr Create2 CreateXY = _ZN7WebCore27TranslateTransformOperation6createERKNS_6LengthES3_NS_18TransformOperation13OperationTypeE;
constexpr Ctor3 Construct = _ZN7WebCore27TranslateTransformOperationC1ERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE;

using Dtor = void (APS5_VABI*)(WebCore::TranslateTransformOperation*);
using GetLength = Length (APS5_VABI*)(const WebCore::TranslateTransformOperation*);
using GetFloat = float (APS5_VABI*)(const WebCore::TranslateTransformOperation*, const WebCore::FloatSize&);
using IsBool = bool (APS5_VABI*)(const WebCore::TranslateTransformOperation*);
using Apply = bool (APS5_VABI*)(const WebCore::TranslateTransformOperation*, WebCore::TransformationMatrix&, const WebCore::FloatSize&);
using Dump = void (APS5_VABI*)(const WebCore::TranslateTransformOperation*, TextStream&);
using Equal = bool (APS5_VABI*)(const WebCore::TranslateTransformOperation*, const WebCore::TransformOperation&);
using EqualPointer = bool (APS5_VABI*)(const WebCore::TranslateTransformOperation*, const WebCore::TransformOperation*);
using Blend = WebCore::TransformOperationRef (APS5_VABI*)(WebCore::TranslateTransformOperation*, const WebCore::TransformOperation*, double, bool);
using Clone = WebCore::TransformOperationRef (APS5_VABI*)(const WebCore::TranslateTransformOperation*);

constexpr Dtor D0 = _ZN7WebCore27TranslateTransformOperationD0Ev;
constexpr Dtor D1 = _ZN7WebCore27TranslateTransformOperationD1Ev;
constexpr Dtor D2 = _ZN7WebCore27TranslateTransformOperationD2Ev;
constexpr GetLength GetX = _ZNK7WebCore27TranslateTransformOperation1xEv;
constexpr GetLength GetY = _ZNK7WebCore27TranslateTransformOperation1yEv;
constexpr GetLength GetZ = _ZNK7WebCore27TranslateTransformOperation1zEv;
constexpr GetFloat GetXSize = _ZNK7WebCore27TranslateTransformOperation1xERKNS_9FloatSizeE;
constexpr GetFloat GetYSize = _ZNK7WebCore27TranslateTransformOperation1yERKNS_9FloatSizeE;
constexpr GetFloat GetZSize = _ZNK7WebCore27TranslateTransformOperation1zERKNS_9FloatSizeE;
constexpr IsBool IsIdentity = _ZNK7WebCore27TranslateTransformOperation10isIdentityEv;
constexpr IsBool IsRepresentableIn2D = _ZNK7WebCore27TranslateTransformOperation19isRepresentableIn2DEv;
constexpr Apply ApplyOp = _ZNK7WebCore27TranslateTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE;
constexpr Dump DumpOp = _ZNK7WebCore27TranslateTransformOperation4dumpERN3WTF10TextStreamE;
constexpr Equal EqualOp = _ZNK7WebCore27TranslateTransformOperationeqERKNS_18TransformOperationE;
constexpr Blend BlendOp = _ZN7WebCore27TranslateTransformOperation5blendEPKNS_18TransformOperationEdb;
constexpr Clone CloneOp = _ZNK7WebCore27TranslateTransformOperation5cloneEv;

bool SameLength(const Length& length, int value, LengthType type) {
    return !length.m_isFloat && length.m_intValue == value && length.type() == type && !length.m_hasQuirk;
}

bool SameFloatLength(const Length& length, float value, LengthType type) {
    return length.m_isFloat && length.m_floatValue == value && length.type() == type && !length.m_hasQuirk;
}

void TestConstructors() {
    WebCore::TranslateTransformOperation object;
    Construct(&object, Length{10, LengthType::Fixed}, Length{20, LengthType::Percent}, Length{30, LengthType::Fixed}, OpType::Translate3D);
    Require(object.m_refCount == 1);
    Require(object.m_type == OpType::Translate3D);
    Require(object.m_vptr != nullptr);
    Require(SameLength(object.m_x, 10, LengthType::Fixed));
    Require(SameLength(object.m_y, 20, LengthType::Percent));
    Require(SameLength(object.m_z, 30, LengthType::Fixed));

    WebCore::TranslateTransformOperation copyConstructed;
    _ZN7WebCore27TranslateTransformOperationC2ERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE(
        &copyConstructed, Length{1, LengthType::Fixed}, Length{2, LengthType::Fixed}, Length{3, LengthType::Fixed}, OpType::Translate);
    Require(copyConstructed.m_refCount == 1 && copyConstructed.m_type == OpType::Translate);
    Require(SameLength(copyConstructed.m_x, 1, LengthType::Fixed));

    Require(ThrowsInvalid([&] {
        WebCore::TranslateTransformOperation bad;
        Construct(&bad, Length{1, LengthType::Fixed}, Length{2, LengthType::Fixed}, Length{3, LengthType::Fixed}, OpType::Scale);
    }));
    Require(ThrowsInvalid([&] { CreateXY(Length{1, LengthType::Fixed}, Length{2, LengthType::Fixed}, OpType::RotateY); }));
    Require(ThrowsInvalid([&] { CreateFixed(Length{1, LengthType::Fixed}, Length{2, LengthType::Fixed}, Length{3, LengthType::Fixed}, OpType::Skew); }));

    Require(ThrowsInvalid([&] { D2(nullptr); }));
    Require(ThrowsInvalid([&] { D1(nullptr); }));

    Require(ThrowsNotImplemented([&] {
        WebCore::TranslateTransformOperation calculated;
        Construct(&calculated, Length{LengthType::Calculated}, Length{2, LengthType::Fixed}, Length{3, LengthType::Fixed}, OpType::Translate);
    }));
    Require(ThrowsNotImplemented([&] {
        WebCore::TranslateTransformOperation calculated;
        Construct(&calculated, Length{1, LengthType::Fixed}, Length{LengthType::Calculated}, Length{3, LengthType::Fixed}, OpType::Translate);
    }));
    Require(ThrowsNotImplemented([&] {
        WebCore::TranslateTransformOperation calculated;
        Construct(&calculated, Length{1, LengthType::Fixed}, Length{2, LengthType::Fixed}, Length{LengthType::Calculated}, OpType::Translate);
    }));
}

void TestCreate() {
    auto twoArg = CreateXY(Length{10, LengthType::Fixed}, Length{20, LengthType::Percent}, OpType::Translate);
    Require(twoArg.m_ptr != nullptr);
    Require(twoArg.m_ptr->m_refCount == 1);
    Require(twoArg.m_ptr->m_type == OpType::Translate);
    auto* operation = static_cast<WebCore::TranslateTransformOperation*>(twoArg.m_ptr);
    Require(SameLength(operation->m_x, 10, LengthType::Fixed));
    Require(SameLength(operation->m_y, 20, LengthType::Percent));
    Require(SameLength(operation->m_z, 0, LengthType::Fixed));

    auto threeArg = CreateFixed(Length{1, LengthType::Fixed}, Length{2, LengthType::Fixed}, Length{3, LengthType::Fixed}, OpType::TranslateZ);
    Require(threeArg.m_ptr != nullptr && threeArg.m_ptr->m_refCount == 1);
    Require(threeArg.m_ptr->m_type == OpType::TranslateZ);
    auto* zOperation = static_cast<WebCore::TranslateTransformOperation*>(threeArg.m_ptr);
    Require(SameLength(zOperation->m_x, 1, LengthType::Fixed));
    Require(SameLength(zOperation->m_y, 2, LengthType::Fixed));
    Require(SameLength(zOperation->m_z, 3, LengthType::Fixed));

    Require(SameLength(GetX(operation), 10, LengthType::Fixed));
    Require(SameLength(GetY(operation), 20, LengthType::Percent));
    Require(SameLength(GetZ(zOperation), 3, LengthType::Fixed));

    auto floatLengths = CreateFixed(Length{1.5f, LengthType::Fixed}, Length{2.25f, LengthType::Fixed}, Length{0.5f, LengthType::Fixed}, OpType::Translate);
    auto* floatLengthsSelf = static_cast<WebCore::TranslateTransformOperation*>(floatLengths.m_ptr);
    Require(SameFloatLength(GetX(floatLengthsSelf), 1.5f, LengthType::Fixed));
    Require(SameFloatLength(GetY(floatLengthsSelf), 2.25f, LengthType::Fixed));
    Require(SameFloatLength(GetZ(floatLengthsSelf), 0.5f, LengthType::Fixed));

    Require(ThrowsNotImplemented([&] { CreateXY(Length{LengthType::Calculated}, Length{2, LengthType::Fixed}, OpType::Translate); }));
    Require(ThrowsNotImplemented([&] { CreateFixed(Length{1, LengthType::Fixed}, Length{2, LengthType::Fixed}, Length{LengthType::Calculated}, OpType::TranslateZ); }));

    D0(floatLengthsSelf);
    D0(operation);
    D0(zOperation);
}

void TestFloatGetters() {
    auto operation = CreateFixed(Length{10, LengthType::Fixed}, Length{25, LengthType::Percent}, Length{12, LengthType::Percent}, OpType::Translate);
    auto* self = static_cast<WebCore::TranslateTransformOperation*>(operation.m_ptr);
    const WebCore::FloatSize borderBox{200.0f, 100.0f};
    Require(GetXSize(self, borderBox) == 10.0f);
    Require(GetYSize(self, borderBox) == 25.0f);
    Require(GetZSize(self, borderBox) == 0.12f);

    auto percent = CreateFixed(Length{50, LengthType::Percent}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* percentSelf = static_cast<WebCore::TranslateTransformOperation*>(percent.m_ptr);
    Require(GetXSize(percentSelf, borderBox) == 100.0f);
    Require(GetYSize(percentSelf, borderBox) == 0.0f);

    auto floatLength = CreateFixed(Length{1.5f, LengthType::Fixed}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* floatSelf = static_cast<WebCore::TranslateTransformOperation*>(floatLength.m_ptr);
    Require(GetXSize(floatSelf, borderBox) == 1.5f);

    auto fillLength = CreateFixed(Length{LengthType::FillAvailable}, Length{LengthType::Auto}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* fillSelf = static_cast<WebCore::TranslateTransformOperation*>(fillLength.m_ptr);
    Require(GetXSize(fillSelf, borderBox) == 200.0f);
    Require(GetYSize(fillSelf, borderBox) == 100.0f);

    Require(ThrowsNotImplemented([&] { CreateFixed(Length{LengthType::Calculated}, Length{LengthType::Calculated}, Length{LengthType::Calculated}, OpType::Translate); }));

    D0(floatSelf);
    D0(fillSelf);
    D0(percentSelf);
    D0(self);
}

void TestIdentityAnd2D() {
    auto identity = CreateFixed(Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* identitySelf = static_cast<WebCore::TranslateTransformOperation*>(identity.m_ptr);
    Require(IsIdentity(identitySelf));
    D0(identitySelf);

    auto floatZero = CreateFixed(Length{0.0f, LengthType::Fixed}, Length{0.0f, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* floatZeroSelf = static_cast<WebCore::TranslateTransformOperation*>(floatZero.m_ptr);
    Require(IsIdentity(floatZeroSelf));
    D0(floatZeroSelf);

    auto moved = CreateFixed(Length{1, LengthType::Fixed}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::TranslateX);
    auto* movedSelf = static_cast<WebCore::TranslateTransformOperation*>(moved.m_ptr);
    Require(!IsIdentity(movedSelf));
    D0(movedSelf);

    auto percentZero = CreateFixed(Length{0, LengthType::Percent}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* percentZeroSelf = static_cast<WebCore::TranslateTransformOperation*>(percentZero.m_ptr);
    Require(IsIdentity(percentZeroSelf));
    D0(percentZeroSelf);

    auto percentMoved = CreateFixed(Length{50, LengthType::Percent}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* percentMovedSelf = static_cast<WebCore::TranslateTransformOperation*>(percentMoved.m_ptr);
    Require(!IsIdentity(percentMovedSelf));
    D0(percentMovedSelf);

    auto autoMember = CreateFixed(Length{LengthType::Auto}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* autoMemberSelf = static_cast<WebCore::TranslateTransformOperation*>(autoMember.m_ptr);
    Require(!IsIdentity(autoMemberSelf));
    D0(autoMemberSelf);

    auto fillMember = CreateFixed(Length{LengthType::FillAvailable}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* fillMemberSelf = static_cast<WebCore::TranslateTransformOperation*>(fillMember.m_ptr);
    Require(!IsIdentity(fillMemberSelf));
    D0(fillMemberSelf);

    Require(ThrowsNotImplemented([&] { CreateFixed(Length{0, LengthType::Fixed}, Length{LengthType::Calculated}, Length{0, LengthType::Fixed}, OpType::Translate); }));

    auto flat = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* flatSelf = static_cast<WebCore::TranslateTransformOperation*>(flat.m_ptr);
    Require(IsRepresentableIn2D(flatSelf));
    D0(flatSelf);

    auto deep = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{5, LengthType::Fixed}, OpType::TranslateZ);
    auto* deepSelf = static_cast<WebCore::TranslateTransformOperation*>(deep.m_ptr);
    Require(!IsRepresentableIn2D(deepSelf));
    D0(deepSelf);

    auto autoZ = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{LengthType::Auto}, OpType::Translate);
    auto* autoZSelf = static_cast<WebCore::TranslateTransformOperation*>(autoZ.m_ptr);
    Require(!IsRepresentableIn2D(autoZSelf));
    D0(autoZSelf);

    auto percentZ = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{12, LengthType::Percent}, OpType::TranslateZ);
    auto* percentZSelf = static_cast<WebCore::TranslateTransformOperation*>(percentZ.m_ptr);
    Require(!IsRepresentableIn2D(percentZSelf));
    D0(percentZSelf);
}

void TestApply() {
    auto operation = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{5, LengthType::Fixed}, OpType::Translate3D);
    auto* self = static_cast<WebCore::TranslateTransformOperation*>(operation.m_ptr);
    WebCore::TransformationMatrix matrix;
    Require(!ApplyOp(self, matrix, WebCore::FloatSize{200.0f, 100.0f}));
    Require(matrix.m_matrix[3][0] == 10.0);
    Require(matrix.m_matrix[3][1] == 20.0);
    Require(matrix.m_matrix[3][2] == 5.0);
    Require(matrix.m_matrix[0][0] == 1.0 && matrix.m_matrix[1][1] == 1.0 && matrix.m_matrix[2][2] == 1.0 && matrix.m_matrix[3][3] == 1.0);

    auto percent = CreateFixed(Length{50, LengthType::Percent}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* percentSelf = static_cast<WebCore::TranslateTransformOperation*>(percent.m_ptr);
    WebCore::TransformationMatrix percentMatrix;
    Require(ApplyOp(percentSelf, percentMatrix, WebCore::FloatSize{200.0f, 100.0f}));
    Require(percentMatrix.m_matrix[3][0] == 100.0);
    D0(percentSelf);

    auto percentZ = CreateFixed(Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, Length{12, LengthType::Percent}, OpType::Translate);
    auto* percentZSelf = static_cast<WebCore::TranslateTransformOperation*>(percentZ.m_ptr);
    WebCore::TransformationMatrix percentZMatrix;
    Require(!ApplyOp(percentZSelf, percentZMatrix, WebCore::FloatSize{200.0f, 100.0f}));
    Require(percentZMatrix.m_matrix[3][2] == static_cast<double>(0.12f));
    D0(percentZSelf);

    auto percentY = CreateFixed(Length{0, LengthType::Fixed}, Length{50, LengthType::Percent}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* percentYSelf = static_cast<WebCore::TranslateTransformOperation*>(percentY.m_ptr);
    WebCore::TransformationMatrix percentYMatrix;
    Require(ApplyOp(percentYSelf, percentYMatrix, WebCore::FloatSize{200.0f, 100.0f}));
    Require(percentYMatrix.m_matrix[3][1] == 50.0);
    D0(percentYSelf);

    D0(self);
}

void TestCloneAndEquality() {
    auto operation = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate);
    auto* self = static_cast<WebCore::TranslateTransformOperation*>(operation.m_ptr);
    auto clone = CloneOp(self);
    Require(clone.m_ptr != nullptr && clone.m_ptr != self);
    Require(clone.m_ptr->m_refCount == 1);
    Require(self->m_refCount == 1);
    auto* cloneSelf = static_cast<WebCore::TranslateTransformOperation*>(clone.m_ptr);
    Require(EqualOp(self, *clone.m_ptr));
    Require(EqualOp(cloneSelf, *self));

    auto other = CreateFixed(Length{11, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate);
    Require(!EqualOp(self, *other.m_ptr));
    auto otherY = CreateFixed(Length{10, LengthType::Fixed}, Length{21, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate);
    Require(!EqualOp(self, *otherY.m_ptr));
    auto otherZ = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{31, LengthType::Fixed}, OpType::Translate);
    Require(!EqualOp(self, *otherZ.m_ptr));

    auto floatVariant = CreateFixed(Length{10.0f, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate);
    Require(EqualOp(self, *floatVariant.m_ptr));

    auto undefined = CreateFixed(Length{LengthType::Undefined}, Length{LengthType::Undefined}, Length{LengthType::Undefined}, OpType::Translate);
    auto undefinedClone = CloneOp(static_cast<WebCore::TranslateTransformOperation*>(undefined.m_ptr));
    Require(EqualOp(static_cast<WebCore::TranslateTransformOperation*>(undefined.m_ptr), *undefinedClone.m_ptr));

    auto quirk = CreateFixed(Length{10, LengthType::Fixed, true}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate);
    Require(!EqualOp(self, *quirk.m_ptr));

    auto translateX = CreateXY(Length{10, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::TranslateX);
    Require(!EqualOp(self, *translateX.m_ptr));

    auto* otherSelf = static_cast<WebCore::TranslateTransformOperation*>(other.m_ptr);
    otherSelf->m_type = OpType::Scale;
    Require(!EqualOp(self, *other.m_ptr));
    otherSelf->m_type = OpType::Translate;

    Require(ThrowsNotImplemented([&] { CreateFixed(Length{LengthType::Calculated}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate); }));

    Require(ThrowsInvalid([&] {
        auto equalPointer = reinterpret_cast<EqualPointer>(EqualOp);
        equalPointer(nullptr, self);
    }));

    D0(cloneSelf);
    D0(static_cast<WebCore::TranslateTransformOperation*>(translateX.m_ptr));
    D0(static_cast<WebCore::TranslateTransformOperation*>(other.m_ptr));
    D0(static_cast<WebCore::TranslateTransformOperation*>(otherY.m_ptr));
    D0(static_cast<WebCore::TranslateTransformOperation*>(otherZ.m_ptr));
    D0(static_cast<WebCore::TranslateTransformOperation*>(floatVariant.m_ptr));
    D0(static_cast<WebCore::TranslateTransformOperation*>(undefined.m_ptr));
    D0(static_cast<WebCore::TranslateTransformOperation*>(undefinedClone.m_ptr));
    D0(static_cast<WebCore::TranslateTransformOperation*>(quirk.m_ptr));
    D0(self);
}

TextStream NewStream() {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.formattingFlags = 0;
    stream.multiLineMode = false;
    stream.indent = 0;
    return stream;
}

std::string Release(TextStream& stream) {
    String released = _ZN3WTF10TextStream7releaseEv(&stream);
    std::string result;
    if (!released.impl) return result;
    Require(released.impl->hashAndFlags & 4);
    for (unsigned i = 0; i < released.impl->length; ++i) {
        result.push_back(static_cast<char>(static_cast<const unsigned char*>(released.impl->data)[i]));
    }
    return result;
}

std::string DumpToString(const WebCore::TranslateTransformOperation* self) {
    TextStream stream = NewStream();
    DumpOp(self, stream);
    Require(stream.formattingFlags == 0);
    return Release(stream);
}

void TestDump() {
    auto operation = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* self = static_cast<WebCore::TranslateTransformOperation*>(operation.m_ptr);
    Require(DumpToString(self) == "translate(10px, 20px, 0px)");

    auto percent = CreateFixed(Length{50, LengthType::Percent}, Length{1.5f, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate3D);
    auto* percentSelf = static_cast<WebCore::TranslateTransformOperation*>(percent.m_ptr);
    Require(DumpToString(percentSelf) == "translate3d(50%, 1.50px, 0px)");

    auto quirk = CreateFixed(Length{10, LengthType::Fixed, true}, Length{20, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* quirkSelf = static_cast<WebCore::TranslateTransformOperation*>(quirk.m_ptr);
    Require(DumpToString(quirkSelf) == "translate(10px has-quirk, 20px, 0px)");

    auto intrinsic = CreateFixed(Length{7, LengthType::FillAvailable}, Length{7, LengthType::MinContent}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* intrinsicSelf = static_cast<WebCore::TranslateTransformOperation*>(intrinsic.m_ptr);
    Require(DumpToString(intrinsicSelf) == "translate(fill-available 7, min-content 7, 0px)");

    auto named = CreateFixed(Length{LengthType::Auto}, Length{LengthType::Undefined}, Length{LengthType::Content}, OpType::Translate);
    auto* namedSelf = static_cast<WebCore::TranslateTransformOperation*>(named.m_ptr);
    Require(DumpToString(namedSelf) == "translate(auto, undefined, content)");

    auto floatZ = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{2.25f, LengthType::Fixed}, OpType::Translate);
    auto* floatZSelf = static_cast<WebCore::TranslateTransformOperation*>(floatZ.m_ptr);
    Require(DumpToString(floatZSelf) == "translate(10px, 20px, 2.25px)");

    Require(ThrowsNotImplemented([&] { CreateFixed(Length{LengthType::Calculated}, Length{20, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate); }));

    auto unknownType = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* unknownTypeSelf = static_cast<WebCore::TranslateTransformOperation*>(unknownType.m_ptr);
    unknownTypeSelf->m_type = static_cast<OpType>(200);
    Require(ThrowsInvalid([&] { DumpToString(unknownTypeSelf); }));

    auto noneType = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* noneTypeSelf = static_cast<WebCore::TranslateTransformOperation*>(noneType.m_ptr);
    noneTypeSelf->m_type = OpType::None;
    Require(DumpToString(noneTypeSelf) == "none(10px, 20px, 0px)");
    noneTypeSelf->m_type = OpType::Translate;

    auto calculatedDump = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* calculatedDumpSelf = static_cast<WebCore::TranslateTransformOperation*>(calculatedDump.m_ptr);
    calculatedDumpSelf->m_x = Length{LengthType::Calculated};
    Require(ThrowsNotImplemented([&] { DumpToString(calculatedDumpSelf); }));
    calculatedDumpSelf->m_x = Length{10, LengthType::Fixed};

    unknownTypeSelf->m_type = OpType::Translate;

    auto relatives = CreateFixed(Length{3, LengthType::Relative}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* relativesSelf = static_cast<WebCore::TranslateTransformOperation*>(relatives.m_ptr);
    Require(DumpToString(relativesSelf) == "translate(relative 3, 0px, 0px)");

    auto translateX = CreateXY(Length{5, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::TranslateX);
    auto* translateXSelf = static_cast<WebCore::TranslateTransformOperation*>(translateX.m_ptr);
    Require(DumpToString(translateXSelf) == "translateX(5px, 0px, 0px)");

    auto translateY = CreateXY(Length{0, LengthType::Fixed}, Length{6.5f, LengthType::Fixed}, OpType::TranslateY);
    auto* translateYSelf = static_cast<WebCore::TranslateTransformOperation*>(translateY.m_ptr);
    Require(DumpToString(translateYSelf) == "translateY(0px, 6.50px, 0px)");

    auto translateZ = CreateFixed(Length{1, LengthType::Fixed}, Length{2, LengthType::Fixed}, Length{3, LengthType::Fixed}, OpType::TranslateZ);
    auto* translateZSelf = static_cast<WebCore::TranslateTransformOperation*>(translateZ.m_ptr);
    Require(DumpToString(translateZSelf) == "translateZ(1px, 2px, 3px)");

    D0(translateZSelf);
    D0(translateYSelf);
    D0(translateXSelf);
    D0(relativesSelf);
    D0(calculatedDumpSelf);
    D0(noneTypeSelf);
    D0(unknownTypeSelf);
    D0(floatZSelf);
    D0(namedSelf);
    D0(intrinsicSelf);
    D0(quirkSelf);
    D0(percentSelf);
    D0(self);
}

void TestRefCounting() {
    auto operation = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate);
    auto* self = static_cast<WebCore::TranslateTransformOperation*>(operation.m_ptr);
    Require(self->m_refCount == 1);
    self->ref();
    Require(self->m_refCount == 2);
    self->deref();
    Require(self->m_refCount == 1);

    auto blended = BlendOp(self, nullptr, 0.25, false);
    Require(blended.m_ptr != nullptr && blended.m_ptr != self);
    Require(blended.m_ptr->m_refCount == 1);
    Require(self->m_refCount == 1);
    D0(static_cast<WebCore::TranslateTransformOperation*>(blended.m_ptr));
    D0(self);
}

void TestBlendToIdentity() {
    auto operation = CreateFixed(Length{10, LengthType::Fixed}, Length{50, LengthType::Percent}, Length{LengthType::Auto}, OpType::Translate3D);
    auto* self = static_cast<WebCore::TranslateTransformOperation*>(operation.m_ptr);

    auto half = BlendOp(self, nullptr, 0.5, true);
    auto* halfOp = static_cast<WebCore::TranslateTransformOperation*>(half.m_ptr);
    Require(halfOp->m_type == OpType::Translate3D);
    Require(SameFloatLength(halfOp->m_x, 5.0f, LengthType::Fixed));
    Require(SameFloatLength(halfOp->m_y, 25.0f, LengthType::Percent));
    Require(SameLength(halfOp->m_z, 0, LengthType::Fixed));
    D0(halfOp);

    auto from = CreateFixed(Length{100, LengthType::Fixed}, Length{100, LengthType::Fixed}, Length{100, LengthType::Fixed}, OpType::Translate);
    auto ignored = BlendOp(self, from.m_ptr, 0.5, true);
    auto* ignoredOp = static_cast<WebCore::TranslateTransformOperation*>(ignored.m_ptr);
    Require(SameFloatLength(ignoredOp->m_x, 5.0f, LengthType::Fixed));
    Require(SameFloatLength(ignoredOp->m_y, 25.0f, LengthType::Percent));
    D0(ignoredOp);
    D0(static_cast<WebCore::TranslateTransformOperation*>(from.m_ptr));

    auto zero = BlendOp(self, nullptr, 0.0, true);
    auto* zeroOp = static_cast<WebCore::TranslateTransformOperation*>(zero.m_ptr);
    Require(SameLength(zeroOp->m_x, 10, LengthType::Fixed));
    Require(SameLength(zeroOp->m_y, 50, LengthType::Percent));
    Require(SameLength(zeroOp->m_z, 0, LengthType::Auto));
    D0(zeroOp);

    auto one = BlendOp(self, nullptr, 1.0, true);
    auto* oneOp = static_cast<WebCore::TranslateTransformOperation*>(one.m_ptr);
    Require(SameLength(oneOp->m_x, 0, LengthType::Fixed));
    Require(SameLength(oneOp->m_y, 0, LengthType::Percent));
    Require(SameLength(oneOp->m_z, 0, LengthType::Fixed));
    D0(oneOp);

    auto quarterAuto = BlendOp(self, nullptr, 0.25, true);
    Require(SameLength(static_cast<WebCore::TranslateTransformOperation*>(quarterAuto.m_ptr)->m_z, 0, LengthType::Auto));
    auto threeQuarterAuto = BlendOp(self, nullptr, 0.75, true);
    Require(SameLength(static_cast<WebCore::TranslateTransformOperation*>(threeQuarterAuto.m_ptr)->m_z, 0, LengthType::Fixed));
    D0(static_cast<WebCore::TranslateTransformOperation*>(quarterAuto.m_ptr));
    D0(static_cast<WebCore::TranslateTransformOperation*>(threeQuarterAuto.m_ptr));

    Require(ThrowsNotImplemented([&] { CreateFixed(Length{LengthType::Calculated}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate); }));
    D0(self);
}

void TestBlendFromOperation() {
    auto to = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate);
    auto* toSelf = static_cast<WebCore::TranslateTransformOperation*>(to.m_ptr);
    auto from = CreateFixed(Length{4, LengthType::Fixed}, Length{8, LengthType::Fixed}, Length{12, LengthType::Fixed}, OpType::Translate);
    auto* fromSelf = static_cast<WebCore::TranslateTransformOperation*>(from.m_ptr);

    auto half = BlendOp(toSelf, from.m_ptr, 0.5, false);
    auto* halfOp = static_cast<WebCore::TranslateTransformOperation*>(half.m_ptr);
    Require(halfOp->m_type == OpType::Translate);
    Require(SameFloatLength(halfOp->m_x, 7.0f, LengthType::Fixed));
    Require(SameFloatLength(halfOp->m_y, 14.0f, LengthType::Fixed));
    Require(SameFloatLength(halfOp->m_z, 21.0f, LengthType::Fixed));
    Require(toSelf->m_refCount == 1);
    D0(halfOp);

    auto zero = BlendOp(toSelf, from.m_ptr, 0.0, false);
    Require(SameLength(static_cast<WebCore::TranslateTransformOperation*>(zero.m_ptr)->m_x, 4, LengthType::Fixed));
    D0(static_cast<WebCore::TranslateTransformOperation*>(zero.m_ptr));

    auto one = BlendOp(toSelf, from.m_ptr, 1.0, false);
    Require(SameLength(static_cast<WebCore::TranslateTransformOperation*>(one.m_ptr)->m_x, 10, LengthType::Fixed));
    D0(static_cast<WebCore::TranslateTransformOperation*>(one.m_ptr));

    auto fromNull = BlendOp(toSelf, nullptr, 0.25, false);
    auto* fromNullOp = static_cast<WebCore::TranslateTransformOperation*>(fromNull.m_ptr);
    Require(fromNullOp->m_type == OpType::Translate);
    Require(SameFloatLength(fromNullOp->m_x, 2.5f, LengthType::Fixed));
    Require(SameFloatLength(fromNullOp->m_y, 5.0f, LengthType::Fixed));
    Require(SameFloatLength(fromNullOp->m_z, 7.5f, LengthType::Fixed));
    D0(fromNullOp);

    auto to3dNull = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate3D);
    auto* to3dNullSelf = static_cast<WebCore::TranslateTransformOperation*>(to3dNull.m_ptr);
    auto fromNull3d = BlendOp(to3dNullSelf, nullptr, 0.5, false);
    auto* fromNull3dOp = static_cast<WebCore::TranslateTransformOperation*>(fromNull3d.m_ptr);
    Require(fromNull3dOp->m_type == OpType::Translate3D);
    Require(SameFloatLength(fromNull3dOp->m_z, 15.0f, LengthType::Fixed));
    D0(fromNull3dOp);
    D0(to3dNullSelf);

    auto percentTimesTo = CreateFixed(Length{50, LengthType::Percent}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* percentTimesToSelf = static_cast<WebCore::TranslateTransformOperation*>(percentTimesTo.m_ptr);
    auto percentTimesFrom = CreateFixed(Length{25, LengthType::Percent}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* percentTimesFromSelf = static_cast<WebCore::TranslateTransformOperation*>(percentTimesFrom.m_ptr);
    auto percentTimesBlend = BlendOp(percentTimesToSelf, percentTimesFrom.m_ptr, 0.5, false);
    auto* percentTimesBlendOp = static_cast<WebCore::TranslateTransformOperation*>(percentTimesBlend.m_ptr);
    Require(SameFloatLength(percentTimesBlendOp->m_x, 37.5f, LengthType::Percent));
    D0(percentTimesBlendOp);
    D0(percentTimesFromSelf);
    D0(percentTimesToSelf);

    auto translateX = CreateXY(Length{10, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::TranslateX);
    auto* translateXSelf = static_cast<WebCore::TranslateTransformOperation*>(translateX.m_ptr);
    auto translateY = CreateXY(Length{20, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::TranslateY);
    auto shared = BlendOp(translateXSelf, translateY.m_ptr, 0.5, false);
    auto* sharedOp = static_cast<WebCore::TranslateTransformOperation*>(shared.m_ptr);
    Require(sharedOp->m_type == OpType::Translate);
    Require(SameFloatLength(sharedOp->m_x, 15.0f, LengthType::Fixed));
    D0(sharedOp);

    auto translate3d = CreateFixed(Length{30, LengthType::Fixed}, Length{0, LengthType::Fixed}, Length{4, LengthType::Fixed}, OpType::Translate3D);
    auto* translate3dSelf = static_cast<WebCore::TranslateTransformOperation*>(translate3d.m_ptr);
    auto to3d = BlendOp(translate3dSelf, translateXSelf, 0.5, false);
    auto* to3dOp = static_cast<WebCore::TranslateTransformOperation*>(to3d.m_ptr);
    Require(to3dOp->m_type == OpType::Translate3D);
    Require(SameFloatLength(to3dOp->m_x, 20.0f, LengthType::Fixed));
    D0(to3dOp);
    D0(translate3dSelf);

    auto flat = CreateFixed(Length{10, LengthType::Fixed}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* flatSelf = static_cast<WebCore::TranslateTransformOperation*>(flat.m_ptr);
    auto deepFrom = CreateFixed(Length{20, LengthType::Fixed}, Length{0, LengthType::Fixed}, Length{7, LengthType::Fixed}, OpType::TranslateZ);
    auto promoted = BlendOp(flatSelf, deepFrom.m_ptr, 0.5, false);
    auto* promotedOp = static_cast<WebCore::TranslateTransformOperation*>(promoted.m_ptr);
    Require(promotedOp->m_type == OpType::Translate3D);
    Require(SameFloatLength(promotedOp->m_x, 15.0f, LengthType::Fixed));
    Require(SameFloatLength(promotedOp->m_z, 3.5f, LengthType::Fixed));
    D0(promotedOp);
    D0(static_cast<WebCore::TranslateTransformOperation*>(deepFrom.m_ptr));
    D0(flatSelf);

    fromSelf->m_type = OpType::Scale;
    auto foreign = BlendOp(toSelf, from.m_ptr, 0.5, false);
    Require(foreign.m_ptr == toSelf);
    Require(toSelf->m_refCount == 2);
    toSelf->deref();
    Require(toSelf->m_refCount == 1);
    fromSelf->m_type = OpType::Translate;

    auto percentFrom = CreateFixed(Length{50, LengthType::Percent}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate);
    auto* percentFromSelf = static_cast<WebCore::TranslateTransformOperation*>(percentFrom.m_ptr);
    Require(ThrowsNotImplemented([&] { BlendOp(toSelf, percentFrom.m_ptr, 0.5, false); }));

    auto zeroProgress = BlendOp(toSelf, percentFrom.m_ptr, 0.0, false);
    Require(SameLength(static_cast<WebCore::TranslateTransformOperation*>(zeroProgress.m_ptr)->m_x, 50, LengthType::Percent));
    D0(static_cast<WebCore::TranslateTransformOperation*>(zeroProgress.m_ptr));

    auto percentTo = CreateFixed(Length{50, LengthType::Percent}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* percentToSelf = static_cast<WebCore::TranslateTransformOperation*>(percentTo.m_ptr);
    auto zeroFrom = CreateFixed(Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* zeroFromSelf = static_cast<WebCore::TranslateTransformOperation*>(zeroFrom.m_ptr);
    auto percentBlend = BlendOp(percentToSelf, zeroFrom.m_ptr, 0.5, false);
    auto* percentBlendOp = static_cast<WebCore::TranslateTransformOperation*>(percentBlend.m_ptr);
    Require(SameFloatLength(percentBlendOp->m_x, 25.0f, LengthType::Percent));
    D0(percentBlendOp);

    auto zeroTo = CreateFixed(Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* zeroToSelf = static_cast<WebCore::TranslateTransformOperation*>(zeroTo.m_ptr);
    auto fromZeroBlend = BlendOp(zeroToSelf, percentFrom.m_ptr, 0.5, false);
    auto* fromZeroBlendOp = static_cast<WebCore::TranslateTransformOperation*>(fromZeroBlend.m_ptr);
    Require(SameFloatLength(fromZeroBlendOp->m_x, 25.0f, LengthType::Percent));
    D0(fromZeroBlendOp);

    D0(percentToSelf);
    D0(zeroFromSelf);
    D0(zeroToSelf);
    D0(percentFromSelf);
    D0(translateXSelf);
    D0(static_cast<WebCore::TranslateTransformOperation*>(translateY.m_ptr));
    D0(fromSelf);
    D0(toSelf);
}

void TestSelfOrCopy() {
    using SelfOrCopy = WebCore::TransformOperationRef (APS5_VABI*)(WebCore::TranslateTransformOperation*, const WebCore::FloatSize&);
    const WebCore::FloatSize borderBox{200.0f, 100.0f};

    auto operation = CreateFixed(Length{10, LengthType::Fixed}, Length{25, LengthType::Percent}, Length{5, LengthType::Fixed}, OpType::Translate3D);
    auto* self = static_cast<WebCore::TranslateTransformOperation*>(operation.m_ptr);
    auto selfOrCopy = reinterpret_cast<SelfOrCopy>(const_cast<void*>(static_cast<const void* const*>(self->m_vptr)[3]));
    auto same = selfOrCopy(self, borderBox);
    Require(same.m_ptr == self);
    Require(self->m_refCount == 2);
    self->deref();
    Require(self->m_refCount == 1);

    auto zCalculated = CreateFixed(Length{10, LengthType::Fixed}, Length{25, LengthType::Percent}, Length{5, LengthType::Fixed}, OpType::Translate3D);
    auto* zSelf = static_cast<WebCore::TranslateTransformOperation*>(zCalculated.m_ptr);
    zSelf->m_z = Length{LengthType::Calculated};
    Require(ThrowsNotImplemented([&] { selfOrCopy(zSelf, borderBox); }));
    zSelf->m_z = Length{5, LengthType::Fixed};
    D0(zSelf);
    D0(self);
}

void TestCalculatedMembers() {
    auto operation = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate);
    auto* self = static_cast<WebCore::TranslateTransformOperation*>(operation.m_ptr);

    self->m_x = Length{LengthType::Calculated};
    Require(ThrowsNotImplemented([&] { GetX(self); }));
    Require(ThrowsNotImplemented([&] { CloneOp(self); }));
    self->m_x = Length{10, LengthType::Fixed};

    self->m_y = Length{LengthType::Calculated};
    Require(ThrowsNotImplemented([&] { GetY(self); }));
    self->m_y = Length{20, LengthType::Fixed};

    self->m_z = Length{LengthType::Calculated};
    Require(ThrowsNotImplemented([&] { GetZ(self); }));
    Require(ThrowsNotImplemented([&] { D1(self); }));
    self->m_z = Length{30, LengthType::Fixed};

    self->m_x = Length{LengthType::Calculated};
    Require(ThrowsNotImplemented([&] { EqualOp(self, *self); }));
    self->m_x = Length{10, LengthType::Fixed};

    D0(self);
}

void TestNullThis() {
    const WebCore::FloatSize borderBox{200.0f, 100.0f};
    WebCore::TransformationMatrix matrix;
    TextStream stream = NewStream();

    Require(ThrowsInvalid([&] { GetX(nullptr); }));
    Require(ThrowsInvalid([&] { GetY(nullptr); }));
    Require(ThrowsInvalid([&] { GetZ(nullptr); }));
    Require(ThrowsInvalid([&] { GetXSize(nullptr, borderBox); }));
    Require(ThrowsInvalid([&] { GetYSize(nullptr, borderBox); }));
    Require(ThrowsInvalid([&] { GetZSize(nullptr, borderBox); }));
    Require(ThrowsInvalid([&] { IsIdentity(nullptr); }));
    Require(ThrowsInvalid([&] { IsRepresentableIn2D(nullptr); }));
    Require(ThrowsInvalid([&] { ApplyOp(nullptr, matrix, borderBox); }));
    Require(ThrowsInvalid([&] { DumpOp(nullptr, stream); }));
    Require(ThrowsInvalid([&] { CloneOp(nullptr); }));
    Require(ThrowsInvalid([&] { BlendOp(nullptr, nullptr, 0.5, false); }));

    auto operation = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{30, LengthType::Fixed}, OpType::Translate);
    auto* self = static_cast<WebCore::TranslateTransformOperation*>(operation.m_ptr);
    D0(self);
}

void TestVtableSlots() {
    auto operation = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{0, LengthType::Fixed}, OpType::Translate);
    auto* self = static_cast<WebCore::TranslateTransformOperation*>(operation.m_ptr);
    using PrimitiveType = OpType (APS5_VABI*)(const WebCore::TranslateTransformOperation*);
    auto vtable = static_cast<const void* const*>(self->m_vptr);

    auto primitiveType = reinterpret_cast<PrimitiveType>(const_cast<void*>(vtable[8]));
    Require(primitiveType(self) == OpType::Translate);
    using IsAffected = bool (APS5_VABI*)(const WebCore::TranslateTransformOperation*);
    auto isAffected = reinterpret_cast<IsAffected>(const_cast<void*>(vtable[9]));
    Require(!isAffected(self));

    WebCore::TransformationMatrix matrix;
    using ApplySlot = bool (APS5_VABI*)(const WebCore::TranslateTransformOperation*, WebCore::TransformationMatrix&, const WebCore::FloatSize&);
    auto applySlot = reinterpret_cast<ApplySlot>(const_cast<void*>(vtable[6]));
    Require(!applySlot(self, matrix, WebCore::FloatSize{200.0f, 100.0f}));
    Require(matrix.m_matrix[3][0] == 10.0 && matrix.m_matrix[3][1] == 20.0);

    using CloneSlot = WebCore::TransformOperationRef (APS5_VABI*)(const WebCore::TranslateTransformOperation*);
    auto cloneSlot = reinterpret_cast<CloneSlot>(const_cast<void*>(vtable[2]));
    auto clone = cloneSlot(self);
    Require(clone.m_ptr != nullptr && clone.m_ptr != self);

    using DumpSlot = void (APS5_VABI*)(const WebCore::TranslateTransformOperation*, TextStream&);
    auto dumpSlot = reinterpret_cast<DumpSlot>(const_cast<void*>(vtable[11]));
    TextStream stream = NewStream();
    dumpSlot(self, stream);
    Require(Release(stream) == "translate(10px, 20px, 0px)");

    using BlendSlot = WebCore::TransformOperationRef (APS5_VABI*)(WebCore::TranslateTransformOperation*, const WebCore::TransformOperation*, double, bool);
    auto blendSlot = reinterpret_cast<BlendSlot>(const_cast<void*>(vtable[7]));
    auto blended = blendSlot(self, nullptr, 0.25, false);
    Require(SameFloatLength(static_cast<WebCore::TranslateTransformOperation*>(blended.m_ptr)->m_x, 2.5f, LengthType::Fixed));
    D0(static_cast<WebCore::TranslateTransformOperation*>(blended.m_ptr));

    using D0Slot = void (APS5_VABI*)(WebCore::TranslateTransformOperation*);
    auto deletingSlot = reinterpret_cast<D0Slot>(const_cast<void*>(vtable[1]));
    deletingSlot(static_cast<WebCore::TranslateTransformOperation*>(clone.m_ptr));
    using D1Slot = void (APS5_VABI*)(WebCore::TranslateTransformOperation*);
    auto completeSlot = reinterpret_cast<D1Slot>(const_cast<void*>(vtable[0]));
    completeSlot(self);
    _ZN3WTF8fastFreeEPv(self);

    auto deep = CreateFixed(Length{10, LengthType::Fixed}, Length{20, LengthType::Fixed}, Length{5, LengthType::Fixed}, OpType::Translate3D);
    auto* deepSelf = static_cast<WebCore::TranslateTransformOperation*>(deep.m_ptr);
    auto deepVtable = static_cast<const void* const*>(deepSelf->m_vptr);
    Require(reinterpret_cast<PrimitiveType>(const_cast<void*>(deepVtable[8]))(deepSelf) == OpType::Translate3D);
    D0(deepSelf);
}

}  // namespace

int main() {
    TestConstructors();
    TestCreate();
    TestFloatGetters();
    TestIdentityAnd2D();
    TestApply();
    TestCloneAndEquality();
    TestDump();
    TestRefCounting();
    TestBlendToIdentity();
    TestBlendFromOperation();
    TestSelfOrCopy();
    TestCalculatedMembers();
    TestNullThis();
    TestVtableSlots();
    return 0;
}
