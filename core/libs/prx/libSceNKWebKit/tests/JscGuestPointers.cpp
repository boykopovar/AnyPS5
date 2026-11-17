#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using NKWebKit::InspectorAbi::ScriptCallFrame;
using NKWebKit::InspectorAbi::ScriptCallStack;
using NKWebKit::WtfAbi::DoubleConversionStringBuilder;
using NKWebKit::WtfAbi::DoubleToStringConverter;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

static unsigned char* MapPages(std::size_t count) {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096 * count, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096 * count, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(page);
}

static void Protect(void* page, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, readable ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
    Require(mprotect(page, 4096, readable ? PROT_READ : PROT_NONE) == 0);
#endif
}

static void UnmapPages(void* page, std::size_t count) {
#ifdef _WIN32
    static_cast<void>(count);
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096 * count) == 0);
#endif
}

static bool ToExponential(const DoubleToStringConverter* converter, double value, int digits, DoubleConversionStringBuilder* builder) {
    return _ZNK3WTF17double_conversion23DoubleToStringConverter13ToExponentialEdiPNS0_13StringBuilderE(converter, value, digits, builder);
}

static void TestDoubleToStringConverter() {
    const char* name = "_ZNK3WTF17double_conversion23DoubleToStringConverter13ToExponentialEdiPNS0_13StringBuilderE";
    const auto* ecma = _ZN3WTF17double_conversion23DoubleToStringConverter19EcmaScriptConverterEv();
    auto* unreadableConverter = reinterpret_cast<const DoubleToStringConverter*>(0x10);
    auto* unreadableBuilder = reinterpret_cast<DoubleConversionStringBuilder*>(0x10);

    unsigned char* pages = MapPages(3);
    std::memset(pages, 0xcc, 4096 * 2);
    Protect(pages + 4096 * 2, false);
    char* text = reinterpret_cast<char*>(pages + 1024);
    auto* builder = reinterpret_cast<DoubleConversionStringBuilder*>(pages);
    *builder = DoubleConversionStringBuilder{{text, 64}, 2};
    Require(ToExponential(ecma, 123.456, 2, builder));
    Require(builder->position == 9 && std::memcmp(text + 2, "1.23e+2", 7) == 0 && static_cast<unsigned char>(text[9]) == 0xcc);
    Require(static_cast<unsigned char>(text[0]) == 0xcc && static_cast<unsigned char>(text[1]) == 0xcc);

    Require(FaultsAsNotImplemented([&] { return ToExponential(unreadableConverter, 1.0, 2, builder); }, name));
    Require(!ToExponential(unreadableConverter, 1.0, 200, builder));
    Require(FaultsAsNotImplemented([&] { return ToExponential(ecma, 1.0, 2, unreadableBuilder); }, name));
    Require(FaultsAsNotImplemented([&] { return ToExponential(ecma, -std::numeric_limits<double>::infinity(), 2, unreadableBuilder); }, name));

    auto* badBuffer = reinterpret_cast<DoubleConversionStringBuilder*>(pages + 64);
    *badBuffer = DoubleConversionStringBuilder{{reinterpret_cast<char*>(0x10), 64}, 0};
    Require(FaultsAsNotImplemented([&] { return ToExponential(ecma, 1.0, 2, badBuffer); }, name));
    Require(badBuffer->position == 0);

    char* symbols = reinterpret_cast<char*>(pages + 4096);
    std::memcpy(symbols + 4096 - 4, "Inf", 4);
    std::memset(symbols + 2048, 'x', 2048 - 4);
    DoubleToStringConverter converter = *ecma;
    converter.infinitySymbol = symbols + 4096 - 4;
    converter.nanSymbol = nullptr;
    *builder = DoubleConversionStringBuilder{{text, 64}, 0};
    Require(ToExponential(&converter, -std::numeric_limits<double>::infinity(), 2, builder));
    Require(builder->position == 4 && std::memcmp(text, "-Inf", 4) == 0);
    Require(!ToExponential(&converter, std::numeric_limits<double>::quiet_NaN(), 2, builder) && builder->position == 4);
    converter.infinitySymbol = reinterpret_cast<const char*>(0x10);
    Require(FaultsAsNotImplemented([&] { return ToExponential(&converter, std::numeric_limits<double>::infinity(), 2, builder); }, name));
    converter.infinitySymbol = symbols + 2048;
    std::memset(symbols + 4096 - 4, 'x', 4);
    bool overflowed = false;
    try {
        ToExponential(&converter, std::numeric_limits<double>::infinity(), 2, builder);
    } catch (const std::out_of_range&) {
        overflowed = true;
    }
    Require(overflowed && builder->position == 4);

    char* writableText = reinterpret_cast<char*>(pages + 4096);
    std::memset(writableText, 0xcc, 64);
    auto* readOnlyBuilder = reinterpret_cast<DoubleConversionStringBuilder*>(pages + 512);
    *readOnlyBuilder = DoubleConversionStringBuilder{{writableText, 64}, 0};
    Protect(pages, true);
    Require(FaultsAsNotImplemented([&] { return ToExponential(ecma, 1.0, 2, readOnlyBuilder); }, name));
    Require(readOnlyBuilder->position == 0 && static_cast<unsigned char>(writableText[0]) == 0xcc);
    UnmapPages(pages, 3);
}

