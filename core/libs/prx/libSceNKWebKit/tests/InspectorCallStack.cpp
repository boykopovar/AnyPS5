#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <utility>

using NKWebKit::InspectorAbi::AsyncStackTrace;
using NKWebKit::InspectorAbi::AsyncStackTraceState;
using NKWebKit::InspectorAbi::ScriptCallFrame;
using NKWebKit::InspectorAbi::ScriptCallStack;
using NKWebKit::InspectorAbi::ScriptCallStackRef;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Inspector call stack check failed at line %u\n", location.line());
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

using TraceCall = void (APS5_VABI *)(AsyncStackTrace*);
using TraceCallDepth = void (APS5_VABI *)(AsyncStackTrace*, std::size_t);
using TracePredicate = bool (APS5_VABI *)(const AsyncStackTrace*);
using FrameCopy = void (APS5_VABI *)(ScriptCallFrame*, const ScriptCallFrame*);
using FrameAssign = ScriptCallFrame& (APS5_VABI *)(ScriptCallFrame*, const ScriptCallFrame*);
using FrameDestructor = void (APS5_VABI *)(ScriptCallFrame*);
using FieldString = void* (APS5_VABI *)(const void*);
using FieldMutableString = void* (APS5_VABI *)(void*);
using FieldUnsigned = std::uint32_t (APS5_VABI *)(const void*);
using FrameBool = bool (APS5_VABI *)(const ScriptCallFrame*);
using StackCreate = ScriptCallStackRef (APS5_VABI *)();
using StackConstructor = void (APS5_VABI *)(ScriptCallStack*);
using StackDestructor = void (APS5_VABI *)(ScriptCallStack*);
using StackAt = const ScriptCallFrame& (APS5_VABI *)(const ScriptCallStack*, std::size_t);
using StackFirst = const ScriptCallFrame* (APS5_VABI *)(const ScriptCallStack*);
using StackEqual = bool (APS5_VABI *)(const ScriptCallStack*, ScriptCallStack*);

static_assert(std::is_same_v<decltype(&_ZN9Inspector15AsyncStackTrace18didCancelAsyncCallEv), TraceCall>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15AsyncStackTrace20didDispatchAsyncCallEv), TraceCall>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15AsyncStackTrace21willDispatchAsyncCallEm), TraceCallDepth>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15AsyncStackTrace6removeEv), TraceCall>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15AsyncStackTrace8truncateEm), TraceCallDepth>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15AsyncStackTraceD1Ev), TraceCall>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15AsyncStackTraceD2Ev), TraceCall>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallFrameC1ERKS0_), FrameCopy>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallFrameC2ERKS0_), FrameCopy>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallFrameD1Ev), FrameDestructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallFrameD2Ev), FrameDestructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallFrameaSERKS0_), FrameAssign>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallStack6createEv), StackCreate>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallStackC1Ev), StackConstructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallStackC2Ev), StackConstructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallStackD1Ev), StackDestructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector15ScriptCallStackD2Ev), StackDestructor>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15AsyncStackTrace8isLockedEv), TracePredicate>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15AsyncStackTrace9isPendingEv), TracePredicate>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallFrame10lineNumberEv), FieldUnsigned>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallFrame12columnNumberEv), FieldUnsigned>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallFrame12functionNameEv), FieldMutableString>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallFrame8isNativeEv), FrameBool>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallFrame8sourceIDEv), FieldUnsigned>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallFrame9sourceURLEv), FieldString>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallStack23firstNonNativeCallFrameEv), StackFirst>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallStack2atEm), StackAt>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallStack4sizeEv), FieldUnsigned>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptCallStack7isEqualEPS0_), StackEqual>);

struct Text {
    const void* data;
    unsigned length;
    bool is8Bit;
};

Text Text8(const char* text) {
    return {text, static_cast<unsigned>(std::strlen(text)), true};
}

Text Text16(const char16_t* text, unsigned length) {
    return {text, length, false};
}

struct TextImpl {
    StringImpl value;
    explicit TextImpl(const Text& text) : value{2, text.length, text.data, text.is8Bit ? 4U : 0U} {}
};

String MakeString(TextImpl& text) {
    text.value.refCount += 2;
    return String(&text.value);
}

ScriptCallFrame MakeFrame(TextImpl& functionName, TextImpl& scriptName, TextImpl& preRedirectURL,
    std::uint32_t sourceID, unsigned lineNumber, unsigned column) {
    ScriptCallFrame frame;
    frame.functionName = MakeString(functionName);
    frame.scriptName = MakeString(scriptName);
    frame.preRedirectURL = MakeString(preRedirectURL);
    frame.sourceID = sourceID;
    frame.lineNumber = lineNumber;
    frame.column = column;
    return frame;
}

ScriptCallFrame EmptyFrame() {
    ScriptCallFrame frame;
    frame.sourceID = 0;
    frame.lineNumber = 0;
    frame.column = 0;
    return frame;
}

ScriptCallFrame MakeNativeFrame(TextImpl& scriptName) {
    ScriptCallFrame frame = EmptyFrame();
    frame.scriptName = MakeString(scriptName);
    return frame;
}

void ClearFrame(ScriptCallFrame& frame) {
    frame.functionName.impl = nullptr;
    frame.scriptName.impl = nullptr;
    frame.preRedirectURL.impl = nullptr;
}

