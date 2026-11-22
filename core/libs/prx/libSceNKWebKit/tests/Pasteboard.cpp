#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceNKWebKit/src/PasteboardAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::Pasteboard;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::Vector;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Pasteboard check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

String Make(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

String Make16(const char16_t* text, unsigned length) {
    return _ZN3WTF10StringImpl6createEPKDsj(text, length);
}

std::u16string Units(const String& string) {
    if (!string.impl) return u"<null>";
    const StringView view = NKWebKit::WtfAbi::ViewOf(string.impl);
    std::u16string result;
    for (unsigned index = 0; index < view.length; ++index) result.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    return result;
}

std::u16string Read(Pasteboard& pasteboard, const char* type) {
    String typeString = Make(type);
    String result = _ZN7WebCore10Pasteboard10readStringERKN3WTF6StringE(&pasteboard, &typeString);
    return Units(result);
}

void Write(Pasteboard& pasteboard, const char* type, const char* data) {
    String typeString = Make(type);
    String dataString = Make(data);
    _ZN7WebCore10Pasteboard11writeStringERKN3WTF6StringES4_(&pasteboard, &typeString, &dataString);
}

void Write(Pasteboard& pasteboard, const char* type, const char16_t* data, unsigned length) {
    String typeString = Make(type);
    String dataString = Make16(data, length);
    _ZN7WebCore10Pasteboard11writeStringERKN3WTF6StringES4_(&pasteboard, &typeString, &dataString);
}

std::vector<std::u16string> ReadAll(Pasteboard& pasteboard, const char* type) {
    String typeString = Make(type);
    Vector<String> result = _ZN7WebCore10Pasteboard14readAllStringsERKN3WTF6StringE(&pasteboard, &typeString);
    std::vector<std::u16string> values;
    for (unsigned index = 0; index < result.size; ++index) values.push_back(Units(result[index]));
    return values;
}

bool ReadThrows(Pasteboard* self, const String* type) {
    try {
        (void)_ZN7WebCore10Pasteboard10readStringERKN3WTF6StringE(self, type);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

bool WriteThrows(Pasteboard* self, const String* type, const String* data) {
    try {
        _ZN7WebCore10Pasteboard11writeStringERKN3WTF6StringES4_(self, type, data);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

bool ReadAllThrows(Pasteboard* self, const String* type) {
    try {
        (void)_ZN7WebCore10Pasteboard14readAllStringsERKN3WTF6StringE(self, type);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestRoundTripASCII() {
    Pasteboard pasteboard;
    Write(pasteboard, "text/plain", "hello");
    Require(Read(pasteboard, "text/plain") == u"hello");
}

void TestRoundTripUTF16() {
    Pasteboard pasteboard;
    const char16_t text[] = {0x65E5, 0x672C, 0x8A9E, 0xD83D, 0xDE00, 0x0000, 0x0041};
    Write(pasteboard, "text/html", text, 7);
    Require(Read(pasteboard, "text/html") == std::u16string(text, 7));
}

void TestRoundTripUnpairedSurrogate() {
    Pasteboard pasteboard;
    const char16_t text[] = {0xD800, 0x0041, 0xDFFF};
    Write(pasteboard, "text/plain", text, 3);
    Require(Read(pasteboard, "text/plain") == std::u16string(text, 3));
}

void TestStoreIsSharedAcrossHandles() {
    Pasteboard writer;
    Pasteboard reader;
    Write(writer, "text/html", "shared");
    Require(Read(reader, "text/html") == u"shared");
}

void TestEmptyValueReadsNull() {
    Pasteboard pasteboard;
    Write(pasteboard, "text/plain", "");
    String typeString = Make("text/plain");
    String value = _ZN7WebCore10Pasteboard10readStringERKN3WTF6StringE(&pasteboard, &typeString);
    Require(value.impl == nullptr);
}

void TestUnwrittenTypeIsNull() {
    Pasteboard pasteboard;
    String typeString = Make("application/x-anyps5-absent");
    String value = _ZN7WebCore10Pasteboard10readStringERKN3WTF6StringE(&pasteboard, &typeString);
    Require(value.impl == nullptr);
}

void TestOverwriteKeepsOneEntry() {
    Pasteboard pasteboard;
    Write(pasteboard, "text/plain", "first");
    Write(pasteboard, "text/plain", "second");
    Require(Read(pasteboard, "text/plain") == u"second");
    Require(ReadAll(pasteboard, "text/plain").size() == 1);
}

void TestReadAllStringsAbsent() {
    Pasteboard pasteboard;
    Require(ReadAll(pasteboard, "application/x-anyps5-absent").empty());
}

void TestReadAllStringsOne() {
    Pasteboard pasteboard;
    Write(pasteboard, "text/plain", "one");
    const auto values = ReadAll(pasteboard, "text/plain");
    Require(values.size() == 1);
    Require(values[0] == u"one");
}

void TestReadAllStringsEmptyValue() {
    Pasteboard pasteboard;
    Write(pasteboard, "text/plain", "");
    Require(ReadAll(pasteboard, "text/plain").empty());
}

void TestSeveralTypesStaySeparate() {
    Pasteboard pasteboard;
    Write(pasteboard, "text/plain", "plain");
    Write(pasteboard, "text/html", "<p>markup</p>");
    Write(pasteboard, "text/uri-list", "https://anyps5.invalid/");
    Require(ReadAll(pasteboard, "text/plain").size() == 1);
    Require(Read(pasteboard, "text/plain") == u"plain");
    Require(Read(pasteboard, "text/html") == u"<p>markup</p>");
    Require(Read(pasteboard, "text/uri-list") == u"https://anyps5.invalid/");
}

void TestChangeCountAdvances() {
    Pasteboard pasteboard;
    Require(pasteboard.changeCount == 0);
    Write(pasteboard, "text/plain", "a");
    Write(pasteboard, "text/html", "b");
    Require(pasteboard.changeCount == 2);
}

void TestNullArgumentsThrow() {
    Pasteboard pasteboard;
    String type = Make("text/plain");
    String data = Make("x");
    Require(ReadThrows(nullptr, &type));
    Require(ReadThrows(&pasteboard, nullptr));
    Require(WriteThrows(nullptr, &type, &data));
    Require(WriteThrows(&pasteboard, nullptr, &data));
    Require(WriteThrows(&pasteboard, &type, nullptr));
    Require(ReadAllThrows(nullptr, &type));
    Require(ReadAllThrows(&pasteboard, nullptr));
}

void* APS5_VABI HeapAllocate(std::size_t size) { return std::malloc(size == 0 ? 1 : size); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t size) {
    return std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
}
void* APS5_VABI HeapReallocate(void* pointer, std::size_t size) {
    return std::realloc(pointer, size == 0 ? 1 : size);
}
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

}  // namespace

int main() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(HeapAllocate),
        reinterpret_cast<void*>(HeapFree),
        reinterpret_cast<void*>(HeapCallocate),
        reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign),
        reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    TestRoundTripASCII();
    TestRoundTripUTF16();
    TestRoundTripUnpairedSurrogate();
    TestStoreIsSharedAcrossHandles();
    TestEmptyValueReadsNull();
    TestUnwrittenTypeIsNull();
    TestOverwriteKeepsOneEntry();
    TestReadAllStringsAbsent();
    TestReadAllStringsOne();
    TestReadAllStringsEmptyValue();
    TestSeveralTypesStaySeparate();
    TestChangeCountAdvances();
    TestNullArgumentsThrow();
    return 0;
}
