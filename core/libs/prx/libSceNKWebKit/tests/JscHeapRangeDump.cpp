#include "prx/libSceNKWebKit/include/JscHeapRangeDumpExports.hpp"
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
using NKWebKit::DomJitAbi::HeapRange;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "JscHeapRangeDump check failed at line %u\n", location.line());
        std::abort();
    }
}

struct CaptureStream {
    const void* const* vtable;
    std::string text;
    std::vector<std::string> formats;
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

template <typename TException, typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

std::string Dump(const HeapRange& range) {
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, AsStream(stream));
    Check((stream.formats == std::vector<std::string>{"0x%x-0x%x"}));
    return stream.text;
}

void DumpsBeginAndEndAsLowercaseHex() {
    Check(Dump(HeapRange{0x1, 0x2b}) == "0x1-0x2b");
    Check(Dump(HeapRange{0xabc, 0xdef0}) == "0xabc-0xdef0");
}

void DumpsBottomAndTopRanges() {
    Check(Dump(HeapRange{0, 0}) == "0x0-0x0");
    Check(Dump(HeapRange{0, UINT16_MAX}) == "0x0-0xffff");
}

void DumpsTheDefaultConstructedNoneRange() {
    Check(Dump(HeapRange{UINT16_MAX, UINT16_MAX}) == "0xffff-0xffff");
}

void DumpsInvertedRangesUnchanged() {
    Check(Dump(HeapRange{0x20, 0x10}) == "0x20-0x10");
}

void ReadsOnlyTheFourByteRange() {
    alignas(4) const std::uint16_t words[3] = {0x12, 0x34, 0x56};
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(reinterpret_cast<const HeapRange*>(words), AsStream(stream));
    Check(stream.text == "0x12-0x34");
}

void PropagatesVprintfExceptions() {
    const HeapRange range{1, 2};
    CaptureStream stream{ThrowingVtable + 2};
    Check(Throws<std::runtime_error>([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, AsStream(stream)); }));
}

void RejectsInvalidArguments() {
    const HeapRange range{1, 2};
    CaptureStream stream{CaptureVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(nullptr, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, nullptr); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, AsStream(noVtable)); }));
    CaptureStream noVprintf{MissingVprintfVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(&range, AsStream(noVprintf)); }));
    Check(stream.text.empty());
}

}  // namespace

int main() {
    DumpsBeginAndEndAsLowercaseHex();
    DumpsBottomAndTopRanges();
    DumpsTheDefaultConstructedNoneRange();
    DumpsInvertedRangesUnchanged();
    ReadsOnlyTheFourByteRange();
    PropagatesVprintfExceptions();
    RejectsInvalidArguments();
    std::puts("JscHeapRangeDump tests passed");
    return 0;
}
