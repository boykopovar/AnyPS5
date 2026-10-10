#include <nid/MachONidPatcher.hpp>
#include <nid/NidResolver.hpp>
#include <nid/NidPatcherUtils.hpp>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace Nid {

namespace {

using Internal::Read;
using Internal::Write;

constexpr std::uint32_t kLcSegment64 = 0x19;
constexpr std::uint32_t kLcCodeSignature = 0x1d;
constexpr std::uint32_t kLcDyldInfo = 0x22;
constexpr std::uint32_t kLcDyldInfoOnly = 0x80000022u;
constexpr std::uint32_t kLcDyldExportsTrie = 0x80000033u;
constexpr std::uint32_t kLcDyldChainedFixups = 0x80000034u;
constexpr std::uint32_t kLcLoadDylib = 0x0c;
constexpr std::uint32_t kLcLoadWeakDylib = 0x80000018u;
constexpr std::uint32_t kLcReexportDylib = 0x8000001fu;
constexpr std::uint32_t kLcLazyLoadDylib = 0x20;
constexpr std::uint32_t kLcLoadUpwardDylib = 0x80000023u;

constexpr std::uint64_t kExportWeakDefinition = 0x04;
constexpr std::uint64_t kExportReexport = 0x08;
constexpr std::uint64_t kExportStubAndResolver = 0x10;

constexpr std::uint32_t kImportFormatPlain = 1;
constexpr std::uint32_t kImportFormatAddend = 2;
constexpr std::uint32_t kImportFormatAddend64 = 3;

constexpr std::uint64_t kPageSize = 0x1000;

struct MachHeader64 {
    std::uint32_t Magic;
    std::uint32_t CpuType;
    std::uint32_t CpuSubtype;
    std::uint32_t FileType;
    std::uint32_t CommandCount;
    std::uint32_t CommandBytes;
    std::uint32_t Flags;
    std::uint32_t Reserved;
};

struct LoadCommand {
    std::uint32_t Cmd;
    std::uint32_t Size;
};

struct SegmentCommand64 {
    std::uint32_t Cmd;
    std::uint32_t Size;
    char Name[16];
    std::uint64_t VmAddress;
    std::uint64_t VmSize;
    std::uint64_t FileOffset;
    std::uint64_t FileSize;
    std::uint32_t MaxProtection;
    std::uint32_t InitProtection;
    std::uint32_t SectionCount;
    std::uint32_t Flags;
};

struct LinkeditDataCommand {
    std::uint32_t Cmd;
    std::uint32_t Size;
    std::uint32_t DataOffset;
    std::uint32_t DataSize;
};

struct DyldInfoCommand {
    std::uint32_t Cmd;
    std::uint32_t Size;
    std::uint32_t RebaseOffset;
    std::uint32_t RebaseSize;
    std::uint32_t BindOffset;
    std::uint32_t BindSize;
    std::uint32_t WeakBindOffset;
    std::uint32_t WeakBindSize;
    std::uint32_t LazyBindOffset;
    std::uint32_t LazyBindSize;
    std::uint32_t ExportOffset;
    std::uint32_t ExportSize;
};

struct ChainedFixupsHeader {
    std::uint32_t Version;
    std::uint32_t StartsOffset;
    std::uint32_t ImportsOffset;
    std::uint32_t SymbolsOffset;
    std::uint32_t ImportsCount;
    std::uint32_t ImportsFormat;
    std::uint32_t SymbolsFormat;
};

void RequireRange(const std::vector<std::uint8_t>& binary, std::uint64_t offset, std::uint64_t size) {
    if (offset > binary.size() || size > binary.size() - offset) throw std::runtime_error("Mach-O range out of bounds");
}

std::uint64_t ReadUleb(const std::vector<std::uint8_t>& data, std::size_t& cursor, std::size_t end) {
    std::uint64_t value = 0;
    unsigned shift = 0;
    for (;;) {
        if (cursor >= end) throw std::runtime_error("truncated uleb128 in export trie");
        const auto byte = data[cursor++];
        if (shift >= 64 || (shift == 63 && (byte & 0x7e) != 0)) throw std::runtime_error("uleb128 overflow in export trie");
        value |= std::uint64_t{byte & 0x7fu} << shift;
        if ((byte & 0x80) == 0) return value;
        shift += 7;
    }
}

void WriteUleb(std::vector<std::uint8_t>& out, std::uint64_t value) {
    do {
        std::uint8_t byte = value & 0x7f;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        out.push_back(byte);
    } while (value != 0);
}

std::size_t UlebSize(std::uint64_t value) {
    std::size_t size = 1;
    while (value >>= 7) ++size;
    return size;
}

std::string ReadTrieString(const std::vector<std::uint8_t>& data, std::size_t& cursor, std::size_t end) {
    const auto start = cursor;
    while (cursor < end && data[cursor] != 0) ++cursor;
    if (cursor >= end) throw std::runtime_error("unterminated string in export trie");
    std::string text(reinterpret_cast<const char*>(data.data() + start), cursor - start);
    ++cursor;
    return text;
}

struct Image {
    MachHeader64 Header {};
    std::vector<std::size_t> Commands;
};

Image ParseImage(const std::vector<std::uint8_t>& binary) {
    Image image;
    image.Header = Read<MachHeader64>(binary, 0);
    if (image.Header.Magic != kMachOMagic64) throw std::runtime_error("not a 64-bit little-endian Mach-O image");
    RequireRange(binary, sizeof(MachHeader64), image.Header.CommandBytes);
    std::size_t cursor = sizeof(MachHeader64);
    const auto end = cursor + image.Header.CommandBytes;
    for (std::uint32_t index = 0; index < image.Header.CommandCount; ++index) {
        if (cursor + sizeof(LoadCommand) > end) throw std::runtime_error("load command out of bounds");
        const auto command = Read<LoadCommand>(binary, cursor);
        if (command.Size < sizeof(LoadCommand) || command.Size % 8 != 0 || command.Size > end - cursor) throw std::runtime_error("invalid load command size");
        image.Commands.push_back(cursor);
        cursor += command.Size;
    }
    return image;
}

struct TrieLocation {
    std::size_t OffsetField;
    std::size_t SizeField;
};

std::optional<TrieLocation> FindTrie(const std::vector<std::uint8_t>& binary, const Image& image) {
    std::optional<TrieLocation> found;
    for (const auto offset : image.Commands) {
        const auto cmd = Read<LoadCommand>(binary, offset).Cmd;
        std::optional<TrieLocation> here;
        if (cmd == kLcDyldExportsTrie) here = TrieLocation{offset + offsetof(LinkeditDataCommand, DataOffset), offset + offsetof(LinkeditDataCommand, DataSize)};
        else if (cmd == kLcDyldInfo || cmd == kLcDyldInfoOnly) {
            if (Read<DyldInfoCommand>(binary, offset).ExportSize != 0)
                here = TrieLocation{offset + offsetof(DyldInfoCommand, ExportOffset), offset + offsetof(DyldInfoCommand, ExportSize)};
        }
        if (!here) continue;
        if (found) throw std::runtime_error("Mach-O image has more than one export trie");
        found = here;
    }
    return found;
}

void ParseTrieNode(const std::vector<std::uint8_t>& data, std::size_t start, std::size_t end, std::size_t node, std::string& prefix, std::set<std::size_t>& visited, std::vector<MachOExport>& out) {
    if (node >= end - start) throw std::runtime_error("export trie node out of bounds");
    if (!visited.insert(node).second) throw std::runtime_error("export trie has a cycle");
    std::size_t cursor = start + node;
    const auto terminalSize = ReadUleb(data, cursor, end);
    if (terminalSize > end - cursor) throw std::runtime_error("export trie terminal out of bounds");
    const auto children = cursor + terminalSize;
    if (terminalSize != 0) {
        MachOExport entry;
        entry.Name = prefix;
        entry.Flags = ReadUleb(data, cursor, children);
        if (entry.Flags & kExportReexport) {
            entry.Other = ReadUleb(data, cursor, children);
            entry.ImportName = ReadTrieString(data, cursor, children);
        } else {
            entry.Address = ReadUleb(data, cursor, children);
            if (entry.Flags & kExportStubAndResolver) entry.Other = ReadUleb(data, cursor, children);
        }
        out.push_back(std::move(entry));
    }
    cursor = children;
    if (cursor >= end) throw std::runtime_error("export trie child count out of bounds");
    const auto childCount = data[cursor++];
    for (unsigned index = 0; index < childCount; ++index) {
        const auto label = ReadTrieString(data, cursor, end);
        const auto child = ReadUleb(data, cursor, end);
        const auto length = prefix.size();
        prefix += label;
        ParseTrieNode(data, start, end, static_cast<std::size_t>(child), prefix, visited, out);
        prefix.resize(length);
    }
}

std::vector<MachOExport> ParseTrie(const std::vector<std::uint8_t>& binary, std::uint32_t offset, std::uint32_t size) {
    RequireRange(binary, offset, size);
    std::vector<MachOExport> exports;
    if (size == 0) return exports;
    std::string prefix;
    std::set<std::size_t> visited;
    ParseTrieNode(binary, offset, std::size_t{offset} + size, 0, prefix, visited, exports);
    return exports;
}

class TrieBuilder {
public:
    explicit TrieBuilder(const std::vector<MachOExport>& exports) {
        for (const auto& entry : exports) Insert(entry);
    }

