#include <elfpatcher/windows/WindowsTlsBuilder.hpp>
#include <elfpatcher/windows/WindowsStubEmitter.hpp>
#include <elfpatcher/windows/WindowsTlsTemplateBuilder.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <relinker/analysis/CodeInstructionCollector.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <bit>
#include <iomanip>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>

namespace Elfpatcher::Windows {

namespace {

constexpr std::size_t kJumpSize = 5;

struct MovedInstruction {
    std::uint32_t Rva;
    std::vector<std::uint8_t> Bytes;
    std::optional<std::size_t> RipDisplacement;
};

struct TlsAccess {
    std::uint32_t Rva;
    Domain::FileByteOffset FileOffset;
    std::size_t Length;
    bool StoreImmediate;
    std::uint32_t Immediate;
    std::uint8_t Register;
    std::uint32_t Displacement;
    std::uint8_t AluOpcode;
    std::vector<std::uint8_t> Address = {};
    bool Wide = true;
    std::vector<MovedInstruction> Moved = {};
};

bool isAluReadOpcode(std::uint8_t opcode) {
    return opcode == 0x03 || opcode == 0x0b || opcode == 0x13 || opcode == 0x1b || opcode == 0x23 || opcode == 0x2b || opcode == 0x33 || opcode == 0x3b;
}

std::optional<std::vector<std::uint8_t>> registerAddress(const std::uint8_t* bytes, const std::size_t position, const std::size_t length, const std::uint8_t rex) {
    if (length - position < 2)
        return std::nullopt;
    const auto modrm = bytes[position + 1];
    const auto mod = modrm >> 6;
    if (mod == 3 || (mod == 0 && (modrm & 7) == 5))
        return std::nullopt;
    std::size_t size = 2;
    if ((modrm & 7) == 4) {
        if (length - position < 3)
            return std::nullopt;
        const auto sib = bytes[position + 2];
        const bool noBase = mod == 0 && (sib & 7) == 5;
        const bool noIndex = ((sib >> 3) & 7) == 4 && (rex & 2) == 0;
        if ((noBase && noIndex) || ((sib & 7) == 4 && (rex & 1) == 0))
            return std::nullopt;
        size += noBase ? 5 : 1;
    }
    size += mod == 1 ? 1 : mod == 2 ? 4 : 0;
    if (length - position != size)
        return std::nullopt;
    std::vector<std::uint8_t> address{static_cast<std::uint8_t>(0x48 | (rex & 3)), 0x8d, static_cast<std::uint8_t>(modrm & 0xc7)};
    address.insert(address.end(), bytes + position + 2, bytes + length);
    return address;
}

void patchAccess(std::vector<PeSection>& sections, const TlsAccess& access, const std::uint32_t target) {
    for (auto& section : sections) {
        if (access.Rva < section.Rva || access.Rva - section.Rva >= section.Data.size())
            continue;
        const auto offset = access.Rva - section.Rva;
        if (access.Length < kJumpSize)
            throw Domain::RelinkerException("TLS instruction is shorter than a jump", access.Rva);
        if (access.Length > section.Data.size() - offset)
            throw Domain::RelinkerException("TLS instruction crosses a PE section boundary", access.Rva);
        WindowsStubEmitter jump(access.Rva);
        jump.Rip({0xe9}, target);
        const auto bytes = jump.TakeBytes();
        std::fill_n(section.Data.begin() + offset, access.Length, 0x90);
        std::copy(bytes.begin(), bytes.end(), section.Data.begin() + offset);
        return;
    }
    throw Domain::RelinkerException("TLS instruction is outside the PE image", access.Rva);
}

}

PeDirectory WindowsTlsBuilder::Build(const std::vector<std::uint8_t>& source, const std::vector<Domain::ProgramHeader>& headers, const WindowsLoadImage& image, std::vector<PeSection>& sections, std::vector<std::uint32_t>& relocations, std::uint32_t& nextRva, std::uint32_t* tlsIndexRva) const {
    const Domain::ProgramHeader* tls = nullptr;
    const Codegen::X64InstructionDecoder decoder;

    std::vector<TlsAccess> accesses;
    std::set<std::uint32_t> branchTargets;
    const auto instructions = Relinker::CodeInstructionCollector().Collect(source, headers);

    for (const auto& header : headers) {
        if (header.Type == 7) {
            if (tls != nullptr)
                throw Domain::RelinkerException("Multiple ELF TLS segments");
            tls = &header;
        }
        if (header.Type != 1 || (header.Flags & 1) == 0)
            continue;
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
                if (target >= 0 && target <= std::numeric_limits<std::uint32_t>::max())
                    branchTargets.insert(static_cast<std::uint32_t>(target));
            }

            if (info.SegmentPrefix != 0 && !(insidePrevious && info.SegmentPrefix != 0x64)) {
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
                const bool aluRead = supportedPrefixes && (info.RexPrefix == 0x48 || info.RexPrefix == 0x4c) && loadRegister != 4 && info.Length - position == 7 && isAluReadOpcode(bytes[position]) && (bytes[position + 1] & 0xc7) == 0x04 && bytes[position + 2] == 0x25;
                const bool wide = (info.RexPrefix & 8) != 0;
                const auto address = supportedPrefixes && !loadValue && !aluRead && bytes[position] == 0x8b && (wide || !hasOperandSizePrefix) ? registerAddress(bytes, position, info.Length, info.RexPrefix) : std::nullopt;
                const auto addressRegister = static_cast<std::uint8_t>(((bytes[position + 1] >> 3) & 7) | ((info.RexPrefix & 4) << 1));
                if (address && addressRegister != 4) {
                    std::vector<MovedInstruction> moved;
                    auto length = info.Length;
                    while (length < kJumpSize) {
                        if (length >= header.FileSize - offset)
                            throw Domain::RelinkerException("Short guest TLS instruction ends its segment", header.Offset + offset);
                        const auto next = decoder.DecodeInstruction(bytes + length, header.FileSize - offset - length);
                        if (next.FlowKind != Codegen::ControlFlowKind::Sequential || next.HasBranchTarget || next.SegmentPrefix != 0)
                            throw Domain::RelinkerException("Short guest TLS instruction is followed by an instruction that cannot move", header.Offset + offset + length);
                        moved.push_back({CheckedRva(rva + length), std::vector<std::uint8_t>(bytes + length, bytes + length + next.Length), next.HasRipRelativeDisp ? std::optional<std::size_t>(next.RipRelativeDispOffset) : std::nullopt});
                        length += next.Length;
                    }
                    accesses.push_back({rva, header.Offset + offset, length, false, 0, addressRegister, 0, 0, *address, wide, std::move(moved)});
                    continue;
                }
                if (!loadValue && !storeImmediate && !aluRead) {
                    std::ostringstream message;
                    message << "Unsupported Windows guest TLS instruction (bytes:" << std::hex << std::setfill('0');
                    for (std::size_t index = 0; index < info.Length; ++index)
                        message << ' ' << std::setw(2) << static_cast<unsigned int>(bytes[index]);
                    message << ')';
                    throw Domain::RelinkerException(message.str(), header.Offset + offset);
                }
                accesses.push_back({rva, header.Offset + offset, info.Length, storeImmediate, storeImmediate ? Io::ReadU32(source, header.Offset + offset + position + 7) : 0, storeImmediate ? std::uint8_t{0} : loadRegister, storeImmediate ? 0 : Io::ReadU32(source, header.Offset + offset + position + 3), aluRead ? bytes[position] : std::uint8_t{0}});
            }
        }
    }

    if (tls == nullptr) {
        if (!accesses.empty())
            throw Domain::RelinkerException("Guest TLS access without PT_TLS");
        return {};
    }

    if (tls->FileSize > tls->MemorySize || tls->Offset > source.size() || tls->FileSize > source.size() - tls->Offset || (tls->Alignment > 1 && !std::has_single_bit(tls->Alignment)) || tls->Alignment > 8192 || tls->MemorySize > 0x7fff0000u)
        throw Domain::RelinkerException("Invalid or unsupported ELF TLS layout", tls->Offset);
    // Native homebrew linkers can emit an empty PT_TLS placeholder.
    // It needs no Windows TLS directory unless guest code accesses TLS.
    if (tls->MemorySize == 0) {
        if (!accesses.empty())
            throw Domain::RelinkerException("Guest TLS access with empty PT_TLS", tls->Offset);
        return {};
    }
    const auto alignment = std::max<std::uint64_t>(tls->Alignment, 16);
    const auto blockSize = CheckedRva((tls->MemorySize + alignment - 1) & ~(alignment - 1));
    const auto templateOffset = CheckedRva((64 + alignment - 1) & ~(alignment - 1));
    constexpr std::uint32_t threadControlBlockSize = 0x30;
    constexpr std::int64_t stackGuardDisplacement = 0x28;
    for (const auto& access : accesses) {
        if (access.StoreImmediate || access.Displacement == 0) continue;
        const auto displacement = static_cast<std::int64_t>(std::bit_cast<std::int32_t>(access.Displacement));
        if (displacement != stackGuardDisplacement && (displacement > 0 || displacement < -static_cast<std::int64_t>(blockSize)))
            throw Domain::RelinkerException("Windows guest TLS load displacement " + std::to_string(displacement) + " is not in the thread TLS block, 0 or the stack guard at 0x28", access.FileOffset);
    }
    PeSection data{".gtls", nextRva, SectionRead | SectionWrite | 0x40u, std::vector<std::uint8_t>(templateOffset + blockSize + threadControlBlockSize)};

    const auto indexRva = nextRva + 40;
    if (tlsIndexRva != nullptr) *tlsIndexRva = indexRva;
    const auto callbackTableRva = nextRva + 48;
    const auto templateRva = nextRva + templateOffset;
    const auto codeRva = AlignRva(nextRva + data.Data.size());

    WindowsStubEmitter code(codeRva);
    const auto loadPointer = [&] {
        code.Rip({0x8b, 0x0d}, indexRva);
        code.Emit({0x65, 0x48, 0x8b, 0x04, 0x25, 0x58, 0, 0, 0, 0x48, 0x8b, 0x04, 0xc8, 0x48, 0x8d, 0x80});
        code.U32(blockSize);
    };

    code.Emit({0x83, 0xfa, 1});
    const auto processAttach = code.Branch({0x0f, 0x84});
    code.Emit({0x83, 0xfa, 2});
    const auto skipCallback = code.Branch({0x0f, 0x85});
    code.PatchBranch(processAttach, code.GetRva());
    loadPointer();

    code.Emit({0x48, 0x89, 0x00});
    code.PatchBranch(skipCallback, code.GetRva());
    code.Emit({0xc3});

    for (const auto& access : accesses) {
        const auto target = branchTargets.upper_bound(access.Rva);
        if (target != branchTargets.end() && *target < access.Rva + access.Length)
            throw Domain::RelinkerException("Branch enters a guest TLS instruction", *target);
        patchAccess(sections, access, code.GetRva());
        code.Emit({0x48, 0x8d, 0x64, 0x24, 0x80});
        if (!access.Address.empty()) {
            code.Emit({0x51});
            code.Emit({0x50});
            for (const auto byte : access.Address)
                code.Emit({byte});
            code.Emit({0x50});
            loadPointer();
            code.Emit({0x59});
            const auto rex = static_cast<std::uint8_t>((access.Wide ? 0x48 : 0x40) | ((access.Register >> 3) << 2));
            if (rex != 0x40)
                code.Emit({rex});
            code.Emit({0x8b, static_cast<std::uint8_t>(0x04 | ((access.Register & 7) << 3)), 0x08});
            if (access.Register == 0) {
                code.Emit({0x48, 0x8d, 0x64, 0x24, 0x08});
                code.Emit({0x59});
            } else if (access.Register == 1) {
                code.Emit({0x58});
                code.Emit({0x48, 0x8d, 0x64, 0x24, 0x08});
            } else {
                code.Emit({0x58});
                code.Emit({0x59});
            }
        } else if (access.AluOpcode != 0 && access.Register == 0) {
            code.Emit({0x51});
            code.Emit({0x52});
            code.Emit({0x50});
            loadPointer();
            code.Emit({0x48, 0x8b, 0x90});
            code.U32(access.Displacement);
            code.Emit({0x58});
            code.Emit({0x48, access.AluOpcode, 0xc2});
            code.Emit({0x5a});
            code.Emit({0x59});
        } else if (access.AluOpcode != 0 && access.Register == 1) {
            code.Emit({0x51});
            code.Emit({0x50});
            loadPointer();
            code.Emit({0x48, 0x8b, 0x80});
            code.U32(access.Displacement);
            code.Emit({0x48, 0x8b, 0x4c, 0x24, 0x08});
            code.Emit({0x48, access.AluOpcode, 0xc8});
            code.Emit({0x58});
            code.Emit({0x48, 0x8d, 0x64, 0x24, 0x08});
        } else {
            const bool preserveAccumulator = access.StoreImmediate || access.Register != 0;
            const bool preserveCounter = access.Register != 1;
            if (preserveCounter) code.Emit({0x51});
            if (preserveAccumulator) code.Emit({0x50});
            loadPointer();
            if (access.AluOpcode != 0) {
                code.Emit({0x48, 0x8b, 0x80});
                code.U32(access.Displacement);
                code.Emit({static_cast<std::uint8_t>(0x48 | ((access.Register >> 3) << 2)), access.AluOpcode, static_cast<std::uint8_t>(0xc0 | ((access.Register & 7) << 3))});
            } else if (!access.StoreImmediate && access.Displacement != 0) {
                code.Emit({0x48, 0x8b, 0x80});
                code.U32(access.Displacement);
            }
            if (access.StoreImmediate) {
                code.Emit({0xc7, 0x40, 0x28});
                code.U32(access.Immediate);
            } else if (access.AluOpcode == 0 && access.Register != 0) {
                code.Emit({static_cast<std::uint8_t>(0x48 | (access.Register >> 3)), 0x89, static_cast<std::uint8_t>(0xc0 | (access.Register & 7))});
            }
            if (preserveAccumulator) code.Emit({0x58});
            if (preserveCounter) code.Emit({0x59});
        }
        code.Emit({0x48, 0x8d, 0xa4, 0x24, 0x80, 0, 0, 0});
        for (const auto& instruction : access.Moved) {
            auto bytes = instruction.Bytes;
            if (instruction.RipDisplacement) {
                const auto target = static_cast<std::int64_t>(instruction.Rva) + static_cast<std::int64_t>(bytes.size()) + std::bit_cast<std::int32_t>(Io::ReadU32(bytes, *instruction.RipDisplacement));
                const auto displacement = target - (static_cast<std::int64_t>(code.GetRva()) + static_cast<std::int64_t>(bytes.size()));
                if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
                    throw Domain::RelinkerException("Moved RIP-relative instruction exceeds rel32 range from its TLS stub", access.FileOffset);
                Io::WriteU32(bytes, *instruction.RipDisplacement, static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement)));
            }
            for (const auto byte : bytes)
                code.Emit({byte});
        }
        code.Rip({0xe9}, CheckedRva(access.Rva + access.Length));
    }

    const auto templateBytes = WindowsTlsTemplateBuilder().Build(*tls, image, sections, templateRva, relocations);
    std::copy(templateBytes.begin(), templateBytes.end(), data.Data.begin() + templateOffset);
    const auto writeAddress = [&](const std::size_t offset, const std::uint32_t rva) {
        Io::WriteU64(data.Data, offset, ImageBase + rva);
        relocations.push_back(CheckedRva(data.Rva + offset));
    };

    writeAddress(0, templateRva);
    writeAddress(8, CheckedRva(templateRva + blockSize + threadControlBlockSize));
    writeAddress(16, indexRva);
    writeAddress(24, callbackTableRva);
    writeAddress(48, codeRva);

    Io::WriteU32(data.Data, 36, static_cast<std::uint32_t>(std::bit_width(alignment)) << 20);
    const PeDirectory directory{data.Rva, 40};
    sections.push_back(std::move(data));
    sections.push_back({".gtcode", codeRva, SectionRead | SectionExecute | 0x20u, code.TakeBytes()});
    nextRva = AlignRva(codeRva + sections.back().Data.size());
    return directory;
}

}
