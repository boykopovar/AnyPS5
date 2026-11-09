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
constexpr std::size_t kGnuHashTag = 0;
constexpr std::int64_t kDtGnuHash = 0x6FFFFEF5;
constexpr std::uint64_t kGnuHashOffset = kCodeOffset + 0x600;
constexpr std::uint64_t kBogusOffset = 0xFFFFFFFFFFFFFFFEull;

void WriteTag(Bytes& image, const std::size_t index, const std::int64_t tag, const std::uint64_t value) {
    Write<std::int64_t>(image, kTagBase + index * kTagStride, tag);
    Write<std::uint64_t>(image, kTagBase + index * kTagStride + 8, value);
}

void WriteGnuHashHeader(Bytes& image, const std::uint32_t buckets, const std::uint32_t symndx, const std::uint32_t maskwords, const std::uint32_t shift2) {
    Write<std::uint32_t>(image, kGnuHashOffset, buckets);
    Write<std::uint32_t>(image, kGnuHashOffset + 4, symndx);
    Write<std::uint32_t>(image, kGnuHashOffset + 8, maskwords);
    Write<std::uint32_t>(image, kGnuHashOffset + 12, shift2);
    Write<std::uint64_t>(image, kGnuHashOffset + 16, 0x123456789ABCDEF0ull);
}

} // namespace

int main(const int argc, char** argv) {
    try {
        require(argc == 2, "usage: guest_relinker_gnu_hash_bloom <relinker>");
        const std::string binary = argv[1];
        const TempDirectory directory;
        const auto input = directory.Path() / "input.elf";
        const auto output = directory.Path() / "output.elf";

        {
            Bytes image = MakeExecutable(kCode);
            WriteGnuHashHeader(image, 0, 1, 1, 0);
            WriteTag(image, kGnuHashTag, kDtGnuHash, kDynamicVaddr + 0x600);
            WriteFile(input, image);

            const auto run = RunRelinker(binary, {"--skip-sce-module", "--to-intel", input.string(), output.string()}, directory.Path() / "relinker.log");
            require(run.ExitCode != 0, "Relinker accepted zero buckets and exited 0:\n" + run.Output);
        }

        {
            Bytes image = MakeExecutable(kCode);
            WriteGnuHashHeader(image, 2, 0, 1, 0);
            WriteTag(image, kGnuHashTag, kDtGnuHash, kDynamicVaddr + 0x600);
            WriteFile(input, image);

            const auto run = RunRelinker(binary, {"--skip-sce-module", "--to-intel", input.string(), output.string()}, directory.Path() / "relinker.log");
            require(run.ExitCode != 0, "Relinker accepted zero symndx and exited 0:\n" + run.Output);
        }

        {
            Bytes image = MakeExecutable(kCode);
            WriteGnuHashHeader(image, 2, 1, 0, 0);
            WriteTag(image, kGnuHashTag, kDtGnuHash, kDynamicVaddr + 0x600);
            WriteFile(input, image);

            const auto run = RunRelinker(binary, {"--skip-sce-module", "--to-intel", input.string(), output.string()}, directory.Path() / "relinker.log");
            require(run.ExitCode != 0, "Relinker accepted zero maskwords and exited 0:\n" + run.Output);
        }

        {
            Bytes image = MakeExecutable(kCode);
            WriteGnuHashHeader(image, 2, 1, 1, 0);
            WriteTag(image, kGnuHashTag, kDtGnuHash, kBogusOffset);
            WriteFile(input, image);

            const auto run = RunRelinker(binary, {"--skip-sce-module", "--to-intel", input.string(), output.string()}, directory.Path() / "relinker.log");
            require(run.ExitCode != 0, "Relinker accepted unmapped GNU hash table and exited 0:\n" + run.Output);
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "guest_relinker_gnu_hash_bloom: " << error.what() << '\n';
        return 1;
    }
}