    std::vector<std::uint8_t> Build() {
        std::vector<Node*> order;
        Collect(&root, order);
        for (bool changed = true; changed;) {
            changed = false;
            std::uint64_t offset = 0;
            for (auto* node : order) {
                if (node->Offset != offset) {
                    node->Offset = offset;
                    changed = true;
                }
                offset += NodeSize(*node);
            }
        }
        std::vector<std::uint8_t> out;
        for (auto* node : order) {
            if (out.size() != node->Offset) throw std::runtime_error("export trie layout mismatch");
            const auto terminal = TerminalBytes(*node);
            WriteUleb(out, terminal.size());
            out.insert(out.end(), terminal.begin(), terminal.end());
            if (node->Children.size() > 255) throw std::runtime_error("export trie node has too many children");
            out.push_back(static_cast<std::uint8_t>(node->Children.size()));
            for (const auto& [label, child] : node->Children) {
                out.insert(out.end(), label.begin(), label.end());
                out.push_back(0);
                WriteUleb(out, child->Offset);
            }
        }
        while (out.size() % 8 != 0) out.push_back(0);
        return out;
    }

private:
    struct Node {
        std::vector<std::pair<std::string, std::unique_ptr<Node>>> Children;
        std::optional<MachOExport> Entry;
        std::uint64_t Offset = 0;
    };

