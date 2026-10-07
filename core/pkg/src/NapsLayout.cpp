#include <pkg/NapsLayout.hpp>
#include <pkg/ByteOrder.hpp>
#include <pkg/PackageError.hpp>
#include <algorithm>
#include <map>

namespace Pkg {

namespace {

constexpr std::uint32_t Half = 0x20000;
constexpr std::uint64_t Page = 0x40000;
constexpr std::uint32_t Modulo = 0x3ffff;
constexpr std::size_t RecordSize = 9;

struct Record {
    bool Run = false;
    std::uint32_t SourceModulo = 0;
    std::uint32_t MountModulo = 0;
    std::uint32_t EvenLength = 0;
    std::uint32_t Even = 0;
    std::uint32_t Odd = 0;
    std::uint32_t Predictor = 0;
    bool StoredFlag = false;
    std::uint32_t KeyIndex = 0;
    std::uint64_t SourceBase = 0;
};

struct Span {
    std::size_t Record;
    std::size_t Run;
    std::uint32_t SourceEnd;
};

Record decodeRecord(const std::uint8_t* raw) {
    const std::uint64_t low = LoadLittle64(raw);
    const std::uint64_t high = raw[8];
    Record record;
    record.Run = ((low >> 18) & 1) != 0;
    record.SourceModulo = static_cast<std::uint32_t>(low & Modulo);
    if (record.Run) {
        record.KeyIndex = static_cast<std::uint32_t>((low >> 48) & 3);
        record.SourceBase = ((high & 0xff) << 14) | ((low >> 50) & 0x3fff);
        return record;
    }
    record.MountModulo = static_cast<std::uint32_t>((low >> 20) & Modulo);
    record.EvenLength = static_cast<std::uint32_t>(((low >> 38) & 0x1ffff) + 1);
    record.Even = static_cast<std::uint32_t>((low >> 55) & 7);
    record.Odd = static_cast<std::uint32_t>((low >> 58) & 7);
    record.Predictor = static_cast<std::uint32_t>(((high & 7) << 3) | ((low >> 61) & 7));
    record.StoredFlag = (high & 8) != 0;
    return record;
}

bool isCodecSelector(const std::uint32_t selector) {
    return selector == 1 || (selector >= 4 && selector <= 7);
}

std::uint32_t krakenFlags(const std::uint32_t even, const std::uint32_t odd, const bool twoHalves) {
    std::uint32_t flags = 0;
    if ((even & 4) != 0) flags |= 0x02;
    if ((even & 2) != 0) flags |= 0x01;
    if (twoHalves && (odd & 4) != 0) flags |= 0x20;
    if (twoHalves && (odd & 2) != 0) flags |= 0x10;
    return flags;
}

std::vector<InnerBlock> restoreSparsePages(const std::vector<std::uint8_t>& naps, const std::vector<Record>& records, const std::vector<Span>& spans,
                                           const std::vector<InnerBlock>& blocks, const std::uint64_t covered, const std::uint64_t mountEnd,
                                           const std::uint64_t pages, const std::size_t pageIndexAt) {
    if (covered > mountEnd || (mountEnd - covered) % Page != 0) throw PackageError("NAPS layout does not cover the inner mount");
    std::vector<std::size_t> pageAnchors(static_cast<std::size_t>(pages));
    std::map<std::size_t, std::uint64_t> anchors;
    std::size_t previous = 0;
    for (std::uint64_t page = 0; page < pages; ++page) {
        const std::uint8_t* group = naps.data() + pageIndexAt + (page / 8) * 10;
        const std::size_t index = (group[0] | group[1] << 8 | group[2] << 16) + (page % 8 == 0 ? 0 : group[3 + page % 8 - 1]);
        if (index < previous || index >= records.size() || records[index].Run) throw PackageError("Invalid NAPS page index", page);
        previous = index;
        pageAnchors[static_cast<std::size_t>(page)] = index;
        anchors[index] = page * Page;
    }
    std::vector<InnerBlock> restored;
    std::vector<std::uint64_t> positions(spans.size());
    std::uint64_t shift = 0;
    for (std::size_t index = 0; index < spans.size(); ++index) {
        std::uint64_t mountOffset = (index < blocks.size() ? blocks[index].MountOffset : covered) + shift;
        const auto anchor = anchors.find(spans[index].Record);
        if (anchor != anchors.end() && mountOffset < anchor->second) {
            const std::uint64_t padding = (anchor->second - mountOffset + Page - 1) / Page * Page;
            if (padding > mountEnd - covered - shift) throw PackageError("NAPS page index exceeds the inner mount");
            for (std::uint64_t page = 0; page < padding; page += Page)
                restored.push_back(InnerBlock{0, 0, 0, mountOffset + page, static_cast<std::uint32_t>(Page), InnerBlockKind::Zero, 0});
            shift += padding;
            mountOffset += padding;
        }
        if ((mountOffset & Modulo) != records[spans[index].Record].MountModulo) throw PackageError("NAPS block disagrees with its mount offset", mountOffset);
        positions[index] = mountOffset;
        if (index == blocks.size()) break;
        restored.push_back(blocks[index]);
        restored.back().MountOffset = mountOffset;
    }
    if (positions.back() != mountEnd) throw PackageError("NAPS layout does not reach the inner mount end");
    std::size_t span = 0;
    for (std::uint64_t page = 0; page < pages; ++page) {
        while (span + 1 < positions.size() && positions[span] < page * Page) ++span;
        if (spans[span].Record != pageAnchors[static_cast<std::size_t>(page)]) throw PackageError("NAPS page index disagrees with the restored layout", page);
    }
    return restored;
}

}

InnerLayout ReadNapsLayout(const std::vector<std::uint8_t>& naps, const std::uint64_t sourceSize) {
    if (naps.size() < 16) throw PackageError("NAPS layout is truncated");
    const std::uint64_t first = LoadLittle64(naps.data());
    const std::uint64_t second = LoadLittle64(naps.data() + 8);
    const std::uint64_t fileCount = (first & 0xffffff) + 1;
    const std::uint64_t compression = (first >> 24) & 3;
    const std::uint64_t shufflePatterns = (first >> 28) & 0xf;
    const std::uint64_t pages = (first >> 32) & 0xffffff;
    const std::uint64_t outerBlocks = second & 0xffffff;
    const std::uint64_t recordCount = ((second >> 24) & 0xffffff) + 2;
    if (compression != 0 && compression != 2) throw PackageError("Unsupported NAPS compression type " + std::to_string(compression));
    if (shufflePatterns != 0) throw PackageError("NAPS shuffle patterns are not supported");
    const std::uint64_t fileOffsetsAt = 16 + (outerBlocks + shufflePatterns) * 8;
    const std::uint64_t pageIndexAt = fileOffsetsAt + fileCount * 6;
    const std::uint64_t recordsAt = (pageIndexAt + ((pages + 8) >> 3) * 10 + 7) & ~std::uint64_t{7};
    if (recordsAt > naps.size() || recordCount > (naps.size() - recordsAt) / RecordSize) throw PackageError("NAPS layout is truncated");

    InnerLayout layout;
    std::uint64_t mountEnd = 0;
    for (std::uint64_t index = 0; index < fileCount; ++index) {
        const std::uint8_t* entry = naps.data() + fileOffsetsAt + index * 6;
        layout.FileOffsets.push_back(LoadLittle(entry, 5));
        if (index + 1 == fileCount && entry[5] == 0x40) mountEnd = layout.FileOffsets.back();
    }

    std::vector<Record> records(static_cast<std::size_t>(recordCount));
    for (std::size_t index = 0; index < records.size(); ++index) records[index] = decodeRecord(naps.data() + recordsAt + index * RecordSize);
    if (records.size() < 3 || !records.front().Run || records.back().Run || records.back().Even != 0 || records.back().Odd != 0)
        throw PackageError("Unsupported NAPS layout: missing initial run or terminator");
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto& record = records[index];
        if (record.Run && (record.KeyIndex != 0 || records[index + 1].Run)) throw PackageError("Unsupported NAPS run record", index);
        if (!record.Run && (record.StoredFlag || record.Predictor != 0)) throw PackageError("Unsupported NAPS layout dialect", index);
    }

