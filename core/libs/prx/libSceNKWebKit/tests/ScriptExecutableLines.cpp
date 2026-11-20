#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/ScriptExecutableAbi.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using namespace NKWebKit::JscAbi;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ScriptExecutableLines check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Cell {
    alignas(16) unsigned char bytes[0x80] = {};

    template <typename T>
    void Put(std::size_t offset, T value) {
        std::memcpy(bytes + offset, &value, sizeof(value));
    }
};

struct Unlinked {
    alignas(16) unsigned char bytes[0x20] = {};

    void Set(std::uint32_t lineCount, std::uint32_t bodyEndColumn, std::uint32_t highBits) {
        const std::uint32_t lineWord = (lineCount & kUnlinkedBitfieldMask) | (highBits << 31);
        const std::uint32_t columnWord = (bodyEndColumn & kUnlinkedBitfieldMask) | (highBits << 31);
        std::memcpy(bytes + kUnlinkedLineCountOffset, &lineWord, sizeof(lineWord));
        std::memcpy(bytes + kUnlinkedBodyEndColumnOffset, &columnWord, sizeof(columnWord));
    }
};

struct RareData {
    alignas(16) unsigned char bytes[0x20] = {};
};

static void Prepare(Cell& cell, std::uint8_t type, std::uint32_t firstLineZeroBased, std::uint32_t startColumnZeroBased) {
    cell.Put<std::uint8_t>(kCellTypeOffset, type);
    cell.Put<std::uint32_t>(kSourceFirstLineOffset, firstLineZeroBased);
    cell.Put<std::uint32_t>(kSourceStartColumnOffset, startColumnZeroBased);
}

static void TestGlobalTypes() {
    const std::uint8_t types[] = {0x0, 0x8, 0x9, 0xa, 0xb, 0xd, 0xff};
    for (std::uint8_t type : types) {
        Cell cell;
        Prepare(cell, type, 4, 7);
        cell.Put<std::uint32_t>(kGlobalExecutableLastLineOffset, 120);
        cell.Put<std::uint32_t>(kGlobalExecutableEndColumnOffset, 33);
        Require(_ZNK3JSC16ScriptExecutable8lastLineEv(&cell) == 120);
        Require(_ZNK3JSC16ScriptExecutable9endColumnEv(&cell) == 33);
    }

    Cell unparsed;
    Prepare(unparsed, 0x9, 0, 0);
    unparsed.Put<std::int32_t>(kGlobalExecutableLastLineOffset, -1);
    unparsed.Put<std::uint32_t>(kGlobalExecutableEndColumnOffset, 0xffffffffu);
    Require(_ZNK3JSC16ScriptExecutable8lastLineEv(&unparsed) == -1);
    Require(_ZNK3JSC16ScriptExecutable9endColumnEv(&unparsed) == 0xffffffffu);
}

static void TestFunctionRareData() {
    Cell cell;
    RareData rare;
    Prepare(cell, kFunctionExecutableType, 9, 2);
    const std::uint32_t lineCount = 5, endColumn = 41;
    std::memcpy(rare.bytes + kRareDataLineCountOffset, &lineCount, sizeof(lineCount));
    std::memcpy(rare.bytes + kRareDataEndColumnOffset, &endColumn, sizeof(endColumn));
    cell.Put<const void*>(kFunctionExecutableRareDataOffset, rare.bytes);
    cell.Put<const void*>(kFunctionExecutableUnlinkedOffset, nullptr);
    Require(_ZNK3JSC16ScriptExecutable8lastLineEv(&cell) == 9 + 1 + 5);
    Require(_ZNK3JSC16ScriptExecutable9endColumnEv(&cell) == 41);
}

static void TestFunctionUnlinked() {
    Cell cell;
    Unlinked unlinked;
    Prepare(cell, kFunctionExecutableType, 9, 2);
    cell.Put<const void*>(kFunctionExecutableRareDataOffset, nullptr);
    cell.Put<const void*>(kFunctionExecutableUnlinkedOffset, unlinked.bytes);

    unlinked.Set(6, 17, 1);
    Require(_ZNK3JSC16ScriptExecutable8lastLineEv(&cell) == 9 + 1 + 6);
    Require(_ZNK3JSC16ScriptExecutable9endColumnEv(&cell) == 17 + 1);

    unlinked.Set(0, 17, 0);
    Require(_ZNK3JSC16ScriptExecutable8lastLineEv(&cell) == 9 + 1);
    Require(_ZNK3JSC16ScriptExecutable9endColumnEv(&cell) == 17 + 2 + 1);
}

template <typename F>
static bool Throws(F function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void TestNull() {
    Require(Throws([] { _ZNK3JSC16ScriptExecutable8lastLineEv(nullptr); }));
    Require(Throws([] { _ZNK3JSC16ScriptExecutable9endColumnEv(nullptr); }));

    Cell cell;
    Prepare(cell, kFunctionExecutableType, 0, 0);
    Require(Throws([&] { _ZNK3JSC16ScriptExecutable8lastLineEv(&cell); }));
    Require(Throws([&] { _ZNK3JSC16ScriptExecutable9endColumnEv(&cell); }));
}

int main() {
    TestGlobalTypes();
    TestFunctionRareData();
    TestFunctionUnlinked();
    TestNull();
    std::puts("ScriptExecutableLines ok");
    return 0;
}