    Node root;

    void Insert(const MachOExport& entry) {
        Node* node = &root;
        std::string_view rest = entry.Name;
        while (!rest.empty()) {
            bool descended = false;
            for (auto& [label, child] : node->Children) {
                std::size_t common = 0;
                while (common < label.size() && common < rest.size() && label[common] == rest[common]) ++common;
                if (common == 0) continue;
                if (common < label.size()) {
                    auto middle = std::make_unique<Node>();
                    middle->Children.emplace_back(label.substr(common), std::move(child));
                    label.resize(common);
                    child = std::move(middle);
                }
                node = child.get();
                rest.remove_prefix(common);
                descended = true;
                break;
            }
            if (!descended) {
                node->Children.emplace_back(std::string(rest), std::make_unique<Node>());
                node = node->Children.back().second.get();
                rest = {};
            }
        }
        if (node->Entry) throw std::runtime_error("duplicate patched export name: " + entry.Name);
        node->Entry = entry;
    }

    static void Collect(Node* node, std::vector<Node*>& order) {
        order.push_back(node);
        for (auto& [label, child] : node->Children) Collect(child.get(), order);
    }

    static std::vector<std::uint8_t> TerminalBytes(const Node& node) {
        std::vector<std::uint8_t> bytes;
        if (!node.Entry) return bytes;
        const auto& entry = *node.Entry;
        WriteUleb(bytes, entry.Flags);
        if (entry.Flags & kExportReexport) {
            WriteUleb(bytes, entry.Other);
            bytes.insert(bytes.end(), entry.ImportName.begin(), entry.ImportName.end());
            bytes.push_back(0);
        } else {
            WriteUleb(bytes, entry.Address);
            if (entry.Flags & kExportStubAndResolver) WriteUleb(bytes, entry.Other);
        }
        return bytes;
    }

