#include "ElfFixture.hpp"
#include "RelinkerProcess.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace RelinkerTests;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

const Bytes kCode = {0xC3};

constexpr std::size_t kTagStride = 16;
constexpr std::size_t kTagBase = kCodeOffset + kDynamicVaddr;
constexpr std::size_t kEntryPointOffset = 24;
constexpr std::uint64_t kEntryVaddr = 0x100;
constexpr std::size_t kRelaTag = 4;
constexpr std::size_t kRelaSzTag = 5;
constexpr std::int64_t kDtRela = 7;
constexpr std::int64_t kDtRelaSz = 8;
constexpr std::uint64_t kImageSize = 0x8000;
constexpr std::uint64_t kRelaEntry = kCodeOffset + 0x700;

void WriteTag(Bytes& image, const std::size_t index, const std::int64_t tag, const std::uint64_t value) {
    Write<std::int64_t>(image, kTagBase + index * kTagStride, tag);
    Write<std::uint64_t>(image, kTagBase + index * kTagStride + 8, value);
}

Bytes MakeTlsRelocImage(const std::uint64_t relType, const std::int64_t addend) {
    Bytes image = MakeExecutable(kCode);
    Write<std::uint64_t>(image, kEntryPointOffset, kEntryVaddr);
    Write<std::uint8_t>(image, kCodeOffset + kEntryVaddr, 0xC3);
    WriteTag(image, kRelaTag, kDtRela, 0x700);
    WriteTag(image, kRelaSzTag, kDtRelaSz, 24);

    Write<std::uint64_t>(image, kRelaEntry, 0x1000);
    Write<std::uint64_t>(image, kRelaEntry + 8, (1ULL << 32) | relType);
    Write<std::int64_t>(image, kRelaEntry + 16, addend);
    return image;
}

} // namespace

int main(const int argc, char** argv) {
    try {
        require(argc == 2, "usage: guest_relinker_tls_types_tests <relinker>");
        const std::string binary = argv[1];

        const std::vector<std::string> flags = {"--windows", "--to-intel"};

        for (const auto& flag : flags) {
            const TempDirectory directory;
            const auto input = directory.Path() / "input.elf";
            const auto output = directory.Path() / "output.bin";

            Bytes imageDtpMod = MakeTlsRelocImage(16, 0);
            WriteFile(input, imageDtpMod);
            const auto runDtpMod = RunRelinker(binary, {"--skip-sce-module", flag, input.string(), output.string()}, directory.Path() / "relinker.log");
            std::cout << "DTPMOD64 (" << flag << ") exit code: " << runDtpMod.ExitCode << "\n";

            Bytes imageDtpOff = MakeTlsRelocImage(17, 8);
            WriteFile(input, imageDtpOff);
            const auto runDtpOff = RunRelinker(binary, {"--skip-sce-module", flag, input.string(), output.string()}, directory.Path() / "relinker.log");
            std::cout << "DTPOFF64 (" << flag << ") exit code: " << runDtpOff.ExitCode << "\n";
        }

        std::cout << "GuestRelinkerTlsRelocTypes audit completed successfully.\n";
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
