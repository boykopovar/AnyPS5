#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

constexpr unsigned MaxStringLength = 0x7FFFFFFF;

std::string captured;
bool captureOkay = true;
std::vector<const void*> printTargets;
std::vector<const char*> events;

void APS5_VABI CaptureVprintf(void* stream, const char* format, VaList* args) {
    (void)stream;
    if (std::strcmp(format, "%s") != 0) {
        captureOkay = false;
        return;
    }
    const char* text = nullptr;
    if (args->gp_offset < 48) {
        text = *reinterpret_cast<const char* const*>(static_cast<const char*>(args->reg_save_area) + args->gp_offset);
        args->gp_offset += 8;
    } else {
        text = *static_cast<const char* const*>(args->overflow_arg_area);
        args->overflow_arg_area = static_cast<char*>(args->overflow_arg_area) + 8;
    }
    if (!text) {
        captureOkay = false;
        return;
    }
    captured += text;
    printTargets.push_back(stream);
}

struct FakePrintStream {
    const void* const* vtable;
};

FakePrintStream innerStream{nullptr};
FakePrintStream outerStream{nullptr};

NKWebKit::WtfAbi::PrintStream& FakeStream() {
    return *reinterpret_cast<NKWebKit::WtfAbi::PrintStream*>(&outerStream);
}

NKWebKit::WtfAbi::PrintStream* APS5_VABI FakeBegin(NKWebKit::WtfAbi::PrintStream* stream) {
    (void)stream;
    events.push_back("begin");
    return reinterpret_cast<NKWebKit::WtfAbi::PrintStream*>(&innerStream);
}

void APS5_VABI FakeEnd(NKWebKit::WtfAbi::PrintStream* stream) {
    (void)stream;
    events.push_back("end");
}

NKWebKit::WtfAbi::PrintStream* APS5_VABI FakeBeginNull(NKWebKit::WtfAbi::PrintStream* stream) {
    (void)stream;
    events.push_back("begin");
    return nullptr;
}
const void* const outerVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&CaptureVprintf),
    nullptr,
    reinterpret_cast<const void*>(&FakeBegin),
    reinterpret_cast<const void*>(&FakeEnd),
};

const void* const outerVtableNullBegin[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&CaptureVprintf),
    nullptr,
    reinterpret_cast<const void*>(&FakeBeginNull),
    reinterpret_cast<const void*>(&FakeEnd),
};

const void* const innerVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&CaptureVprintf),
    nullptr,
    nullptr,
    nullptr,
};

const void* const outerVtableNoEnd[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&CaptureVprintf),
    nullptr,
    reinterpret_cast<const void*>(&FakeBegin),
    nullptr,
};

const void* const innerVtableNoVprintf[] = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

void ResetCapture() {
    captured.clear();
    captureOkay = true;
    printTargets.clear();
    events.clear();
    outerStream.vtable = outerVtable;
    innerStream.vtable = innerVtable;
}

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF print stream test failed at line %u\n", location.line());
        std::abort();
    }
}

void CheckAtomicSection(const char* text, std::source_location location = std::source_location::current()) {
    Check(captureOkay, location);
    Check(captured == text, location);
    Check(events.size() == 2 && std::strcmp(events[0], "begin") == 0 && std::strcmp(events[1], "end") == 0, location);
    Check(!printTargets.empty(), location);
    for (const void* target : printTargets) Check(target == &innerStream, location);
}

void CheckDirectPrint(std::source_location location = std::source_location::current()) {
    Check(captureOkay, location);
    Check(events.empty(), location);
    for (const void* target : printTargets) Check(target == &outerStream, location);
}

template<typename TAction> void Throws(TAction action, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        action();
    } catch (const std::exception&) {
        threw = true;
    }
    Check(threw, location);
}

const unsigned char* U8(const char* data) {
    return reinterpret_cast<const unsigned char*>(data);
}

