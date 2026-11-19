#include "prx/libSceNKWebKit/include/WtfUrlDumpExports.hpp"
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
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

constexpr unsigned Is8BitFlag = 4;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WtfUrlDump check failed at line %u\n", location.line());
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
    CaptureStream* target = nullptr;
    std::string text;
    std::vector<std::string> formats;
    std::vector<std::string> events;
};

CaptureStream* AsCapture(PrintStream* stream) {
    return reinterpret_cast<CaptureStream*>(stream);
}

PrintStream* AsStream(CaptureStream& stream) {
    return reinterpret_cast<PrintStream*>(&stream);
}

void APS5_VABI CaptureVprintf(PrintStream* stream, const char* format, VaList* args) {
    auto* capture = AsCapture(stream);
    char buffer[256];
    const int written = vsnprintf_nid_postfix(buffer, sizeof(buffer), format, args);
    Check(written >= 0 && static_cast<std::size_t>(written) < sizeof(buffer));
    capture->text.append(buffer, static_cast<std::size_t>(written));
    capture->formats.emplace_back(format);
    capture->events.emplace_back("vprintf");
}

void APS5_VABI ThrowingVprintf(PrintStream*, const char*, VaList*) {
    throw std::runtime_error("vprintf failed");
}

PrintStream* APS5_VABI SelfBegin(PrintStream* stream) {
    AsCapture(stream)->events.emplace_back("begin");
    return stream;
}

PrintStream* APS5_VABI TargetBegin(PrintStream* stream) {
    auto* capture = AsCapture(stream);
    capture->events.emplace_back("begin");
    return AsStream(*capture->target);
}

void APS5_VABI RecordEnd(PrintStream* stream) {
    AsCapture(stream)->events.emplace_back("end");
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
    reinterpret_cast<const void*>(&SelfBegin),
    reinterpret_cast<const void*>(&RecordEnd),
};

const void* const ForwardingVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&TargetBegin),
    reinterpret_cast<const void*>(&RecordEnd),
};

const void* const ThrowingVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&ThrowingVprintf),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&SelfBegin),
    reinterpret_cast<const void*>(&RecordEnd),
};

const void* const MissingVprintfVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&SelfBegin),
    reinterpret_cast<const void*>(&RecordEnd),
};

const void* const MissingEndVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&CaptureVprintf),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&SelfBegin),
    nullptr,
};

StringImpl Latin1(const std::string& characters) {
    return StringImpl{2, static_cast<unsigned>(characters.size()), characters.data(), Is8BitFlag};
}

StringImpl Utf16(const std::u16string& characters) {
    return StringImpl{2, static_cast<unsigned>(characters.size()), characters.data(), 0};
}

URL MakeUrl(StringImpl* impl) {
    URL url{};
    url.impl = impl;
    return url;
}

std::string PrintString(StringImpl* impl, std::vector<std::string>* formats = nullptr) {
    CaptureStream stream{CaptureVtable + 2};
    String string(impl);
    _ZN3WTF13printInternalERNS_11PrintStreamERKNS_6StringE(AsStream(stream), &string);
    string.impl = nullptr;
    if (formats) *formats = stream.formats;
    Check(stream.events.size() == stream.formats.size());
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

void PrintsNullStringAsEmpty() {
    std::vector<std::string> formats;
    Check(PrintString(nullptr, &formats).empty());
    Check(formats == std::vector<std::string>{"%s"});
}

void PrintsLatin1AsUtf8() {
    const std::string characters = "caf\xE9 \xFF";
    StringImpl impl = Latin1(characters);
    std::vector<std::string> formats;
    Check(PrintString(&impl, &formats) == "caf\xC3\xA9 \xC3\xBF");
    Check(formats == std::vector<std::string>{"%s"});
    Check(impl.refCount == 2);
}

void PrintsUtf16Leniently() {
    const std::u16string characters{u'A', u'é', u'€', char16_t{0xD83D}, char16_t{0xDE00}, char16_t{0xDC00}};
    StringImpl impl = Utf16(characters);
    Check(PrintString(&impl) == "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xED\xB0\x80");
}

void StopsAtEmbeddedNull() {
    const std::string characters("ab\0cd", 5);
    StringImpl impl = Latin1(characters);
    Check(PrintString(&impl) == "ab");
}

void ReportsOutOfMemoryPastMaxLength() {
    StringImpl huge{2, 715827883, nullptr, 0};
    std::vector<std::string> formats;
    Check(PrintString(&huge, &formats) == "(Out of memory while converting String to utf8)");
    Check(formats == std::vector<std::string>(3, "%s"));
    StringImpl huge8{2, 0xFFFFFFFF, nullptr, Is8BitFlag};
    Check(PrintString(&huge8) == "(Out of memory while converting String to utf8)");
}

void RejectsInvalidPrintInternalArguments() {
    const std::string characters = "x";
    StringImpl impl = Latin1(characters);
    String string(&impl);
    CaptureStream stream{CaptureVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF13printInternalERNS_11PrintStreamERKNS_6StringE(AsStream(stream), nullptr); }));
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF13printInternalERNS_11PrintStreamERKNS_6StringE(nullptr, &string); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF13printInternalERNS_11PrintStreamERKNS_6StringE(AsStream(noVtable), &string); }));
    CaptureStream noVprintf{MissingVprintfVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF13printInternalERNS_11PrintStreamERKNS_6StringE(AsStream(noVprintf), &string); }));
    string.impl = nullptr;
    Check(stream.text.empty() && noVprintf.text.empty());
    Check(guestAllocations.empty());
}