void DestroyFrame(ScriptCallFrame& frame) {
    _ZN9Inspector15ScriptCallFrameD1Ev(&frame);
    ClearFrame(frame);
}

ScriptCallStack* NewStack(const ScriptCallFrame* frames, unsigned count) {
    auto* stack = static_cast<ScriptCallStack*>(_ZN3WTF10fastMallocEm(sizeof(ScriptCallStack)));
    _ZN9Inspector15ScriptCallStackC1Ev(stack);
    if (!count) return stack;
    auto* buffer = static_cast<ScriptCallFrame*>(_ZN3WTF10fastMallocEm(sizeof(ScriptCallFrame) * count));
    for (unsigned i = 0; i < count; ++i) _ZN9Inspector15ScriptCallFrameC1ERKS0_(&buffer[i], &frames[i]);
    stack->frames.buffer = buffer;
    stack->frames.capacity = count;
    stack->frames.size = count;
    return stack;
}

void ReleaseStack(ScriptCallStack* stack) {
    if (!stack) return;
    if (stack->refCount != 1) {
        --stack->refCount;
        return;
    }
    _ZN9Inspector15ScriptCallStackD1Ev(stack);
    _ZN3WTF8fastFreeEPv(stack);
}

AsyncStackTrace* MakeTrace(ScriptCallStack* callStack, AsyncStackTrace* parent, AsyncStackTraceState state, bool singleShot) {
    auto* trace = static_cast<AsyncStackTrace*>(_ZN3WTF10fastMallocEm(sizeof(AsyncStackTrace)));
    trace->refCount = 1;
    trace->callStack = callStack;
    ++callStack->refCount;
    trace->parent = parent;
    if (parent) ++parent->refCount;
    trace->childCount = 0;
    trace->state = state;
    trace->truncated = false;
    trace->singleShot = singleShot;
    if (parent) {
        ++parent->childCount;
        callStack->parentStackTrace = parent;
        ++parent->refCount;
    }
    return trace;
}

void ReleaseTrace(AsyncStackTrace* trace) {
    if (!trace) return;
    if (trace->refCount != 1) {
        --trace->refCount;
        return;
    }
    _ZN9Inspector15AsyncStackTraceD1Ev(trace);
    _ZN3WTF8fastFreeEPv(trace);
}

void TestFrameCopies() {
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script.js"));
    TextImpl preRedirect(Text8("redirect.js"));
    ScriptCallFrame source = MakeFrame(functionName, scriptName, preRedirect, 0xFFFFFFFFu, 7, 9);
    Require(functionName.value.refCount == 4);
    Require(scriptName.value.refCount == 4);
    Require(preRedirect.value.refCount == 4);

    alignas(ScriptCallFrame) unsigned char firstStorage[sizeof(ScriptCallFrame)];
    auto* first = reinterpret_cast<ScriptCallFrame*>(firstStorage);
    _ZN9Inspector15ScriptCallFrameC1ERKS0_(first, &source);
    Require(first->functionName.impl == &functionName.value);
    Require(first->scriptName.impl == &scriptName.value);
    Require(first->preRedirectURL.impl == &preRedirect.value);
    Require(first->sourceID == 0xFFFFFFFFu);
    Require(first->lineNumber == 7);
    Require(first->column == 9);
    Require(functionName.value.refCount == 6);
    Require(scriptName.value.refCount == 6);
    Require(preRedirect.value.refCount == 6);
    DestroyFrame(*first);
    Require(functionName.value.refCount == 4);

    alignas(ScriptCallFrame) unsigned char secondStorage[sizeof(ScriptCallFrame)];
    auto* second = reinterpret_cast<ScriptCallFrame*>(secondStorage);
    _ZN9Inspector15ScriptCallFrameC2ERKS0_(second, &source);
    Require(second->functionName.impl == &functionName.value);
    Require(second->lineNumber == 7);
    Require(functionName.value.refCount == 6);
    DestroyFrame(*second);
    Require(functionName.value.refCount == 4);

    ScriptCallFrame empty = EmptyFrame();
    alignas(ScriptCallFrame) unsigned char copyStorage[sizeof(ScriptCallFrame)];
    auto* copy = reinterpret_cast<ScriptCallFrame*>(copyStorage);
    _ZN9Inspector15ScriptCallFrameC1ERKS0_(copy, &empty);
    Require(copy->functionName.impl == nullptr);
    Require(copy->scriptName.impl == nullptr);
    Require(copy->preRedirectURL.impl == nullptr);
    Require(copy->sourceID == 0);
    DestroyFrame(*copy);
}

void TestFrameAssignment() {
    TextImpl firstFunction(Text8("first"));
    TextImpl firstScript(Text8("first.js"));
    TextImpl firstRedirect(Text8("first-redirect.js"));
    TextImpl secondFunction(Text8("second"));
    TextImpl secondScript(Text8("second.js"));
    TextImpl secondRedirect(Text8("second-redirect.js"));
    ScriptCallFrame source = MakeFrame(firstFunction, firstScript, firstRedirect, 11, 12, 13);
    ScriptCallFrame target = MakeFrame(secondFunction, secondScript, secondRedirect, 21, 22, 23);

    ScriptCallFrame& result = _ZN9Inspector15ScriptCallFrameaSERKS0_(&target, &source);
    Require(&result == &target);
    Require(target.functionName.impl == &firstFunction.value);
    Require(target.scriptName.impl == &firstScript.value);
    Require(target.preRedirectURL.impl == &firstRedirect.value);
    Require(target.sourceID == 11);
    Require(target.lineNumber == 12);
    Require(target.column == 13);
    Require(firstFunction.value.refCount == 6);
    Require(secondFunction.value.refCount == 2);

    _ZN9Inspector15ScriptCallFrameaSERKS0_(&target, &target);
    Require(target.functionName.impl == &firstFunction.value);
    Require(target.scriptName.impl == &firstScript.value);
    Require(target.preRedirectURL.impl == &firstRedirect.value);
    Require(target.sourceID == 11);
    Require(firstFunction.value.refCount == 6);
    Require(firstScript.value.refCount == 6);
    Require(firstRedirect.value.refCount == 6);
}

