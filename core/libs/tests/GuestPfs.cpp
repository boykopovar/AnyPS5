#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libScePfs/libScePfs.hpp"
#include <algorithm>
#include <array>
#include <vector>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int InvalidArgument = static_cast<int>(0x8a780001);
constexpr int UnsupportedFormat = static_cast<int>(0x8a780002);
constexpr int InvalidTable = static_cast<int>(0x8a780003);
constexpr int UnsupportedLayout = static_cast<int>(0x8a780004);
constexpr int OutOfRange = static_cast<int>(0x8a780005);

constexpr std::uint32_t PfsMagic = 0x43534650;

struct PfsHeader {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t indexCount;
    std::uint8_t reserved8[0xc];
    std::uint32_t blockCount;
    std::uint64_t uncompressedSize;
    std::uint8_t reserved20[8];
    std::uint8_t layoutTag;
    std::uint8_t reserved29[7];
    std::uint64_t tableOffset;
    std::uint8_t reserved38[0x10];
};

static_assert(sizeof(PfsHeader) == 0x48);

struct IndexEntry {
    std::uint64_t words[2];
};

#define Require(value) do { if (!(value)) { std::fprintf(stderr, "Require failed %s:%d\n", __FILE__, __LINE__); std::abort(); } } while (0)

template <typename T>
void Fill(T& object, std::uint8_t value) {
    std::memset(&object, value, sizeof(object));
}

void FillBytes(void* object, std::size_t bytes, std::uint8_t value) {
    std::memset(object, value, bytes);
}

PfsHeader HeaderV2(std::uint32_t blocks, std::uint64_t total, std::uint64_t tableOffset) {
    PfsHeader header{};
    Fill(header, 0xa5);
    header.magic = PfsMagic;
    header.version = 2;
    header.indexCount = 0;
    header.blockCount = blocks;
    header.uncompressedSize = total;
    header.layoutTag = 2;
    header.tableOffset = tableOffset;
    return header;
}

void UncompressedSize() {
    PfsHeader header = HeaderV2(4, 0x80000, 0x200);
    std::uint64_t size = 0;
    Require(scePfsGetUncompressedSize(nullptr, &size) == InvalidArgument);
    Require(scePfsGetUncompressedSize(&header, nullptr) == InvalidArgument);
    header.magic = 0;
    Require(scePfsGetUncompressedSize(&header, &size) == UnsupportedFormat);
    header = HeaderV2(4, 0x80000, 0x200);
    header.version = 4;
    Require(scePfsGetUncompressedSize(&header, &size) == UnsupportedFormat);
    header.version = 2;
    header.layoutTag = 3;
    Require(scePfsGetUncompressedSize(&header, &size) == UnsupportedLayout);
    header.layoutTag = 2;
    Require(scePfsGetUncompressedSize(&header, &size) == 0);
    Require(size == 0x80000);
    header.version = 3;
    header.layoutTag = 7;
    Require(scePfsGetUncompressedSize(&header, &size) == 0);
    Require(size == 0x80000);
}

void WorkBufferSize() {
    Require(scePfsGetWorkBufferSize() == 0xed0d8);
}

void Pread() {
    PfsHeader header = HeaderV2(4, 0x80000, 0x200);
    std::array<std::uint8_t, 0x1000> buffer{};
    std::array<std::uint8_t, 0x200> work{};
    FillBytes(buffer.data(), buffer.size(), 0xa5);
    errno = 71;
    Require(scePfsPread(nullptr, 0x1000, 0, &header, work.data()) == InvalidArgument);
    Require(scePfsPread(nullptr, 0, 0, nullptr, nullptr) == InvalidArgument);
    Require(scePfsPread(buffer.data(), 0, 0xffffffffffffffffULL, nullptr, nullptr) == 0);
    Require(scePfsPread(buffer.data(), 0x1000, 0x1000000000000ULL, &header, work.data()) == InvalidArgument);
    Require(scePfsPread(buffer.data(), 0x1000, 0, nullptr, work.data()) == InvalidArgument);
    Require(scePfsPread(buffer.data(), 0x1000, 0, &header, nullptr) == InvalidArgument);
    header.magic = 0;
    Require(scePfsPread(buffer.data(), 0x1000, 0, &header, work.data()) == UnsupportedFormat);
    header = HeaderV2(4, 0x80000, 0x200);
    Require(scePfsPread(buffer.data(), 0x2000, 0x7f000, &header, work.data()) == OutOfRange);
    Require(scePfsPread(buffer.data(), 0x1000, 0x90000, &header, work.data()) == OutOfRange);
    Require(scePfsPread(buffer.data(), 0xfffffffffffff000ULL, 0x1000, &header, work.data()) == OutOfRange);
    Require(errno == 71);
    for (std::uint8_t byte : buffer) Require(byte == 0xa5);
}