static String MakeString(StringImpl& impl) {
    impl.refCount += 2;
    return String(&impl);
}

static void TestScriptCallFrame() {
    const char* name1 = "_ZN9Inspector15ScriptCallFrameC1ERKS0_";
    const char* name2 = "_ZN9Inspector15ScriptCallFrameC2ERKS0_";
    static const char characters[] = "frame";
    StringImpl impl{2, 5, characters, 4};
    ScriptCallFrame source;
    source.functionName = MakeString(impl);
    source.scriptName = MakeString(impl);
    source.preRedirectURL = MakeString(impl);
    source.sourceID = 7;
    source.lineNumber = 8;
    source.column = 9;
    unsigned refCount = impl.refCount;

    auto* unreadable = reinterpret_cast<ScriptCallFrame*>(0x10);
    unsigned char* pages = MapPages(2);
    std::memset(pages, 0xcc, 4096 * 2);
    auto* readOnly = reinterpret_cast<ScriptCallFrame*>(pages + 4096);
    Protect(pages + 4096, true);

    ScriptCallFrame* writable = reinterpret_cast<ScriptCallFrame*>(pages);
    Require(FaultsAsNotImplemented([&] { _ZN9Inspector15ScriptCallFrameC1ERKS0_(writable, unreadable); return 0; }, name1));
    Require(FaultsAsNotImplemented([&] { _ZN9Inspector15ScriptCallFrameC2ERKS0_(readOnly, &source); return 0; }, name2));
    Require(impl.refCount == refCount && pages[0] == 0xcc);

    _ZN9Inspector15ScriptCallFrameC1ERKS0_(writable, &source);
    Require(writable->functionName.impl == &impl && writable->scriptName.impl == &impl && writable->preRedirectURL.impl == &impl);
    Require(writable->sourceID == 7 && writable->lineNumber == 8 && writable->column == 9 && impl.refCount == refCount + 6);
    for (std::size_t index = offsetof(ScriptCallFrame, column) + sizeof(unsigned); index < 4096; ++index)
        Require(pages[index] == 0xcc);
    _ZN9Inspector15ScriptCallFrameD1Ev(writable);
    Require(impl.refCount == refCount);

    UnmapPages(pages, 2);
}

