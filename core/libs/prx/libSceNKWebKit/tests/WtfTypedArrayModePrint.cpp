#include "prx/libSceNKWebKit/include/WtfTypedArrayModePrintExports.hpp"
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
        std::fprintf(stderr, "WtfTypedArrayModePrint check failed at line %u\n", location.line());
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

template <typename TException, typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

void Print(PrintStream* out, std::uint32_t mode) {
    _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(out, mode);
}

std::string Printed(std::uint32_t mode) {
    CaptureStream stream{CaptureVtable + 2};
    Print(AsStream(stream), mode);
    Check((stream.formats == std::vector<std::string>{"%s"}));
    Check((stream.events == std::vector<std::string>{"begin", "vprintf", "end"}));
    return stream.text;
}

void PrintsEveryModeName() {
    Check(Printed(0) == "FastTypedArray");
    Check(Printed(1) == "OversizeTypedArray");
    Check(Printed(2) == "WastefulTypedArray");
    Check(Printed(3) == "DataViewMode");
}

void PrintsToTheStreamBeginReturns() {
    CaptureStream target{CaptureVtable + 2};
    CaptureStream outer{ForwardingVtable + 2, &target};
    Print(AsStream(outer), 3);
    Check(target.text == "DataViewMode");
    Check((target.events == std::vector<std::string>{"vprintf"}));
    Check((outer.events == std::vector<std::string>{"begin", "end"}));
    Check(outer.text.empty());
}

void RejectsModesOutsideTheEnum() {
    CaptureStream stream{CaptureVtable + 2};
    Check(Throws<std::out_of_range>([&] { Print(AsStream(stream), 4); }));
    Check(Throws<std::out_of_range>([&] { Print(AsStream(stream), UINT32_MAX); }));
    Check(stream.events.empty() && stream.text.empty());
}

void PropagatesExceptionsWithoutCallingEnd() {
    CaptureStream stream{ThrowingVtable + 2};
    Check(Throws<std::runtime_error>([&] { Print(AsStream(stream), 0); }));
    Check((stream.events == std::vector<std::string>{"begin"}));
}

void RejectsInvalidStreams() {
    Check(Throws<std::invalid_argument>([] { Print(nullptr, 0); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { Print(AsStream(noVtable), 0); }));
    CaptureStream noBegin{MissingBeginVtable + 2};
    Check(Throws<std::invalid_argument>([&] { Print(AsStream(noBegin), 0); }));
    CaptureStream noEnd{MissingEndVtable + 2};
    Check(Throws<std::invalid_argument>([&] { Print(AsStream(noEnd), 0); }));
    Check(noBegin.events.empty() && noEnd.events.empty());
}

}  // namespace

int main() {
    PrintsEveryModeName();
    PrintsToTheStreamBeginReturns();
    RejectsModesOutsideTheEnum();
    PropagatesExceptionsWithoutCallingEnd();
    RejectsInvalidStreams();
    std::puts("WtfTypedArrayModePrint tests passed");
    return 0;
}
