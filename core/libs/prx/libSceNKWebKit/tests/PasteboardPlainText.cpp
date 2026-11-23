#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceNKWebKit/src/PasteboardAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::CanSmartReplace;
using NKWebKit::WebCoreAbi::CannotSmartReplace;
using NKWebKit::WebCoreAbi::Pasteboard;
using NKWebKit::WebCoreAbi::PasteboardUniquePtr;
using NKWebKit::WtfAbi::String;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Pasteboard plain text check failed at line %d\n", line);
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

std::u16string Units(const String& value) {
    std::u16string result;
    const auto view = NKWebKit::WtfAbi::ViewOf(value.impl);
    for (unsigned index = 0; index < view.length; ++index) result.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    return result;
}

std::u16string Read(Pasteboard& pasteboard, const char* type) {
    String typeString = Make(type);
    return Units(_ZN7WebCore10Pasteboard10readStringERKN3WTF6StringE(&pasteboard, &typeString));
}

std::vector<std::u16string> ReadAll(Pasteboard& pasteboard, const char* type) {
    String typeString = Make(type);
    auto strings = _ZN7WebCore10Pasteboard14readAllStringsERKN3WTF6StringE(&pasteboard, &typeString);
    std::vector<std::u16string> result;
    for (unsigned index = 0; index < strings.size; ++index) result.push_back(Units(strings[index]));
    return result;
}

void WritePlainText(Pasteboard& pasteboard, const char16_t* text, unsigned length, NKWebKit::WebCoreAbi::SmartReplaceOption option) {
    String textString = Make16(text, length);
    _ZN7WebCore10Pasteboard14writePlainTextERKN3WTF6StringENS0_18SmartReplaceOptionE(&pasteboard, &textString, option);
}

void WritePlainText(Pasteboard& pasteboard, const char* text, NKWebKit::WebCoreAbi::SmartReplaceOption option) {
    String textString = Make(text);
    _ZN7WebCore10Pasteboard14writePlainTextERKN3WTF6StringENS0_18SmartReplaceOptionE(&pasteboard, &textString, option);
}

bool WritePlainTextThrows(Pasteboard* self, const String* text) {
    try {
        _ZN7WebCore10Pasteboard14writePlainTextERKN3WTF6StringENS0_18SmartReplaceOptionE(self, text, CannotSmartReplace);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestFactoryReturnsDistinctHandles() {
    PasteboardUniquePtr first = _ZN7WebCore10Pasteboard21createForCopyAndPasteEv();
    PasteboardUniquePtr second = _ZN7WebCore10Pasteboard21createForCopyAndPasteEv();
    Require(first.pointer != nullptr);
    Require(second.pointer != nullptr);
    Require(first.pointer != second.pointer);
    Require(first.pointer->changeCount == 0);
    Require(second.pointer->changeCount == 0);
}

void TestPlainTextLandsUnderItsOwnType() {
    Pasteboard pasteboard;
    WritePlainText(pasteboard, "plain", CannotSmartReplace);
    Require(Read(pasteboard, "text/plain;charset=utf-8") == u"plain");
    Require(ReadAll(pasteboard, "text/plain;charset=utf-8") == std::vector<std::u16string>{u"plain"});
    Require(Read(pasteboard, "text/plain").empty());
    Require(Read(pasteboard, "text/html").empty());
}

void TestPlainTextUsesUTF16() {
    Pasteboard pasteboard;
    const char16_t text[] = {0x65E5, 0xD800, 0xDE00, 0xDFFF, 0x0000};
    WritePlainText(pasteboard, text, 5, CannotSmartReplace);
    Require(Read(pasteboard, "text/plain;charset=utf-8") == std::u16string(text, 5));
}

void TestPlainTextOverwrites() {
    Pasteboard pasteboard;
    WritePlainText(pasteboard, "first", CannotSmartReplace);
    WritePlainText(pasteboard, "second", CanSmartReplace);
    Require(ReadAll(pasteboard, "text/plain;charset=utf-8") == std::vector<std::u16string>{u"second"});
    Require(Read(pasteboard, "text/plain;charset=utf-8") == u"second");
}

void TestPlainTextEmptyValue() {
    Pasteboard pasteboard;
    WritePlainText(pasteboard, "before-empty", CannotSmartReplace);
    Require(Read(pasteboard, "text/plain;charset=utf-8") == u"before-empty");
    WritePlainText(pasteboard, "", CannotSmartReplace);
    Require(Read(pasteboard, "text/plain;charset=utf-8").empty());
    Require(ReadAll(pasteboard, "text/plain;charset=utf-8").empty());
    Require(pasteboard.changeCount == 2);
}

void TestPlainTextAdvancesChangeCount() {
    Pasteboard pasteboard;
    Require(pasteboard.changeCount == 0);
    WritePlainText(pasteboard, "a", CannotSmartReplace);
    WritePlainText(pasteboard, "b", CannotSmartReplace);
    Require(pasteboard.changeCount == 2);
}

void TestPlainTextPreservesOtherTypes() {
    Pasteboard pasteboard;
    String type = Make("text/html");
    String html = Make("<p>preserved</p>");
    _ZN7WebCore10Pasteboard11writeStringERKN3WTF6StringES4_(&pasteboard, &type, &html);
    WritePlainText(pasteboard, "plain-after-html", CannotSmartReplace);
    Require(Read(pasteboard, "text/html") == u"<p>preserved</p>");
    Require(Read(pasteboard, "text/plain;charset=utf-8") == u"plain-after-html");
}

void TestNullArgumentsThrow() {
    Pasteboard pasteboard;
    String text = Make("text");
    Require(WritePlainTextThrows(nullptr, &text));
    Require(WritePlainTextThrows(&pasteboard, nullptr));
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
    TestFactoryReturnsDistinctHandles();
    TestPlainTextLandsUnderItsOwnType();
    TestPlainTextUsesUTF16();
    TestPlainTextOverwrites();
    TestPlainTextEmptyValue();
    TestPlainTextAdvancesChangeCount();
    TestPlainTextPreservesOtherTypes();
    TestNullArgumentsThrow();
    return 0;
}
