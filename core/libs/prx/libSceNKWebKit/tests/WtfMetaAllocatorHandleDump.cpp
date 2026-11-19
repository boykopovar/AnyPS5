#include "prx/libSceNKWebKit/include/WtfMetaAllocatorHandleDumpExports.hpp"
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
using NKWebKit::WtfDumpAbi::MetaAllocatorHandle;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WtfMetaAllocatorHandleDump check failed at line %u\n", location.line());
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
    char buffer[64];
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

std::string Pointer(const void* value) {
    char buffer[64];
    const int written = snprintf_nid_postfix(buffer, sizeof(buffer), "%p", value);
    Check(written > 0 && static_cast<std::size_t>(written) < sizeof(buffer));
    return std::string(buffer, static_cast<std::size_t>(written));
}

MetaAllocatorHandle Handle(std::uintptr_t start, std::uintptr_t end) {
    MetaAllocatorHandle handle{};
    handle.refCount = 1;
    handle.parentAndRed = 1;
    handle.start = reinterpret_cast<void*>(start);
    handle.end = reinterpret_cast<void*>(end);
    return handle;
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

void DumpsStartAndEndAsRawPointers() {
    const MetaAllocatorHandle handle = Handle(0x7f001000, 0x7f003000);
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, AsStream(stream));
    Check(stream.text == Pointer(handle.start) + "..." + Pointer(handle.end));
    Check(stream.text.find("7f001000") != std::string::npos);
    Check(stream.text.find("7f003000") != std::string::npos);
    Check((stream.formats == std::vector<std::string>{"%p", "%s", "%p"}));
    Check((stream.events == std::vector<std::string>{"begin", "vprintf", "vprintf", "vprintf", "end"}));
}

void DumpsAnEmptyRange() {
    const MetaAllocatorHandle handle = Handle(0x4000, 0x4000);
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, AsStream(stream));
    Check(stream.text == Pointer(handle.start) + "..." + Pointer(handle.start));
}

void PrintsToTheStreamBeginReturns() {
    const MetaAllocatorHandle handle = Handle(0x10000, 0x18000);
    CaptureStream target{CaptureVtable + 2};
    CaptureStream outer{ForwardingVtable + 2, &target};
    _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, AsStream(outer));
    Check(target.text == Pointer(handle.start) + "..." + Pointer(handle.end));
    Check((target.events == std::vector<std::string>{"vprintf", "vprintf", "vprintf"}));
    Check((outer.events == std::vector<std::string>{"begin", "end"}));
    Check(outer.text.empty());
}

void PropagatesExceptionsWithoutCallingEnd() {
    const MetaAllocatorHandle handle = Handle(0x1000, 0x2000);
    CaptureStream stream{ThrowingVtable + 2};
    Check(Throws<std::runtime_error>([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, AsStream(stream)); }));
    Check((stream.events == std::vector<std::string>{"begin"}));
}

void RejectsInvalidHandlesAndStreams() {
    const MetaAllocatorHandle handle = Handle(0x1000, 0x2000);
    CaptureStream stream{CaptureVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(nullptr, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, nullptr); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, AsStream(noVtable)); }));
    CaptureStream noBegin{MissingBeginVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, AsStream(noBegin)); }));
    CaptureStream noEnd{MissingEndVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(&handle, AsStream(noEnd)); }));
    Check(stream.events.empty() && noBegin.events.empty() && noEnd.events.empty());
}

}  // namespace

int main() {
    DumpsStartAndEndAsRawPointers();
    DumpsAnEmptyRange();
    PrintsToTheStreamBeginReturns();
    PropagatesExceptionsWithoutCallingEnd();
    RejectsInvalidHandlesAndStreams();
    std::puts("WtfMetaAllocatorHandleDump tests passed");
    return 0;
}
