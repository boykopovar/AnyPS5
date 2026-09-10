#include "WindowsTlsBuilder.hpp"
#include "WindowsStubEmitter.hpp"
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <bit>
#include <limits>
#include <set>

namespace Elfpatcher::Windows {

namespace {

struct TlsAccess {
    std::uint32_t Rva;
    std::size_t Length;
};

void patchAccess(std::vector<PeSection>& sections, const TlsAccess& access, const std::uint32_t target) {
    for (auto& section : sections) {
        if (access.Rva < section.Rva || access.Rva - section.Rva >= section.Data.size())
            continue;
        const auto offset = access.Rva - section.Rva;
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

PeDirectory WindowsTlsBuilder::Build(const std::vector<std::uint8_t>& source, const std::vector<Domain::ProgramHeader>& headers, const WindowsLoadImage& image, std::vector<PeSection>& sections, std::vector<std::uint32_t>& relocations, std::uint32_t& nextRva) const {
    const Domain::ProgramHeader* tls = nullptr;
    const Codegen::X64InstructionDecoder decoder;

    std::vector<TlsAccess> accesses;
    std::set<std::uint32_t> branchTargets;

    for (const auto& header : headers) {
        if (header.Type == 7) {
            if (tls != nullptr)
                throw Domain::RelinkerException("Multiple ELF TLS segments");
            tls = &header;
        }
        if (header.Type != 1 || (header.Flags & 1) == 0)
            continue;
        for (std::uint64_t offset = 0; offset < header.FileSize;) {
            const auto* bytes = source.data() + header.Offset + offset;
            const auto info = decoder.DecodeInstruction(bytes, header.FileSize - offset);
            const auto rva = image.GetRva(header.MappedAddress + offset, info.Length);

            if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
                const auto target = static_cast<std::int64_t>(rva) + static_cast<std::int64_t>(info.Length) + info.BranchDisp;
                if (target >= 0 && target <= std::numeric_limits<std::uint32_t>::max())
                    branchTargets.insert(static_cast<std::uint32_t>(target));
            }

            if (info.SegmentPrefix != 0) {
                const auto position = info.OpcodeOffset;
                if (info.SegmentPrefix != 0x64 || info.RexPrefix != 0x48 || info.Length - position != 7 || bytes[position] != 0x8b || bytes[position + 1] != 0x04 || bytes[position + 2] != 0x25 || Io::ReadU32(source, header.Offset + offset + position + 3) != 0)
                    throw Domain::RelinkerException("Unsupported Windows guest TLS instruction", header.Offset + offset);
                accesses.push_back({rva, info.Length});
            }
            offset += info.Length;
        }
    }

    if (tls == nullptr) {
        if (!accesses.empty())
            throw Domain::RelinkerException("Guest TLS access without PT_TLS");
        return {};
    }

    if (tls->MemorySize == 0 || tls->FileSize > tls->MemorySize || tls->Offset > source.size() || tls->FileSize > source.size() - tls->Offset || !std::has_single_bit(tls->Alignment) || tls->Alignment > 8192 || tls->MemorySize > 0x7fff0000u)
        throw Domain::RelinkerException("Invalid or unsupported ELF TLS layout", tls->Offset);
    const auto alignment = std::max<std::uint64_t>(tls->Alignment, 16);
    const auto blockSize = CheckedRva((tls->MemorySize + alignment - 1) & ~(alignment - 1));
    const auto templateOffset = CheckedRva((64 + alignment - 1) & ~(alignment - 1));
    PeSection data{".gtls", nextRva, SectionRead | SectionWrite | 0x40u, std::vector<std::uint8_t>(templateOffset + blockSize + 16)};

    const auto indexRva = nextRva + 40;
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
        code.Emit({0x48, 0x8d, 0x64, 0x24, 0x80, 0x51});
        loadPointer();
        code.Emit({0x59, 0x48, 0x8d, 0xa4, 0x24, 0x80, 0, 0, 0});
        code.Rip({0xe9}, CheckedRva(access.Rva + access.Length));
    }

    std::copy_n(source.begin() + tls->Offset, tls->FileSize, data.Data.begin() + templateOffset);
    const auto writeAddress = [&](const std::size_t offset, const std::uint32_t rva) {
        Io::WriteU64(data.Data, offset, ImageBase + rva);
        relocations.push_back(CheckedRva(data.Rva + offset));
    };

    writeAddress(0, templateRva);
    writeAddress(8, CheckedRva(templateRva + blockSize + 16));
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
