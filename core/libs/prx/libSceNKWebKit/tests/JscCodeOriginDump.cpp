#include "prx/libSceNKWebKit/include/JscCodeOriginDumpExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Jsc::WtfAbi::PrintStream;
using NKWebKit::JscCodeOriginAbi::CodeOrigin;
using NKWebKit::JscCodeOriginAbi::OutOfLineCodeOrigin;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "JscCodeOriginDump check failed at line %u\n", location.line());
        std::abort();
    }
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
    char buffer[128];
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
    nullptr,
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

const void* const MissingBeginVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&Unused),
    reinterpret_cast<const void*>(&CaptureVprintf),
    reinterpret_cast<const void*>(&Unused),
    nullptr,
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

template <typename TException, typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

constexpr std::uintptr_t Inline(std::uint32_t offset, std::uint32_t checkpoint) {
    return static_cast<std::uintptr_t>(offset << 2 | checkpoint) << 48;
}

std::string Dump(std::uintptr_t value) {
    const CodeOrigin origin{value};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, AsStream(stream));
    return stream.text;
}

void DumpsUnsetOriginsAsNone() {
    const CodeOrigin unset{2};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&unset, AsStream(stream));
    Check(stream.text == "<none>");
    Check((stream.formats == std::vector<std::string>{"%s"}));
    Check((stream.events == std::vector<std::string>{"begin", "vprintf", "end"}));
    Check(Dump(8 | 2) == "<none>");
}

void DumpsInlineBytecodeIndex() {
    const CodeOrigin origin{Inline(5, 0)};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, AsStream(stream));
    Check(stream.text == "bc#5");
    Check((stream.formats == std::vector<std::string>{"%s", "%u"}));
    Check((stream.events == std::vector<std::string>{"begin", "begin", "vprintf", "vprintf", "end", "end"}));
    Check(Dump(Inline(0, 0)) == "bc#0");
}

void DumpsCheckpoints() {
    const CodeOrigin origin{Inline(7, 3)};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, AsStream(stream));
    Check(stream.text == "bc#7cp#3");
    Check((stream.formats == std::vector<std::string>{"%s", "%u", "%s", "%u"}));
    Check((stream.events == std::vector<std::string>{"begin", "begin", "vprintf", "vprintf", "end", "begin", "vprintf", "vprintf", "end", "end"}));
    Check(Dump(std::uintptr_t{0xffff} << 48) == "bc#16383cp#3");
}

void DumpsOutOfLineBytecodeIndex() {
    alignas(8) const OutOfLineCodeOrigin outOfLine{nullptr, 100000u << 2 | 1};
    Check(Dump(reinterpret_cast<std::uintptr_t>(&outOfLine) | 1) == "bc#100000cp#1");
    alignas(8) const OutOfLineCodeOrigin large{nullptr, 0xfffffffcu};
    Check(Dump(reinterpret_cast<std::uintptr_t>(&large) | 1) == "bc#1073741823");
}

void PrintsToTheStreamBeginReturns() {
    const CodeOrigin origin{Inline(9, 2)};
    CaptureStream target{CaptureVtable + 2};
    CaptureStream outer{ForwardingVtable + 2, &target};
    _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, AsStream(outer));
    Check(target.text == "bc#9cp#2");
    Check((outer.events == std::vector<std::string>{"begin", "end"}));
    Check((target.events == std::vector<std::string>{"begin", "vprintf", "vprintf", "end", "begin", "vprintf", "vprintf", "end"}));
    Check(outer.text.empty());
}

void ThrowsForInlinedOrigins() {
    alignas(8) const unsigned char frame[16]{};
    const CodeOrigin inlined{Inline(3, 0) | reinterpret_cast<std::uintptr_t>(frame)};
    CaptureStream stream{CaptureVtable + 2};
    Check(Throws<std::runtime_error>([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&inlined, AsStream(stream)); }));
    alignas(8) const OutOfLineCodeOrigin outOfLine{frame, 100000u << 2};
    const CodeOrigin inlinedOutOfLine{reinterpret_cast<std::uintptr_t>(&outOfLine) | 1};
    Check(Throws<std::runtime_error>([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&inlinedOutOfLine, AsStream(stream)); }));
    Check(stream.events.empty());
}

void PropagatesExceptionsWithoutCallingEnd() {
    const CodeOrigin origin{Inline(1, 0)};
    CaptureStream stream{ThrowingVtable + 2};
    Check(Throws<std::runtime_error>([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, AsStream(stream)); }));
    Check((stream.events == std::vector<std::string>{"begin", "begin"}));
}

void RejectsInvalidOriginsAndStreams() {
    const CodeOrigin origin{Inline(1, 0)};
    const CodeOrigin nullOutOfLine{1};
    CaptureStream stream{CaptureVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(nullptr, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&nullOutOfLine, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, nullptr); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, AsStream(noVtable)); }));
    CaptureStream noBegin{MissingBeginVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, AsStream(noBegin)); }));
    CaptureStream noEnd{MissingEndVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(&origin, AsStream(noEnd)); }));
    Check(stream.events.empty() && noBegin.events.empty() && noEnd.events.empty());
}

}  // namespace

int main() {
    DumpsUnsetOriginsAsNone();
    DumpsInlineBytecodeIndex();
    DumpsCheckpoints();
    DumpsOutOfLineBytecodeIndex();
    PrintsToTheStreamBeginReturns();
    ThrowsForInlinedOrigins();
    PropagatesExceptionsWithoutCallingEnd();
    RejectsInvalidOriginsAndStreams();
    std::puts("JscCodeOriginDump tests passed");
    return 0;
}
