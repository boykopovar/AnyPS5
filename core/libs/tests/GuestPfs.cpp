#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libScePfs/libScePfs.hpp"
#include <array>
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
    Require(command[2] == (0x2a0 - 0x200) | 0x8000000000000000ULL);
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
    return 0;
}
