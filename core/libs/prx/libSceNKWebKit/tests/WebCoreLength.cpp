#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::CalculationAbi::CalculationValue;
using CalculationRef = WTF::Ref<CalculationValue>;

struct FakeNode {
    void* const* vptr;
};

unsigned g_evaluateCalls;
float g_evaluateMaxValue;
float g_evaluateResult;
unsigned g_destroyCalls;
const void* g_destroyedNode;

float APS5_VABI FakeEvaluate(const void* node, float maxValue) {
    ++g_evaluateCalls;
    g_evaluateMaxValue = maxValue;
    return g_evaluateResult;
}

void APS5_VABI FakeDestroyNode(void* node) {
    ++g_destroyCalls;
    g_destroyedNode = node;
    _ZN3WTF8fastFreeEPv(node);
}

void* const g_nodeVtable[] = {
    reinterpret_cast<void*>(&FakeDestroyNode),
    reinterpret_cast<void*>(&FakeDestroyNode),
    reinterpret_cast<void*>(&FakeEvaluate),
    nullptr,
    nullptr,
};

FakeNode* NewNode() {
    auto* node = static_cast<FakeNode*>(_ZN3WTF10fastMallocEm(sizeof(FakeNode)));
    node->vptr = g_nodeVtable;
    return node;
}

CalculationValue* NewCalculationValue(FakeNode* node, bool clamp) {
    auto* value = static_cast<CalculationValue*>(_ZN3WTF10fastMallocEm(sizeof(CalculationValue)));
    value->m_refCount = 1;
    value->m_expression = node;
    value->m_shouldClampToNonNegative = clamp;
    return value;
}

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore::Length check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

using ConstructLengthExport = void (APS5_VABI*)(WebCore::Length*, CalculationRef&&);
using RefExport = void (APS5_VABI*)(const WebCore::Length*);
using NonNanExport = float (APS5_VABI*)(const WebCore::Length*, int);

ConstructLengthExport CompleteConstructor() {
    return _ZN7WebCore6LengthC1EON3WTF3RefINS_16CalculationValueENS1_13DumbPtrTraitsIS3_EEEE;
}
ConstructLengthExport BaseConstructor() {
    return _ZN7WebCore6LengthC2EON3WTF3RefINS_16CalculationValueENS1_13DumbPtrTraitsIS3_EEEE;
}
RefExport LengthRef() {
    return _ZNK7WebCore6Length3refEv;
}
RefExport LengthDeref() {
    return _ZNK7WebCore6Length5derefEv;
}
NonNanExport NonNanCalculatedValue() {
    return _ZNK7WebCore6Length21nonNanCalculatedValueEi;
}

unsigned Construct(ConstructLengthExport constructor, WebCore::Length* length, CalculationValue* value) {
    constructor(length, WTF::adoptRef(*value));
    return length->m_calculationValueHandle;
}

void TestConstructor() {
    FakeNode* node = NewNode();
    CalculationValue* value = NewCalculationValue(node, false);
    WebCore::Length length{};
    Require(Construct(BaseConstructor(), &length, value) == 1);
    Require(length.m_type == WebCore::LengthType::Calculated);
    Require(!length.m_hasQuirk);
    Require(!length.m_isFloat);
    Require(value->m_refCount == 1);
    Require(g_destroyCalls == 0);

    WebCore::Length dirty{};
    dirty.m_floatValue = 42.0f;
    dirty.m_hasQuirk = true;
    dirty.m_isFloat = true;
    FakeNode* dirtyNode = NewNode();
    CalculationValue* dirtyValue = NewCalculationValue(dirtyNode, false);
    Construct(CompleteConstructor(), &dirty, dirtyValue);
    Require(dirty.m_type == WebCore::LengthType::Calculated);
    Require(!dirty.m_hasQuirk);
    Require(!dirty.m_isFloat);
    LengthDeref()(&dirty);
    Require(g_destroyCalls == 1);
    Require(g_destroyedNode == dirtyNode);

    WebCore::Length complete{};
    FakeNode* completeNode = NewNode();
    CalculationValue* completeValue = NewCalculationValue(completeNode, true);
    unsigned completeHandle = Construct(CompleteConstructor(), &complete, completeValue);
    Require(completeHandle == 3);
    Require(complete.m_type == WebCore::LengthType::Calculated);
    Require(!complete.m_hasQuirk);
    Require(!complete.m_isFloat);
    Require(completeValue->m_refCount == 1);

    LengthDeref()(&complete);
    Require(g_destroyCalls == 2);
    Require(g_destroyedNode == completeNode);
    LengthDeref()(&length);
    Require(g_destroyCalls == 3);
    Require(g_destroyedNode == node);
}