    static std::uint64_t NodeSize(const Node& node) {
        const auto terminal = TerminalBytes(node).size();
        std::uint64_t size = UlebSize(terminal) + terminal + 1;
        for (const auto& [label, child] : node.Children) size += label.size() + 1 + UlebSize(child->Offset);
        return size;
    }
};

std::string StripUnderscore(const std::string& name) {
    return !name.empty() && name[0] == '_' ? name.substr(1) : name;
}

bool IsGuestFacingImport(const std::string& name) {
    using namespace Internal;
    const bool hasNidPostfix = name.size() >= kNidPostfixLen && name.compare(name.size() - kNidPostfixLen, kNidPostfixLen, kNidPostfix) == 0;
    const bool hasScePrefix = name.size() >= 3u &&
        std::tolower(static_cast<unsigned char>(name[0])) == 's' &&
        std::tolower(static_cast<unsigned char>(name[1])) == 'c' &&
        std::tolower(static_cast<unsigned char>(name[2])) == 'e';
    return hasNidPostfix || hasScePrefix;
}

std::vector<bool> DylibOrdinalsArePrx(const std::vector<std::uint8_t>& binary, const Image& image) {
    std::vector<bool> result;
    for (const auto offset : image.Commands) {
        const auto command = Read<LoadCommand>(binary, offset);
        if (command.Cmd != kLcLoadDylib && command.Cmd != kLcLoadWeakDylib && command.Cmd != kLcReexportDylib && command.Cmd != kLcLazyLoadDylib && command.Cmd != kLcLoadUpwardDylib) continue;
        const auto nameOffset = Read<std::uint32_t>(binary, offset + 8);
        if (nameOffset >= command.Size) throw std::runtime_error("dylib name out of bounds");
        const auto name = Internal::ReadCStr(binary, offset + nameOffset);
        result.push_back(name.size() >= 4 && name.compare(name.size() - 4, 4, ".prx") == 0);
    }
    return result;
}

void PatchChainedImports(std::vector<std::uint8_t>& binary, const Image& image) {
    std::optional<LinkeditDataCommand> fixups;
    for (const auto offset : image.Commands) {
        const auto cmd = Read<LoadCommand>(binary, offset).Cmd;
        if (cmd == kLcDyldChainedFixups) fixups = Read<LinkeditDataCommand>(binary, offset);
        if ((cmd == kLcDyldInfo || cmd == kLcDyldInfoOnly)) {
            const auto info = Read<DyldInfoCommand>(binary, offset);
            if (info.BindSize != 0 || info.LazyBindSize != 0 || info.WeakBindSize != 0) throw std::runtime_error("Mach-O bind opcodes are not supported; link with chained fixups");
        }
    }
    if (!fixups || fixups->DataSize == 0) return;
    RequireRange(binary, fixups->DataOffset, fixups->DataSize);
    const auto base = std::size_t{fixups->DataOffset};
    const auto header = Read<ChainedFixupsHeader>(binary, base);
    if (header.Version != 0) throw std::runtime_error("unsupported chained fixups version");
    if (header.SymbolsFormat != 0) throw std::runtime_error("compressed chained fixup symbols are not supported");
    const std::size_t entrySize = header.ImportsFormat == kImportFormatPlain ? 4 : header.ImportsFormat == kImportFormatAddend ? 8 : header.ImportsFormat == kImportFormatAddend64 ? 16 : 0;
    if (entrySize == 0) throw std::runtime_error("unsupported chained import format");
    if (header.ImportsOffset > fixups->DataSize || std::uint64_t{header.ImportsCount} * entrySize > fixups->DataSize - header.ImportsOffset || header.SymbolsOffset > fixups->DataSize)
        throw std::runtime_error("chained imports out of bounds");
    const auto symbols = base + header.SymbolsOffset;
    const auto symbolsEnd = base + fixups->DataSize;
    const auto prx = DylibOrdinalsArePrx(binary, image);

    struct Import { std::uint32_t NameOffset; std::uint32_t Ordinal; };
    std::vector<Import> imports;
    for (std::uint32_t index = 0; index < header.ImportsCount; ++index) {
        const auto at = base + header.ImportsOffset + index * entrySize;
        if (header.ImportsFormat == kImportFormatAddend64) {
            const auto raw = Read<std::uint64_t>(binary, at);
            imports.push_back({static_cast<std::uint32_t>(raw >> 32), static_cast<std::uint32_t>(raw & 0xffff)});
        } else {
            const auto raw = Read<std::uint32_t>(binary, at);
            imports.push_back({raw >> 9, raw & 0xff});
        }
    }
    std::set<std::uint32_t> nameOffsets;
    for (const auto& item : imports) nameOffsets.insert(item.NameOffset);
    std::set<std::uint32_t> rewritten;
    for (const auto& item : imports) {
        const bool special = header.ImportsFormat == kImportFormatAddend64 ? item.Ordinal >= 0xfff0 : item.Ordinal >= 0xf0;
        if (item.Ordinal == 0 || special || item.Ordinal > prx.size() || !prx[item.Ordinal - 1]) continue;
        if (rewritten.contains(item.NameOffset)) continue;
        const auto at = symbols + item.NameOffset;
        if (at >= symbolsEnd) throw std::runtime_error("chained import name out of bounds");
        const auto name = Internal::ReadCStr(binary, at);
        if (at + name.size() >= symbolsEnd) throw std::runtime_error("unterminated chained import name");
        const auto bare = StripUnderscore(name);
        if (bare == name || !IsGuestFacingImport(bare)) continue;
        const auto patched = "_" + ResolveOneName(bare);
        if (patched.size() > name.size()) throw std::runtime_error("import NID longer than original name: " + name);
        const auto inside = nameOffsets.upper_bound(item.NameOffset);
        if (inside != nameOffsets.end() && *inside <= item.NameOffset + name.size()) throw std::runtime_error("chained import names share storage: " + name);
        std::memcpy(binary.data() + at, patched.data(), patched.size());
        std::fill(binary.begin() + static_cast<std::ptrdiff_t>(at + patched.size()), binary.begin() + static_cast<std::ptrdiff_t>(at + name.size() + 1), 0);
        rewritten.insert(item.NameOffset);
    }
}

void WriteTrie(std::vector<std::uint8_t>& binary, const Image& image, const TrieLocation& location, const std::vector<std::uint8_t>& trie) {
    const auto oldOffset = Read<std::uint32_t>(binary, location.OffsetField);
    const auto oldSize = Read<std::uint32_t>(binary, location.SizeField);
    if (trie.size() <= oldSize) {
        std::copy(trie.begin(), trie.end(), binary.begin() + oldOffset);
        std::fill(binary.begin() + oldOffset + static_cast<std::ptrdiff_t>(trie.size()), binary.begin() + oldOffset + oldSize, 0);
        Write(binary, location.SizeField, static_cast<std::uint32_t>(trie.size()));
        return;
    }
    std::optional<std::size_t> linkedit;
    for (const auto offset : image.Commands) {
        const auto cmd = Read<LoadCommand>(binary, offset).Cmd;
        if (cmd == kLcCodeSignature) throw std::runtime_error("signed Mach-O images cannot grow their export trie");
        if (cmd != kLcSegment64) continue;
        const auto segment = Read<SegmentCommand64>(binary, offset);
        if (std::strncmp(segment.Name, "__LINKEDIT", sizeof(segment.Name)) == 0) linkedit = offset;
    }
    if (!linkedit) throw std::runtime_error("Mach-O image has no __LINKEDIT segment");
    auto segment = Read<SegmentCommand64>(binary, *linkedit);
    if (segment.FileOffset + segment.FileSize != binary.size()) throw std::runtime_error("__LINKEDIT does not end the file");
    const auto newOffset = (binary.size() + 7) & ~std::size_t{7};
    if (newOffset + trie.size() > 0xffffffffu) throw std::runtime_error("export trie offset overflow");
    binary.resize(newOffset, 0);
    binary.insert(binary.end(), trie.begin(), trie.end());
    std::fill(binary.begin() + oldOffset, binary.begin() + oldOffset + oldSize, 0);
    segment.FileSize = binary.size() - segment.FileOffset;
    segment.VmSize = std::max(segment.VmSize, (segment.FileSize + kPageSize - 1) & ~(kPageSize - 1));
    Write(binary, *linkedit, segment);
    Write(binary, location.OffsetField, static_cast<std::uint32_t>(newOffset));
    Write(binary, location.SizeField, static_cast<std::uint32_t>(trie.size()));
}

}