String Make8(const char* data) {
    return _ZN3WTF10StringImpl6createEPKhj(U8(data), static_cast<unsigned>(std::strlen(data)));
}

String Make16(const std::u16string& data) {
    return _ZN3WTF10StringImpl6createEPKDsj(data.data(), static_cast<unsigned>(data.size()));
}

std::string PrintOf(const StringImpl* impl) {
    ResetCapture();
    _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(FakeStream(), impl);
    CheckDirectPrint();
    return captured;
}

void TestNullStringImpl() {    Check(PrintOf(nullptr) == "(null StringImpl*)");
}

void TestPrintStrings() {
    Check(PrintOf(Make8("Hello").impl) == "Hello");
    Check(PrintOf(Make8("").impl) == "");
    Check(PrintOf(Make16(u"").impl) == "");
    Check(PrintOf(Make8("caf\xE9s").impl) == std::string("caf\xC3\xA9s", 6));
    Check(PrintOf(Make16(u"h\xE9llo").impl) == std::string("h\xC3\xA9llo", 6));
    Check(PrintOf(Make16(u"\xD800").impl) == std::string("\xED\xA0\x80", 3));
    Check(PrintOf(Make16(u"\xD83D\xDE00").impl) == std::string("\xF0\x9F\x98\x80", 4));
    Check(PrintOf(Make16(u"a\xDC00z").impl) == std::string("a\xED\xB0\x80z", 5));
}

