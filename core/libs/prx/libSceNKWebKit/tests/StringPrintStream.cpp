#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringPrintStream;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "StringPrintStream check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

void APS5_VABI Print(StringPrintStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    _ZN3WTF17StringPrintStream7vprintfEPKcP13__va_list_tag(stream, format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    va_list arguments;
    va_start(arguments, format);
    _ZN3WTF17StringPrintStream7vprintfEPKcP13__va_list_tag(stream, format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
}

using VprintfSlot = void (APS5_VABI*)(StringPrintStream*, const char*, VaList*);

void APS5_VABI PrintThroughVtable(StringPrintStream* stream, const char* format, ...) {
    const auto slot = reinterpret_cast<VprintfSlot>(const_cast<void*>(stream->vtable[2]));
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    slot(stream, format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    va_list arguments;
    va_start(arguments, format);
    slot(stream, format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
}

struct Stream {
    StringPrintStream object;
    Stream() {
        std::memset(&object, 0xA5, sizeof(object));
        _ZN3WTF17StringPrintStreamC1Ev(&object);
    }
    ~Stream() { _ZN3WTF17StringPrintStreamD1Ev(&object); }
    StringPrintStream* get() { return &object; }
};

std::string Text(const String& string) {
    Require(string.impl && (string.impl->hashAndFlags & 4));
    return std::string(static_cast<const char*>(string.impl->data), string.impl->length);
}

void TestConstruction() {
    Stream stream;
    Require(stream.object.buffer == stream.object.inlineBuffer);
    Require(stream.object.next == 0 && stream.object.size == 128 && stream.object.buffer[0] == 0);
    const void* const* table = stream.object.vtable - 2;
    Require(!table[0] && !table[1]);
    for (int slot = 2; slot < 8; ++slot) Require(table[slot] != nullptr);
#ifndef _WIN32
    Require(table[2] == reinterpret_cast<const void*>(&_ZN3WTF17StringPrintStreamD1Ev));
    Require(table[3] == reinterpret_cast<const void*>(&_ZN3WTF17StringPrintStreamD0Ev));
    Require(table[4] == reinterpret_cast<const void*>(&_ZN3WTF17StringPrintStream7vprintfEPKcP13__va_list_tag));
    Require(table[6] == reinterpret_cast<const void*>(&_ZN3WTF11PrintStream5beginEv));
    Require(table[7] == reinterpret_cast<const void*>(&_ZN3WTF11PrintStream3endEv));
#endif
    reinterpret_cast<void (APS5_VABI*)(StringPrintStream*)>(const_cast<void*>(stream.object.vtable[3]))(stream.get());
    Require(reinterpret_cast<void* (APS5_VABI*)(void*)>(const_cast<void*>(stream.object.vtable[4]))(stream.get()) == stream.get());

    StringPrintStream base;
    std::memset(&base, 0x5A, sizeof(base));
    _ZN3WTF17StringPrintStreamC2Ev(&base);
    Require(base.vtable == stream.object.vtable && base.buffer == base.inlineBuffer);
    _ZN3WTF17StringPrintStreamD2Ev(&base);
}

void TestGrowth() {
    {
        Stream stream;
        for (int i = 0; i < 3; ++i) Print(stream.get(), "%0120d", i);
        Require(stream.object.next == 360 && stream.object.size == 482 && stream.object.buffer != stream.object.inlineBuffer);
        Require(std::string(stream.object.buffer, 360) == std::string(119, '0') + "0" + std::string(119, '0') + "1" + std::string(119, '0') + "2");
        Require(stream.object.buffer[360] == 0);
        char* heap = stream.object.buffer;
        _ZN3WTF17StringPrintStream5resetEv(stream.get());
        Require(stream.object.next == 0 && stream.object.size == 482 && stream.object.buffer == heap && heap[0] == 0);
        Print(stream.get(), "%s", "abc");
        Require(stream.object.next == 3 && stream.object.size == 482);
        CString text = _ZN3WTF17StringPrintStream9toCStringEv(stream.get());
        Require(text.buffer && text.buffer->length == 3 && std::memcmp(reinterpret_cast<const char*>(text.buffer + 1), "abc", 4) == 0);
    }
    {
        Stream stream;
        Print(stream.get(), "%01000d", 7);
        Require(stream.object.next == 1000 && stream.object.size == 2002);
    }
    {
        Stream stream;
        Print(stream.get(), "%0127d", 1);
        Require(stream.object.next == 127 && stream.object.size == 128 && stream.object.buffer == stream.object.inlineBuffer);
        PrintThroughVtable(stream.get(), "x");
        Require(stream.object.next == 128 && stream.object.size == 258 && stream.object.buffer[127] == 'x');
    }
    {
        Stream stream;
        std::string expected;
        for (int i = 0; i < 50; ++i) {
            Print(stream.get(), "%d,", i);
            expected += std::to_string(i) + ",";
        }
        Require(stream.object.next == 140 && stream.object.size == 258);
        Require(Text(_ZN3WTF17StringPrintStream8toStringEv(stream.get())) == expected);
    }
}

void TestConversions() {
    Stream stream;
    Print(stream.get(), "caf%s", "\xC3\xA9");
    String utf8 = _ZN3WTF17StringPrintStream8toStringEv(stream.get());
    Require(utf8.impl && utf8.impl->length == 4);
    auto expected = _ZN3WTF17StringPrintStream11tryToStringEv(stream.get());
    Require(expected.hasValue && expected.impl && expected.impl->length == 4);
    String owner(expected.impl);

    _ZN3WTF17StringPrintStream5resetEv(stream.get());
    Print(stream.get(), "%s", "a\xFF");
    Require(!_ZN3WTF17StringPrintStream8toStringEv(stream.get()).impl);
    Require(Text(_ZN3WTF17StringPrintStream26toStringWithLatin1FallbackEv(stream.get())) == "a\xFF");
    auto invalid = _ZN3WTF17StringPrintStream11tryToStringEv(stream.get());
    Require(invalid.hasValue && !invalid.impl);
}

void TestDeletingDestructor() {
    auto* heap = static_cast<StringPrintStream*>(_ZN3WTF10fastMallocEm(sizeof(StringPrintStream)));
    _ZN3WTF17StringPrintStreamC1Ev(heap);
    Print(heap, "%0300d", 1);
    Require(heap->buffer != heap->inlineBuffer);
    reinterpret_cast<void (APS5_VABI*)(StringPrintStream*)>(const_cast<void*>(heap->vtable[1]))(heap);
}

void TestNullArguments() {
    Stream stream;
    RequireInvalidArgument([] { _ZN3WTF17StringPrintStreamC1Ev(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF17StringPrintStreamD1Ev(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF17StringPrintStream5resetEv(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF17StringPrintStream8toStringEv(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF17StringPrintStream9toCStringEv(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF17StringPrintStream11tryToStringEv(nullptr); });
    RequireInvalidArgument([&] { _ZN3WTF17StringPrintStream7vprintfEPKcP13__va_list_tag(stream.get(), nullptr, nullptr); });
}

}

int main() {
    TestConstruction();
    TestGrowth();
    TestConversions();
    TestDeletingDestructor();
    TestNullArguments();
    std::puts("StringPrintStream tests passed");
    return 0;
}