void TestFrameDestruction() {
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script"));
    TextImpl preRedirect(Text8("redirect"));
    {
        ScriptCallFrame frame = MakeFrame(functionName, scriptName, preRedirect, 0, 1, 2);
        Require(functionName.value.refCount == 4);
        _ZN9Inspector15ScriptCallFrameD1Ev(&frame);
        ClearFrame(frame);
        Require(functionName.value.refCount == 2);
        Require(scriptName.value.refCount == 2);
        Require(preRedirect.value.refCount == 2);
    }
    {
        ScriptCallFrame frame = MakeFrame(functionName, scriptName, preRedirect, 0, 1, 2);
        _ZN9Inspector15ScriptCallFrameD2Ev(&frame);
        ClearFrame(frame);
        Require(functionName.value.refCount == 2);
    }
}

void TestFrameAccessors() {
    TextImpl functionName(Text8("functionName"));
    TextImpl scriptName(Text8("sourceURL"));
    TextImpl preRedirect(Text8("preRedirectURL"));
    ScriptCallFrame frame = MakeFrame(functionName, scriptName, preRedirect, 0xFFFFFFFFu, 17, 19);
    Require(_ZNK9Inspector15ScriptCallFrame12functionNameEv(&frame) == &frame.functionName);
    Require(_ZNK9Inspector15ScriptCallFrame9sourceURLEv(&frame) == &frame.scriptName);
    Require(_ZNK9Inspector15ScriptCallFrame10lineNumberEv(&frame) == 17);
    Require(_ZNK9Inspector15ScriptCallFrame12columnNumberEv(&frame) == 19);
    Require(_ZNK9Inspector15ScriptCallFrame8sourceIDEv(&frame) == 0xFFFFFFFFu);
}

void TestIsNative() {
    static constexpr char native8[] = "[native code]";
    static constexpr char16_t native16[] = u"[native code]";
    static constexpr char nativeSpace[] = "[native code] ";
    static constexpr char nativeShort[] = "[native cod";

    TextImpl eight(Text8(native8));
    TextImpl sixteen(Text16(native16, 13));
    TextImpl space(Text8(nativeSpace));
    TextImpl shortText(Text8(nativeShort));
    TextImpl empty(Text8(""));

    ScriptCallFrame frame = EmptyFrame();
    frame.scriptName = MakeString(eight);
    Require(_ZNK9Inspector15ScriptCallFrame8isNativeEv(&frame));
    frame.scriptName = MakeString(sixteen);
    Require(_ZNK9Inspector15ScriptCallFrame8isNativeEv(&frame));
    frame.scriptName = MakeString(space);
    Require(!_ZNK9Inspector15ScriptCallFrame8isNativeEv(&frame));
    frame.scriptName = MakeString(shortText);
    Require(!_ZNK9Inspector15ScriptCallFrame8isNativeEv(&frame));
    frame.scriptName = MakeString(empty);
    Require(!_ZNK9Inspector15ScriptCallFrame8isNativeEv(&frame));
    frame.scriptName = String();
    Require(!_ZNK9Inspector15ScriptCallFrame8isNativeEv(&frame));
}

void TestStackCreate() {
    {
        auto stack = _ZN9Inspector15ScriptCallStack6createEv();
        Require(stack.ptr != nullptr);
        Require(stack.ptr->refCount == 1);
        Require(stack.ptr->frames.buffer == nullptr);
        Require(stack.ptr->frames.capacity == 0);
        Require(stack.ptr->frames.size == 0);
        Require(!stack.ptr->truncated);
        Require(stack.ptr->parentStackTrace == nullptr);
    }
}

void TestStackConstruct() {
    alignas(ScriptCallStack) unsigned char firstStorage[sizeof(ScriptCallStack)];
    alignas(ScriptCallStack) unsigned char secondStorage[sizeof(ScriptCallStack)];
    std::memset(firstStorage, 0xCD, sizeof(firstStorage));
    std::memset(secondStorage, 0xCD, sizeof(secondStorage));
    auto* first = reinterpret_cast<ScriptCallStack*>(firstStorage);
    auto* second = reinterpret_cast<ScriptCallStack*>(secondStorage);
    _ZN9Inspector15ScriptCallStackC1Ev(first);
    _ZN9Inspector15ScriptCallStackC2Ev(second);
    Require(std::memcmp(firstStorage, secondStorage, sizeof(firstStorage)) == 0);
    Require(first->refCount == 1);
    Require(first->frames.buffer == nullptr);
    Require(first->frames.capacity == 0);
    Require(first->frames.size == 0);
    Require(!first->truncated);
    Require(first->parentStackTrace == nullptr);
}