void DumpsUrlStringBetweenBeginAndEnd() {
    const std::string characters = "https://example.com/caf\xE9";
    StringImpl impl = Latin1(characters);
    const URL url = MakeUrl(&impl);
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, AsStream(stream));
    Check(stream.text == "https://example.com/caf\xC3\xA9");
    Check(stream.formats == std::vector<std::string>{"%s"});
    Check((stream.events == std::vector<std::string>{"begin", "vprintf", "end"}));
    Check(impl.refCount == 2);
    Check(guestAllocations.empty());
}

void DumpsNullUrlStringAsEmpty() {
    const URL url = MakeUrl(nullptr);
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, AsStream(stream));
    Check(stream.text.empty());
    Check((stream.events == std::vector<std::string>{"begin", "vprintf", "end"}));
}

void DumpPrintsToBegunStreamAndEndsOriginal() {
    const std::string characters = "about:blank";
    StringImpl impl = Latin1(characters);
    const URL url = MakeUrl(&impl);
    CaptureStream target{CaptureVtable + 2};
    CaptureStream origin{ForwardingVtable + 2, &target};
    _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, AsStream(origin));
    Check(target.text == "about:blank");
    Check((target.events == std::vector<std::string>{"vprintf"}));
    Check((origin.events == std::vector<std::string>{"begin", "end"}));
}

void DumpRejectsInvalidArguments() {
    const std::string characters = "x";
    StringImpl impl = Latin1(characters);
    const URL url = MakeUrl(&impl);
    CaptureStream stream{CaptureVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF3URL4dumpERNS_11PrintStreamE(nullptr, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, nullptr); }));
    CaptureStream noEnd{MissingEndVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, AsStream(noEnd)); }));
    Check(noEnd.events.empty() && stream.events.empty());
}

void DumpPropagatesVprintfFailureWithoutEnd() {
    const std::string characters = "leak check";
    StringImpl impl = Latin1(characters);
    const URL url = MakeUrl(&impl);
    CaptureStream stream{ThrowingVtable + 2};
    Check(Throws<std::runtime_error>([&] { _ZNK3WTF3URL4dumpERNS_11PrintStreamE(&url, AsStream(stream)); }));
    Check((stream.events == std::vector<std::string>{"begin"}));
    Check(guestAllocations.empty());
}

}  // namespace

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    PrintsNullStringAsEmpty();
    PrintsLatin1AsUtf8();
    PrintsUtf16Leniently();
    StopsAtEmbeddedNull();
    ReportsOutOfMemoryPastMaxLength();
    RejectsInvalidPrintInternalArguments();
    DumpsUrlStringBetweenBeginAndEnd();
    DumpsNullUrlStringAsEmpty();
    DumpPrintsToBegunStreamAndEndsOriginal();
    DumpRejectsInvalidArguments();
    DumpPropagatesVprintfFailureWithoutEnd();
    return 0;
}
