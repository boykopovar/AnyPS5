#include "prx/libSceNKWebKit/include/WtfThreadDumpExports.hpp"
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

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WtfThreadDump check failed at line %u\n", location.line());
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

template <typename TException, typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

void Dump(const void* thread, PrintStream* out) {
    _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(thread, out);
}

void DumpsThreadPrefixAndRawPointer() {
    alignas(16) unsigned char thread[64]{};
    CaptureStream stream{CaptureVtable + 2};
    Dump(thread, AsStream(stream));
    Check(stream.text == "Thread:" + Pointer(thread));
    Check((stream.formats == std::vector<std::string>{"%s", "%p"}));
    Check((stream.events == std::vector<std::string>{"begin", "vprintf", "vprintf", "end"}));
}

void PrintsTheAddressNotTheContents() {
    const void* thread = reinterpret_cast<const void*>(std::uintptr_t{0x7f0012345670});
    CaptureStream stream{CaptureVtable + 2};
    Dump(thread, AsStream(stream));
    Check(stream.text == "Thread:" + Pointer(thread));
    Check(stream.text.find("7f0012345670") != std::string::npos);
}

void PrintsANullThreadPointer() {
    CaptureStream stream{CaptureVtable + 2};
    Dump(nullptr, AsStream(stream));
    Check(stream.text == "Thread:" + Pointer(nullptr));
}

void PrintsToTheStreamBeginReturns() {
    alignas(16) unsigned char thread[64]{};
    CaptureStream target{CaptureVtable + 2};
    CaptureStream outer{ForwardingVtable + 2, &target};
    Dump(thread, AsStream(outer));
    Check(target.text == "Thread:" + Pointer(thread));
    Check((target.events == std::vector<std::string>{"vprintf", "vprintf"}));
    Check((outer.events == std::vector<std::string>{"begin", "end"}));
    Check(outer.text.empty());
}

void PropagatesExceptionsWithoutCallingEnd() {
    alignas(16) unsigned char thread[64]{};
    CaptureStream stream{ThrowingVtable + 2};
    Check(Throws<std::runtime_error>([&] { Dump(thread, AsStream(stream)); }));
    Check((stream.events == std::vector<std::string>{"begin"}));
}

void RejectsInvalidStreams() {
    alignas(16) unsigned char thread[64]{};
    Check(Throws<std::invalid_argument>([&] { Dump(thread, nullptr); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { Dump(thread, AsStream(noVtable)); }));
    CaptureStream noBegin{MissingBeginVtable + 2};
    Check(Throws<std::invalid_argument>([&] { Dump(thread, AsStream(noBegin)); }));
    CaptureStream noEnd{MissingEndVtable + 2};
    Check(Throws<std::invalid_argument>([&] { Dump(thread, AsStream(noEnd)); }));
    Check(noBegin.events.empty() && noEnd.events.empty());
}

}  // namespace

int main() {
    DumpsThreadPrefixAndRawPointer();
    PrintsTheAddressNotTheContents();
    PrintsANullThreadPointer();
    PrintsToTheStreamBeginReturns();
    PropagatesExceptionsWithoutCallingEnd();
    RejectsInvalidStreams();
    std::puts("WtfThreadDump tests passed");
    return 0;
}
