#include <elfpatcher/macos/MacOsImage.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <elfpatcher/windows/WindowsStubEmitter.hpp>
#include <elfpatcher/windows/WindowsTrampolineBuilder.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <relinker/analysis/CodeInstructionCollector.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <bit>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>

namespace Elfpatcher::MacOs {

namespace {

using Windows::LoadRva;

constexpr std::uint64_t PageSize = 0x1000;
constexpr std::uint64_t UserAddressEnd = 0x7fffffe00000;

constexpr std::uint32_t MhMagic64 = 0xfeedfacf;
constexpr std::uint32_t CpuTypeX8664 = 0x01000007;
constexpr std::uint32_t CpuSubtypeX8664All = 3;
constexpr std::uint32_t MhExecute = 2;
constexpr std::uint32_t MhDylib = 6;
constexpr std::uint32_t MhNoUndefs = 0x1;
constexpr std::uint32_t MhDyldLink = 0x4;
constexpr std::uint32_t MhTwoLevel = 0x80;
constexpr std::uint32_t MhWeakDefines = 0x8000;

constexpr std::uint32_t LcSegment64 = 0x19;
constexpr std::uint32_t LcSymtab = 0x2;
constexpr std::uint32_t LcDysymtab = 0xb;
constexpr std::uint32_t LcLoadDylib = 0xc;
constexpr std::uint32_t LcIdDylib = 0xd;
constexpr std::uint32_t LcLoadDylinker = 0xe;
constexpr std::uint32_t LcUuid = 0x1b;
constexpr std::uint32_t LcRpath = 0x8000001c;
constexpr std::uint32_t LcDyldInfoOnly = 0x80000022;
constexpr std::uint32_t LcMain = 0x80000028;
constexpr std::uint32_t LcBuildVersion = 0x32;

constexpr std::uint32_t VmProtRead = 1;
constexpr std::uint32_t VmProtWrite = 2;
constexpr std::uint32_t VmProtExecute = 4;

constexpr std::uint32_t SectionModInitFuncPointers = 0x9;

constexpr std::uint8_t RebaseTypePointer = 1;
constexpr std::uint8_t RebaseOpcodeDone = 0x00;
constexpr std::uint8_t RebaseOpcodeSetTypeImm = 0x10;
constexpr std::uint8_t RebaseOpcodeSetSegmentAndOffsetUleb = 0x20;
constexpr std::uint8_t RebaseOpcodeDoRebaseImmTimes = 0x50;

constexpr std::uint8_t BindTypePointer = 1;
constexpr std::uint8_t BindSpecialDylibFlatLookup = 0x0e;
constexpr std::uint8_t BindSymbolFlagsWeakImport = 0x1;
constexpr std::uint8_t BindOpcodeDone = 0x00;
constexpr std::uint8_t BindOpcodeSetDylibSpecialImm = 0x30;
constexpr std::uint8_t BindOpcodeSetSymbolTrailingFlagsImm = 0x40;
constexpr std::uint8_t BindOpcodeSetTypeImm = 0x50;
constexpr std::uint8_t BindOpcodeSetAddendSleb = 0x60;
constexpr std::uint8_t BindOpcodeSetSegmentAndOffsetUleb = 0x70;
constexpr std::uint8_t BindOpcodeDoBind = 0x90;

constexpr std::uint64_t MetaVersion = 2;
constexpr std::size_t MetaLoadBias = 0x08;
constexpr std::size_t MetaAllocateSlot = 0x10;
constexpr std::size_t MetaRegisterSlot = 0x18;
constexpr std::size_t MetaTlsDescriptor = 0x20;
constexpr std::size_t MetaTlsKey = MetaTlsDescriptor + 0x28;
constexpr std::size_t MetaCxaAtexitSlot = 0x50;
constexpr std::size_t MetaHeaderCount = 0x58;
constexpr std::size_t MetaHeaders = 0x60;

constexpr std::uint8_t EntryStub[] = {
    0x31, 0xc9, 0x48, 0x83, 0x3c, 0xca, 0x00, 0x74, 0x05, 0x48, 0xff, 0xc1, 0xeb, 0xf4, 0x48, 0x8d,
    0x44, 0x0f, 0x03, 0x48, 0xc1, 0xe0, 0x03, 0x48, 0x29, 0xc4, 0x48, 0x83, 0xe4, 0xf0, 0x48, 0x89,
    0x3c, 0x24, 0x45, 0x31, 0xc0, 0x49, 0x39, 0xf8, 0x74, 0x0e, 0x4e, 0x8b, 0x0c, 0xc6, 0x4e, 0x89,
    0x4c, 0xc4, 0x08, 0x49, 0xff, 0xc0, 0xeb, 0xed, 0x48, 0xc7, 0x44, 0xfc, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x4c, 0x8d, 0x54, 0xfc, 0x10, 0x45, 0x31, 0xc0, 0x49, 0x39, 0xc8, 0x74, 0x0d, 0x4e, 0x8b,
    0x0c, 0xc2, 0x4f, 0x89, 0x0c, 0xc2, 0x49, 0xff, 0xc0, 0xeb, 0xee, 0x49, 0xc7, 0x04, 0xca, 0x00,
    0x00, 0x00, 0x00, 0x48, 0x89, 0xe6, 0x48, 0x8d, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xec,
    0x10, 0x0f, 0xae, 0x1c, 0x24, 0xc7, 0x44, 0x24, 0x04, 0xc0, 0x9f, 0x00, 0x00, 0x0f, 0xae, 0x54,
    0x24, 0x04, 0xff, 0x15, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xae, 0x14, 0x24, 0x48, 0x83, 0xc4, 0x10,
    0x0f, 0x0b,
};
constexpr std::size_t EntryStubEntryDisplacement = 0x69;
constexpr std::size_t EntryStubStartDisplacement = 0x84;

struct MacOsTracePrologue {
    std::vector<std::uint8_t> Bytes;
    std::size_t EntryDisplacement;
};

MacOsTracePrologue BuildTracePrologue(const std::uint32_t prologueRva, const std::uint32_t tableRva, const std::uint32_t tableEndRva, const std::uint32_t functionSlotRva) {
    std::vector<std::uint8_t> code = {0x57, 0x56, 0x52, 0x51, 0x53, 0x41, 0x54, 0x48, 0x83, 0xec, 0x08};
    const auto rip = [&](const std::initializer_list<std::uint8_t> opcode, const std::uint32_t target) {
        code.insert(code.end(), opcode);
        const auto displacement = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(prologueRva + code.size() + 4);
        if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
            throw Domain::RelinkerException("Import trace stub exceeds rel32 range");
        Io::AppendU32(code, static_cast<std::uint32_t>(displacement));
    };
    rip({0x48, 0x8d, 0x1d}, tableRva);
    rip({0x4c, 0x8d, 0x25}, tableEndRva);
    const auto loop = code.size();
    code.insert(code.end(), {0x4c, 0x39, 0xe3, 0x73, 0x00});
    const auto exit = code.size();
    code.insert(code.end(), {0x48, 0x8b, 0x03, 0x48, 0x8b, 0x3c, 0x03, 0x48, 0x8b, 0x73, 0x08, 0x48, 0x01, 0xde, 0x48, 0x8b, 0x53, 0x10, 0x48, 0x01, 0xda});
    rip({0xff, 0x15}, functionSlotRva);
    code.insert(code.end(), {0x48, 0x8b, 0x0b, 0x48, 0x89, 0x04, 0x0b, 0x48, 0x83, 0xc3, 0x18, 0xeb});
    const auto back = static_cast<std::int64_t>(loop) - static_cast<std::int64_t>(code.size() + 1);
    if (back < -128 || back > 127)
        throw Domain::RelinkerException("Import trace loop exceeds rel8 range");
    code.push_back(static_cast<std::uint8_t>(static_cast<std::int8_t>(back)));
    if (code.size() - exit > 127)
        throw Domain::RelinkerException("Import trace loop exceeds rel8 range");
    code[exit - 1] = static_cast<std::uint8_t>(code.size() - exit);
    code.insert(code.end(), {0x48, 0x83, 0xc4, 0x08, 0x41, 0x5c, 0x5b, 0x59, 0x5a, 0x5e, 0x5f, 0xe9, 0x00, 0x00, 0x00, 0x00});
    const auto entry = code.size() - 4;
    return {std::move(code), entry};
}

struct Section {
    std::string Name;
    std::uint64_t Offset;
    std::uint64_t Size;
    std::uint32_t Flags;
    std::uint32_t AlignShift;
};

struct Segment {
    std::string Name;
    std::uint64_t Rva;
    std::uint64_t MemorySize;
    std::uint32_t Protection;
    std::vector<std::uint8_t> Data;
    std::vector<Section> Sections;
    std::uint64_t FileOffset = 0;
};

std::uint64_t AlignUp(const std::uint64_t value, const std::uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

void AppendUleb(std::vector<std::uint8_t>& out, std::uint64_t value) {
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

void AppendSleb(std::vector<std::uint8_t>& out, std::int64_t value) {
    for (bool more = true; more;) {
        std::uint8_t byte = value & 0x7f;
        value >>= 7;
        more = !((value == 0 && (byte & 0x40) == 0) || (value == -1 && (byte & 0x40) != 0));
        if (more) byte |= 0x80;
        out.push_back(byte);
    }
}

void AppendCString(std::vector<std::uint8_t>& out, const std::string& text) {
    out.insert(out.end(), text.begin(), text.end());
    out.push_back(0);
}

void AppendName16(std::vector<std::uint8_t>& out, const std::string& name) {
    if (name.size() > 16) throw Domain::RelinkerException("Mach-O segment or section name is too long: " + name);
    out.insert(out.end(), name.begin(), name.end());
    out.insert(out.end(), 16 - name.size(), 0);
}

void PadTo(std::vector<std::uint8_t>& out, const std::size_t alignment) {
    while (out.size() % alignment != 0) out.push_back(0);
}

void FitDyldSegmentLimit(std::vector<Windows::PeSection>& sections, const std::size_t limit) {
    while (sections.size() > limit) {
        auto best = sections.end();
        std::uint64_t bestGap = 0;
        for (auto section = sections.begin(); std::next(section) != sections.end(); ++section) {
            const auto next = std::next(section);
            if (section->Name == ".amdstub" || next->Name == ".amdstub" || section->Characteristics != next->Characteristics ||
                section->Rva + section->MemorySize() != next->Rva)
                continue;
            const auto gap = next->Rva - section->Rva - section->Data.size();
            if (best == sections.end() || gap < bestGap) {
                best = section;
                bestGap = gap;
            }
        }
        if (best == sections.end()) return;
        const auto next = std::next(best);
        const auto memorySize = next->Rva + next->MemorySize() - best->Rva;
        if (!next->Data.empty()) {
            best->Data.resize(next->Rva - best->Rva);
            best->Data.insert(best->Data.end(), next->Data.begin(), next->Data.end());
        }
        best->VirtualSize = static_cast<std::uint32_t>(memorySize);
        sections.erase(next);
    }
}

std::pair<std::uint8_t, std::uint64_t> Locate(const std::vector<Segment>& segments, const std::uint64_t rva, const std::uint64_t size) {
    for (std::size_t index = 0; index < segments.size(); ++index) {
        const auto& segment = segments[index];
        if (segment.MemorySize == 0 || rva < segment.Rva || rva - segment.Rva >= segment.MemorySize || size > segment.MemorySize - (rva - segment.Rva)) continue;
        if (index > 15) throw Domain::RelinkerException("Too many Mach-O segments for dyld opcodes", rva);
        return {static_cast<std::uint8_t>(index), rva - segment.Rva};
    }
    throw Domain::RelinkerException("Mach-O fixup target is outside the image", rva);
}

Segment& SegmentAt(std::vector<Segment>& segments, const std::uint64_t rva, const std::uint64_t size) {
    for (auto& segment : segments) {
        if (rva < segment.Rva || rva - segment.Rva + size > segment.MemorySize) continue;
        if (rva - segment.Rva + size > segment.Data.size()) segment.Data.resize(rva - segment.Rva + size);
        return segment;
    }
    throw Domain::RelinkerException("Mach-O image patch is outside the image data", rva);
}

void AppendDylibCommand(std::vector<std::uint8_t>& out, const std::uint32_t cmd, const std::string& path) {
    const auto size = static_cast<std::uint32_t>(AlignUp(24 + path.size() + 1, 8));
    const auto start = out.size();
    Io::AppendU32(out, cmd);
    Io::AppendU32(out, size);
    Io::AppendU32(out, 24);
    Io::AppendU32(out, 2);
    Io::AppendU32(out, 0x10000);
    Io::AppendU32(out, 0x10000);
    AppendCString(out, path);
    out.resize(start + size, 0);
}

void AppendPathCommand(std::vector<std::uint8_t>& out, const std::uint32_t cmd, const std::string& path) {
    const auto size = static_cast<std::uint32_t>(AlignUp(12 + path.size() + 1, 8));
    const auto start = out.size();
    Io::AppendU32(out, cmd);
    Io::AppendU32(out, size);
    Io::AppendU32(out, 12);
    AppendCString(out, path);
    out.resize(start + size, 0);
}

struct GsTrap {
    std::uint32_t Rva;
    std::size_t Length;
};

struct TlsAccess {
    std::uint32_t Rva;
    std::size_t Length;
    bool StoreImmediate;
    std::uint32_t Immediate;
    std::uint8_t Register;
    std::uint32_t Displacement;
    std::uint8_t AluOpcode;
};

bool IsAluReadOpcode(std::uint8_t opcode) {
    return opcode == 0x03 || opcode == 0x0b || opcode == 0x13 || opcode == 0x1b || opcode == 0x23 || opcode == 0x2b || opcode == 0x33 || opcode == 0x3b;
}

struct TlsScan {
    const Domain::ProgramHeader* Tls = nullptr;
    std::vector<TlsAccess> Accesses;
    std::set<std::uint32_t> BranchTargets;
    std::vector<GsTrap> GsTraps;
};

TlsScan ScanTls(const std::vector<std::uint8_t>& source, const std::vector<Domain::ProgramHeader>& headers, const Windows::WindowsLoadImage& image, const std::function<void(std::string_view)>& message) {
    TlsScan scan;
    const Codegen::X64InstructionDecoder decoder;
    const auto instructions = Relinker::CodeInstructionCollector().Collect(source, headers, {}, nullptr, message);
    for (const auto& header : headers) {
        if (header.Type == 7) {
            if (scan.Tls != nullptr) throw Domain::RelinkerException("Multiple ELF TLS segments");
            scan.Tls = &header;
        }
        if (header.Type != 1 || (header.Flags & 1) == 0) continue;
        std::uint64_t decodedEnd = 0;
        for (auto instruction = instructions.lower_bound(header.MappedAddress); instruction != instructions.end() && *instruction - header.MappedAddress < header.FileSize; ++instruction) {
            const auto offset = *instruction - header.MappedAddress;
            const auto* bytes = source.data() + header.Offset + offset;
            const auto info = decoder.DecodeInstruction(bytes, header.FileSize - offset);
            const auto rva = image.GetRva(header.MappedAddress + offset, info.Length);
            const bool insidePrevious = *instruction < decodedEnd;
            decodedEnd = std::max(decodedEnd, *instruction + info.Length);
            if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
                const auto target = static_cast<std::int64_t>(rva) + static_cast<std::int64_t>(info.Length) + info.BranchDisp;
                if (target >= 0 && target <= std::numeric_limits<std::uint32_t>::max()) scan.BranchTargets.insert(static_cast<std::uint32_t>(target));
            }
            if (info.SegmentPrefix == 0 || (insidePrevious && info.SegmentPrefix != 0x64)) continue;
            if (info.SegmentPrefix == 0x65) {
                if (info.HasMemoryOperand)
                    scan.GsTraps.push_back({rva, info.Length});
                continue;
            }
            const auto position = info.OpcodeOffset;
            bool hasOperandSizePrefix = false;
            bool supportedPrefixes = info.SegmentPrefix == 0x64;
            for (std::size_t prefix = 0; prefix < position; ++prefix) {
                const auto value = bytes[prefix];
                if (value == 0x66) hasOperandSizePrefix = true;
                else if (value != 0x64 && !(prefix + 1 == position && value >= 0x40 && value <= 0x4f)) supportedPrefixes = false;
            }
            const auto loadRegister = info.Length - position == 7 ? static_cast<std::uint8_t>(((bytes[position + 1] >> 3) & 7) | ((info.RexPrefix & 4) << 1)) : std::uint8_t{4};
            const bool loadValue = supportedPrefixes && (info.RexPrefix == 0x48 || info.RexPrefix == 0x4c) && loadRegister != 4 && bytes[position] == 0x8b && (bytes[position + 1] & 0xc7) == 0x04 && bytes[position + 2] == 0x25;
            const bool storeImmediate = supportedPrefixes && !hasOperandSizePrefix && (info.RexPrefix == 0 || info.RexPrefix == 0x40) && info.Length - position == 11 && bytes[position] == 0xc7 && bytes[position + 1] == 0x04 && bytes[position + 2] == 0x25 && Io::ReadU32(source, header.Offset + offset + position + 3) == 0x28;
            const bool aluRead = supportedPrefixes && (info.RexPrefix == 0x48 || info.RexPrefix == 0x4c) && loadRegister != 4 && IsAluReadOpcode(bytes[position]) && (bytes[position + 1] & 0xc7) == 0x04 && bytes[position + 2] == 0x25;
            if (!loadValue && !storeImmediate && !aluRead) {
                std::ostringstream message;
                message << "Unsupported macOS guest TLS instruction (bytes:" << std::hex << std::setfill('0');
                for (std::size_t index = 0; index < info.Length; ++index) message << ' ' << std::setw(2) << static_cast<unsigned int>(bytes[index]);
                message << ')';
                throw Domain::RelinkerException(message.str(), header.Offset + offset);
            }
            scan.Accesses.push_back({rva, info.Length, storeImmediate, storeImmediate ? Io::ReadU32(source, header.Offset + offset + position + 7) : 0, storeImmediate ? std::uint8_t{0} : loadRegister, storeImmediate ? 0 : Io::ReadU32(source, header.Offset + offset + position + 3), aluRead ? bytes[position] : std::uint8_t{0}});
        }
    }
    return scan;
}

class TrieBuilder {
public:
    explicit TrieBuilder(const std::vector<MacOsExport>& exports) {
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
            const auto terminal = TerminalBytes(*node);
            AppendUleb(out, terminal.size());
            out.insert(out.end(), terminal.begin(), terminal.end());
            if (node->Children.size() > 255) throw Domain::RelinkerException("Mach-O export trie node has too many children");
            out.push_back(static_cast<std::uint8_t>(node->Children.size()));
            for (const auto& [label, child] : node->Children) {
                AppendCString(out, label);
                AppendUleb(out, child->Offset);
            }
        }
        PadTo(out, 8);
        return out;
    }

private:
    struct Node {
        std::vector<std::pair<std::string, std::unique_ptr<Node>>> Children;
        std::optional<MacOsExport> Export;
        std::uint64_t Offset = 0;
    };

    Node root;

    void Insert(const MacOsExport& entry) {
        Node* node = &root;
        std::string_view rest = entry.Symbol;
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
        if (node->Export) throw Domain::RelinkerException("Duplicate Mach-O export: " + entry.Symbol);
        node->Export = entry;
    }

    static void Collect(Node* node, std::vector<Node*>& order) {
        order.push_back(node);
        for (auto& [label, child] : node->Children) Collect(child.get(), order);
    }

    static std::vector<std::uint8_t> TerminalBytes(const Node& node) {
        std::vector<std::uint8_t> bytes;
        if (!node.Export) return bytes;
        AppendUleb(bytes, node.Export->Flags);
        AppendUleb(bytes, node.Export->Value);
        return bytes;
    }

    static std::uint64_t NodeSize(const Node& node) {
        const auto terminal = TerminalBytes(node).size();
        std::uint64_t size = UlebSize(terminal) + terminal + 1;
        for (const auto& [label, child] : node.Children) size += label.size() + 1 + UlebSize(child->Offset);
        return size;
    }
};

}

std::uint64_t GuestTlsBlockSize(const Domain::ProgramHeader& tls) {
    const auto alignment = std::max<std::uint64_t>(tls.Alignment, 1);
    if (tls.FileSize > tls.MemorySize || !std::has_single_bit(alignment) || alignment > 8192 || tls.MemorySize > 0x7fff0000u)
        throw Domain::RelinkerException("Invalid or unsupported ELF TLS layout", tls.Offset);
    return AlignUp(tls.MemorySize, alignment);
}

std::vector<std::uint8_t> WriteMacOsImage(MacOsImageInput& input) {
    auto& image = *input.Image;
    const auto& headers = *input.Headers;
    const auto imageBase = image.GetImageBase();
    const auto tls = ScanTls(*input.Source, headers, image, input.Message);
    const bool hasTls = tls.Tls != nullptr && (tls.Tls->MemorySize != 0 || !tls.Accesses.empty());
    if (!hasTls && !tls.Accesses.empty()) throw Domain::RelinkerException("Guest TLS access without a non-empty PT_TLS");
    if (!hasTls && !input.TlsModuleSlots.empty()) throw Domain::RelinkerException("Guest TLS module relocation without a non-empty PT_TLS");

    std::vector<Segment> segments;
    if (input.Executable) segments.push_back({"__PAGEZERO", 0, 0, 0, {}, {}});
    segments.push_back({"__TEXT", 0, LoadRva, VmProtRead | VmProtExecute, {}, {}});
    const auto guestSegments = segments.size();
    auto sections = image.BuildSections();
    for (const auto& trap : tls.GsTraps) {
        const auto target = tls.BranchTargets.upper_bound(trap.Rva);
        if (target != tls.BranchTargets.end() && *target < trap.Rva + trap.Length)
            throw Domain::RelinkerException("Branch enters a guest GS instruction", *target);
        if (input.Trampolines != nullptr) {
            for (const auto& site : *input.Trampolines) {
                const auto siteRva = image.GetRva(site.Address, site.Length);
                if (siteRva < trap.Rva + trap.Length && trap.Rva < siteRva + site.Length)
                    throw Domain::RelinkerException("AMD-only instruction is also a guest GS access", siteRva);
            }
        }
        const auto section = std::find_if(sections.begin(), sections.end(), [&](const auto& candidate) {
            return trap.Rva >= candidate.Rva && trap.Rva - candidate.Rva < candidate.Data.size();
        });
        if (section == sections.end()) throw Domain::RelinkerException("Guest GS instruction is outside the image", trap.Rva);
        const auto offset = trap.Rva - section->Rva;
        if (trap.Length > section->Data.size() - offset) throw Domain::RelinkerException("Guest GS instruction crosses a section boundary", trap.Rva);
        std::fill_n(section->Data.begin() + offset, trap.Length, 0x90);
        std::copy(std::begin(kStubOpUd2), std::end(kStubOpUd2), section->Data.begin() + offset);
    }
    if (input.Trampolines != nullptr && !input.Trampolines->empty()) {
        for (const auto& access : tls.Accesses) {
            for (const auto& site : *input.Trampolines) {
                const auto siteRva = image.GetRva(site.Address, site.Length);
                if (siteRva < access.Rva + access.Length && access.Rva < siteRva + site.Length)
                    throw Domain::RelinkerException("AMD-only instruction is also a guest TLS access", siteRva);
            }
        }
        auto stubRva = Windows::AlignRva(image.GetEndRva());
        Windows::WindowsTrampolineBuilder().Build(*input.Trampolines, image, sections, stubRva);
    }
    FitDyldSegmentLimit(sections, 16 - guestSegments - 2);
    std::uint64_t guestEnd = image.GetEndRva();
    for (auto& section : sections) {
        std::uint32_t protection = 0;
        if (section.Characteristics & Windows::SectionRead) protection |= VmProtRead;
        if (section.Characteristics & Windows::SectionWrite) protection |= VmProtWrite;
        if (section.Characteristics & Windows::SectionExecute) protection |= VmProtExecute;
        auto name = section.Name == ".amdstub" ? std::string("__AMDSTUB") : "__ELF" + std::to_string(segments.size() - guestSegments);
        guestEnd = std::max<std::uint64_t>(guestEnd, section.Rva + section.MemorySize());
        segments.push_back({std::move(name), section.Rva, section.MemorySize(), protection, std::move(section.Data), {}});
    }

    const auto metaRva = static_cast<std::uint32_t>(AlignUp(guestEnd, PageSize));
    const auto metaSize = MetaHeaders + headers.size() * 56;
    std::vector<std::uint8_t> meta(metaSize, 0);
    Io::WriteU64(meta, 0, MetaVersion);
    const auto firstLoad = std::find_if(headers.begin(), headers.end(), [](const auto& header) { return header.Type == 1; });
    if (firstLoad == headers.end()) throw Domain::RelinkerException("No PT_LOAD segments found");
    Io::WriteU64(meta, MetaLoadBias, imageBase + image.GetRva(firstLoad->MappedAddress) - firstLoad->MappedAddress);
    input.Rebases.push_back(metaRva + MetaLoadBias);
    Io::WriteU64(meta, MetaHeaderCount, headers.size());
    for (std::size_t index = 0; index < headers.size(); ++index) {
        const auto& header = headers[index];
        const auto at = MetaHeaders + index * 56;
        Io::WriteU32(meta, at, header.Type);
        Io::WriteU32(meta, at + 4, header.Flags);
        Io::WriteU64(meta, at + 8, header.Offset);
        Io::WriteU64(meta, at + 16, header.MappedAddress);
        Io::WriteU64(meta, at + 24, header.PhysicalAddress);
        Io::WriteU64(meta, at + 32, header.FileSize);
        Io::WriteU64(meta, at + 40, header.MemorySize);
        Io::WriteU64(meta, at + 48, header.Alignment);
    }
    const auto descriptor = metaRva + static_cast<std::uint32_t>(MetaTlsDescriptor);
    const auto initPointer = AlignUp(metaSize, 8);
    const auto startSlot = initPointer + 8;
    const auto traceFunctionSlot = input.TraceImports ? startSlot + 8 : startSlot;
    const auto tlsExports = traceFunctionSlot + 8;
    std::map<std::string, std::uint64_t> tlsImportSlots;
    for (const auto& import : input.TlsImports) tlsImportSlots.emplace(import.Symbol, 0);
    auto metaEnd = tlsExports + input.TlsExports.size() * 16;
    for (auto& [symbol, slot] : tlsImportSlots) {
        slot = metaEnd;
        metaEnd += 8;
        input.Binds.push_back({symbol, metaRva + static_cast<std::uint32_t>(slot)});
    }
    std::uint64_t moduleArgsSlot = 0;
    std::uint64_t moduleResultSlot = 0;
    if (input.InitRva != 0) {
        moduleArgsSlot = metaEnd;
        moduleResultSlot = metaEnd + 8;
        metaEnd += 16;
        input.Binds.push_back({"___aps5_get_pending_module_args_nid_no_patch", metaRva + static_cast<std::uint32_t>(moduleArgsSlot)});
        input.Binds.push_back({"___aps5_set_module_init_result_nid_no_patch", metaRva + static_cast<std::uint32_t>(moduleResultSlot)});
    }
    meta.resize(metaEnd, 0);
    for (std::size_t index = 0; index < input.TlsExports.size(); ++index) {
        const auto& variable = input.TlsExports[index];
        if (!hasTls) throw Domain::RelinkerException("Guest TLS export has no TLS block: " + variable.Symbol);
        if (variable.Offset >= tls.Tls->MemorySize) throw Domain::RelinkerException("Guest TLS export is outside the TLS block: " + variable.Symbol);
        const auto at = tlsExports + index * 16;
        Io::WriteU64(meta, at, imageBase + descriptor);
        Io::WriteU64(meta, at + 8, variable.Offset);
        input.Rebases.push_back(metaRva + static_cast<std::uint32_t>(at));
        input.Exports.push_back({variable.Symbol, metaRva + static_cast<std::uint32_t>(at), variable.Flags});
    }
    const auto codeRva = static_cast<std::uint32_t>(AlignUp(metaRva + metaEnd, PageSize));
    Windows::WindowsStubEmitter code(codeRva);

    if (hasTls) {
        const auto& header = *tls.Tls;
        const auto blockSize = GuestTlsBlockSize(header);
        constexpr std::int64_t StackGuardDisplacement = 0x28;
        for (const auto& access : tls.Accesses) {
            if (access.StoreImmediate || access.Displacement == 0) continue;
            const auto displacement = static_cast<std::int64_t>(std::bit_cast<std::int32_t>(access.Displacement));
            if (displacement != StackGuardDisplacement && (displacement > 0 || displacement < -static_cast<std::int64_t>(blockSize)))
                throw Domain::RelinkerException("macOS guest TLS load displacement " + std::to_string(displacement) + " is not in the thread TLS block, 0 or the stack guard at 0x28");
        }
        Io::WriteU64(meta, MetaTlsDescriptor, 1);
        if (header.FileSize != 0) {
            Io::WriteU64(meta, MetaTlsDescriptor + 8, imageBase + image.GetRva(header.MappedAddress, header.FileSize));
            input.Rebases.push_back(descriptor + 8);
        }
        Io::WriteU64(meta, MetaTlsDescriptor + 0x10, header.FileSize);
        Io::WriteU64(meta, MetaTlsDescriptor + 0x18, blockSize);
        Io::WriteU64(meta, MetaTlsDescriptor + 0x20, std::max<std::uint64_t>(header.Alignment, 1));
        input.Binds.push_back({"_Aps5GuestTlsAllocate_nid_no_patch", metaRva + static_cast<std::uint32_t>(MetaAllocateSlot)});
        input.Binds.push_back({"_Aps5GuestTlsRegister_nid_no_patch", metaRva + static_cast<std::uint32_t>(MetaRegisterSlot)});
        for (const auto slot : input.TlsModuleSlots) {
            auto& segment = SegmentAt(segments, slot, 8);
            Io::WriteU64(segment.Data, slot - segment.Rva, imageBase + descriptor);
            input.Rebases.push_back(slot);
        }

        for (const auto& access : tls.Accesses) {
            const auto target = tls.BranchTargets.upper_bound(access.Rva);
            if (target != tls.BranchTargets.end() && *target < access.Rva + access.Length)
                throw Domain::RelinkerException("Branch enters a guest TLS instruction", *target);
            const auto stub = code.GetRva();
            const bool preserveAccumulator = access.StoreImmediate || access.Register != 0;
            code.Emit({0x48, 0x8d, 0x64, 0x24, 0x80});
            if (access.AluOpcode != 0) {
                code.Emit({0x50});
                code.Emit({0x9c});
            } else {
                code.Emit({0x9c});
                if (preserveAccumulator) code.Emit({0x50});
            }
            code.Rip({0x48, 0x8b, 0x05}, metaRva + static_cast<std::uint32_t>(MetaTlsKey));
            code.Emit({0x65, 0x48, 0x8b, 0x04, 0xc5, 0, 0, 0, 0});
            code.Emit({0x48, 0x85, 0xc0});
            const auto allocated = code.Branch({0x0f, 0x85});
            code.Rip({0x48, 0x8d, 0x05}, descriptor);
            code.Rip({0xff, 0x15}, metaRva + static_cast<std::uint32_t>(MetaAllocateSlot));
            code.PatchBranch(allocated, code.GetRva());
            if (access.StoreImmediate) {
                code.Emit({0xc7, 0x40, 0x28});
                code.U32(access.Immediate);
            } else if (access.Displacement != 0) {
                code.Emit({0x48, 0x8b, 0x80});
                code.U32(access.Displacement);
            }
            if (access.AluOpcode != 0 && access.Register == 0) {
                code.Emit({0x9d});
                code.Emit({0x48, 0x87, 0x04, 0x24});
                code.Emit({0x48, access.AluOpcode, 0x04, 0x24});
                code.Emit({0x48, 0x8d, 0x64, 0x24, 0x08});
            } else if (access.AluOpcode != 0) {
                code.Emit({0x9d});
                code.Emit({static_cast<std::uint8_t>(0x48 | ((access.Register >> 3) << 2)), access.AluOpcode, static_cast<std::uint8_t>(0xc0 | ((access.Register & 7) << 3))});
                code.Emit({0x58});
            } else {
                if (!access.StoreImmediate && access.Register != 0)
                    code.Emit({static_cast<std::uint8_t>(0x48 | (access.Register >> 3)), 0x89, static_cast<std::uint8_t>(0xc0 | (access.Register & 7))});
                if (preserveAccumulator) code.Emit({0x58});
                code.Emit({0x9d});
            }
            code.Emit({0x48, 0x8d, 0xa4, 0x24, 0x80, 0, 0, 0});
            code.Rip({0xe9}, static_cast<std::uint32_t>(access.Rva + access.Length));

            auto& site = SegmentAt(segments, access.Rva, access.Length);
            const auto at = access.Rva - site.Rva;
            const auto displacement = static_cast<std::int64_t>(stub) - static_cast<std::int64_t>(access.Rva + 5);
            std::fill_n(site.Data.begin() + static_cast<std::ptrdiff_t>(at), access.Length, 0x90);
            site.Data[at] = 0xe9;
            Io::WriteU32(site.Data, at + 1, static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement)));
        }
    }

    const bool hasFini = input.FiniRva != 0 || !input.FiniArrayRvas.empty();
    std::uint32_t finiStub = 0;
    const auto callZeroArgs = [&] { code.Emit({0x31, 0xff, 0x31, 0xf6, 0x31, 0xd2}); };
    const auto callSlot = [&](const std::uint32_t slot) {
        code.Rip({0x48, 0x8b, 0x05}, slot);
        code.Emit({0x48, 0x85, 0xc0, 0x74, 0x08});
        callZeroArgs();
        code.Emit({0xff, 0xd0});
    };
    if (hasFini) {
        finiStub = code.GetRva();
        code.Emit({0x55, 0x48, 0x89, 0xe5});
        for (auto slot = input.FiniArrayRvas.rbegin(); slot != input.FiniArrayRvas.rend(); ++slot) callSlot(*slot);
        if (input.FiniRva != 0) {
            callZeroArgs();
            code.Rip({0xe8}, input.FiniRva);
        }
        code.Emit({0x5d, 0xc3});
        input.Binds.push_back({"___cxa_atexit", metaRva + static_cast<std::uint32_t>(MetaCxaAtexitSlot)});
    }

    const bool hasInit = hasTls || !input.TlsImports.empty() || input.InitRva != 0 || !input.InitArrayRvas.empty() || hasFini;
    std::vector<Section> dataSections{{"__meta", 0, metaSize, 0, 3}};
    if (hasInit) {
        const auto initStub = code.GetRva();
        code.Emit({0x55, 0x48, 0x89, 0xe5});
        if (hasTls) {
            code.Rip({0x48, 0x8d, 0x3d}, descriptor);
            code.Rip({0xff, 0x15}, metaRva + static_cast<std::uint32_t>(MetaRegisterSlot));
        }
        for (const auto& import : input.TlsImports) {
            code.Rip({0x48, 0x8b, 0x05}, metaRva + static_cast<std::uint32_t>(tlsImportSlots.at(import.Symbol)));
            if (import.Module) code.Emit({0x48, 0x8b, 0x00});
            else code.Emit({0x48, 0x8b, 0x40, 0x08});
            if (import.Addend != 0) {
                code.Emit({0x48, 0xba});
                code.U64(static_cast<std::uint64_t>(import.Addend));
                code.Emit({0x48, 0x01, 0xd0});
            }
            code.Rip({0x48, 0x89, 0x05}, import.Rva);
        }
        if (input.InitRva != 0) {
            code.Rip({0xff, 0x15}, metaRva + static_cast<std::uint32_t>(moduleArgsSlot));
            code.Emit({0x48, 0x8b, 0x38, 0x48, 0x8b, 0x70, 0x08, 0x31, 0xd2});
            code.Rip({0xe8}, input.InitRva);
            code.Emit({0x89, 0xc7});
            code.Rip({0xff, 0x15}, metaRva + static_cast<std::uint32_t>(moduleResultSlot));
        }
        for (const auto slot : input.InitArrayRvas) callSlot(slot);
        if (hasFini) {
            code.Rip({0x48, 0x8d, 0x3d}, finiStub);
            code.Emit({0x31, 0xf6});
            code.Rip({0x48, 0x8d, 0x15}, 0);
            code.Rip({0xff, 0x15}, metaRva + static_cast<std::uint32_t>(MetaCxaAtexitSlot));
        }
        code.Emit({0x5d, 0xc3});
        Io::WriteU64(meta, initPointer, imageBase + initStub);
        input.Rebases.push_back(metaRva + static_cast<std::uint32_t>(initPointer));
        dataSections.push_back({"__mod_init_func", initPointer, 8, SectionModInitFuncPointers, 3});
    }
    if (input.Executable) {
        input.Binds.push_back({"_Aps5StartGuest_nid_no_patch", metaRva + static_cast<std::uint32_t>(startSlot)});
        if (input.TraceImports)
            input.Binds.push_back({"_TraceImport_nid_no_patch", metaRva + static_cast<std::uint32_t>(traceFunctionSlot)});
    }
    std::size_t traceEntryPatch = 0;
    std::uint32_t tracePrologueRva = 0;
    if (input.Executable && input.TraceImports) {
        const auto tableRva = code.GetRva();
        const auto tableEnd = tableRva + static_cast<std::uint32_t>(input.Traced.size() * 24);
        std::map<std::string, std::uint32_t> stringRva;
        std::vector<std::string> stringOrder;
        auto cursor = tableEnd;
        const auto intern = [&](const std::string& text) {
            const auto existing = stringRva.find(text);
            if (existing != stringRva.end()) return existing->second;
            stringRva.emplace(text, cursor);
            stringOrder.push_back(text);
            cursor += static_cast<std::uint32_t>(text.size() + 1);
            return stringRva.at(text);
        };
        std::vector<std::uint8_t> table;
        for (std::size_t index = 0; index < input.Traced.size(); ++index) {
            const auto entry = tableRva + static_cast<std::uint32_t>(index * 24);
            const auto relative = [entry](const std::uint32_t target) { return static_cast<std::uint64_t>(static_cast<std::int64_t>(target) - static_cast<std::int64_t>(entry)); };
            Io::AppendU64(table, relative(input.Traced[index].Rva));
            Io::AppendU64(table, relative(intern(input.Traced[index].Name)));
            Io::AppendU64(table, relative(intern(input.Traced[index].Library)));
        }
        for (const auto byte : table) code.Emit({byte});
        for (const auto& text : stringOrder) {
            if (code.GetRva() != stringRva.at(text)) throw Domain::RelinkerException("Import trace string was placed at the wrong address");
            for (const auto character : text) code.Emit({static_cast<std::uint8_t>(character)});
            code.Emit({0});
        }
        if (code.GetRva() != cursor) throw Domain::RelinkerException("Import trace table size changed while it was written");
        tracePrologueRva = cursor;
        const auto prologue = BuildTracePrologue(cursor, tableRva, tableEnd, metaRva + static_cast<std::uint32_t>(traceFunctionSlot));
        traceEntryPatch = static_cast<std::size_t>(code.GetRva() - codeRva) + prologue.EntryDisplacement;
        for (const auto byte : prologue.Bytes) code.Emit({byte});
    }
    const auto metaBytes = meta.size();
    segments.push_back({"__APS5DATA", metaRva, metaBytes, VmProtRead | VmProtWrite, std::move(meta), std::move(dataSections)});
    auto stubs = code.TakeBytes();
    if (!stubs.empty()) segments.push_back({"__APS5TEXT", codeRva, stubs.size(), VmProtRead | VmProtExecute, std::move(stubs), {}});

    std::sort(segments.begin() + static_cast<std::ptrdiff_t>(guestSegments), segments.end(), [](const Segment& left, const Segment& right) { return left.Rva < right.Rva; });
    for (std::size_t index = guestSegments; index < segments.size(); ++index) {
        auto& data = segments[index].Data;
        if (segments[index].Sections.empty()) {
            auto used = data.size();
            while (used != 0 && data[used - 1] == 0) --used;
            data.resize(used);
        }
        if (segments[index].Name == "__APS5TEXT" && traceEntryPatch != 0 && data.size() < traceEntryPatch + 4)
            data.resize(traceEntryPatch + 4, 0);
        data.resize(AlignUp(data.size(), PageSize), 0);
    }

    std::vector<std::uint8_t> rebase;
    rebase.push_back(RebaseOpcodeSetTypeImm | RebaseTypePointer);
    auto rebaseTargets = input.Rebases;
    std::sort(rebaseTargets.begin(), rebaseTargets.end());
    for (const auto rva : rebaseTargets) {
        const auto [segment, offset] = Locate(segments, rva, 8);
        rebase.push_back(RebaseOpcodeSetSegmentAndOffsetUleb | segment);
        AppendUleb(rebase, offset);
        rebase.push_back(RebaseOpcodeDoRebaseImmTimes | 1);
    }
    rebase.push_back(RebaseOpcodeDone);
    PadTo(rebase, 8);

    std::vector<std::uint8_t> bind;
    bind.push_back(BindOpcodeSetDylibSpecialImm | BindSpecialDylibFlatLookup);
    bind.push_back(BindOpcodeSetTypeImm | BindTypePointer);
    for (const auto& item : input.Binds) {
        const auto [segment, offset] = Locate(segments, item.Rva, 8);
        bind.push_back(item.Weak ? BindOpcodeSetSymbolTrailingFlagsImm | BindSymbolFlagsWeakImport : BindOpcodeSetSymbolTrailingFlagsImm);
        AppendCString(bind, item.Symbol);
        bind.push_back(BindOpcodeSetAddendSleb);
        AppendSleb(bind, item.Addend);
        bind.push_back(BindOpcodeSetSegmentAndOffsetUleb | segment);
        AppendUleb(bind, offset);
        bind.push_back(BindOpcodeDoBind);
    }
    bind.push_back(BindOpcodeDone);
    PadTo(bind, 8);

    const auto exports = input.Exports.empty() ? std::vector<std::uint8_t>{} : TrieBuilder(input.Exports).Build();

    std::vector<std::uint8_t> commands;
    std::uint32_t commandCount = 0;
    std::vector<std::size_t> segmentCommandOffsets;
    for (const auto& segment : segments) {
        segmentCommandOffsets.push_back(commands.size());
        Io::AppendU32(commands, LcSegment64);
        Io::AppendU32(commands, static_cast<std::uint32_t>(72 + 80 * segment.Sections.size()));
        AppendName16(commands, segment.Name);
        for (int field = 0; field < 4; ++field) Io::AppendU64(commands, 0);
        Io::AppendU32(commands, segment.Protection);
        Io::AppendU32(commands, segment.Protection);
        Io::AppendU32(commands, static_cast<std::uint32_t>(segment.Sections.size()));
        Io::AppendU32(commands, 0);
        for (const auto& section : segment.Sections) {
            AppendName16(commands, section.Name);
            AppendName16(commands, segment.Name);
            Io::AppendU64(commands, 0);
            Io::AppendU64(commands, section.Size);
            Io::AppendU32(commands, 0);
            Io::AppendU32(commands, section.AlignShift);
            Io::AppendU32(commands, 0);
            Io::AppendU32(commands, 0);
            Io::AppendU32(commands, section.Flags);
            for (int field = 0; field < 3; ++field) Io::AppendU32(commands, 0);
        }
        ++commandCount;
    }
    const auto linkeditCommand = commands.size();
    Io::AppendU32(commands, LcSegment64);
    Io::AppendU32(commands, 72);
    AppendName16(commands, "__LINKEDIT");
    for (int field = 0; field < 4; ++field) Io::AppendU64(commands, 0);
    Io::AppendU32(commands, VmProtRead);
    Io::AppendU32(commands, VmProtRead);
    Io::AppendU32(commands, 0);
    Io::AppendU32(commands, 0);
    ++commandCount;

    if (!input.Executable) {
        AppendDylibCommand(commands, LcIdDylib, input.InstallName);
        ++commandCount;
    }

    const auto dyldInfoCommand = commands.size();
    Io::AppendU32(commands, LcDyldInfoOnly);
    Io::AppendU32(commands, 48);
    for (int field = 0; field < 10; ++field) Io::AppendU32(commands, 0);
    ++commandCount;

    const auto symtabCommand = commands.size();
    Io::AppendU32(commands, LcSymtab);
    Io::AppendU32(commands, 24);
    for (int field = 0; field < 4; ++field) Io::AppendU32(commands, 0);
    ++commandCount;

    Io::AppendU32(commands, LcDysymtab);
    Io::AppendU32(commands, 80);
    for (int field = 0; field < 18; ++field) Io::AppendU32(commands, 0);
    ++commandCount;

    if (input.Executable) {
        AppendPathCommand(commands, LcLoadDylinker, "/usr/lib/dyld");
        ++commandCount;
    }

    const auto uuidCommand = commands.size();
    Io::AppendU32(commands, LcUuid);
    Io::AppendU32(commands, 24);
    for (int field = 0; field < 4; ++field) Io::AppendU32(commands, 0);
    ++commandCount;

    Io::AppendU32(commands, LcBuildVersion);
    Io::AppendU32(commands, 24);
    Io::AppendU32(commands, 1);
    Io::AppendU32(commands, 0x000b0000);
    Io::AppendU32(commands, 0x000b0000);
    Io::AppendU32(commands, 0);
    ++commandCount;

    std::size_t mainCommand = 0;
    if (input.Executable) {
        mainCommand = commands.size();
        Io::AppendU32(commands, LcMain);
        Io::AppendU32(commands, 24);
        Io::AppendU64(commands, 0);
        Io::AppendU64(commands, 0);
        ++commandCount;
    }

    AppendDylibCommand(commands, LcLoadDylib, "/usr/lib/libSystem.B.dylib");
    ++commandCount;
    for (const auto& library : input.Dylibs) {
        AppendDylibCommand(commands, LcLoadDylib, library);
        ++commandCount;
    }
    for (const auto& path : input.RunPaths) {
        AppendPathCommand(commands, LcRpath, path);
        ++commandCount;
    }

    std::vector<std::uint8_t> file;
    constexpr std::size_t HeaderSize = 32;
    const auto stubOffset = AlignUp(HeaderSize + commands.size(), 16);
    const auto traceJump = input.Executable && input.TraceImports;
    const auto entryOffset = stubOffset + (traceJump ? 5 : 0);
    if (entryOffset + (input.Executable ? sizeof(EntryStub) : 0) > LoadRva)
        throw Domain::RelinkerException("Mach-O load commands do not fit before the guest image");
    file.resize(LoadRva, 0);
    if (input.Executable) {
        if (traceJump) {
            const auto text = std::find_if(segments.begin(), segments.end(), [](const Segment& segment) { return segment.Name == "__APS5TEXT"; });
            if (text == segments.end() || traceEntryPatch + 4 > text->Data.size())
                throw Domain::RelinkerException("Import trace prologue is missing from the Mach-O image");
            const auto displacement = static_cast<std::int64_t>(entryOffset) - static_cast<std::int64_t>(codeRva + traceEntryPatch + 4);
            if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
                throw Domain::RelinkerException("macOS entry stub target is out of rel32 range");
            Io::WriteU32(text->Data, traceEntryPatch, static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement)));
            const auto jump = static_cast<std::int64_t>(tracePrologueRva) - static_cast<std::int64_t>(stubOffset + 5);
            if (jump < std::numeric_limits<std::int32_t>::min() || jump > std::numeric_limits<std::int32_t>::max())
                throw Domain::RelinkerException("Import trace prologue is out of rel32 range");
            file[stubOffset] = 0xe9;
            Io::WriteU32(file, stubOffset + 1, static_cast<std::uint32_t>(static_cast<std::int32_t>(jump)));
        }
        std::copy(std::begin(EntryStub), std::end(EntryStub), file.begin() + static_cast<std::ptrdiff_t>(entryOffset));
        const auto writeDisplacement = [&](const std::size_t field, const std::uint64_t targetRva) {
            const auto displacement = static_cast<std::int64_t>(targetRva) - static_cast<std::int64_t>(entryOffset + field + 4);
            if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
                throw Domain::RelinkerException("macOS entry stub target is out of rel32 range");
            Io::WriteU32(file, entryOffset + field, static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement)));
        };
        writeDisplacement(EntryStubEntryDisplacement, input.EntryRva);
        writeDisplacement(EntryStubStartDisplacement, metaRva + startSlot);
    }

    for (std::size_t index = guestSegments; index < segments.size(); ++index) {
        auto& segment = segments[index];
        segment.FileOffset = segment.Data.empty() ? 0 : file.size();
        file.insert(file.end(), segment.Data.begin(), segment.Data.end());
    }

    const auto linkeditOffset = file.size();
    const auto rebaseOffset = file.size();
    file.insert(file.end(), rebase.begin(), rebase.end());
    const auto bindOffset = file.size();
    file.insert(file.end(), bind.begin(), bind.end());
    const auto exportOffset = file.size();
    file.insert(file.end(), exports.begin(), exports.end());
    const auto stringOffset = file.size();
    file.insert(file.end(), 8, 0);
    const auto linkeditSize = file.size() - linkeditOffset;
    const auto imageEnd = segments.back().Rva + AlignUp(segments.back().MemorySize, PageSize);
    if (file.size() > std::numeric_limits<std::uint32_t>::max())
        throw Domain::RelinkerException("Mach-O output exceeds 4 GiB");
    if (imageBase + imageEnd + AlignUp(linkeditSize, PageSize) > UserAddressEnd)
        throw Domain::RelinkerException("Mach-O image ends outside the macOS user address space");

    const auto writeSegment = [&](const std::size_t at, const std::uint64_t vmaddr, const std::uint64_t vmsize, const std::uint64_t fileoff, const std::uint64_t filesize) {
        Io::WriteU64(commands, at + 24, vmaddr);
        Io::WriteU64(commands, at + 32, vmsize);
        Io::WriteU64(commands, at + 40, fileoff);
        Io::WriteU64(commands, at + 48, filesize);
    };
    std::size_t index = 0;
    if (input.Executable) writeSegment(segmentCommandOffsets[index++], 0, imageBase, 0, 0);
    writeSegment(segmentCommandOffsets[index++], imageBase, LoadRva, 0, LoadRva);
    for (; index < segments.size(); ++index) {
        const auto& segment = segments[index];
        writeSegment(segmentCommandOffsets[index], imageBase + segment.Rva, AlignUp(segment.MemorySize, PageSize), segment.FileOffset, segment.Data.size());
        for (std::size_t sectionIndex = 0; sectionIndex < segment.Sections.size(); ++sectionIndex) {
            const auto& section = segment.Sections[sectionIndex];
            const auto at = segmentCommandOffsets[index] + 72 + sectionIndex * 80;
            Io::WriteU64(commands, at + 32, imageBase + segment.Rva + section.Offset);
            Io::WriteU32(commands, at + 48, static_cast<std::uint32_t>(segment.FileOffset + section.Offset));
        }
    }
    writeSegment(linkeditCommand, imageBase + imageEnd, AlignUp(linkeditSize, PageSize), linkeditOffset, linkeditSize);
    Io::WriteU32(commands, dyldInfoCommand + 8, static_cast<std::uint32_t>(rebaseOffset));
    Io::WriteU32(commands, dyldInfoCommand + 12, static_cast<std::uint32_t>(rebase.size()));
    Io::WriteU32(commands, dyldInfoCommand + 16, static_cast<std::uint32_t>(bindOffset));
    Io::WriteU32(commands, dyldInfoCommand + 20, static_cast<std::uint32_t>(bind.size()));
    Io::WriteU32(commands, dyldInfoCommand + 40, static_cast<std::uint32_t>(exportOffset));
    Io::WriteU32(commands, dyldInfoCommand + 44, static_cast<std::uint32_t>(exports.size()));
    Io::WriteU32(commands, symtabCommand + 8, static_cast<std::uint32_t>(stringOffset));
    Io::WriteU32(commands, symtabCommand + 16, static_cast<std::uint32_t>(stringOffset));
    Io::WriteU32(commands, symtabCommand + 20, 8);
    if (input.Executable) Io::WriteU64(commands, mainCommand + 8, stubOffset);

    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const auto byte : file) hash = (hash ^ byte) * 0x100000001b3ull;
    Io::WriteU64(commands, uuidCommand + 8, hash);
    Io::WriteU64(commands, uuidCommand + 16, hash * 0x9e3779b97f4a7c15ull);
    commands[uuidCommand + 14] = static_cast<std::uint8_t>((commands[uuidCommand + 14] & 0x0f) | 0x40);

    Io::WriteU32(file, 0, MhMagic64);
    Io::WriteU32(file, 4, CpuTypeX8664);
    Io::WriteU32(file, 8, CpuSubtypeX8664All);
    Io::WriteU32(file, 12, input.Executable ? MhExecute : MhDylib);
    Io::WriteU32(file, 16, commandCount);
    Io::WriteU32(file, 20, static_cast<std::uint32_t>(commands.size()));
    const bool weakDefinitions = std::any_of(input.Exports.begin(), input.Exports.end(), [](const auto& entry) { return (entry.Flags & ExportWeakDefinition) != 0; });
    Io::WriteU32(file, 24, MhNoUndefs | MhDyldLink | MhTwoLevel | (weakDefinitions ? MhWeakDefines : 0));
    std::copy(commands.begin(), commands.end(), file.begin() + HeaderSize);
    return file;
}

}