void TestStackAtAndSize() {
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script"));
    TextImpl preRedirect(Text8("redirect"));
    ScriptCallFrame frames[] = {
        MakeFrame(functionName, scriptName, preRedirect, 0, 1, 2),
        MakeFrame(functionName, scriptName, preRedirect, 0, 1, 3),
        MakeFrame(functionName, scriptName, preRedirect, 0, 1, 4),
    };
    auto* stack = NewStack(frames, 3);
    Require(_ZNK9Inspector15ScriptCallStack4sizeEv(stack) == 3);
    for (unsigned i = 0; i < 3; ++i)
        Require(&_ZNK9Inspector15ScriptCallStack2atEm(stack, i) == &stack->frames.buffer[i]);
    RequireThrows([&] { _ZNK9Inspector15ScriptCallStack2atEm(stack, 3); });
    ReleaseStack(stack);
}

void TestFirstNonNative() {
    TextImpl nativeName(Text8("[native code]"));
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script"));
    TextImpl preRedirect(Text8("redirect"));
    ScriptCallFrame nativeFrame = MakeNativeFrame(nativeName);
    ScriptCallFrame otherFrame = MakeFrame(functionName, scriptName, preRedirect, 0, 1, 2);

    auto* empty = NewStack(nullptr, 0);
    Require(_ZNK9Inspector15ScriptCallStack23firstNonNativeCallFrameEv(empty) == nullptr);
    ReleaseStack(empty);

    ScriptCallFrame twoNative[] = { nativeFrame, nativeFrame };
    auto* natives = NewStack(twoNative, 2);
    Require(_ZNK9Inspector15ScriptCallStack23firstNonNativeCallFrameEv(natives) == nullptr);
    ReleaseStack(natives);

    ScriptCallFrame mixed[] = { nativeFrame, otherFrame, otherFrame };
    auto* frames = NewStack(mixed, 3);
    Require(_ZNK9Inspector15ScriptCallStack23firstNonNativeCallFrameEv(frames) == &frames->frames.buffer[1]);
    ReleaseStack(frames);
}

void TestStackIsEqual() {
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script"));
    TextImpl preRedirect(Text8("redirect"));
    const char16_t same16[] = u"same";
    const char16_t other16[] = u"other";
    TextImpl same8(Text8("same"));
    TextImpl sameWide(Text16(same16, 4));
    TextImpl otherWide(Text16(other16, 5));

    ScriptCallFrame firstFrame = MakeFrame(functionName, scriptName, preRedirect, 1, 2, 3);
    ScriptCallFrame sameTextFrame = MakeFrame(same8, same8, same8, 1, 2, 3);
    ScriptCallFrame wideTextFrame = MakeFrame(sameWide, sameWide, sameWide, 1, 2, 3);
    ScriptCallFrame otherTextFrame = MakeFrame(otherWide, otherWide, otherWide, 1, 2, 3);
    ScriptCallFrame tallerFrames[] = {
        MakeFrame(functionName, scriptName, preRedirect, 1, 2, 3),
        MakeFrame(functionName, scriptName, preRedirect, 1, 2, 3),
    };

    auto* first = NewStack(&firstFrame, 1);
    auto* second = NewStack(&firstFrame, 1);
    Require(_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, nullptr) == false);
    Require(_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second));

    auto* taller = NewStack(tallerFrames, 2);
    Require(!_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, taller));

    second->frames.buffer[0].sourceID = 0xFFFFFFFFu;
    Require(_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second));

    second->frames.buffer[0].preRedirectURL = MakeString(otherWide);
    Require(!_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second));
    second->frames.buffer[0].preRedirectURL = MakeString(preRedirect);
    Require(_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second));

    second->frames.buffer[0].lineNumber = 7;
    Require(!_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second));
    second->frames.buffer[0].lineNumber = 2;

    second->frames.buffer[0].column = 7;
    Require(!_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second));
    second->frames.buffer[0].column = 3;
    Require(_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second));

    ScriptCallFrame otherFrame = MakeFrame(otherWide, otherWide, otherWide, 1, 2, 3);
    auto* other = NewStack(&otherFrame, 1);
    Require(!_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, other));

    auto* eightBit = NewStack(&sameTextFrame, 1);
    auto* sixteenBit = NewStack(&wideTextFrame, 1);
    Require(_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(eightBit, sixteenBit));

    ReleaseStack(first);
    ReleaseStack(second);
    ReleaseStack(taller);
    ReleaseStack(other);
    ReleaseStack(eightBit);
    ReleaseStack(sixteenBit);
}

void TestStackDestructionParentHeld() {
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script"));
    TextImpl preRedirect(Text8("redirect"));
    ScriptCallFrame frame = MakeFrame(functionName, scriptName, preRedirect, 0, 1, 2);
    auto* parentStack = NewStack(&frame, 1);
    auto* parent = MakeTrace(parentStack, nullptr, AsyncStackTraceState::Pending, true);
    auto* stack = NewStack(&frame, 1);
    stack->parentStackTrace = parent;
    ++parent->refCount;
    Require(functionName.value.refCount == 8);
    Require(parent->refCount == 2);

    ReleaseStack(stack);
    Require(parent->refCount == 1);
    Require(functionName.value.refCount == 6);

    ReleaseTrace(parent);
    ReleaseStack(parentStack);
    Require(functionName.value.refCount == 4);
}