void StoredReads(std::uint16_t version, bool shortBlocks) {
    constexpr std::uint64_t half = 0x20000;
    std::array<std::uint64_t, 5> ends{0, 2 * half, 4 * half, 6 * half, 8 * half};
    if (shortBlocks && version == 2) ends.back() -= half;
    if (shortBlocks && version == 3) ends = {0, half, half + 1, 3 * half + 1, 4 * half + 6};
    const auto dataSize = ends.back();
    constexpr std::uint64_t tableOffset = 0x100;
    constexpr std::uint64_t dataOffset = 0x1000;
    std::vector<std::uint8_t> image(dataOffset + dataSize);
    std::vector<std::uint8_t> work(scePfsGetWorkBufferSize());
    const auto put = [&](std::size_t offset, const auto& value) {
        std::memcpy(image.data() + offset, &value, sizeof(value));
    };
    PfsHeader header{};
    header.magic = PfsMagic;
    header.version = version;
    header.blockCount = static_cast<std::uint32_t>(dataSize / half);
    header.uncompressedSize = dataSize;
    header.layoutTag = 2;
    header.tableOffset = tableOffset;
    if (version == 2) {
        for (unsigned i = 0; i <= header.blockCount; ++i) put(tableOffset + i * 8, dataOffset + i * half);
    } else {
        header.indexCount = 2;
        put(sizeof(header), IndexEntry{{(tableOffset << 16) | 3, 5ULL << 20}});
        put(sizeof(header) + sizeof(IndexEntry), IndexEntry{{(dataOffset << 16) | 7, dataSize << 16}});
        for (unsigned i = 0; i <= 4; ++i) {
            const auto start = ends[i];
            const auto firstSizeMinusOne = i == 4 ? 0 : std::min(ends[i + 1] - start, half) - 1;
            put(tableOffset + i * sizeof(IndexEntry), IndexEntry{{start | (1ULL << 51) | (1ULL << 55), start | (firstSizeMinusOne << 44)}});
        }
    }
    put(0, header);
    auto* data = image.data() + dataOffset;
    for (std::uint64_t i = 0; i < dataSize; ++i) data[i] = static_cast<std::uint8_t>(i * 13 + (i >> 8) * 901 + 7);

    struct Read { unsigned block; std::uint64_t offset, count; };
    const Read reads[] = {{0, 0, ends[1]}, {0, 3, 128}, {1, ends[1], 192},
                          {2, ends[2] - 5, 20}, {1, 0, ends[2]}, {3, ends[3], ends[4] - ends[3]}};
    for (const auto& read : reads) {
        std::array<std::uint64_t, 6> command{};
        Require(scePfsGetSingleBlockReadCommand(image.data(), image.data() + tableOffset, read.block, command.data()) == 0);
        std::vector<std::uint8_t> actual(read.count + 2, 0xb5);
        Require(scePfsPreadDecompressBlock(image.data() + command[1], actual.data() + 1, command.data(), read.count, read.offset, work.data()) == 0);
        const auto blockStart = ends[read.block];
        for (std::size_t i = 0; i < read.count; ++i) {
            const auto position = read.offset + i;
            Require(actual[i + 1] == (position >= blockStart && position < ends[read.block + 1] ? data[position] : 0xb5));
        }
        Require(actual.front() == 0xb5 && actual.back() == 0xb5);
        std::fill(actual.begin(), actual.end(), 0xb5);
        Require(scePfsPread(actual.data() + 1, read.count, read.offset, image.data(), work.data()) == 0);
        Require(std::memcmp(actual.data() + 1, data + read.offset, read.count) == 0);
        Require(actual.front() == 0xb5 && actual.back() == 0xb5);
    }
    if (version == 3) {
        std::array<std::uint64_t, 2> tableCommand{};
        Require(scePfsGetBlockTableReadCommand(image.data(), tableCommand.data()) == 0);
        Require(tableCommand[0] == 5 * sizeof(IndexEntry));
        const auto first = ends[2] - 1;
        std::vector<std::uint8_t> throughLast(dataSize - first);
        Require(scePfsPread(throughLast.data(), throughLast.size(), first, image.data(), work.data()) == 0);
        Require(std::memcmp(throughLast.data(), data + first, throughLast.size()) == 0);
        const auto regionIndex = sizeof(header) + sizeof(IndexEntry);
        put(regionIndex, IndexEntry{{(dataOffset << 16) | 7, (dataSize - 1) << 16}});
        std::uint8_t byte = 0xb5;
        Require(scePfsPread(&byte, 1, ends[3], image.data(), work.data()) == InvalidTable);
        Require(byte == 0xb5);
        put(regionIndex, IndexEntry{{(dataOffset << 16) | 7, dataSize << 16}});
        auto extended = header;
        ++extended.uncompressedSize;
        put(0, extended);
        std::array<std::uint8_t, 2> atEnd{};
        Require(scePfsPread(atEnd.data(), 2, dataSize - 1, image.data(), work.data()) == InvalidTable);
        put(0, header);
    }
    if (version == 2 && shortBlocks) {
        std::array<std::uint64_t, 6> command{};
        Require(scePfsGetSingleBlockReadCommand(image.data(), image.data() + tableOffset, 3, command.data()) == 0);
        std::array<std::uint8_t, 3> output{0xb5, 0xb5, 0xb5};
        const auto* last = image.data() + command[1];
        Require(scePfsPreadDecompressBlock(last, output.data(), command.data(), 3, dataSize - 1, work.data()) == 0);
        Require((output == std::array<std::uint8_t, 3>{data[dataSize - 1], 0xb5, 0xb5}));
        const auto saved = output;
        Require(scePfsPreadDecompressBlock(last, output.data(), command.data(), 1, dataSize, work.data()) == OutOfRange);
        Require(scePfsPreadDecompressBlock(last, output.data(), command.data(), 1, dataSize + 1, work.data()) == OutOfRange);
        Require(output == saved);
    }
}

