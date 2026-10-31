#include "prx/libScePfs/include/Api.hpp"
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int InvalidArgument = static_cast<int>(0x8a780001);
constexpr int UnsupportedFormat = static_cast<int>(0x8a780002);
constexpr int InvalidTable = static_cast<int>(0x8a780003);
constexpr int UnsupportedLayout = static_cast<int>(0x8a780004);
constexpr int OutOfRange = static_cast<int>(0x8a780005);

constexpr std::uint32_t PfsMagic = 0x43534650;
constexpr std::uint64_t OffsetMask48 = 0xffffffffffffULL;
constexpr std::uint64_t OffsetMask44 = 0xfffffffffffULL;
constexpr std::uint64_t IndexEntryTypeMask = 0xffULL;

struct PfsHeaderLayout {
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

static_assert(sizeof(PfsHeaderLayout) == 0x48);

struct PfsIndexEntry {
    std::uint64_t words[2];
};

const PfsIndexEntry* IndexEntries(const void* header) {
    return reinterpret_cast<const PfsIndexEntry*>(static_cast<const std::uint8_t*>(header) + sizeof(PfsHeaderLayout));
}

const PfsIndexEntry* FindIndexEntry(const void* header, std::uint64_t type) {
    const auto* layout = static_cast<const PfsHeaderLayout*>(header);
    const auto* entries = IndexEntries(header);
    for (std::uint16_t i = 0; i < layout->indexCount; ++i) {
        if ((entries[i].words[0] & IndexEntryTypeMask) == type) return &entries[i];
    }
    return nullptr;
}

}

