#include "prx/libSceNKWebKit/include/JscCellAttributesDumpExports.hpp"
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
using NKWebKit::JscCellAttributesAbi::CellAttributes;
using NKWebKit::JscCellAttributesAbi::DestructionMode;
using NKWebKit::JscCellAttributesAbi::HeapCellKind;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "JscCellAttributesDump check failed at line %u\n", location.line());
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

CellAttributes Attributes(std::int8_t destruction, std::int8_t kind) {
    return {static_cast<DestructionMode>(destruction), static_cast<HeapCellKind>(kind)};
}

std::string Dump(const CellAttributes& attributes) {
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, AsStream(stream));
    return stream.text;
}

void DumpsEveryModeAndKind() {
    Check(Dump(Attributes(0, 0)) == "{DoesNotNeedDestruction, JSCell}");
    Check(Dump(Attributes(1, 0)) == "{NeedsDestruction, JSCell}");
    Check(Dump(Attributes(0, 1)) == "{DoesNotNeedDestruction, JSCellWithIndexingHeader}");
    Check(Dump(Attributes(1, 1)) == "{NeedsDestruction, JSCellWithIndexingHeader}");
    Check(Dump(Attributes(0, 2)) == "{DoesNotNeedDestruction, Auxiliary}");
    Check(Dump(Attributes(1, 2)) == "{NeedsDestruction, Auxiliary}");
}

void BracketsEachEnumPrintWithBeginAndEnd() {
    const CellAttributes attributes = Attributes(1, 2);
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, AsStream(stream));
    Check((stream.formats == std::vector<std::string>{"%s", "%s", "%s", "%s", "%s"}));
    Check((stream.events == std::vector<std::string>{
        "begin", "vprintf", "begin", "vprintf", "end", "vprintf", "begin", "vprintf", "end", "vprintf", "end"}));
}

void PrintsToTheStreamBeginReturns() {
    const CellAttributes attributes = Attributes(0, 1);
    CaptureStream target{CaptureVtable + 2};
    CaptureStream outer{ForwardingVtable + 2, &target};
    _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, AsStream(outer));
    Check(target.text == "{DoesNotNeedDestruction, JSCellWithIndexingHeader}");
    Check((target.events == std::vector<std::string>{
        "vprintf", "begin", "vprintf", "end", "vprintf", "begin", "vprintf", "end", "vprintf"}));
    Check((outer.events == std::vector<std::string>{"begin", "end"}));
    Check(outer.text.empty());
}

void RejectsUnknownEnumValues() {
    for (const auto destruction : {std::int8_t{2}, std::int8_t{-1}, std::int8_t{127}}) {
        const CellAttributes attributes = Attributes(destruction, 0);
        CaptureStream stream{CaptureVtable + 2};
        Check(Throws<std::invalid_argument>([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, AsStream(stream)); }));
        Check(stream.text == "{");
        Check((stream.events == std::vector<std::string>{"begin", "vprintf"}));
    }
    for (const auto kind : {std::int8_t{3}, std::int8_t{-1}, std::int8_t{-128}}) {
        const CellAttributes attributes = Attributes(1, kind);
        CaptureStream stream{CaptureVtable + 2};
        Check(Throws<std::invalid_argument>([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, AsStream(stream)); }));
        Check(stream.text == "{NeedsDestruction, ");
    }
}

void PropagatesExceptionsWithoutCallingEnd() {
    const CellAttributes attributes = Attributes(0, 0);
    CaptureStream stream{ThrowingVtable + 2};
    Check(Throws<std::runtime_error>([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, AsStream(stream)); }));
    Check((stream.events == std::vector<std::string>{"begin"}));
}

void RejectsInvalidAttributesAndStreams() {
    const CellAttributes attributes = Attributes(0, 0);
    CaptureStream stream{CaptureVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(nullptr, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, nullptr); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, AsStream(noVtable)); }));
    CaptureStream noBegin{MissingBeginVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, AsStream(noBegin)); }));
    CaptureStream noEnd{MissingEndVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(&attributes, AsStream(noEnd)); }));
    Check(stream.events.empty() && noBegin.events.empty() && noEnd.events.empty());
}

}  // namespace

int main() {
    DumpsEveryModeAndKind();
    BracketsEachEnumPrintWithBeginAndEnd();
    PrintsToTheStreamBeginReturns();
    RejectsUnknownEnumValues();
    PropagatesExceptionsWithoutCallingEnd();
    RejectsInvalidAttributesAndStreams();
    std::puts("JscCellAttributesDump tests passed");
    return 0;
}
