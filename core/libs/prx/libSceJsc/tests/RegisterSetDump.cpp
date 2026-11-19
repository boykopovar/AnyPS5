#include "prx/libSceJsc/include/RegisterSetDumpExports.hpp"
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

using Jsc::RegisterSetAbi::RegisterSet;
using Jsc::WtfAbi::PrintStream;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "RegisterSetDump check failed at line %u\n", location.line());
        std::abort();
    }
}

struct CaptureStream {
    const void* const* vtable;
    CaptureStream* target = nullptr;
    std::string text;
    std::vector<std::string> formats;
    std::vector<std::string> events;
    int depth = 0;
    int throwAfter = -1;
};

CaptureStream* AsCapture(PrintStream* stream) {
    return reinterpret_cast<CaptureStream*>(stream);
}

PrintStream* AsStream(CaptureStream& stream) {
    return reinterpret_cast<PrintStream*>(&stream);
}

void APS5_VABI CaptureVprintf(PrintStream* stream, const char* format, VaList* args) {
    auto* capture = AsCapture(stream);
    if (capture->throwAfter == 0) throw std::runtime_error("vprintf failed");
    if (capture->throwAfter > 0) --capture->throwAfter;
    char buffer[128];
    const int written = vsnprintf_nid_postfix(buffer, sizeof(buffer), format, args);
    Check(written >= 0 && static_cast<std::size_t>(written) < sizeof(buffer));
    capture->text.append(buffer, static_cast<std::size_t>(written));
    capture->formats.emplace_back(format);
    capture->events.emplace_back("vprintf");
}

PrintStream* APS5_VABI SelfBegin(PrintStream* stream) {
    auto* capture = AsCapture(stream);
    capture->events.emplace_back("begin");
    ++capture->depth;
    return stream;
}

PrintStream* APS5_VABI TargetBegin(PrintStream* stream) {
    auto* capture = AsCapture(stream);
    capture->events.emplace_back("begin");
    ++capture->depth;
    return AsStream(*capture->target);
}

void APS5_VABI RecordEnd(PrintStream* stream) {
    auto* capture = AsCapture(stream);
    capture->events.emplace_back("end");
    --capture->depth;
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

std::string Dump(std::uint64_t bits) {
    const RegisterSet set{bits};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(&set, AsStream(stream));
    Check(stream.depth == 0);
    for (const std::string& format : stream.formats) Check(format == "%s");
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

void DumpsAnEmptySet() {
    const RegisterSet set{0};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(&set, AsStream(stream));
    Check(stream.text == "[]");
    Check((stream.events == std::vector<std::string>{"begin", "vprintf", "end", "begin", "vprintf", "end"}));
}

void DumpsOneRegisterWithNestedPrints() {
    const RegisterSet set{1};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(&set, AsStream(stream));
    Check(stream.text == "[%rax]");
    const std::vector<std::string> expected{
        "begin", "vprintf", "end",
        "begin",
        "begin", "vprintf", "end",
        "begin", "begin", "vprintf", "vprintf", "end", "end",
        "end",
        "begin", "vprintf", "end",
    };
    Check(stream.events == expected);
}

void DumpsFirmwareRegisterSets() {
    Check(Dump(0x30) == "[%rsp, %rbp]");
    Check(Dump(0x800) == "[%r11]");
    Check(Dump(0xf028) == "[%rbx, %rbp, %r12, %r13, %r14, %r15]");
    Check(Dump(0x3c6) == "[%rcx, %rdx, %rsi, %rdi, %r8, %r9]");
    Check(Dump(0xffff) == "[%rax, %rcx, %rdx, %rbx, %rsp, %rbp, %rsi, %rdi, %r8, %r9, %r10, %r11, %r12, %r13, %r14, %r15]");
    Check(Dump(0xffff0000) ==
          "[%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7, %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14, %xmm15]");
}

void IgnoresBitsPastTheLastRegister() {
    Check(Dump(0x100000000ull) == "[]");
    Check(Dump(0xffffffff00000000ull | 0x10001) == "[%rax, %xmm0]");
}

void PrintsToTheStreamsBeginReturns() {
    const RegisterSet set{0x80010000};
    CaptureStream target{CaptureVtable + 2};
    CaptureStream outer{ForwardingVtable + 2, &target};
    _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(&set, AsStream(outer));
    Check(target.text == "[%xmm0, %xmm15]");
    Check(outer.text.empty());
    Check(outer.depth == 0 && target.depth == 0);
    Check((outer.events == std::vector<std::string>{"begin", "end", "begin", "end", "begin", "end", "begin", "end"}));
}

void PropagatesExceptionsWithoutCallingEnd() {
    const RegisterSet set{1};
    CaptureStream stream{CaptureVtable + 2};
    stream.throwAfter = 2;
    Check(Throws<std::runtime_error>([&] { _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(&set, AsStream(stream)); }));
    Check(stream.text == "[");
    Check(stream.depth == 3);
}

void RejectsInvalidArguments() {
    const RegisterSet set{1};
    CaptureStream stream{CaptureVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(nullptr, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(&set, nullptr); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(&set, AsStream(noVtable)); }));
    CaptureStream noBegin{MissingBeginVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(&set, AsStream(noBegin)); }));
    Check(stream.events.empty() && noBegin.events.empty());
}

}  // namespace

int main() {
    DumpsAnEmptySet();
    DumpsOneRegisterWithNestedPrints();
    DumpsFirmwareRegisterSets();
    IgnoresBitsPastTheLastRegister();
    PrintsToTheStreamsBeginReturns();
    PropagatesExceptionsWithoutCallingEnd();
    RejectsInvalidArguments();
    std::puts("RegisterSetDump tests passed");
    return 0;
}
