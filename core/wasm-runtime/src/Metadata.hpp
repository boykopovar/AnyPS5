#ifndef CORE_WASM_RUNTIME_SRC_METADATA_HPP
#define CORE_WASM_RUNTIME_SRC_METADATA_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace WasmRuntime {

enum class RelocKind : std::uint8_t {
    Relative = 1,
    GuestSymbol = 2,
    ImportStub = 3,
    HostData = 4,
    TlsModuleId = 5,
    TlsOffset = 6,
    DtpOffset = 7,
};

struct SegmentMeta {
    std::uint64_t vaddr = 0;
    std::uint64_t memsz = 0;
    std::uint32_t flags = 0;
    std::span<const std::uint8_t> data;
};

struct PhdrMeta {
    std::uint32_t kind = 0;
    std::uint32_t flags = 0;
    std::uint64_t vaddr = 0;
    std::uint64_t memsz = 0;
};

struct TlsMeta {
    std::uint64_t initVaddr = 0;
    std::uint64_t initSize = 0;
    std::uint64_t memSize = 0;
    std::uint64_t align = 0;
};

struct ArrayMeta {
    std::uint64_t vaddr = 0;
    std::uint32_t count = 0;
};

struct ExportMeta {
    std::string nid;
    std::uint64_t vaddr = 0;
    bool data = false;
};

struct RelocMeta {
    std::uint64_t offset = 0;
    RelocKind kind = RelocKind::Relative;
    std::int64_t addend = 0;
    std::uint32_t target = 0;
};

struct SymbolRef {
    std::uint32_t image = 0;
    std::uint64_t vaddr = 0;
};

struct ImageMeta {
    std::string name;
    std::uint64_t span = 0;
    std::uint64_t align = 0;
    std::vector<SegmentMeta> segments;
    std::vector<PhdrMeta> phdrs;
    std::optional<TlsMeta> tls;
    std::optional<std::uint64_t> entry;
    std::optional<std::uint64_t> init;
    std::optional<std::uint64_t> fini;
    std::optional<ArrayMeta> initArray;
    std::optional<ArrayMeta> finiArray;
    std::vector<std::string> needed;
    std::vector<ExportMeta> exports;
    std::vector<RelocMeta> relocations;
};

struct Metadata {
    std::vector<ImageMeta> images;
    std::vector<std::string> stubs;
    std::vector<std::string> hostData;
    std::vector<SymbolRef> symbols;
    std::span<const std::uint8_t> addrMap;
};

Metadata DecodeMetadata(std::span<const std::uint8_t> bytes);

}  // namespace WasmRuntime

#endif
