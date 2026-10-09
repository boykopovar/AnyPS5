#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>
#include <elfpatcher/general/ElfConstants.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <vector>

namespace {

using Elfpatcher::DT_JMPREL;
using Elfpatcher::DT_NULL;
using Elfpatcher::DT_PLTRELSZ;
using Elfpatcher::DT_RELA;
using Elfpatcher::DT_RELASZ;
using Elfpatcher::PT_DYNAMIC;
using Elfpatcher::PT_LOAD;
using Elfpatcher::R_X86_64_RELATIVE;
using Relinker::UnusedNidFilter::BuildRelativeRelocationIndex;

constexpr std::size_t FileSize = 512;
constexpr std::size_t PhdrOffset = 64;
constexpr std::size_t PhdrEntrySize = 56;
constexpr std::size_t LoadFileOffset = 256;
constexpr std::uint64_t LoadFileSize = 64;
constexpr std::uint64_t LoadVaddr = 0x1000;
constexpr std::size_t DynamicFileOffset = 320;
constexpr std::uint64_t DynamicFileSize = 48;
constexpr std::uint64_t RelaEntrySize = 24;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void requireFailure(const std::function<void()>& operation, const char* message) {
    try {
        operation();
    } catch (const Relinker::RelinkerException&) {
        return;
    }
    throw std::runtime_error(message);
}

template<typename TValue>
void write(std::vector<std::uint8_t>& bytes, std::size_t offset, TValue value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("Test fixture write is out of bounds");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void writeRelocation(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t slot, std::uint64_t target) {
    write<std::uint64_t>(bytes, offset, slot);
    write<std::uint64_t>(bytes, offset + 8, R_X86_64_RELATIVE);
    write<std::uint64_t>(bytes, offset + 16, target);
}

std::vector<std::uint8_t> fixture(std::int64_t addressTag, std::uint64_t tableVaddr, std::int64_t sizeTag, std::uint64_t tableSize) {
    std::vector<std::uint8_t> bytes(FileSize, 0);
    bytes[0] = 0x7f;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    write<std::uint64_t>(bytes, 32, static_cast<std::uint64_t>(PhdrOffset));
    write<std::uint16_t>(bytes, 54, static_cast<std::uint16_t>(PhdrEntrySize));
    write<std::uint16_t>(bytes, 56, 2);

    write<std::uint32_t>(bytes, PhdrOffset, PT_LOAD);
    write<std::uint64_t>(bytes, PhdrOffset + 8, static_cast<std::uint64_t>(LoadFileOffset));
    write<std::uint64_t>(bytes, PhdrOffset + 16, LoadVaddr);
    write<std::uint64_t>(bytes, PhdrOffset + 32, LoadFileSize);

    const std::size_t dynamicPhdr = PhdrOffset + PhdrEntrySize;
    write<std::uint32_t>(bytes, dynamicPhdr, PT_DYNAMIC);
    write<std::uint64_t>(bytes, dynamicPhdr + 8, static_cast<std::uint64_t>(DynamicFileOffset));
    write<std::uint64_t>(bytes, dynamicPhdr + 16, 0x4000);
    write<std::uint64_t>(bytes, dynamicPhdr + 32, DynamicFileSize);

    writeRelocation(bytes, LoadFileOffset, 0x2000, 0x3000);
    writeRelocation(bytes, LoadFileOffset + RelaEntrySize, 0x2008, 0x3008);

    write<std::int64_t>(bytes, DynamicFileOffset, addressTag);
    write<std::uint64_t>(bytes, DynamicFileOffset + 8, tableVaddr);
    write<std::int64_t>(bytes, DynamicFileOffset + 16, sizeTag);
    write<std::uint64_t>(bytes, DynamicFileOffset + 24, tableSize);
    write<std::int64_t>(bytes, DynamicFileOffset + 32, DT_NULL);

    return bytes;
}

std::vector<std::uint8_t> relaFixture(std::uint64_t tableSize) {
    return fixture(DT_RELA, LoadVaddr, DT_RELASZ, tableSize);
}

void tableInsideSegmentIsIndexed() {
    const auto index = BuildRelativeRelocationIndex(relaFixture(2 * RelaEntrySize));
    require(index->TargetOfSlot(0x2000) == 0x3000, "First relative relocation was not indexed");
    require(index->TargetOfSlot(0x2008) == 0x3008, "Second relative relocation was not indexed");
    require(!index->TargetOfSlot(0x2010).has_value(), "An absent slot reported a target");
}

void tableBeyondSegmentIsRejected() {
    const auto beyond = LoadFileSize + 2 * RelaEntrySize;
    require(LoadFileOffset + beyond <= FileSize, "Fixture must keep the overlong table inside the file");
    requireFailure([&] { BuildRelativeRelocationIndex(relaFixture(beyond)); },
        "A relocation table extending past its PT_LOAD was accepted");
}

void unalignedTableSizeIsRejected() {
    requireFailure([&] { BuildRelativeRelocationIndex(relaFixture(RelaEntrySize + 12)); },
        "A relocation table size that is not a multiple of the entry size was accepted");
}

void hugeTableSizeIsRejected() {
    requireFailure([&] { BuildRelativeRelocationIndex(relaFixture(RelaEntrySize * 1000000000000000ull)); },
        "A relocation table size near the end of the address space was accepted");
}

void jmprelTableBeyondSegmentIsRejected() {
    const auto beyond = LoadFileSize + 2 * RelaEntrySize;
    requireFailure([&] { BuildRelativeRelocationIndex(fixture(DT_JMPREL, LoadVaddr, DT_PLTRELSZ, beyond)); },
        "A PLT relocation table extending past its PT_LOAD was accepted");
}

void tableOutsideEverySegmentIsRejected() {
    requireFailure([&] { BuildRelativeRelocationIndex(fixture(DT_RELA, 0x900000, DT_RELASZ, RelaEntrySize)); },
        "A relocation table outside every PT_LOAD was accepted");
}

void segmentBeyondFileIsRejected() {
    auto bytes = relaFixture(RelaEntrySize);
    write<std::uint64_t>(bytes, PhdrOffset + 32, FileSize);
    requireFailure([&] { BuildRelativeRelocationIndex(bytes); },
        "A PT_LOAD extending past the input file was accepted");
}

void dynamicSegmentBeyondFileIsRejected() {
    auto bytes = relaFixture(RelaEntrySize);
    write<std::uint64_t>(bytes, PhdrOffset + PhdrEntrySize + 32, FileSize);
    requireFailure([&] { BuildRelativeRelocationIndex(bytes); },
        "A PT_DYNAMIC extending past the input file was accepted");
}

}

int main() {
    try {
        tableInsideSegmentIsIndexed();
        tableBeyondSegmentIsRejected();
        unalignedTableSizeIsRejected();
        hugeTableSizeIsRejected();
        jmprelTableBeyondSegmentIsRejected();
        tableOutsideEverySegmentIsRejected();
        segmentBeyondFileIsRejected();
        dynamicSegmentBeyondFileIsRejected();
        std::puts("Relocation table bounds tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
