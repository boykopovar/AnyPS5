#include "prx/libScePfs/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <limits>
#include <vector>
#include <cstddef>
#include <cstdint>
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

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

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

bool FindIndexEntry(const void* header, std::uint16_t indexCount, std::uint64_t type, PfsIndexEntry& out, bool& found) {
    found = false;
    const auto* entries = IndexEntries(header);
    for (std::uint16_t i = 0; i < indexCount; ++i) {
        PfsIndexEntry entry{};
        if (!CopyIn(&entry, entries + i, sizeof(entry))) return false;
        if ((entry.words[0] & IndexEntryTypeMask) == type) {
            out = entry;
            found = true;
            return true;
        }
    }
    return true;
}


constexpr std::uint64_t HalfBlock = 0x20000;
constexpr std::uint64_t FullBlock = 2 * HalfBlock;

template<class T>
T* AtOffset(T* base, std::uint64_t offset) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    if (offset > UINTPTR_MAX - address) return nullptr;
    return reinterpret_cast<T*>(address + offset);
}

struct BlockCommand {
    std::uint64_t encodedSize, fileOffset, extent, first, second, index;

    bool Version3() const { return (extent >> 63) != 0; }
    bool SingleStoredHalf() const { return !Version3() && extent == HalfBlock && encodedSize == HalfBlock; }
    std::uint64_t LogicalSize() const {
        if (Version3()) return extent & INT64_MAX;
        return SingleStoredHalf() ? HalfBlock : FullBlock;
    }
    std::uint64_t LogicalOffset() const { return Version3() ? second & OffsetMask44 : index * FullBlock; }
};

static_assert(sizeof(BlockCommand) == 48);

int CopyStoredBlock(const void* source, void* destination, const BlockCommand& block, std::uint64_t size,
                    std::uint64_t offset, void* work, std::vector<std::uint8_t>& bytes, const char* function) {
    if (!block.Version3() && block.index > (UINT64_MAX - FullBlock) / FullBlock) return OutOfRange;
    const auto start = block.LogicalOffset();
    const auto length = block.LogicalSize();
    if (length > FullBlock || size > UINT64_MAX - offset) return OutOfRange;
    const auto begin = std::max(start, offset);
    const auto end = std::min(start + length, offset + size);
    if (end < begin || (block.SingleStoredHalf() && size != 0 && offset == start + length)) return OutOfRange;

    if (block.Version3()) {
        const auto firstSize = ((block.second >> 44) & 0x1ffff) + 1;
        const bool firstStored = (block.first & (1ULL << 51)) != 0;
        const bool secondStored = length <= HalfBlock || (block.first & (1ULL << 55)) != 0;
        if (!firstStored || !secondStored || ((block.first >> 44) & 0xf) != 0) {
            NotImplemented_nid_no_patch(function);
        }
        if (firstSize != std::min(length, HalfBlock) || block.encodedSize != length) return InvalidTable;
    } else if (block.extent != HalfBlock || (block.encodedSize != HalfBlock && block.encodedSize != FullBlock)) {
        NotImplemented_nid_no_patch(function);
    }

    const auto copySize = static_cast<std::size_t>(end - begin);
    bytes.resize(block.encodedSize);
    if (!CopyIn(bytes.data(), source, block.encodedSize) || !CopyOut(work, bytes.data(), block.encodedSize)) return InvalidArgument;
    if (copySize && !CopyOut(AtOffset(destination, begin - offset), bytes.data() + (begin - start), copySize)) return InvalidArgument;
    return 0;
}

int FirstFileBlock(const void* table, std::uint64_t entries, std::uint64_t offset, std::uint64_t& index) {
    std::uint64_t low = 0, high = entries;
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        PfsIndexEntry entry{};
        if (!CopyIn(&entry, AtOffset(table, mid * sizeof(entry)), sizeof(entry))) return InvalidArgument;
        if ((entry.words[1] & OffsetMask44) <= offset) low = mid + 1;
        else high = mid;
    }
    if (!low || low >= entries) return InvalidTable;
    index = low - 1;
    return 0;
}

}