void TestStackDestructionLastParent() {
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script"));
    TextImpl preRedirect(Text8("redirect"));
    ScriptCallFrame frame = MakeFrame(functionName, scriptName, preRedirect, 0, 1, 2);
    auto* parentStack = NewStack(&frame, 1);
    auto* parent = MakeTrace(parentStack, nullptr, AsyncStackTraceState::Pending, true);
    ReleaseStack(parentStack);
    auto* stack = NewStack(&frame, 1);
    stack->parentStackTrace = parent;
    Require(parent->refCount == 1);
    Require(functionName.value.refCount == 8);

    ReleaseStack(stack);
    Require(functionName.value.refCount == 4);
}

void TestTraceState() {
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script"));
    TextImpl preRedirect(Text8("redirect"));
    ScriptCallFrame frame = MakeFrame(functionName, scriptName, preRedirect, 0, 1, 2);
    auto* stack = NewStack(&frame, 1);

    struct Case {
        AsyncStackTraceState state;
        unsigned childCount;
        bool pending;
        bool locked;
    };
    const Case cases[] = {
        {AsyncStackTraceState::Pending, 0, true, true},
        {AsyncStackTraceState::Active, 0, false, true},
        {AsyncStackTraceState::Dispatched, 0, false, false},
        {AsyncStackTraceState::Dispatched, 1, false, false},
        {AsyncStackTraceState::Dispatched, 2, false, true},
        {AsyncStackTraceState::Canceled, 2, false, true},
    };
    for (const auto& item : cases) {
        auto* trace = MakeTrace(stack, nullptr, item.state, true);
        trace->childCount = item.childCount;
        Require(_ZNK9Inspector15AsyncStackTrace9isPendingEv(trace) == item.pending);
        Require(_ZNK9Inspector15AsyncStackTrace8isLockedEv(trace) == item.locked);
        trace->childCount = 0;
        ReleaseTrace(trace);
    }
    ReleaseStack(stack);
}

struct Fixture {
    TextImpl parentFunction;
    TextImpl parentScript;
    TextImpl parentRedirect;
    TextImpl childFunction;
    TextImpl childScript;
    TextImpl childRedirect;
    ScriptCallFrame parentFrame;
    ScriptCallFrame childFrame;
    ScriptCallStack* parentStack;
    ScriptCallStack* childStack;
    AsyncStackTrace* parent;
    AsyncStackTrace* child;

    Fixture(AsyncStackTraceState state, bool singleShot, unsigned childCount)
        : parentFunction(Text8("parentFunction")),
          parentScript(Text8("parentScript")),
          parentRedirect(Text8("parentRedirect")),
          childFunction(Text8("childFunction")),
          childScript(Text8("childScript")),
          childRedirect(Text8("childRedirect")),
          parentFrame(MakeFrame(parentFunction, parentScript, parentRedirect, 1, 2, 3)),
          childFrame(MakeFrame(childFunction, childScript, childRedirect, 4, 5, 6)) {
        parentStack = NewStack(&parentFrame, 1);
        childStack = NewStack(&childFrame, 1);
        parent = MakeTrace(parentStack, nullptr, AsyncStackTraceState::Pending, true);
        child = MakeTrace(childStack, parent, state, singleShot);
        child->childCount = childCount;
    }

    void Destroy() {
        ReleaseTrace(child);
        ReleaseTrace(parent);
        ReleaseStack(childStack);
        ReleaseStack(parentStack);
    }
};

void CheckDispatch(AsyncStackTraceState start, bool singleShot, unsigned childCount,
    AsyncStackTraceState expected, bool removed) {
    Fixture fixture(start, singleShot, childCount);
    const unsigned parentRefs = fixture.parent->refCount;
    const unsigned childStackRefs = fixture.childStack->refCount;
    _ZN9Inspector15AsyncStackTrace20didDispatchAsyncCallEv(fixture.child);
    Require(fixture.child->state == expected);
    if (removed) {
        Require(fixture.parent->childCount == 0);
        Require(fixture.child->parent == nullptr);
        Require(fixture.childStack->parentStackTrace == nullptr);
        Require(fixture.parent->refCount == parentRefs - 2);
    } else {
        Require(fixture.parent->childCount == 1);
        Require(fixture.child->parent == fixture.parent);
        Require(fixture.childStack->parentStackTrace == fixture.parent);
        Require(fixture.parent->refCount == parentRefs);
    }
    Require(fixture.childStack->refCount == childStackRefs);
    fixture.Destroy();
}

void TestDidDispatch() {
    CheckDispatch(AsyncStackTraceState::Active, false, 0, AsyncStackTraceState::Pending, false);
    CheckDispatch(AsyncStackTraceState::Active, true, 0, AsyncStackTraceState::Dispatched, true);
    CheckDispatch(AsyncStackTraceState::Active, true, 1, AsyncStackTraceState::Dispatched, false);
    CheckDispatch(AsyncStackTraceState::Canceled, true, 0, AsyncStackTraceState::Dispatched, true);
}