void HeaderReadCommand() {
    std::array<std::uint64_t, 8> command{};
    FillBytes(command.data(), command.size() * 8, 0xa5);
    Require(scePfsGetHeaderReadCommand(nullptr) == InvalidArgument);
    Require(scePfsGetHeaderReadCommand(command.data()) == 0);
    Require(command[0] == 0x1000);
    for (std::size_t i = 1; i < 6; ++i) Require(command[i] == 0);
    Require(command[6] == 0xa5a5a5a5a5a5a5a5ULL);
    Require(command[7] == 0xa5a5a5a5a5a5a5a5ULL);
}

void BlockTableReadCommand() {
    PfsHeader header = HeaderV2(4, 0x80000, 0x2000);
    std::array<std::uint64_t, 6> command{};
    FillBytes(command.data(), sizeof(command), 0xa5);
    Require(scePfsGetBlockTableReadCommand(nullptr, command.data()) == InvalidArgument);
    Require(scePfsGetBlockTableReadCommand(&header, nullptr) == InvalidArgument);
    header.version = 4;
    Require(scePfsGetBlockTableReadCommand(&header, command.data()) == UnsupportedFormat);
    header.version = 2;
    header.layoutTag = 3;
    Require(scePfsGetBlockTableReadCommand(&header, command.data()) == UnsupportedLayout);
    header.layoutTag = 2;
    Require(scePfsGetBlockTableReadCommand(&header, command.data()) == 0);
    Require(command[0] == (4 + 1) * 8);
    Require(command[1] == 0x2000);
    Require(command[2] == 0xa5a5a5a5a5a5a5a5ULL);
    header.version = 3;
    header.indexCount = 0;
    Require(scePfsGetBlockTableReadCommand(&header, command.data()) == InvalidTable);
}

