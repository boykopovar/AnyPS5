#include "Program.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "wasm-runtime/Guest.hpp"
#include "wasm-runtime/GuestCpu.hpp"
#include "wasm-runtime/NidThunk.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace WasmRuntime {
namespace {

constexpr std::uint64_t ImageAlignment = 0x10000;
constexpr std::uint64_t ThreadControlBlockSize = 0x100;

std::unique_ptr<Program> g_program;

void* GuestPointer(std::uint64_t address) {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(address));
}

std::uint64_t AlignUp(std::uint64_t value, std::uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

std::uint64_t Allocate(std::uint64_t bytes, std::uint64_t alignment) {
    return reinterpret_cast<std::uintptr_t>(GuestArena::GuestArenaAllocate_nid_postfix(static_cast<std::size_t>(bytes), static_cast<std::size_t>(alignment)));
}

void PlaceImages(Program& program) {
    for (const auto& image : program.metadata.images) {
        std::uint64_t lowest = ~std::uint64_t{0};
        for (const auto& segment : image.segments) lowest = std::min(lowest, segment.vaddr);
        if (!image.segments.empty() && lowest != 0) throw std::runtime_error(image.name + " is not position independent");
        const auto base = Allocate(AlignUp(std::max<std::uint64_t>(image.span, 1), ImageAlignment), std::max(image.align, ImageAlignment));
        for (const auto& segment : image.segments) {
            std::memcpy(GuestPointer(base + segment.vaddr), segment.data.data(), segment.data.size());
            std::memset(GuestPointer(base + segment.vaddr + segment.data.size()), 0, segment.memsz - segment.data.size());
        }
        program.bases.push_back(base);
    }
    const auto stubBytes = std::max<std::uint64_t>(program.metadata.stubs.size() * StubStride, StubStride);
    program.stubBase = Allocate(stubBytes, ImageAlignment);
    std::memset(GuestPointer(program.stubBase), 0xCC, stubBytes);
    program.addrMap = Allocate(std::max<std::uint64_t>(program.metadata.addrMap.size(), 16), ImageAlignment);
    std::memcpy(GuestPointer(program.addrMap), program.metadata.addrMap.data(), program.metadata.addrMap.size());
}

void LayOutTls(Program& program) {
    auto& tls = program.tls;
    tls.offsets.assign(program.metadata.images.size(), 0);
    std::uint64_t used = 0;
    for (std::size_t index = 0; index < program.metadata.images.size(); ++index) {
        const auto& block = program.metadata.images[index].tls;
        if (!block) continue;
        const auto alignment = std::max<std::uint64_t>(block->align, 1);
        used = AlignUp(used + block->memSize, alignment);
        tls.offsets[index] = -static_cast<std::int64_t>(used);
        tls.alignment = std::max(tls.alignment, alignment);
    }
    tls.staticSize = AlignUp(used, tls.alignment);
}

std::vector<const void*> ResolveHostData(const Program& program) {
    std::vector<const void*> addresses;
    std::string missing;
    for (const auto& name : program.metadata.hostData) {
        const void* address = FindDataExport(name.c_str());
        if (!address) missing += (missing.empty() ? "" : ", ") + name;
        addresses.push_back(address);
    }
    if (!missing.empty()) throw std::runtime_error("the runtime does not provide the data symbols " + missing);
    return addresses;
}

void Relocate(Program& program) {
    const auto hostData = ResolveHostData(program);
    const auto& metadata = program.metadata;
    for (std::size_t index = 0; index < metadata.images.size(); ++index) {
        const auto base = program.bases[index];
        for (const auto& relocation : metadata.images[index].relocations) {
            const auto symbol = [&] {
                if (relocation.target >= metadata.symbols.size()) throw std::runtime_error("relocation names a missing symbol");
                return metadata.symbols[relocation.target];
            };
            const auto addend = static_cast<std::uint64_t>(relocation.addend);
            std::uint64_t value = 0;
            switch (relocation.kind) {
            case RelocKind::Relative:
                value = base + addend;
                break;
            case RelocKind::GuestSymbol: {
                const auto target = symbol();
                value = program.bases.at(target.image) + target.vaddr + addend;
                break;
            }
            case RelocKind::ImportStub:
                value = program.stubBase + StubStride * relocation.target + addend;
                break;
            case RelocKind::HostData:
                value = reinterpret_cast<std::uintptr_t>(hostData.at(relocation.target)) + addend;
                break;
            case RelocKind::TlsModuleId:
                value = symbol().image + 1;
                break;
            case RelocKind::TlsOffset: {
                const auto target = symbol();
                value = static_cast<std::uint64_t>(program.tls.offsets.at(target.image) + static_cast<std::int64_t>(target.vaddr) + relocation.addend);
                break;
            }
            case RelocKind::DtpOffset:
                value = symbol().vaddr + addend;
                break;
            }
            std::memcpy(GuestPointer(base + relocation.offset), &value, sizeof(value));
        }
    }
}

}  // namespace

Program& LoadProgram(std::vector<std::uint8_t> metadataBytes) {
    if (g_program) throw std::runtime_error("a program is already loaded");
    auto program = std::make_unique<Program>();
    program->metadataBytes = std::move(metadataBytes);
    program->metadata = DecodeMetadata(program->metadataBytes);
    PlaceImages(*program);
    LayOutTls(*program);
    Relocate(*program);
    g_program = std::move(program);
    return *g_program;
}

Program& CurrentProgram() {
    if (!g_program) throw std::runtime_error("no program is loaded");
    return *g_program;
}

bool ProgramLoaded() {
    return g_program != nullptr;
}

std::uint64_t ExportAddress(const Program& program, std::size_t image, const std::string& nid) {
    for (const auto& exported : program.metadata.images.at(image).exports) {
        if (exported.nid == nid) return program.bases[image] + exported.vaddr;
    }
    throw std::runtime_error(program.metadata.images.at(image).name + " does not export " + nid);
}

void InitializeImages(Program& program) {
    const auto& images = program.metadata.images;
    for (std::size_t index = images.size(); index-- > 0;) {
        const auto base = program.bases[index];
        if (images[index].init) CallGuest(base + *images[index].init, {});
        if (const auto& array = images[index].initArray) {
            for (std::uint32_t slot = 0; slot < array->count; ++slot) {
                std::uint64_t target;
                std::memcpy(&target, GuestPointer(base + array->vaddr + 8 * slot), sizeof(target));
                if (target != 0 && target != ~std::uint64_t{0}) CallGuest(target, {});
            }
        }
    }
}

ThreadTls CreateThreadTls(const Program& program) {
    const auto& tls = program.tls;
    const auto bytes = tls.staticSize + ThreadControlBlockSize;
    const auto block = Allocate(bytes, std::max<std::uint64_t>(tls.alignment, 64));
    const auto threadPointer = block + tls.staticSize;
    std::memset(GuestPointer(block), 0, bytes);
    for (std::size_t index = 0; index < program.metadata.images.size(); ++index) {
        const auto& image = program.metadata.images[index];
        if (!image.tls) continue;
        const auto start = static_cast<std::uint64_t>(static_cast<std::int64_t>(threadPointer) + tls.offsets[index]);
        std::memcpy(GuestPointer(start), GuestPointer(program.bases[index] + image.tls->initVaddr), image.tls->initSize);
    }
    std::memcpy(GuestPointer(threadPointer), &threadPointer, sizeof(threadPointer));
    return {block, bytes, threadPointer};
}

}  // namespace WasmRuntime