extern "C" {

int APS5_VABI scePfsGetBlockTableReadCommand(const void* header, void* command) {
    if (!header || !command) return InvalidArgument;
    PfsHeaderLayout layout{};
    if (!CopyIn(&layout, header, sizeof(layout))) return InvalidArgument;
    const std::uint16_t version = layout.version;
    if ((version & 0xfffe) != 2) return UnsupportedFormat;
    if (version == 3) {
        if (!layout.indexCount) return InvalidTable;
        PfsIndexEntry index{};
        bool found = false;
        if (!FindIndexEntry(header, layout.indexCount, 3, index, found)) return InvalidArgument;
        if (!found) return InvalidTable;
        const std::uint64_t fields[2] = {index.words[1] >> 0x10, index.words[0] >> 0x10};
        return CopyOut(command, fields, sizeof(fields)) ? 0 : InvalidArgument;
    }
    if (version != 2) return UnsupportedFormat;
    if (layout.layoutTag != 2) return UnsupportedLayout;
    const std::uint64_t fields[2] = {static_cast<std::uint64_t>(layout.blockCount + 1) << 3, layout.tableOffset};
    return CopyOut(command, fields, sizeof(fields)) ? 0 : InvalidArgument;
}

int APS5_VABI scePfsGetHeaderReadCommand(void* command) {
    if (!command) return InvalidArgument;
    const std::uint64_t fields[6] = {0x1000, 0, 0, 0, 0, 0};
    return CopyOut(command, fields, sizeof(fields)) ? 0 : InvalidArgument;
}

int APS5_VABI scePfsGetPreadBlockRange(std::uint64_t size, std::uint64_t offset, std::uint32_t* firstBlockOut, std::uint32_t* lastBlockOut, const void* header, const void* blockTable) {
    if (!firstBlockOut || !lastBlockOut || !header || !blockTable) return InvalidArgument;
    PfsHeaderLayout layout{};
    if (!CopyIn(&layout, header, sizeof(layout))) return InvalidArgument;
    const std::uint16_t version = layout.version;
    if ((version & 0xfffe) != 2) return UnsupportedFormat;
    const std::uint64_t total = layout.uncompressedSize;
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
            PfsIndexEntry entry{};
            if (!CopyIn(&entry, cursor, sizeof(entry))) return InvalidArgument;
            found = entry.words[1] & OffsetMask44;
            ++first;
            ++cursor;
        } while (found < offset);
        std::uint32_t last;
        std::uint64_t endFound = 0;
        if (end) {
            cursor = entries + lastIndex;
            last = static_cast<std::uint32_t>(lastIndex) - 1;
            do {
                PfsIndexEntry entry{};
                if (!CopyIn(&entry, cursor, sizeof(entry))) return InvalidArgument;
                endFound = entry.words[1] & OffsetMask44;
                ++last;
                ++cursor;
            } while (endFound < end);
        } else {
            last = 0xffffffff;
        }
        first -= found != offset;
        last -= endFound != offset;
        if (!CopyOut(firstBlockOut, &first, sizeof(first)) || !CopyOut(lastBlockOut, &last, sizeof(last))) return InvalidArgument;
        return 0;
    }
    if (layout.layoutTag != 2) return UnsupportedLayout;
    if (offset > total || size > total - offset) return OutOfRange;
    const std::uint64_t end = offset + size;
    const std::uint64_t firstIndex = offset >> 0x12;
    const std::uint64_t lastIndex = end ? (end - 1) >> 0x12 : 0xffffffffULL;
    if (firstIndex > 0xffffffffULL || lastIndex > 0xffffffffULL) return OutOfRange;
    const auto first = static_cast<std::uint32_t>(firstIndex);
    const auto last = static_cast<std::uint32_t>(lastIndex);
    if (!CopyOut(firstBlockOut, &first, sizeof(first)) || !CopyOut(lastBlockOut, &last, sizeof(last))) return InvalidArgument;
    return 0;
}

