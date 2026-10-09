#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using Relinker::UnusedNidFilter::BuildRelativeRelocationIndex;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void put16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    for (std::size_t i = 0; i < 2; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}

void put64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}

constexpr std::uint64_t kBaseVaddr = 0x400000;
constexpr std::uint64_t kPhOff = 64;
constexpr std::uint64_t kDynOff = 176;
constexpr std::uint64_t kRelaOff = 224;
constexpr std::uint64_t kFileSize = 248;
constexpr std::uint64_t kSlotVaddr = 0x401000;
constexpr std::uint64_t kTargetVaddr = 0x402000;
constexpr std::int64_t kDtRela = 7;
constexpr std::int64_t kDtRelaSz = 8;
constexpr std::int64_t kDtNull = 0;
constexpr std::int64_t kDtOsRela = 0x6100002f;
constexpr std::int64_t kDtOsRelaSz = 0x61000031;
constexpr std::uint64_t kRX8664Relative = 8;

std::vector<std::uint8_t> buildElf(bool useOsTag) {
    std::vector<std::uint8_t> bytes(kFileSize, 0);
    bytes[0] = 0x7f; bytes[1] = 'E'; bytes[2] = 'L'; bytes[3] = 'F'; bytes[4] = 2;
    put64(bytes, 32, kPhOff);
    put16(bytes, 54, 56);
    put16(bytes, 56, 2);

    put32(bytes, kPhOff + 0, 1);
    put64(bytes, kPhOff + 8, 0);
    put64(bytes, kPhOff + 16, kBaseVaddr);
    put64(bytes, kPhOff + 32, kFileSize);

    const std::size_t dynamicPh = kPhOff + 56;
    put32(bytes, dynamicPh + 0, 2);
    put64(bytes, dynamicPh + 8, kDynOff);
    put64(bytes, dynamicPh + 16, kBaseVaddr + kDynOff);
    put64(bytes, dynamicPh + 32, 48);

    std::size_t entry = kDynOff;
    if (useOsTag) {
        put64(bytes, entry, static_cast<std::uint64_t>(kDtOsRela)); put64(bytes, entry + 8, kRelaOff); entry += 16;
        put64(bytes, entry, static_cast<std::uint64_t>(kDtOsRelaSz)); put64(bytes, entry + 8, 24); entry += 16;
    } else {
        put64(bytes, entry, static_cast<std::uint64_t>(kDtRela)); put64(bytes, entry + 8, kBaseVaddr + kRelaOff); entry += 16;
        put64(bytes, entry, static_cast<std::uint64_t>(kDtRelaSz)); put64(bytes, entry + 8, 24); entry += 16;
    }
    put64(bytes, entry, static_cast<std::uint64_t>(kDtNull)); put64(bytes, entry + 8, 0);

    put64(bytes, kRelaOff + 0, kSlotVaddr);
    put64(bytes, kRelaOff + 8, kRX8664Relative);
    put64(bytes, kRelaOff + 16, kTargetVaddr);
    return bytes;
}

}

int main() {
    try {
        {
            const auto index = BuildRelativeRelocationIndex(buildElf(false));
            const auto target = index->TargetOfSlot(kSlotVaddr);
            require(target.has_value() && *target == kTargetVaddr, "standard DT_RELA relative relocation was not indexed");
        }
        {
            const auto index = BuildRelativeRelocationIndex(buildElf(true));
            const auto target = index->TargetOfSlot(kSlotVaddr);
            require(target.has_value() && *target == kTargetVaddr,
                "DT_OS_RELA relative relocation was not indexed: the OS-specific tag value is a file offset, not a virtual address");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
