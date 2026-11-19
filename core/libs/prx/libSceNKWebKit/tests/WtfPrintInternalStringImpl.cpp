#include "prx/libSceNKWebKit/include/WtfPrintInternalStringImplExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using Jsc::WtfAbi::PrintStream;
using NKWebKit::WtfAbi::StringImpl;

constexpr unsigned Is8BitFlag = 4;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WtfPrintInternalStringImpl check failed at line %u\n", location.line());
        std::abort();
    }
}

std::unordered_set<void*> guestAllocations;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations.insert(pointer);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    Check(guestAllocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

struct CaptureStream {
    const void* const* vtable;
    std::string text;
    std::vector<std::string> formats;
};

void APS5_VABI CaptureVprintf(PrintStream* stream, const char* format, VaList* args) {
    auto* capture = reinterpret_cast<CaptureStream*>(stream);
    char buffer[256];
    const int written = vsnprintf_nid_postfix(buffer, sizeof(buffer), format, args);
    Check(written >= 0 && static_cast<std::size_t>(written) < sizeof(buffer));
    capture->text.append(buffer, static_cast<std::size_t>(written));
    capture->formats.emplace_back(format);
}

void APS5_VABI ThrowingVprintf(PrintStream*, const char*, VaList*) {
    throw std::runtime_error("vprintf failed");
}

void APS5_VABI Unused(PrintStream*) {
    std::abort();
}

const void* const CaptureVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&CaptureVprintf),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
};

const void* const ThrowingVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&ThrowingVprintf),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
};

const void* const MissingVprintfVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
};

PrintStream* AsStream(CaptureStream& stream) {
    return reinterpret_cast<PrintStream*>(&stream);
}

StringImpl Latin1(const std::string& characters) {
    return StringImpl{2, static_cast<unsigned>(characters.size()), characters.data(), Is8BitFlag};
}

StringImpl Utf16(const std::u16string& characters) {
    return StringImpl{2, static_cast<unsigned>(characters.size()), characters.data(), 0};
}

std::string Print(const StringImpl* string, std::vector<std::string>* formats = nullptr) {
    CaptureStream stream{CaptureVtable + 2};
    _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(AsStream(stream), string);
    if (formats) *formats = stream.formats;
    Check(guestAllocations.empty());
    return stream.text;
}

template <typename TException, typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

void PrintsNullStringImpl() {
    std::vector<std::string> formats;
    Check(Print(nullptr, &formats) == "(null StringImpl*)");
    Check(formats == std::vector<std::string>{"%s"});
}

void PrintsLatin1AsUtf8() {
    const std::string characters = "caf\xE9 \xFF";
    const StringImpl string = Latin1(characters);
    std::vector<std::string> formats;
    Check(Print(&string, &formats) == "caf\xC3\xA9 \xC3\xBF");
    Check(formats == std::vector<std::string>{"%s"});
}

void PrintsUtf16AsUtf8() {
    const std::u16string characters = u"Aé€\U0001F600";
    const StringImpl string = Utf16(characters);
    Check(Print(&string) == "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80");
}

void EncodesUnpairedSurrogatesLeniently() {
    const std::u16string characters{u'x', char16_t{0xDC00}, u'y', char16_t{0xD800}, u'z', char16_t{0xDBFF}};
    const StringImpl string = Utf16(characters);
    Check(Print(&string) == "x\xED\xB0\x80y\xED\xA0\x80z\xED\xAF\xBF");
}

void PrintsEmptyString() {
    const std::string characters;
    const StringImpl latin1 = Latin1(characters);
    std::vector<std::string> formats;
    Check(Print(&latin1, &formats).empty());
    Check(formats == std::vector<std::string>{"%s"});
    const std::u16string wide;
    const StringImpl utf16 = Utf16(wide);
    Check(Print(&utf16).empty());
}

void StopsAtEmbeddedNull() {
    const std::string characters("ab\0cd", 5);
    const StringImpl string = Latin1(characters);
    Check(Print(&string) == "ab");
}

void ReportsOutOfMemoryPastMaxLength() {
    const StringImpl huge{2, 715827883, nullptr, 0};
    std::vector<std::string> formats;
    Check(Print(&huge, &formats) == "(Out of memory while converting StringImpl* to utf8)");
    Check(formats == std::vector<std::string>(3, "%s"));
    const StringImpl huge8{2, 0xFFFFFFFF, nullptr, Is8BitFlag};
    Check(Print(&huge8) == "(Out of memory while converting StringImpl* to utf8)");
}

void RejectsInvalidStreams() {
    const std::string characters = "x";
    const StringImpl string = Latin1(characters);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(nullptr, &string); }));
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(nullptr, nullptr); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(AsStream(noVtable), &string); }));
    CaptureStream noVprintf{MissingVprintfVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(AsStream(noVprintf), &string); }));
    Check(noVprintf.text.empty());
}

void PropagatesVprintfFailureAndReleasesUtf8() {
    const std::string characters = "leak check";
    const StringImpl string = Latin1(characters);
    CaptureStream stream{ThrowingVtable + 2};
    Check(Throws<std::runtime_error>([&] { _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(AsStream(stream), &string); }));
    Check(guestAllocations.empty());
}

}  // namespace

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    PrintsNullStringImpl();
    PrintsLatin1AsUtf8();
    PrintsUtf16AsUtf8();
    EncodesUnpairedSurrogatesLeniently();
    PrintsEmptyString();
    StopsAtEmbeddedNull();
    ReportsOutOfMemoryPastMaxLength();
    RejectsInvalidStreams();
    PropagatesVprintfFailureAndReleasesUtf8();
    return 0;
}