void BlockTableReadCommandV3() {
    struct Image {
        PfsHeader header;
        IndexEntry entries[3];
    } image{};
    FillBytes(&image, sizeof(image), 0xa5);
    image.header = HeaderV2(0, 0x80000, 0);
    image.header.version = 3;
    image.header.indexCount = 3;
    image.entries[0].words[0] = 1;
    image.entries[1].words[0] = (0x4500ULL << 0x10) | 3;
    image.entries[1].words[1] = 0x2300ULL << 0x10;
    image.entries[2].words[0] = 7;
    std::array<std::uint64_t, 6> command{};
    Require(scePfsGetBlockTableReadCommand(&image, command.data()) == 0);
    Require(command[0] == 0x2300);
    Require(command[1] == 0x4500);
    image.entries[1].words[0] = 5;
    Require(scePfsGetBlockTableReadCommand(&image, command.data()) == InvalidTable);
}

void PreadBlockRangeV2() {
    PfsHeader header = HeaderV2(4, 0x80000, 0x200);
    std::uint64_t table[5] = {0, 0x20000, 0x40000, 0x60000, 0x80000};
    std::uint32_t first = 0xaa, last = 0xaa;
    Require(scePfsGetPreadBlockRange(0x1000, 0, &first, &last, &header, table) == 0);
    Require(first == 0 && last == 0);
    Require(scePfsGetPreadBlockRange(0x1000, 0x30000, &first, &last, &header, table) == 0);
    Require(first == 0 && last == 0);
    Require(scePfsGetPreadBlockRange(0x2000, 0x3f000, &first, &last, &header, table) == 0);
    Require(first == 0 && last == 1);
    Require(scePfsGetPreadBlockRange(0x40000, 0x40000, &first, &last, &header, table) == 0);
    Require(first == 1 && last == 1);
    Require(scePfsGetPreadBlockRange(0x1, 0x7ffff, &first, &last, &header, table) == 0);
    Require(first == 1 && last == 1);
    Require(scePfsGetPreadBlockRange(0x80000, 0, &first, &last, &header, table) == 0);
    Require(first == 0 && last == 1);
    Require(scePfsGetPreadBlockRange(0x80001, 0, &first, &last, &header, table) == OutOfRange);
    Require(scePfsGetPreadBlockRange(0x1000, 0x80000, &first, &last, &header, table) == OutOfRange);
    Require(scePfsGetPreadBlockRange(0x40001, 0x40000, &first, &last, &header, table) == OutOfRange);
    header.uncompressedSize = 0xffffffffffffffffULL;
    Require(scePfsGetPreadBlockRange(2, 0xfffffffffffffffeULL, &first, &last, &header, table) == OutOfRange);
    Require(scePfsGetPreadBlockRange(0x40000, 0xfffffffffffe0000ULL, &first, &last, &header, table) == OutOfRange);
    Require(scePfsGetPreadBlockRange(0, 0xffffffffffffffffULL, &first, &last, &header, table) == OutOfRange);
    header.uncompressedSize = 0x10000000000000ULL;
    Require(scePfsGetPreadBlockRange(0x10000000000000ULL, 0, &first, &last, &header, table) == OutOfRange);
    header.uncompressedSize = 0x4000000000000ULL;
    Require(scePfsGetPreadBlockRange(0x4000000000000ULL, 0, &first, &last, &header, table) == 0);
    Require(first == 0 && last == 0xffffffff);
    Require(scePfsGetPreadBlockRange(0, 0x4000000000000ULL, &first, &last, &header, table) == OutOfRange);
    header.uncompressedSize = 0x3fffffffc0000ULL;
    Require(scePfsGetPreadBlockRange(0, 0x3fffffffc0000ULL, &first, &last, &header, table) == 0);
    Require(first == 0xffffffff && last == 0xfffffffe);
    header.uncompressedSize = 0x80000;
    Require(scePfsGetPreadBlockRange(0x1000, 0, nullptr, &last, &header, table) == InvalidArgument);
    Require(scePfsGetPreadBlockRange(0x1000, 0, &first, nullptr, &header, table) == InvalidArgument);
    Require(scePfsGetPreadBlockRange(0x1000, 0, &first, &last, nullptr, table) == InvalidArgument);
    Require(scePfsGetPreadBlockRange(0x1000, 0, &first, &last, &header, nullptr) == InvalidArgument);
    header.version = 4;
    Require(scePfsGetPreadBlockRange(0x1000, 0, &first, &last, &header, table) == UnsupportedFormat);
    header.version = 2;
    header.layoutTag = 3;
    Require(scePfsGetPreadBlockRange(0x1000, 0, &first, &last, &header, table) == UnsupportedLayout);
}

