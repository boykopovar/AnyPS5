#include "ElfFixture.hpp"
#include "RelinkerProcess.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace RelinkerTests;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

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

}

int main(const int argc, char** argv) {
    try {
        require(argc == 2, "usage: dynamic_table_bounds_tests <relinker>");
        const std::string binary = argv[1];
        const TempDirectory directory;
        const auto input = directory.Path() / "input.elf";
        const auto output = directory.Path() / "output.elf";

        Bytes image = MakeExecutable(kCode);
        WriteRelaEntry(image, (1ull << 32) | 6ull);
        WriteTag(image, kRelaSzTag, kDtRelaSz, 24);
        WriteTag(image, kSymTabTag, kDtOsSymtab, kBogusOffset);
        WriteFile(input, image);

        const auto run = RunRelinker(binary, {"--skip-sce-module", "--to-intel", input.string(), output.string()}, directory.Path() / "relinker.log");
        require(run.ExitCode != 0, "Relinker read a symbol table outside the file and exited 0:\n" + run.Output);
        require(run.Output.find("Symbol table entry out of bounds") != std::string::npos,
                "Relinker did not report the symbol table entry as out of bounds:\n" + run.Output);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "dynamic_table_bounds_tests: " << error.what() << '\n';
        return 1;
    }
}