int APS5_VABI scePfsGetSingleBlockReadCommand(const void* header, const void* blockTable, int blockIndex, void* command) {
    if (!header || !blockTable || !command) return InvalidArgument;
    PfsHeaderLayout layout{};
    if (!CopyIn(&layout, header, sizeof(layout))) return InvalidArgument;
    std::uint64_t fields[6]{};
    const std::uint16_t version = layout.version;
    if ((version & 0xfffe) != 2) return UnsupportedFormat;
    if (version == 3) {
        PfsIndexEntry blocks{};
        bool foundBlocks = false;
        if (!FindIndexEntry(header, layout.indexCount, 3, blocks, foundBlocks)) return InvalidArgument;
        if (!foundBlocks) {
            if (blockIndex < 0) return OutOfRange;
            return InvalidTable;
        }
        if (blockIndex < 0) return OutOfRange;
        const std::uint64_t bound = blocks.words[1] >> 0x14;
        if (bound - 1 <= static_cast<std::uint64_t>(static_cast<std::int64_t>(blockIndex))) return OutOfRange;
        if (!layout.indexCount) return InvalidTable;
        PfsIndexEntry region{};
        bool foundRegion = false;
        if (!FindIndexEntry(header, layout.indexCount, 7, region, foundRegion)) return InvalidArgument;
        if (!foundRegion) return InvalidTable;
        PfsIndexEntry table[2]{};
        if (!CopyIn(table, static_cast<const PfsIndexEntry*>(blockTable) + blockIndex, sizeof(table))) return InvalidArgument;
        const std::uint64_t start = table[0].words[0] & OffsetMask44;
        fields[0] = (table[1].words[0] & OffsetMask44) - start;
        fields[1] = (region.words[0] >> 0x10) + start;
        fields[2] = ((table[1].words[1] & OffsetMask44) - (table[0].words[1] & OffsetMask44)) | 0x8000000000000000ULL;
        fields[3] = table[0].words[0];
        fields[4] = table[0].words[1];
        fields[5] = static_cast<std::uint64_t>(static_cast<std::int64_t>(blockIndex));
        return CopyOut(command, fields, sizeof(fields)) ? 0 : InvalidArgument;
    }
    if (version != 2) return UnsupportedFormat;
    if (layout.layoutTag != 2) return UnsupportedLayout;
    if (blockIndex < 0) return OutOfRange;
    const std::uint32_t count = layout.blockCount;
    if (count <= static_cast<std::uint32_t>(blockIndex)) return OutOfRange;
    const auto* table = static_cast<const std::uint64_t*>(blockTable);
    std::uint64_t pair[2]{};
    if (!CopyIn(pair, table + blockIndex * 2, sizeof(pair))) return InvalidArgument;
    const std::uint64_t start = pair[0] & OffsetMask48;
    const std::uint64_t next = pair[1] & OffsetMask48;
    fields[1] = start;
    fields[0] = next - start;
    fields[2] = next - start;
    fields[3] = pair[0];
    fields[4] = pair[1];
    if (static_cast<std::uint32_t>(blockIndex * 2 + 2) <= count) {
        std::uint64_t after = 0;
        if (!CopyIn(&after, table + blockIndex * 2 + 2, sizeof(after))) return InvalidArgument;
        fields[0] = (after & OffsetMask48) - start;
    }
    fields[5] = static_cast<std::uint64_t>(static_cast<std::int64_t>(blockIndex));
    return CopyOut(command, fields, sizeof(fields)) ? 0 : InvalidArgument;
}

int APS5_VABI scePfsGetUncompressedSize(const void* header, std::uint64_t* sizeOut) {
    if (!header || !sizeOut) return InvalidArgument;
    PfsHeaderLayout layout{};
    if (!CopyIn(&layout, header, sizeof(layout))) return InvalidArgument;
    if (layout.magic != PfsMagic) return UnsupportedFormat;
    const std::uint16_t version = layout.version;
    if (version == 3) {
        const std::uint64_t size = layout.uncompressedSize;
        return CopyOut(sizeOut, &size, sizeof(size)) ? 0 : InvalidArgument;
    }
    if (version != 2) return UnsupportedFormat;
    if (layout.layoutTag != 2) return UnsupportedLayout;
    const std::uint64_t size = layout.uncompressedSize;
    return CopyOut(sizeOut, &size, sizeof(size)) ? 0 : InvalidArgument;
}