    std::vector<Span> spans;
    std::size_t run = 0;
    for (std::size_t index = 0; index < records.size(); ++index) {
        if (records[index].Run) {
            run = index;
            continue;
        }
        spans.push_back(Span{index, run, index + 1 < records.size() ? records[index + 1].SourceModulo : records[index].SourceModulo});
    }

    std::uint64_t source = 0;
    std::uint64_t mount = 0;
    std::size_t currentRun = records.size();
    for (std::size_t index = 0; index + 1 < spans.size(); ++index) {
        const auto& current = records[spans[index].Record];
        const auto& next = records[spans[index + 1].Record];
        if (spans[index].Run != currentRun) {
            currentRun = spans[index].Run;
            source = records[currentRun].SourceBase << 18 | current.SourceModulo;
        }
        if ((source & Modulo) != current.SourceModulo) throw PackageError("NAPS source cursor disagrees with its record", spans[index].Record);
        InnerBlock block;
        block.SourceOffset = source;
        block.MountOffset = mount;
        block.Length = ((next.MountModulo - current.MountModulo - 1) & Modulo) + 1;
        block.SourceLength = (spans[index].SourceEnd - current.SourceModulo) & Modulo;
        block.EvenSourceLength = current.EvenLength;
        const bool twoHalves = block.Length > Half;
        const bool zero = (current.Even == 0 && current.Odd == 0) ||
            (current.Even == 1 && current.EvenLength == 8 && ((current.Odd == 1 && block.SourceLength == 16 && block.Length > 16) || (current.Odd == 0 && block.SourceLength == 8 && block.Length > 8)));
        if (zero) {
            block.Kind = InnerBlockKind::Zero;
        } else if (current.Even == 1 && current.Odd == (twoHalves ? 1u : 0u) && current.EvenLength == std::min(block.Length, Half) && block.SourceLength == (block.Length & Modulo)) {
            block.Kind = InnerBlockKind::Stored;
            block.SourceLength = block.Length;
        } else {
            if (block.SourceLength == 0) throw PackageError("Deduplicated NAPS blocks are not supported", spans[index].Record);
            if (block.SourceLength >= block.Length || !isCodecSelector(current.Even) || (twoHalves ? !isCodecSelector(current.Odd) : current.Odd != 0) ||
                block.EvenSourceLength > block.SourceLength || (twoHalves && block.EvenSourceLength == block.SourceLength))
                throw PackageError("Unsupported NAPS compressed block", spans[index].Record);
            block.Kind = InnerBlockKind::Kraken;
            block.KrakenFlags = krakenFlags(current.Even, current.Odd, twoHalves);
        }
        if (block.Kind != InnerBlockKind::Zero && (block.SourceOffset > sourceSize || block.SourceLength > sourceSize - block.SourceOffset))
            throw PackageError("NAPS block lies outside pfs_image.dat", block.SourceOffset);
        source += block.SourceLength;
        mount += block.Length;
        layout.Blocks.push_back(block);
    }

    if (mountEnd != 0 && mount != mountEnd) {
        layout.Blocks = restoreSparsePages(naps, records, spans, layout.Blocks, mount, mountEnd, pages, static_cast<std::size_t>(pageIndexAt));
        mount = mountEnd;
    }
    if (mount == 0 || (pages != 0 && (mount > pages * Page || mount <= (pages - 1) * Page))) throw PackageError("NAPS layout disagrees with its page count");
    layout.MountSize = mount;
    return layout;
}

}