void TestRefDeref() {
    FakeNode* node = NewNode();
    CalculationValue* value = NewCalculationValue(node, false);
    WebCore::Length length{};
    Construct(CompleteConstructor(), &length, value);
    Require(value->m_refCount == 1);

    unsigned destroysBefore = g_destroyCalls;
    LengthRef()(&length);
    LengthRef()(&length);
    Require(value->m_refCount == 1);
    Require(g_destroyCalls == destroysBefore);

    LengthDeref()(&length);
    Require(g_destroyCalls == destroysBefore);
    LengthDeref()(&length);
    Require(g_destroyCalls == destroysBefore);
    LengthDeref()(&length);
    Require(g_destroyCalls == destroysBefore + 1);
    Require(g_destroyedNode == node);
}

void TestNonNanCalculatedValue() {
    FakeNode* node = NewNode();
    CalculationValue* value = NewCalculationValue(node, false);
    WebCore::Length length{};
    Construct(BaseConstructor(), &length, value);

    g_evaluateResult = 25.0f;
    Require(NonNanCalculatedValue()(&length, 100) == 25.0f);
    Require(g_evaluateCalls == 1);
    Require(g_evaluateMaxValue == 100.0f);

    g_evaluateResult = -5.0f;
    Require(NonNanCalculatedValue()(&length, 100) == -5.0f);

    g_evaluateResult = 1.0f;
    NonNanCalculatedValue()(&length, 0x40000001);
    Require(g_evaluateMaxValue == 1073741824.0f);

    g_evaluateResult = 3.0f;
    NonNanCalculatedValue()(&length, -8);
    Require(g_evaluateMaxValue == -8.0f);

    g_evaluateResult = std::numeric_limits<float>::quiet_NaN();
    Require(NonNanCalculatedValue()(&length, 100) == 0.0f);

    FakeNode* clampedNode = NewNode();
    CalculationValue* clamped = NewCalculationValue(clampedNode, true);
    WebCore::Length clampedLength{};
    Construct(CompleteConstructor(), &clampedLength, clamped);

    g_evaluateResult = -5.0f;
    Require(NonNanCalculatedValue()(&clampedLength, 100) == 0.0f);
    g_evaluateResult = 25.0f;
    Require(NonNanCalculatedValue()(&clampedLength, 100) == 25.0f);
    g_evaluateResult = std::numeric_limits<float>::quiet_NaN();
    Require(NonNanCalculatedValue()(&clampedLength, 100) == 0.0f);

    LengthDeref()(&clampedLength);
    LengthDeref()(&length);
}

void TestSharedHandle() {
    FakeNode* node = NewNode();
    CalculationValue* value = NewCalculationValue(node, false);
    WebCore::Length original{};
    unsigned handle = Construct(CompleteConstructor(), &original, value);

    WebCore::Length copy{original};
    Require(copy.m_calculationValueHandle == handle);
    Require(copy.m_type == WebCore::LengthType::Calculated);
    Require(!copy.m_hasQuirk);
    Require(!copy.m_isFloat);
    unsigned destroysBefore = g_destroyCalls;

    LengthRef()(&copy);
    LengthDeref()(&original);
    Require(g_destroyCalls == destroysBefore);
    g_evaluateResult = 7.0f;
    Require(NonNanCalculatedValue()(&copy, 20) == 7.0f);
    Require(g_evaluateMaxValue == 20.0f);

    LengthDeref()(&copy);
    Require(g_destroyCalls == destroysBefore + 1);
    Require(g_destroyedNode == node);
    Require(Throws([&] { NonNanCalculatedValue()(&copy, 20); }));

    FakeNode* reusedNode = NewNode();
    CalculationValue* reusedValue = NewCalculationValue(reusedNode, false);
    WebCore::Length reused{};
    Construct(CompleteConstructor(), &reused, reusedValue);
    Require(reused.m_calculationValueHandle == handle + 1);
    LengthDeref()(&reused);
}