void CheckCancel(AsyncStackTraceState start, unsigned childCount, bool removed) {
    Fixture fixture(start, true, childCount);
    const unsigned parentRefs = fixture.parent->refCount;
    _ZN9Inspector15AsyncStackTrace18didCancelAsyncCallEv(fixture.child);
    Require(fixture.child->state == AsyncStackTraceState::Canceled);
    if (removed) {
        Require(fixture.parent->childCount == 0);
        Require(fixture.child->parent == nullptr);
        Require(fixture.childStack->parentStackTrace == nullptr);
        Require(fixture.parent->refCount == parentRefs - 2);
    } else {
        Require(fixture.parent->childCount == 1);
        Require(fixture.child->parent == fixture.parent);
        Require(fixture.childStack->parentStackTrace == fixture.parent);
        Require(fixture.parent->refCount == parentRefs);
    }
    fixture.Destroy();
}

void TestDidCancel() {
    CheckCancel(AsyncStackTraceState::Canceled, 2, false);
    CheckCancel(AsyncStackTraceState::Pending, 0, true);
    CheckCancel(AsyncStackTraceState::Pending, 1, false);
    CheckCancel(AsyncStackTraceState::Active, 0, false);
}

void TestRemoveWithoutParent() {
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script"));
    TextImpl preRedirect(Text8("redirect"));
    ScriptCallFrame frame = MakeFrame(functionName, scriptName, preRedirect, 0, 1, 2);
    auto* stack = NewStack(&frame, 1);
    auto* trace = MakeTrace(stack, nullptr, AsyncStackTraceState::Pending, true);
    const unsigned refs = trace->refCount;
    const unsigned stackRefs = stack->refCount;
    _ZN9Inspector15AsyncStackTrace6removeEv(trace);
    Require(trace->refCount == refs);
    Require(trace->childCount == 0);
    Require(trace->parent == nullptr);
    Require(stack->parentStackTrace == nullptr);
    Require(stack->refCount == stackRefs);
    ReleaseTrace(trace);
    ReleaseStack(stack);
}

void TestRemoveWithoutChildren() {
    Fixture fixture(AsyncStackTraceState::Pending, true, 0);
    fixture.parent->childCount = 0;
    const unsigned parentRefs = fixture.parent->refCount;
    const unsigned stackRefs = fixture.childStack->refCount;
    RequireThrows([&] { _ZN9Inspector15AsyncStackTrace6removeEv(fixture.child); });
    Require(fixture.child->parent == fixture.parent);
    Require(fixture.childStack->parentStackTrace == fixture.parent);
    Require(fixture.parent->refCount == parentRefs);
    Require(fixture.childStack->refCount == stackRefs);
    fixture.parent->childCount = 1;
    fixture.Destroy();
}

struct TraceNode {
    TextImpl functionName;
    TextImpl scriptName;
    TextImpl preRedirect;
    ScriptCallStack* stack;
    AsyncStackTrace* trace;

    TraceNode(const char* name, unsigned frameCount)
        : functionName(Text8(name)), scriptName(Text8("script")), preRedirect(Text8("preRedirect")), stack(nullptr), trace(nullptr) {
        auto* buffer = static_cast<ScriptCallFrame*>(_ZN3WTF10fastMallocEm(sizeof(ScriptCallFrame) * frameCount));
        for (unsigned i = 0; i < frameCount; ++i) {
            ScriptCallFrame frame = MakeFrame(functionName, scriptName, preRedirect, i, i, i);
            _ZN9Inspector15ScriptCallFrameC1ERKS0_(&buffer[i], &frame);
        }
        stack = static_cast<ScriptCallStack*>(_ZN3WTF10fastMallocEm(sizeof(ScriptCallStack)));
        _ZN9Inspector15ScriptCallStackC1Ev(stack);
        stack->frames.buffer = buffer;
        stack->frames.capacity = frameCount;
        stack->frames.size = frameCount;
    }

    void Destroy() {
        ReleaseTrace(trace);
        ReleaseStack(stack);
    }
};

void TestTruncateDepthNotReached() {
    TraceNode second("second", 1);
    TraceNode first("first", 1);
    second.trace = MakeTrace(second.stack, nullptr, AsyncStackTraceState::Pending, true);
    first.trace = MakeTrace(first.stack, second.trace, AsyncStackTraceState::Pending, true);
    _ZN9Inspector15AsyncStackTrace8truncateEm(first.trace, 5);
    Require(first.trace->parent == second.trace);
    Require(second.trace->childCount == 1);
    Require(!first.trace->truncated);
    Require(!second.trace->truncated);
    first.Destroy();
    second.Destroy();
}

void TestTruncateNewRootWithoutParent() {
    TraceNode second("second", 2);
    TraceNode first("first", 2);
    second.trace = MakeTrace(second.stack, nullptr, AsyncStackTraceState::Pending, true);
    first.trace = MakeTrace(first.stack, second.trace, AsyncStackTraceState::Pending, true);
    _ZN9Inspector15AsyncStackTrace8truncateEm(first.trace, 3);
    Require(first.trace->parent == second.trace);
    Require(second.trace->childCount == 1);
    Require(!first.trace->truncated);
    Require(!second.trace->truncated);
    first.Destroy();
    second.Destroy();
}