std::vector<MachOExport> ReadMachOExports(const std::vector<std::uint8_t>& binary) {
    const auto image = ParseImage(binary);
    const auto location = FindTrie(binary, image);
    if (!location) return {};
    return ParseTrie(binary, Read<std::uint32_t>(binary, location->OffsetField), Read<std::uint32_t>(binary, location->SizeField));
}

void MachONidPatcher::PatchNids(std::vector<std::uint8_t>& binary, const std::string& libraryName, const std::unordered_set<std::string>& excludedExports) const {
    const auto image = ParseImage(binary);
    const auto location = FindTrie(binary, image);
    if (!location) throw std::runtime_error("no export trie");
    auto exports = ParseTrie(binary, Read<std::uint32_t>(binary, location->OffsetField), Read<std::uint32_t>(binary, location->SizeField));
    if (exports.empty()) throw std::runtime_error("no exported names");

    std::vector<std::string> names;
    names.reserve(exports.size());
    for (const auto& entry : exports) {
        if (entry.Name.empty() || entry.Name[0] != '_') throw std::runtime_error("exported name without a C underscore: " + entry.Name);
        names.push_back(entry.Name.substr(1));
    }
    const auto nidMap = ResolveNids(names, libraryName, excludedExports);
    for (std::size_t index = 0; index < exports.size(); ++index) {
        const auto& name = names[index];
        const bool marked = name.ends_with(Internal::kNidPostfix) || Internal::IsNidNoPatch(name) || Internal::IsNidNoPatchCut(name);
        if ((exports[index].Flags & kExportWeakDefinition) != 0 && !marked) continue;
        const auto found = nidMap.find(name);
        if (found == nidMap.end()) throw std::runtime_error("symbol not in nid map: " + name);
        exports[index].Name = "_" + found->second;
    }
    WriteTrie(binary, image, *location, TrieBuilder(exports).Build());
    PatchChainedImports(binary, ParseImage(binary));
}

}