void TestHandleWrap() {
    unsigned saved = NKWebKit::CalculationAbi::nextAvailableHandle;
    NKWebKit::CalculationAbi::nextAvailableHandle = std::numeric_limits<unsigned>::max() - 1;

    FakeNode* beforeNode = NewNode();
    CalculationValue* beforeValue = NewCalculationValue(beforeNode, false);
    WebCore::Length before{};
    Require(Construct(CompleteConstructor(), &before, beforeValue) == std::numeric_limits<unsigned>::max() - 1);

    FakeNode* wrappedNode = NewNode();
    CalculationValue* wrappedValue = NewCalculationValue(wrappedNode, false);
    WebCore::Length wrapped{};
    unsigned wrappedHandle = Construct(CompleteConstructor(), &wrapped, wrappedValue);
    Require(wrappedHandle != 0);
    Require(wrappedHandle != std::numeric_limits<unsigned>::max());
    Require(wrappedHandle == 1);
    Require(wrapped.m_type == WebCore::LengthType::Calculated);
    g_evaluateResult = 9.0f;
    Require(NonNanCalculatedValue()(&wrapped, 30) == 9.0f);

    LengthDeref()(&wrapped);
    Require(g_destroyedNode == wrappedNode);
    LengthDeref()(&before);
    Require(g_destroyedNode == beforeNode);

    FakeNode* afterNode = NewNode();
    CalculationValue* afterValue = NewCalculationValue(afterNode, false);
    WebCore::Length after{};
    Require(Construct(CompleteConstructor(), &after, afterValue) == 2);
    LengthDeref()(&after);

    NKWebKit::CalculationAbi::nextAvailableHandle = saved;
}

void TestErrors() {
    Require(Throws([] { LengthRef()(nullptr); }));
    Require(Throws([] { LengthDeref()(nullptr); }));
    Require(Throws([] { NonNanCalculatedValue()(nullptr, 1); }));
    Require(Throws([] { Construct(BaseConstructor(), nullptr, NewCalculationValue(NewNode(), false)); }));

    WebCore::Length length{};
    CalculationValue* orphaned = NewCalculationValue(NewNode(), false);
    CalculationRef owner(*orphaned, WTF::AdoptRef);
    {
        CalculationRef moved(std::move(owner));
    }
    Require(Throws([&] { CompleteConstructor()(&length, std::move(owner)); }));

    WebCore::Length plain(0, WebCore::LengthType::Fixed);
    Require(Throws([&] { LengthRef()(&plain); }));
    Require(Throws([&] { LengthDeref()(&plain); }));
    Require(Throws([&] { NonNanCalculatedValue()(&plain, 1); }));

    FakeNode* node = NewNode();
    CalculationValue* value = NewCalculationValue(node, false);
    WebCore::Length calculated{};
    unsigned handle = Construct(BaseConstructor(), &calculated, value);
    calculated.m_calculationValueHandle = 0x7FFFFFFFu;
    Require(Throws([&] { LengthRef()(&calculated); }));
    Require(Throws([&] { LengthDeref()(&calculated); }));
    Require(Throws([&] { NonNanCalculatedValue()(&calculated, 1); }));
    calculated.m_calculationValueHandle = handle;

    CalculationValue* expressionless = NewCalculationValue(nullptr, false);
    WebCore::Length expressionlessLength{};
    Construct(CompleteConstructor(), &expressionlessLength, expressionless);
    Require(Throws([&] { NonNanCalculatedValue()(&expressionlessLength, 1); }));
    LengthDeref()(&expressionlessLength);

    LengthDeref()(&calculated);
    Require(Throws([&] { LengthDeref()(&calculated); }));
    Require(Throws([&] { LengthRef()(&calculated); }));
    Require(Throws([&] { NonNanCalculatedValue()(&calculated, 1); }));

    WebCore::Length afterErase{};
    Construct(CompleteConstructor(), &afterErase, NewCalculationValue(NewNode(), false));
    Require(afterErase.m_calculationValueHandle == handle + 2);
    LengthDeref()(&afterErase);
}

}

int main() {
    TestConstructor();
    TestRefDeref();
    TestNonNanCalculatedValue();
    TestSharedHandle();
    TestHandleWrap();
    TestErrors();
    return 0;
}