void PreadBlockRangeV3() {
    PfsHeader header = HeaderV2(0, 0x80000, 0);
    header.version = 3;
    IndexEntry table[4]{};
    table[0].words[1] = 0;
    table[1].words[1] = 0x20000;
    table[2].words[1] = 0x40000;
    table[3].words[1] = 0x60000;
    std::uint32_t first = 0xaa, last = 0xaa;
    Require(scePfsGetPreadBlockRange(0x1000, 0, &first, &last, &header, table) == 0);
    Require(first == 0 && last == 0);
    Require(scePfsGetPreadBlockRange(0x1000, 0x20000, &first, &last, &header, table) == 0);
    Require(first == 1 && last == 1);
    Require(scePfsGetPreadBlockRange(0x30001, 0x1ffff, &first, &last, &header, table) == 0);
    Require(first == 0 && last == 2);
    Require(scePfsGetPreadBlockRange(0x80001, 0, &first, &last, &header, table) == OutOfRange);
    header.uncompressedSize = 0xffffffffffffffffULL;
    Require(scePfsGetPreadBlockRange(2, 0xfffffffffffffffeULL, &first, &last, &header, table) == OutOfRange);
    Require(scePfsGetPreadBlockRange(0x40000, 0xfffffffffffe0000ULL, &first, &last, &header, table) == OutOfRange);
    header.uncompressedSize = 0x10000000000000ULL;
    Require(scePfsGetPreadBlockRange(0x10000000000000ULL, 0, &first, &last, &header, table) == OutOfRange);
}

void SingleBlockReadCommandV2() {
    PfsHeader header = HeaderV2(4, 0x80000, 0x200);
    std::uint64_t table[8] = {0, 0x18000, 0x20000, 0x38000, 0x40000, 0x58000, 0x60000, 0x78000};
    std::array<std::uint64_t, 6> command{};
    FillBytes(command.data(), sizeof(command), 0xa5);
    Require(scePfsGetSingleBlockReadCommand(nullptr, table, 0, command.data()) == InvalidArgument);
    Require(scePfsGetSingleBlockReadCommand(&header, nullptr, 0, command.data()) == InvalidArgument);
    Require(scePfsGetSingleBlockReadCommand(&header, table, 0, nullptr) == InvalidArgument);
    Require(scePfsGetSingleBlockReadCommand(&header, table, -1, command.data()) == OutOfRange);
    Require(scePfsGetSingleBlockReadCommand(&header, table, 4, command.data()) == OutOfRange);
    header.layoutTag = 3;
    Require(scePfsGetSingleBlockReadCommand(&header, table, 0, command.data()) == UnsupportedLayout);
    header.layoutTag = 2;
    Require(scePfsGetSingleBlockReadCommand(&header, table, 1, command.data()) == 0);
    Require(command[0] == 0x40000 - 0x20000);
    Require(command[1] == 0x20000);
    Require(command[2] == 0x38000 - 0x20000);
    Require(command[3] == 0x20000);
    Require(command[4] == 0x38000);
    Require(command[5] == 1);
    Require(scePfsGetSingleBlockReadCommand(&header, table, 3, command.data()) == 0);
    Require(command[0] == 0x78000 - 0x60000);
    Require(command[1] == 0x60000);
    Require(command[5] == 3);
}

