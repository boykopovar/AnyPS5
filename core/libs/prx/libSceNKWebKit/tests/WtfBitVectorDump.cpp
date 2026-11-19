#include "prx/libSceNKWebKit/include/WtfBitVectorDumpExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Jsc::WtfAbi::PrintStream;
using WTF::BitVector;
using OutOfLineBits = WTF::BitVector::OutOfLineBits;

constexpr std::uintptr_t InlineFlag = std::uintptr_t{1} << 63;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WtfBitVectorDump check failed at line %u\n", location.line());
        std::abort();
    }
}

struct CaptureStream {
    const void* const* vtable;
    std::string text;
    std::vector<std::string> formats;
};

void APS5_VABI CaptureVprintf(PrintStream* stream, const char* format, VaList* args) {
    auto* capture = reinterpret_cast<CaptureStream*>(stream);
    char buffer[16];
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

std::string Expected(std::size_t size, std::initializer_list<std::size_t> setBits) {
    std::string text(size, '-');
    for (std::size_t bit : setBits) text[bit] = '1';
    return text;
}

struct alignas(8) OutOfLineStorage {
    OutOfLineBits header;
    std::uintptr_t words[3];
};

BitVector OutOfLine(OutOfLineStorage& storage) {
    return BitVector{reinterpret_cast<std::uintptr_t>(&storage.header) >> 1};
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

void DumpsEveryInlineBitInOrder() {
    CaptureStream stream{CaptureVtable + 2};
    const BitVector vector{InlineFlag | 0b1011 | (std::uintptr_t{1} << 40) | (std::uintptr_t{1} << 62)};
    _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&vector, AsStream(stream));
    Check(stream.text == Expected(63, {0, 1, 3, 40, 62}));
    Check(stream.formats.size() == 63);
    Check(stream.formats[0] == "1" && stream.formats[2] == "-");
}

void DumpsAnEmptyInlineVectorAsSixtyThreeDashes() {
    CaptureStream stream{CaptureVtable + 2};
    const BitVector vector{InlineFlag};
    _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&vector, AsStream(stream));
    Check(stream.text == std::string(63, '-'));
}

void DumpsOutOfLineBitsAcrossWords() {
    OutOfLineStorage storage{{128}, {std::uintptr_t{1} | (std::uintptr_t{1} << 63), std::uintptr_t{1} << 2, ~std::uintptr_t{0}}};
    const BitVector vector = OutOfLine(storage);
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&vector, AsStream(stream));
    Check(stream.text == Expected(128, {0, 63, 66}));
    Check(stream.formats.size() == 128);
}

void DumpsOnlyTheStoredBitCount() {
    OutOfLineStorage storage{{192}, {0, 0, std::uintptr_t{1} << 63}};
    const BitVector vector = OutOfLine(storage);
    CaptureStream stream{CaptureVtable + 2};
    _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&vector, AsStream(stream));
    Check(stream.text == Expected(192, {191}));
    storage.header.numBits = 64;
    CaptureStream shorter{CaptureVtable + 2};
    _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&vector, AsStream(shorter));
    Check(shorter.text == std::string(64, '-'));
}

void PropagatesExceptionsFromTheVirtualVprintf() {
    CaptureStream stream{ThrowingVtable + 2};
    const BitVector vector{InlineFlag};
    Check(Throws<std::runtime_error>([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&vector, AsStream(stream)); }));
}

void RejectsInvalidVectorsAndStreams() {
    CaptureStream stream{CaptureVtable + 2};
    const BitVector inlineVector{InlineFlag};
    const BitVector empty{0};
    const BitVector deleted{1};
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(nullptr, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&empty, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&deleted, AsStream(stream)); }));
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&inlineVector, nullptr); }));
    CaptureStream noVtable{nullptr};
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&inlineVector, AsStream(noVtable)); }));
    CaptureStream noVprintf{MissingVprintfVtable + 2};
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(&inlineVector, AsStream(noVprintf)); }));
    Check(stream.text.empty() && noVtable.text.empty() && noVprintf.text.empty());
}

}  // namespace

int main() {
    DumpsEveryInlineBitInOrder();
    DumpsAnEmptyInlineVectorAsSixtyThreeDashes();
    DumpsOutOfLineBitsAcrossWords();
    DumpsOnlyTheStoredBitCount();
    PropagatesExceptionsFromTheVirtualVprintf();
    RejectsInvalidVectorsAndStreams();
    std::puts("WtfBitVectorDump tests passed");
    return 0;
}
