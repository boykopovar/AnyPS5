#include "ElfFixture.hpp"
#include "RelinkerProcess.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <string>

namespace {

using namespace RelinkerTests;
using namespace Testing;

const Bytes kCode = {0xC3};

constexpr std::size_t kTagStride = 16;
constexpr std::size_t kTagBase = kCodeOffset + kDynamicVaddr;
constexpr std::size_t kSymTabTag = 2;
constexpr std::size_t kRelaSzTag = 5;
constexpr std::int64_t kDtOsSymtab = 0x61000039;
constexpr std::int64_t kDtRelaSz = 8;
constexpr std::uint64_t kBogusOffset = 0xFFFFFFFFFFFFFFFEull;
constexpr std::size_t kRelaEntry = kCodeOffset + 0x700;

void WriteTag(Bytes& image, const std::size_t index, const std::int64_t tag, const std::uint64_t value) {
    Write<std::int64_t>(image, kTagBase + index * kTagStride, tag);
    Write<std::uint64_t>(image, kTagBase + index * kTagStride + 8, value);
}

void WriteRelaEntry(Bytes& image, const std::uint64_t info) {
    Write<std::uint64_t>(image, kRelaEntry, 0);
    Write<std::uint64_t>(image, kRelaEntry + 8, info);
    Write<std::uint64_t>(image, kRelaEntry + 16, 0);
}

RelinkerRun RelinkWithBogusSymbolTable() {
    const TemporaryDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / "output.elf";
    Bytes image = MakeExecutable(kCode);
    WriteRelaEntry(image, (1ull << 32) | 6ull);
    WriteTag(image, kRelaSzTag, kDtRelaSz, 24);
    WriteTag(image, kSymTabTag, kDtOsSymtab, kBogusOffset);
    WriteFile(input, image);
    return RunRelinker(RequireArgument(0, "relinker"), {"--skip-sce-module", "--to-intel", input.string(), output.string()},
                       directory.Path() / "relinker.log");
}

const Case symbolTableOutsideFileFails{"Relinker_SymbolTableOffsetNearUint64Max_ExitsWithError", [] {
    const auto run = RelinkWithBogusSymbolTable();

    Require(run.ExitCode != 0, "Relinker read a symbol table outside the file and exited 0:\n" + run.Output);
}};

const Case symbolTableOutsideFileReported{"Relinker_SymbolTableOffsetNearUint64Max_ReportsEntryOutOfBounds", [] {
    const auto run = RelinkWithBogusSymbolTable();

    Require(run.Output.find("Symbol table entry out of bounds") != std::string::npos,
            "Relinker did not report the symbol table entry as out of bounds:\n" + run.Output);
}};

} // namespace