std::int64_t APS5_VABI scePfsGetWorkBufferSize(void) {
    return 0xed0d8;
}

std::int64_t APS5_VABI scePfsPread(void* buffer, std::uint64_t size, std::uint64_t offset, const void* header, void* workBuffer) {
    if (buffer == nullptr) return InvalidArgument;
    if (size == 0) return 0;
    if (offset > OffsetMask48 || header == nullptr || workBuffer == nullptr) return InvalidArgument;
    PfsHeaderLayout layout{};
    if (!CopyIn(&layout, header, sizeof(layout))) return InvalidArgument;
    if (layout.magic != PfsMagic) return UnsupportedFormat;
    if (offset > layout.uncompressedSize || size > layout.uncompressedSize - offset) return OutOfRange;
    std::array<std::uint64_t, 2> tableCommand{};
    if (const int code = scePfsGetBlockTableReadCommand(header, tableCommand.data()); code != 0) return code;
    const void* table = AtOffset(header, tableCommand[1]);
    if (!table || !AtOffset(table, tableCommand[0])) return InvalidArgument;
    const bool version3 = layout.version == 3;
    auto blockCount = (std::uint64_t{layout.blockCount} + 1) / 2;
    std::uint64_t index = offset / FullBlock;
    std::uint64_t dataRegionSize = 0;
    if (version3) {
        PfsIndexEntry region{};
        bool foundRegion = false;
        if (!FindIndexEntry(header, layout.indexCount, 7, region, foundRegion)) return InvalidArgument;
        if (!foundRegion) return InvalidTable;
        dataRegionSize = region.words[1] >> 16;
        const auto boundaries = tableCommand[0] / sizeof(PfsIndexEntry);
        if (boundaries < 2 || tableCommand[0] % sizeof(PfsIndexEntry) != 0) return InvalidTable;
        blockCount = boundaries - 1;
        if (const int code = FirstFileBlock(table, boundaries, offset, index); code != 0) return code;
    }
    std::vector<std::uint8_t> bytes;
    auto position = offset;
    const auto end = offset + size;
    while (position < end) {
        if (index >= blockCount || index > std::numeric_limits<int>::max()) return InvalidTable;
        BlockCommand block{};
        const int commandResult = scePfsGetSingleBlockReadCommand(header, table, static_cast<int>(index), &block);
        if (commandResult != 0) return commandResult;
        if (version3) {
            const auto physicalOffset = block.first & OffsetMask44;
            if (physicalOffset > dataRegionSize || block.encodedSize > dataRegionSize - physicalOffset) return InvalidTable;
        }
        const auto blockStart = block.LogicalOffset();
        const auto length = block.LogicalSize();
        if (length > FullBlock || position < blockStart || position - blockStart >= length) return InvalidTable;
        const void* source = AtOffset(header, block.fileOffset);
        if (!source) return InvalidArgument;
        if (const int code = CopyStoredBlock(source, buffer, block, size, offset, workBuffer, bytes, __func__); code != 0) return code;
        position = std::min(blockStart + length, end);
        ++index;
    }
    return 0;
}

int APS5_VABI scePfsPreadDecompressBlock(const void* source, void* destination, const void* command,
                                       std::uint64_t size, std::uint64_t offset, void* workBuffer) {
    BlockCommand block{};
    if (!source || !destination || !workBuffer || !command || !CopyIn(&block, command, sizeof(block))) return InvalidArgument;
    std::vector<std::uint8_t> bytes;
    return CopyStoredBlock(source, destination, block, size, offset, workBuffer, bytes, __func__);
}