void SingleBlockReadCommandV3() {
    struct Image {
        PfsHeader header;
        IndexEntry entries[2];
    } image{};
    FillBytes(&image, sizeof(image), 0xa5);
    image.header = HeaderV2(0, 0x80000, 0);
    image.header.version = 3;
    image.header.indexCount = 2;
    image.entries[0].words[0] = (0x9000ULL << 0x10) | 3;
    image.entries[0].words[1] = 5ULL << 0x14;
    image.entries[1].words[0] = (0x60000ULL << 0x10) | 7;
    IndexEntry table[4]{};
    table[0].words[0] = 0x100;
    table[0].words[1] = 0x200;
    table[1].words[0] = 0x140;
    table[1].words[1] = 0x2a0;
    std::array<std::uint64_t, 6> command{};
    Require(scePfsGetSingleBlockReadCommand(&image, table, -1, command.data()) == OutOfRange);
    Require(scePfsGetSingleBlockReadCommand(&image, table, 0, command.data()) == 0);
    Require(command[0] == 0x140 - 0x100);
    Require(command[1] == 0x60000 + 0x100);
    Require(command[2] == ((0x2a0 - 0x200) | 0x8000000000000000ULL));
    Require(command[3] == 0x100);
    Require(command[4] == 0x200);
    Require(command[5] == 0);
    Require(scePfsGetSingleBlockReadCommand(&image, table, 4, command.data()) == OutOfRange);
    image.entries[0].words[0] = 9;
    Require(scePfsGetSingleBlockReadCommand(&image, table, 0, command.data()) == InvalidTable);
}

void Validate() {
    PfsHeader header = HeaderV2(4, 0x80000, 0x200);
    Require(scePfsValidate(nullptr, 2) == InvalidArgument);
    Require(scePfsValidate(nullptr, 0) == InvalidArgument);
    Require(scePfsValidate(&header, 0) == 0);
    Require(scePfsValidate(&header, 3) == InvalidArgument);
    Require(scePfsValidate(&header, -1) == InvalidArgument);
    header.magic = 0;
    Require(scePfsValidate(&header, 1) == UnsupportedFormat);
    header = HeaderV2(4, 0x80000, 0x200);
    header.layoutTag = 3;
    Require(scePfsValidate(&header, 1) == UnsupportedLayout);
    header.layoutTag = 2;
    Require(scePfsValidate(&header, 1) == 0);
}

void ValidateV2Table() {
    std::array<std::uint8_t, 0x200 + 8 * 5> image{};
    FillBytes(image.data(), image.size(), 0xa5);
    auto* header = reinterpret_cast<PfsHeader*>(image.data());
    *header = HeaderV2(4, 0x80000, 0x200);
    auto* entries = reinterpret_cast<std::uint64_t*>(image.data() + 0x200);
    entries[0] = 0;
    entries[1] = 0x20000;
    entries[2] = 0x40000;
    entries[3] = 0x60000;
    entries[4] = 0x80000;
    Require(scePfsValidate(header, 2) == 0);
    entries[2] = 0x60001;
    Require(scePfsValidate(header, 2) == InvalidTable);
    entries[2] = 0x40000;
    entries[3] = 0x80001;
    Require(scePfsValidate(header, 2) == InvalidTable);
    entries[3] = 0x60000;
    header->blockCount = 0;
    Require(scePfsValidate(header, 2) == 0);
}