void TestTruncateNoLockedAncestor() {
    TraceNode third("third", 2);
    TraceNode second("second", 2);
    TraceNode first("first", 2);
    third.trace = MakeTrace(third.stack, nullptr, AsyncStackTraceState::Dispatched, true);
    second.trace = MakeTrace(second.stack, third.trace, AsyncStackTraceState::Dispatched, true);
    first.trace = MakeTrace(first.stack, second.trace, AsyncStackTraceState::Active, true);
    Require(second.trace->childCount == 1);
    _ZN9Inspector15AsyncStackTrace8truncateEm(first.trace, 3);
    Require(second.trace->refCount == 3);
    Require(third.trace->refCount == 1);
    Require(second.trace->truncated);
    Require(!first.trace->truncated);
    Require(!third.trace->truncated);
    Require(third.trace->childCount == 0);
    Require(second.trace->parent == nullptr);
    Require(second.stack->parentStackTrace == nullptr);
    Require(first.trace->parent == second.trace);
    Require(first.stack->parentStackTrace == second.trace);
    first.Destroy();
    second.Destroy();
    third.Destroy();
}

void TestTruncateLockedAncestor() {
    TraceNode third("third", 2);
    TraceNode second("second", 2);
    TraceNode first("first", 2);
    third.trace = MakeTrace(third.stack, nullptr, AsyncStackTraceState::Pending, true);
    second.trace = MakeTrace(second.stack, third.trace, AsyncStackTraceState::Pending, true);
    first.trace = MakeTrace(first.stack, second.trace, AsyncStackTraceState::Active, true);
    _ZN9Inspector15AsyncStackTrace8truncateEm(first.trace, 3);

    AsyncStackTrace* clone = first.trace->parent;
    Require(clone != nullptr);
    Require(clone != second.trace);
    Require(clone->callStack == second.stack);
    Require(second.stack->refCount == 3);
    Require(clone->childCount == 1);
    Require(clone->singleShot);
    Require(clone->state == AsyncStackTraceState::Pending);
    Require(clone->truncated);
    Require(clone->parent == nullptr);
    Require(second.trace->parent == third.trace);
    Require(second.trace->childCount == 0);
    Require(third.trace->childCount == 1);
    Require(!second.trace->truncated);
    Require(!third.trace->truncated);
    Require(!first.trace->truncated);
    Require(second.trace->refCount == 1);
    Require(third.trace->refCount == 3);

    first.Destroy();
    second.Destroy();
    third.Destroy();
    Require(second.functionName.value.refCount == 2);
}

void TestTruncateTwoClones() {
    TraceNode fourth("fourth", 1);
    TraceNode third("third", 1);
    TraceNode second("second", 1);
    TraceNode first("first", 1);
    fourth.trace = MakeTrace(fourth.stack, nullptr, AsyncStackTraceState::Dispatched, true);
    third.trace = MakeTrace(third.stack, fourth.trace, AsyncStackTraceState::Dispatched, true);
    second.trace = MakeTrace(second.stack, third.trace, AsyncStackTraceState::Pending, true);
    first.trace = MakeTrace(first.stack, second.trace, AsyncStackTraceState::Active, true);
    _ZN9Inspector15AsyncStackTrace8truncateEm(first.trace, 3);

    AsyncStackTrace* secondClone = first.trace->parent;
    Require(secondClone != nullptr);
    Require(secondClone != second.trace);
    AsyncStackTrace* thirdClone = secondClone->parent;
    Require(thirdClone != nullptr);
    Require(thirdClone != third.trace);
    Require(secondClone->callStack == second.stack);
    Require(thirdClone->callStack == third.stack);
    Require(secondClone->childCount == 1);
    Require(thirdClone->childCount == 1);
    Require(!secondClone->truncated);
    Require(thirdClone->truncated);
    Require(thirdClone->parent == nullptr);
    Require(second.trace->parent == third.trace);
    Require(second.trace->childCount == 0);
    Require(third.trace->parent == fourth.trace);
    Require(third.trace->childCount == 1);
    Require(fourth.trace->childCount == 1);
    Require(second.trace->refCount == 1);
    Require(third.trace->refCount == 3);

    first.Destroy();
    second.Destroy();
    third.Destroy();
    fourth.Destroy();
    Require(second.functionName.value.refCount == 2);
}

void TestWillDispatch() {
    TraceNode third("third", 2);
    TraceNode second("second", 2);
    TraceNode first("first", 2);
    third.trace = MakeTrace(third.stack, nullptr, AsyncStackTraceState::Dispatched, true);
    second.trace = MakeTrace(second.stack, third.trace, AsyncStackTraceState::Dispatched, true);
    first.trace = MakeTrace(first.stack, second.trace, AsyncStackTraceState::Pending, true);
    _ZN9Inspector15AsyncStackTrace21willDispatchAsyncCallEm(first.trace, 3);
    Require(first.trace->state == AsyncStackTraceState::Active);
    Require(second.trace->truncated);
    Require(third.trace->childCount == 0);
    Require(second.trace->parent == nullptr);
    Require(second.stack->parentStackTrace == nullptr);
    Require(first.trace->parent == second.trace);
    Require(second.trace->refCount == 3);
    Require(third.trace->refCount == 1);
    first.Destroy();
    second.Destroy();
    third.Destroy();
}