extern "C" {

int APS5_VABI scePfsGetBlockTableReadCommand(const void* header, void* command) {
    if (!header || !command) return InvalidArgument;
    const auto* layout = static_cast<const PfsHeaderLayout*>(header);
    auto* fields = static_cast<std::uint64_t*>(command);
    const std::uint16_t version = layout->version;
    if ((version & 0xfffe) != 2) return UnsupportedFormat;
    if (version == 3) {
        if (!layout->indexCount) return InvalidTable;
        const PfsIndexEntry* index = FindIndexEntry(header, 3);
        if (!index) return InvalidTable;
        fields[1] = index->words[0] >> 0x10;
        fields[0] = index->words[1] >> 0x10;
        return 0;
    }
    if (version != 2) return UnsupportedFormat;
    if (layout->layoutTag != 2) return UnsupportedLayout;
    fields[1] = layout->tableOffset;
    fields[0] = static_cast<std::uint64_t>(layout->blockCount + 1) << 3;
    return 0;
}

int APS5_VABI scePfsGetHeaderReadCommand(void* command) {
    if (!command) return InvalidArgument;
    auto* fields = static_cast<std::uint64_t*>(command);
    fields[0] = 0x1000;
    std::memset(fields + 1, 0, 0x28);
    return 0;
}

int APS5_VABI scePfsGetPreadBlockRange(std::uint64_t size, std::uint64_t offset, std::uint32_t* firstBlockOut, std::uint32_t* lastBlockOut, const void* header, const void* blockTable) {
    if (!firstBlockOut || !lastBlockOut || !header || !blockTable) return InvalidArgument;
    const auto* layout = static_cast<const PfsHeaderLayout*>(header);
    const std::uint16_t version = layout->version;
    if ((version & 0xfffe) != 2) return UnsupportedFormat;
    const std::uint64_t total = layout->uncompressedSize;
    if (version == 3) {
        if (offset > total || size > total - offset) return OutOfRange;
        const std::uint64_t end = offset + size;
        const std::uint64_t firstIndex = offset >> 0x12;
        const std::uint64_t lastIndex = end ? (end - 1) >> 0x12 : 0;
        if (firstIndex > 0xffffffffULL || lastIndex > 0xffffffffULL) return OutOfRange;
        const auto* entries = static_cast<const PfsIndexEntry*>(blockTable);
        const PfsIndexEntry* cursor = entries + firstIndex;
        std::uint32_t first = static_cast<std::uint32_t>(firstIndex) - 1;
        std::uint64_t found;
        do {
            found = cursor->words[1] & OffsetMask44;
            ++first;
            ++cursor;
        } while (found < offset);
        std::uint32_t last;
        std::uint64_t endFound = 0;
        if (end) {
            cursor = entries + lastIndex;
            last = static_cast<std::uint32_t>(lastIndex) - 1;
            do {
                endFound = cursor->words[1] & OffsetMask44;
                ++last;
                ++cursor;
            } while (endFound < end);
        } else {
            last = 0xffffffff;
        }
        first -= found != offset;
        last -= endFound != offset;
        *firstBlockOut = first;
        *lastBlockOut = last;
        return 0;
    }
    if (layout->layoutTag != 2) return UnsupportedLayout;
    if (offset > total || size > total - offset) return OutOfRange;
    const std::uint64_t end = offset + size;
    const std::uint64_t firstIndex = offset >> 0x12;
    const std::uint64_t lastIndex = end ? (end - 1) >> 0x12 : 0xffffffffULL;
    if (firstIndex > 0xffffffffULL || lastIndex > 0xffffffffULL) return OutOfRange;
    *firstBlockOut = static_cast<std::uint32_t>(firstIndex);
    *lastBlockOut = static_cast<std::uint32_t>(lastIndex);
    return 0;
}

int APS5_VABI scePfsGetSingleBlockReadCommand(const void* header, const void* blockTable, int blockIndex, void* command) {
    if (!header || !blockTable || !command) return InvalidArgument;
    const auto* layout = static_cast<const PfsHeaderLayout*>(header);
    auto* fields = static_cast<std::uint64_t*>(command);
    const std::uint16_t version = layout->version;
    if ((version & 0xfffe) != 2) return UnsupportedFormat;
    if (version == 3) {
        const PfsIndexEntry* blocks = FindIndexEntry(header, 3);
        if (!blocks) {
            if (blockIndex < 0) return OutOfRange;
            return InvalidTable;
        }
        if (blockIndex < 0) return OutOfRange;
        const std::uint64_t bound = blocks->words[1] >> 0x14;
        if (bound - 1 <= static_cast<std::uint64_t>(static_cast<std::int64_t>(blockIndex))) return OutOfRange;
        if (!layout->indexCount) return InvalidTable;
        const PfsIndexEntry* region = FindIndexEntry(header, 7);
        if (!region) return InvalidTable;
        const auto* table = static_cast<const PfsIndexEntry*>(blockTable) + blockIndex;
        const std::uint64_t start = table[0].words[0] & OffsetMask44;
        fields[0] = (table[1].words[0] & OffsetMask44) - start;
        fields[1] = (region->words[0] >> 0x10) + start;
        fields[2] = ((table[1].words[1] & OffsetMask44) - (table[0].words[1] & OffsetMask44)) | 0x8000000000000000ULL;
        fields[3] = table[0].words[0];
        fields[4] = table[0].words[1];
        fields[5] = static_cast<std::uint64_t>(static_cast<std::int64_t>(blockIndex));
        return 0;
    }
    if (version != 2) return UnsupportedFormat;
    if (layout->layoutTag != 2) return UnsupportedLayout;
    if (blockIndex < 0) return OutOfRange;
    const std::uint32_t count = layout->blockCount;
    if (count <= static_cast<std::uint32_t>(blockIndex)) return OutOfRange;
    const auto* table = static_cast<const std::uint64_t*>(blockTable);
    const std::uint64_t start = table[blockIndex * 2] & OffsetMask48;
    const std::uint64_t next = table[blockIndex * 2 + 1] & OffsetMask48;
    fields[1] = start;
    fields[0] = next - start;
    fields[2] = next - start;
    fields[3] = table[blockIndex * 2];
    fields[4] = table[blockIndex * 2 + 1];
    if (static_cast<std::uint32_t>(blockIndex * 2 + 2) <= count) {
        fields[0] = (table[blockIndex * 2 + 2] & OffsetMask48) - start;
    }
    fields[5] = static_cast<std::uint64_t>(static_cast<std::int64_t>(blockIndex));
    return 0;
}

int APS5_VABI scePfsGetUncompressedSize(const void* header, std::uint64_t* sizeOut) {
    if (!header || !sizeOut) return InvalidArgument;
    const auto* layout = static_cast<const PfsHeaderLayout*>(header);
    if (layout->magic != PfsMagic) return UnsupportedFormat;
    const std::uint16_t version = layout->version;
    if (version == 3) {
        *sizeOut = layout->uncompressedSize;
        return 0;
    }
    if (version != 2) return UnsupportedFormat;
    if (layout->layoutTag != 2) return UnsupportedLayout;
    *sizeOut = layout->uncompressedSize;
    return 0;
}

std::int64_t APS5_VABI scePfsGetWorkBufferSize(void) {
    return 0xed0d8;
}

std::int64_t APS5_VABI scePfsPread(void* buffer, std::uint64_t size, std::uint64_t offset, const void* header, void* workBuffer) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePfsPreadDecompressBlock() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePfsValidate(const void* header, int mode) {
    if (!header) return InvalidArgument;
    if (mode == 0) return 0;
    if (static_cast<std::uint32_t>(mode) > 2) return InvalidArgument;
    const auto* layout = static_cast<const PfsHeaderLayout*>(header);
    if (layout->magic != PfsMagic) return UnsupportedFormat;
    const std::uint16_t version = layout->version;
    if (version == 2) {
        if (layout->layoutTag != 2) return UnsupportedLayout;
    } else if ((version & 0xfffe) != 2) {
        return UnsupportedFormat;
    }
    if (mode != 2) return 0;
    if (version == 3) {
        if (!layout->indexCount) return UnsupportedFormat;
        const auto* entries = IndexEntries(header);
        const PfsIndexEntry* blocks = nullptr;
        const PfsIndexEntry* region = nullptr;
        for (std::uint16_t i = 0; i < layout->indexCount && !(blocks && region); ++i) {
            const std::uint64_t type = entries[i].words[0] & IndexEntryTypeMask;
            if (type == 3) blocks = &entries[i];
            if (type == 7) region = &entries[i];
        }
        if (!blocks || !region) return UnsupportedFormat;
        const std::uint32_t bound = static_cast<std::uint32_t>(blocks->words[1] >> 0x14);
        const auto* table = reinterpret_cast<const PfsIndexEntry*>(reinterpret_cast<const std::uint8_t*>(header) + (blocks->words[0] >> 0x10));
        const std::uint32_t count = bound >= 2 ? bound : 1;
        const std::uint64_t limit = region->words[1] >> 0x10;
        for (std::uint64_t i = 1; i < count; ++i) {
            const std::uint64_t previous = table[i - 1].words[0] & OffsetMask44;
            const std::uint64_t span = (table[i].words[0] & OffsetMask44) - previous;
            if (span > 0x40000) return InvalidTable;
            const std::uint64_t flag = (table[i - 1].words[1] >> 0x2c) & 0x1ffff;
            if (flag - span < 0xfffffffffffdffffULL) return InvalidTable;
            const std::uint64_t bSpan = (table[i].words[1] & OffsetMask44) - (table[i - 1].words[1] & OffsetMask44);
            if (bSpan > 0x40000) return InvalidTable;
            if (previous > limit) return InvalidTable;
        }
        return 0;
    }
    if (layout->layoutTag != 2) return UnsupportedLayout;
    const std::int32_t count = static_cast<std::int32_t>(layout->blockCount);
    if (count <= 0) return 0;
    const auto* table = reinterpret_cast<const std::uint64_t*>(reinterpret_cast<const std::uint8_t*>(header) + layout->tableOffset);
    std::uint64_t previous = 0;
    for (std::int32_t i = 0; i < count; ++i) {
        const std::uint64_t entry = table[i] & OffsetMask48;
        if (entry > layout->uncompressedSize) return InvalidTable;
        if (entry - previous >= 0x20001) return InvalidTable;
        previous = entry;
    }
    return 0;
}

}