void ValidateV3() {
    struct Image {
        PfsHeader header;
        IndexEntry index[2];
        IndexEntry table[4];
    } image{};
    FillBytes(&image, sizeof(image), 0xa5);
    image.header = HeaderV2(0, 0x100000, 0);
    image.header.version = 3;
    image.header.indexCount = 2;
    image.index[0].words[0] = (0x68ULL << 0x10) | 3;
    image.index[0].words[1] = 4ULL << 0x14;
    image.index[1].words[0] = 7;
    image.index[1].words[1] = 0x90000ULL << 0x10;
    for (int i = 0; i < 4; ++i) {
        image.table[i].words[0] = i * 0x30000;
        image.table[i].words[1] = static_cast<std::uint64_t>(i) * 0x30000 | (0x1ffffULL << 0x2c);
    }
    Require(scePfsValidate(&image, 2) == 0);
    image.table[2].words[0] = 0x30000 + 0x40001;
    Require(scePfsValidate(&image, 2) == InvalidTable);
    image.index[0].words[1] = 0x80000000ULL << 0x14;
    Require(scePfsValidate(&image, 2) == InvalidTable);
    for (const std::uint64_t bound : {0x80000001ULL, 0xffffffffULL, 0ULL, 1ULL}) {
        image.index[0].words[1] = bound << 0x14;
        Require(scePfsValidate(&image, 2) == 0);
    }
    image.index[0].words[1] = 4ULL << 0x14;
    image.table[2].words[0] = 0x60000;
    image.header.indexCount = 1;
    Require(scePfsValidate(&image, 2) == UnsupportedFormat);
    image.header.indexCount = 0;
    Require(scePfsValidate(&image, 2) == UnsupportedFormat);
}

}

int main() {
    UncompressedSize();
    WorkBufferSize();
    Pread();
    for (bool shortBlocks : {false, true}) {
        StoredReads(2, shortBlocks);
        StoredReads(3, shortBlocks);
    }
    HeaderReadCommand();
    BlockTableReadCommand();
    BlockTableReadCommandV3();
    PreadBlockRangeV2();
    PreadBlockRangeV3();
    SingleBlockReadCommandV2();
    SingleBlockReadCommandV3();
    Validate();
    ValidateV2Table();
    ValidateV3();
    {
        PfsHeader header = HeaderV2(4, 0x80000, 0x200);
        PfsHeader headerV3 = HeaderV2(0, 0x80000, 0);
        headerV3.version = 3;
        std::uint64_t table[5] = {0, 0x20000, 0x40000, 0x60000, 0x80000};
        std::array<std::uint64_t, 6> command{};
        std::array<std::uint8_t, 0x100> buffer{};
        std::array<std::uint8_t, 0x100> work{};
        std::uint64_t size = 0;
        std::uint32_t first = 0;
        std::uint32_t last = 0;
        void* bad = reinterpret_cast<void*>(std::uintptr_t{0x10});
        errno = 71;
        Require(scePfsGetHeaderReadCommand(bad) == InvalidArgument);
        Require(scePfsGetBlockTableReadCommand(bad, command.data()) == InvalidArgument);
        Require(scePfsGetBlockTableReadCommand(&header, bad) == InvalidArgument);
        Require(scePfsGetSingleBlockReadCommand(bad, table, 0, command.data()) == InvalidArgument);
        Require(scePfsGetSingleBlockReadCommand(&header, bad, 0, command.data()) == InvalidArgument);
        Require(scePfsGetSingleBlockReadCommand(&header, table, 0, bad) == InvalidArgument);
        Require(scePfsGetUncompressedSize(bad, &size) == InvalidArgument);
        Require(scePfsGetUncompressedSize(&header, reinterpret_cast<std::uint64_t*>(bad)) == InvalidArgument);
        Require(scePfsGetPreadBlockRange(0x1000, 0, reinterpret_cast<std::uint32_t*>(bad), &last, &header, table) == InvalidArgument);
        Require(scePfsGetPreadBlockRange(0x1000, 0, &first, reinterpret_cast<std::uint32_t*>(bad), &header, table) == InvalidArgument);
        Require(scePfsGetPreadBlockRange(0x1000, 0, &first, &last, bad, table) == InvalidArgument);
        Require(scePfsGetPreadBlockRange(0x1000, 0, &first, &last, &headerV3, bad) == InvalidArgument);
        Require(scePfsPread(buffer.data(), 0x100, 0, bad, work.data()) == InvalidArgument);
        Require(scePfsValidate(bad, 1) == InvalidArgument);
        Require(scePfsValidate(bad, 2) == InvalidArgument);
        Require(errno == 71);
    }
    return 0;
}