void TestTraceDestructionWithParent() {
    TextImpl parentFunction(Text8("parentFunction"));
    TextImpl parentScript(Text8("parentScript"));
    TextImpl parentRedirect(Text8("parentRedirect"));
    TextImpl childFunction(Text8("childFunction"));
    TextImpl childScript(Text8("childScript"));
    TextImpl childRedirect(Text8("childRedirect"));
    ScriptCallFrame parentFrame = MakeFrame(parentFunction, parentScript, parentRedirect, 1, 2, 3);
    ScriptCallFrame childFrame = MakeFrame(childFunction, childScript, childRedirect, 4, 5, 6);
    auto* parentStack = NewStack(&parentFrame, 1);
    auto* childStack = NewStack(&childFrame, 1);
    auto* parent = MakeTrace(parentStack, nullptr, AsyncStackTraceState::Pending, true);
    ReleaseStack(parentStack);
    auto* child = MakeTrace(childStack, parent, AsyncStackTraceState::Dispatched, true);
    ReleaseStack(childStack);
    Require(parent->refCount == 3);
    Require(childFunction.value.refCount == 6);
    Require(parentFunction.value.refCount == 6);

    _ZN9Inspector15AsyncStackTraceD1Ev(child);
    _ZN3WTF8fastFreeEPv(child);
    Require(parent->childCount == 0);
    Require(parent->refCount == 1);
    Require(childFunction.value.refCount == 4);

    ReleaseTrace(parent);
    Require(parentFunction.value.refCount == 4);
}

void TestTraceDestructionWithoutParent() {
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script"));
    TextImpl preRedirect(Text8("redirect"));
    ScriptCallFrame frame = MakeFrame(functionName, scriptName, preRedirect, 0, 1, 2);
    auto* stack = NewStack(&frame, 1);
    auto* trace = MakeTrace(stack, nullptr, AsyncStackTraceState::Pending, true);
    ReleaseStack(stack);
    Require(functionName.value.refCount == 6);
    _ZN9Inspector15AsyncStackTraceD2Ev(trace);
    _ZN3WTF8fastFreeEPv(trace);
    Require(functionName.value.refCount == 4);
}

void TestNulls() {
    TextImpl functionName(Text8("function"));
    TextImpl scriptName(Text8("script"));
    TextImpl preRedirect(Text8("redirect"));
    ScriptCallFrame frame = MakeFrame(functionName, scriptName, preRedirect, 0, 1, 2);

    RequireThrows([] { _ZN9Inspector15AsyncStackTrace18didCancelAsyncCallEv(nullptr); });
    RequireThrows([] { _ZN9Inspector15AsyncStackTrace20didDispatchAsyncCallEv(nullptr); });
    RequireThrows([] { _ZN9Inspector15AsyncStackTrace21willDispatchAsyncCallEm(nullptr, 1); });
    RequireThrows([] { _ZN9Inspector15AsyncStackTrace6removeEv(nullptr); });
    RequireThrows([] { _ZN9Inspector15AsyncStackTrace8truncateEm(nullptr, 1); });
    RequireThrows([] { _ZN9Inspector15AsyncStackTraceD1Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector15AsyncStackTraceD2Ev(nullptr); });
    RequireThrows([&] { _ZN9Inspector15ScriptCallFrameC1ERKS0_(nullptr, &frame); });
    RequireThrows([&] { _ZN9Inspector15ScriptCallFrameC1ERKS0_(&frame, nullptr); });
    RequireThrows([&] { _ZN9Inspector15ScriptCallFrameC2ERKS0_(nullptr, &frame); });
    RequireThrows([&] { _ZN9Inspector15ScriptCallFrameC2ERKS0_(&frame, nullptr); });
    RequireThrows([] { _ZN9Inspector15ScriptCallFrameD1Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector15ScriptCallFrameD2Ev(nullptr); });
    RequireThrows([&] { _ZN9Inspector15ScriptCallFrameaSERKS0_(nullptr, &frame); });
    RequireThrows([&] { _ZN9Inspector15ScriptCallFrameaSERKS0_(&frame, nullptr); });
    RequireThrows([] { _ZN9Inspector15ScriptCallStackC1Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector15ScriptCallStackC2Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector15ScriptCallStackD1Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector15ScriptCallStackD2Ev(nullptr); });
    RequireThrows([] { _ZNK9Inspector15AsyncStackTrace8isLockedEv(nullptr); });
    RequireThrows([] { _ZNK9Inspector15AsyncStackTrace9isPendingEv(nullptr); });
    RequireThrows([] { _ZNK9Inspector15ScriptCallFrame8isNativeEv(nullptr); });
    RequireThrows([] { _ZNK9Inspector15ScriptCallStack23firstNonNativeCallFrameEv(nullptr); });
    RequireThrows([] { _ZNK9Inspector15ScriptCallStack2atEm(nullptr, 0); });
    RequireThrows([&] { _ZNK9Inspector15ScriptCallStack7isEqualEPS0_(nullptr, nullptr); });
}

}

int main() {
    TestFrameCopies();
    TestFrameAssignment();
    TestFrameDestruction();
    TestFrameAccessors();
    TestIsNative();
    TestStackCreate();
    TestStackConstruct();
    TestStackAtAndSize();
    TestFirstNonNative();
    TestStackIsEqual();
    TestStackDestructionParentHeld();
    TestStackDestructionLastParent();
    TestTraceState();
    TestDidDispatch();
    TestDidCancel();
    TestRemoveWithoutParent();
    TestRemoveWithoutChildren();
    TestTruncateDepthNotReached();
    TestTruncateNewRootWithoutParent();
    TestTruncateNoLockedAncestor();
    TestTruncateLockedAncestor();
    TestTruncateTwoClones();
    TestWillDispatch();
    TestTraceDestructionWithParent();
    TestTraceDestructionWithoutParent();
    TestNulls();
    std::printf("Inspector call frames, call stacks and async stack traces passed\n");
}