void TestPrintTypedArrayModes() {
    const std::pair<std::uint8_t, const char*> names[] = {
        {0b0001'0000, "FastTypedArray"},
        {0b0011'0000, "OversizeTypedArray"},
        {0b0101'1000, "WastefulTypedArray"},
        {0b0101'1010, "GrowableSharedWastefulTypedArray"},
        {0b0101'1011, "GrowableSharedAutoLengthWastefulTypedArray"},
        {0b0101'1100, "ResizableNonSharedWastefulTypedArray"},
        {0b0101'1101, "ResizableNonSharedAutoLengthWastefulTypedArray"},
        {0b1000'1000, "DataViewMode"},
        {0b1000'1010, "GrowableSharedDataViewMode"},
        {0b1000'1011, "GrowableSharedAutoLengthDataViewMode"},
        {0b1000'1100, "ResizableNonSharedDataViewMode"},
        {0b1000'1101, "ResizableNonSharedAutoLengthDataViewMode"},
    };
    for (const auto& entry : names) {
        ResetCapture();
        _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(FakeStream(), static_cast<JSC::TypedArrayMode>(entry.first));
        CheckAtomicSection(entry.second);
    }
    ResetCapture();
    Throws([] { _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(FakeStream(), static_cast<JSC::TypedArrayMode>(0)); });
    Check(events.empty());
    Throws([] { _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(FakeStream(), static_cast<JSC::TypedArrayMode>(0xFF)); });
    Check(events.empty());
}

void TestPrintOversizedStrings() {
    ResetCapture();
    NKWebKit::WtfAbi::WtfPrintUtf8_nid_no_patch(FakeStream(), StringView{nullptr, MaxStringLength / 3 + 1, false}, "StringView");
    Check(captured == "(Out of memory while converting StringView to utf8)");
    CheckDirectPrint();
    ResetCapture();
    NKWebKit::WtfAbi::WtfPrintUtf8_nid_no_patch(FakeStream(), StringView{nullptr, MaxStringLength / 2 + 1, true}, "StringView");
    Check(captured == "(Out of memory while converting StringView to utf8)");
    CheckDirectPrint();
}

void TestPrintOversizedStringImpl() {
    StringImpl narrow{2, MaxStringLength / 2 + 1, nullptr, 4};
    StringImpl wide{2, MaxStringLength / 3 + 1, nullptr, 0};
    Check(PrintOf(&narrow) == "(Out of memory while converting StringImpl* to utf8)");
    Check(PrintOf(&wide) == "(Out of memory while converting StringImpl* to utf8)");
}

void TestUrlDumpOversized() {
    StringImpl narrow{2, MaxStringLength / 2 + 1, nullptr, 4};
    URL url{};
    url.impl = &narrow;
    ResetCapture();
    _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, FakeStream());
    CheckAtomicSection("(Out of memory while converting String to utf8)");
}

void TestPrintBadStream() {
    FakePrintStream broken{nullptr};
    auto& stream = *reinterpret_cast<NKWebKit::WtfAbi::PrintStream*>(&broken);
    ResetCapture();
    Throws([&] { _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(stream, nullptr); });
    Check(captured.empty());
    Check(captureOkay);
    Check(events.empty());
}

void CheckSectionClosed(std::source_location location = std::source_location::current()) {
    Check(captured.empty(), location);
    Check(events.size() == 2 && std::strcmp(events[0], "begin") == 0 && std::strcmp(events[1], "end") == 0, location);
}

void DumpExampleUrl() {
    String text = Make8("https://example.com/");
    URL url{};
    url.impl = text.impl;
    _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, FakeStream());
}

void TestPrintBeginReturnsNull() {
    ResetCapture();
    outerStream.vtable = outerVtableNullBegin;
    Throws([] { _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(FakeStream(), JSC::TypedArrayMode::FastTypedArray); });
    CheckSectionClosed();
    ResetCapture();
    outerStream.vtable = outerVtableNullBegin;
    Throws([] { DumpExampleUrl(); });
    CheckSectionClosed();
}

void TestPrintInnerStreamCannotPrint() {
    ResetCapture();
    innerStream.vtable = innerVtableNoVprintf;
    Throws([] { _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(FakeStream(), JSC::TypedArrayMode::FastTypedArray); });
    CheckSectionClosed();
    ResetCapture();
    innerStream.vtable = innerVtableNoVprintf;
    Throws([] { DumpExampleUrl(); });
    CheckSectionClosed();
}

void TestPrintMissingEndSlot() {
    ResetCapture();
    outerStream.vtable = outerVtableNoEnd;
    Throws([] { _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(FakeStream(), JSC::TypedArrayMode::FastTypedArray); });
    Check(captured.empty());
    Check(events.empty());
    ResetCapture();
    outerStream.vtable = outerVtableNoEnd;
    Throws([] { DumpExampleUrl(); });
    Check(captured.empty());
    Check(events.empty());
}

void TestUrlDump() {
    String text = Make8("https://example.com/path?q=1");
    URL url{};
    url.impl = text.impl;
    url.bits = 1;
    url.userStart = 8;
    url.userEnd = 8;
    url.passwordEnd = 8;
    url.hostEnd = 24;
    url.pathAfterLastSlash = 25;
    url.pathEnd = 30;
    url.queryEnd = 34;
    ResetCapture();
    _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, FakeStream());
    CheckAtomicSection("https://example.com/path?q=1");

    URL empty{};
    ResetCapture();
    _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&empty, FakeStream());
    CheckAtomicSection("");

    String wide = Make16(u"https://ex\xE4mple.test/");
    URL unicode{};
    unicode.impl = wide.impl;
    ResetCapture();
    _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&unicode, FakeStream());
    CheckAtomicSection(std::string("https://ex\xC3\xA4mple.test/", 22).c_str());

    ResetCapture();
    Throws([&] { _ZNK3WTF3URL4dumpERNS_11PrintStreamE(nullptr, FakeStream()); });
    Check(events.empty());
}

}

int main() {
    TestNullStringImpl();
    TestPrintStrings();
    TestPrintTypedArrayModes();
    TestPrintOversizedStrings();
    TestPrintOversizedStringImpl();
    TestPrintBadStream();
    TestPrintBeginReturnsNull();
    TestPrintInnerStreamCannotPrint();
    TestPrintMissingEndSlot();
    TestUrlDump();
    TestUrlDumpOversized();
    return 0;
}