static void TestScriptCallStack() {
    const char* name = "_ZNK9Inspector15ScriptCallStack7isEqualEPS0_";
    auto* unreadable = reinterpret_cast<ScriptCallStack*>(0x10);
    unsigned char* pages = MapPages(4);
    std::memset(pages, 0, 4096 * 4);
    auto* first = reinterpret_cast<ScriptCallStack*>(pages);
    auto* second = reinterpret_cast<ScriptCallStack*>(pages + 64);
    _ZN9Inspector15ScriptCallStackC1Ev(first);
    _ZN9Inspector15ScriptCallStackC1Ev(second);
    auto* firstFrames = reinterpret_cast<ScriptCallFrame*>(pages + 4096);
    auto* secondFrames = reinterpret_cast<ScriptCallFrame*>(pages + 4096 * 2);
    for (unsigned index = 0; index < 4; ++index) {
        firstFrames[index].lineNumber = index;
        secondFrames[index].lineNumber = index;
    }
    first->frames = {firstFrames, 4, 4};
    second->frames = {secondFrames, 4, 4};
    Require(_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second));
    secondFrames[3].column = 1;
    Require(!_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second));
    secondFrames[3].column = 0;
    Require(!_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, nullptr));

    Require(FaultsAsNotImplemented([&] { return _ZNK9Inspector15ScriptCallStack7isEqualEPS0_(unreadable, second); }, name));
    Require(FaultsAsNotImplemented([&] { return _ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, unreadable); }, name));
    second->frames.buffer = reinterpret_cast<ScriptCallFrame*>(0x10);
    Require(FaultsAsNotImplemented([&] { return _ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second); }, name));

    constexpr unsigned oversized = std::numeric_limits<unsigned>::max();
    Protect(pages + 4096 * 3, false);
    first->frames = {firstFrames, oversized, oversized};
    second->frames = {secondFrames, oversized, oversized};
    for (unsigned index = 0; index < 4; ++index)
        firstFrames[index].lineNumber = secondFrames[index].lineNumber = 0;
    Require(FaultsAsNotImplemented([&] { return _ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second); }, name));
    secondFrames[0].column = 1;
    Require(!_ZNK9Inspector15ScriptCallStack7isEqualEPS0_(first, second));

    UnmapPages(pages, 4);
}

static void TestJscContext() {
    const char* scriptName = "JSScriptCreateReferencingImmortalASCIIText";
    const char* callbackName = "JSGlobalContextSetUnhandledRejectionCallback";
    auto group = JSContextGroupCreate();
    auto ctx = JSGlobalContextCreateInGroup(group, nullptr);
    Require(group != nullptr && ctx != nullptr);

    unsigned char* pages = MapPages(2);
    std::memset(pages, 0, 4096);
    Protect(pages + 4096, false);
    char* tail = reinterpret_cast<char*>(pages + 4096 - 3);
    std::memcpy(tail, "1+1", 3);
    auto script = JSScriptCreateReferencingImmortalASCIIText(group, nullptr, 1, tail, 3, nullptr, nullptr);
    Require(script != nullptr);
    JSScriptRelease(script);
    Require(FaultsAsNotImplemented([&] { return JSScriptCreateReferencingImmortalASCIIText(group, nullptr, 1, tail, 4, nullptr, nullptr); }, scriptName));
    Require(FaultsAsNotImplemented([&] {
        return JSScriptCreateReferencingImmortalASCIIText(group, nullptr, 1, reinterpret_cast<const char*>(pages), std::numeric_limits<std::size_t>::max() / 2, nullptr, nullptr);
    }, scriptName));
    Require(FaultsAsNotImplemented([&] { return JSScriptCreateReferencingImmortalASCIIText(group, nullptr, 1, reinterpret_cast<const char*>(0x10), 4, nullptr, nullptr); }, scriptName));
    tail[1] = static_cast<char>(0x80);
    Require(JSScriptCreateReferencingImmortalASCIIText(group, nullptr, 1, tail, 64, nullptr, nullptr) == nullptr);

    auto notCallable = JSObjectMake(ctx, nullptr, nullptr);
    auto* exception = reinterpret_cast<NKWebKit::JSValueRef*>(pages + 64);
    JSGlobalContextSetUnhandledRejectionCallback(ctx, notCallable, exception);
    Require(*exception != nullptr);
    *exception = nullptr;
    Protect(pages, true);
    Require(FaultsAsNotImplemented([&] { JSGlobalContextSetUnhandledRejectionCallback(ctx, notCallable, exception); return 0; }, callbackName));
    Require(FaultsAsNotImplemented([&] {
        JSGlobalContextSetUnhandledRejectionCallback(ctx, notCallable, reinterpret_cast<NKWebKit::JSValueRef*>(0x10));
        return 0;
    }, callbackName));
    Require(*exception == nullptr);

    UnmapPages(pages, 2);
    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
}

int main() {
    TestDoubleToStringConverter();
    TestScriptCallFrame();
    TestScriptCallStack();
    TestJscContext();
    return 0;
}