int APS5_VABI scePfsValidate(const void* header, int mode) {
    if (!header) return InvalidArgument;
    if (mode == 0) return 0;
    if (static_cast<std::uint32_t>(mode) > 2) return InvalidArgument;
    PfsHeaderLayout layout{};
    if (!CopyIn(&layout, header, sizeof(layout))) return InvalidArgument;
    if (layout.magic != PfsMagic) return UnsupportedFormat;
    const std::uint16_t version = layout.version;
    if (version == 2) {
        if (layout.layoutTag != 2) return UnsupportedLayout;
    } else if ((version & 0xfffe) != 2) {
        return UnsupportedFormat;
    }
    if (mode != 2) return 0;
    if (version == 3) {
        if (!layout.indexCount) return UnsupportedFormat;
        const auto* entries = IndexEntries(header);
        PfsIndexEntry blocks{};
        PfsIndexEntry region{};
        bool hasBlocks = false;
        bool hasRegion = false;
        for (std::uint16_t i = 0; i < layout.indexCount && !(hasBlocks && hasRegion); ++i) {
            PfsIndexEntry entry{};
            if (!CopyIn(&entry, entries + i, sizeof(entry))) return InvalidArgument;
            const std::uint64_t type = entry.words[0] & IndexEntryTypeMask;
            if (type == 3) {
                blocks = entry;
                hasBlocks = true;
            }
            if (type == 7) {
                region = entry;
                hasRegion = true;
            }
        }
        if (!hasBlocks || !hasRegion) return UnsupportedFormat;
        const std::uint32_t bound = static_cast<std::uint32_t>(blocks.words[1] >> 0x14);
        const auto* table = reinterpret_cast<const PfsIndexEntry*>(reinterpret_cast<const std::uint8_t*>(header) + (blocks.words[0] >> 0x10));
        const auto steps = static_cast<std::int32_t>(bound - 1u);
        const std::uint64_t count = steps > 0 ? static_cast<std::uint64_t>(steps) + 1 : 1;
        const std::uint64_t limit = region.words[1] >> 0x10;
        PfsIndexEntry previousEntry{};
        if (count > 1 && !CopyIn(&previousEntry, table, sizeof(previousEntry))) return InvalidArgument;
        for (std::uint64_t i = 1; i < count; ++i) {
            PfsIndexEntry currentEntry{};
            if (!CopyIn(&currentEntry, table + i, sizeof(currentEntry))) return InvalidArgument;
            const std::uint64_t previous = previousEntry.words[0] & OffsetMask44;
            const std::uint64_t span = (currentEntry.words[0] & OffsetMask44) - previous;
            if (span > 0x40000) return InvalidTable;
            const std::uint64_t flag = (previousEntry.words[1] >> 0x2c) & 0x1ffff;
            if (flag - span < 0xfffffffffffdffffULL) return InvalidTable;
            const std::uint64_t bSpan = (currentEntry.words[1] & OffsetMask44) - (previousEntry.words[1] & OffsetMask44);
            if (bSpan > 0x40000) return InvalidTable;
            if (previous > limit) return InvalidTable;
            previousEntry = currentEntry;
        }
        return 0;
    }
    if (layout.layoutTag != 2) return UnsupportedLayout;
    const std::int32_t count = static_cast<std::int32_t>(layout.blockCount);
    if (count <= 0) return 0;
    const auto* table = reinterpret_cast<const std::uint64_t*>(reinterpret_cast<const std::uint8_t*>(header) + layout.tableOffset);
    std::uint64_t previous = 0;
    for (std::int32_t i = 0; i < count; ++i) {
        std::uint64_t rawEntry = 0;
        if (!CopyIn(&rawEntry, table + i, sizeof(rawEntry))) return InvalidArgument;
        const std::uint64_t entry = rawEntry & OffsetMask48;
        if (entry > layout.uncompressedSize) return InvalidTable;
        if (entry - previous >= 0x20001) return InvalidTable;
        previous = entry;
    }
    return 0;
}

}
