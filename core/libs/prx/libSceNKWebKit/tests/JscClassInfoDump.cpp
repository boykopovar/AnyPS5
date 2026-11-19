#include "prx/libSceNKWebKit/include/JscClassInfoDumpExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Jsc::WtfAbi::PrintStream;
using NKWebKit::JscClassInfoAbi::ClassInfoHead;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "JscClassInfoDump check failed at line %u\n", location.line());
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

void DumpsAddressAndClassName() {
    const ClassInfoHead parent{"Object", nullptr};
    const ClassInfoHead info{"Window", &parent};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, AsStream(stream));
    Check(stream.text == Pointer(&info) + ":[Window]");
    Check((stream.formats == std::vector<std::string>{"%p", "%s", "%s", "%s"}));
    Check((stream.events == std::vector<std::string>{"begin", "vprintf", "vprintf", "vprintf", "vprintf", "end"}));
}

void DumpsAnEmptyClassName() {
    const ClassInfoHead info{"", nullptr};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, AsStream(stream));
    Check(stream.text == Pointer(&info) + ":[]");
}

void PassesClassNameAsDataNotFormat() {
    const ClassInfoHead info{"%s%n", nullptr};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, AsStream(stream));
    Check(stream.text == Pointer(&info) + ":[%s%n]");
}

void PrintsToTheStreamBeginReturns() {
    const ClassInfoHead info{"JSArray", nullptr};
    CaptureStream target{CaptureVtable + 2};
    CaptureStream outer{ForwardingVtable + 2, &target};
    _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, AsStream(outer));
    Check(target.text == Pointer(&info) + ":[JSArray]");
    Check((target.events == std::vector<std::string>{"vprintf", "vprintf", "vprintf", "vprintf"}));
    Check((outer.events == std::vector<std::string>{"begin", "end"}));
    Check(outer.text.empty());
}

void PropagatesExceptionsWithoutCallingEnd() {
    const ClassInfoHead info{"Window", nullptr};
    CaptureStream stream{ThrowingVtable + 2};
    Check(Throws<std::runtime_error>([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, AsStream(stream)); }));
    Check((stream.events == std::vector<std::string>{"begin"}));
}

void RejectsInvalidClassInfoAndStreams() {
    const ClassInfoHead info{"Window", nullptr};
    const ClassInfoHead unnamed{nullptr, nullptr};
    CaptureStream stream{CaptureVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(nullptr, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&unnamed, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, nullptr); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, AsStream(noVtable)); }));
    CaptureStream noBegin{MissingBeginVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, AsStream(noBegin)); }));
    CaptureStream noEnd{MissingEndVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(&info, AsStream(noEnd)); }));
    Check(stream.events.empty() && noBegin.events.empty() && noEnd.events.empty());
}

}  // namespace

int main() {
    DumpsAddressAndClassName();
    DumpsAnEmptyClassName();
    PassesClassNameAsDataNotFormat();
    PrintsToTheStreamBeginReturns();
    PropagatesExceptionsWithoutCallingEnd();
    RejectsInvalidClassInfoAndStreams();
    std::puts("JscClassInfoDump tests passed");
    return 0;
}
