#include "Metadata.hpp"
#include <cstring>
#include <stdexcept>
#include <tuple>

namespace WasmRuntime {
namespace {

constexpr char Magic[8] = {'A', 'P', 'S', '5', 'I', 'M', 'G', '\0'};
constexpr std::uint32_t Version = 1;
constexpr std::uint64_t None = ~std::uint64_t{0};

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : _bytes(bytes) {}

    std::span<const std::uint8_t> Take(std::size_t length) {
        if (length > _bytes.size() - _position) throw std::runtime_error("image metadata is truncated");
        const auto slice = _bytes.subspan(_position, length);
        _position += length;
        return slice;
    }

    std::uint8_t U8() { return Take(1)[0]; }

    std::uint32_t U32() {
        std::uint32_t value;
        std::memcpy(&value, Take(4).data(), 4);
        return value;
    }

    std::uint64_t U64() {
        std::uint64_t value;
        std::memcpy(&value, Take(8).data(), 8);
        return value;
    }

    std::string String() {
        const auto length = U32();
        const auto bytes = Take(length);
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }

    std::optional<std::uint64_t> Option() {
        const auto value = U64();
        if (value == None) return std::nullopt;
        return value;
    }

    std::optional<ArrayMeta> Array() {
        const auto vaddr = U64();
        const auto count = U32();
        if (vaddr == None) return std::nullopt;
        return ArrayMeta{vaddr, count};
    }

    std::size_t Position() const { return _position; }

private:
    std::span<const std::uint8_t> _bytes;
    std::size_t _position = 0;
};

}  // namespace

Metadata DecodeMetadata(std::span<const std::uint8_t> bytes) {
    Reader reader(bytes);
    if (std::memcmp(reader.Take(8).data(), Magic, sizeof(Magic)) != 0) throw std::runtime_error("image metadata has a wrong magic");
    if (const auto version = reader.U32(); version != Version) throw std::runtime_error("image metadata version " + std::to_string(version) + " is not supported");
    const auto imageCount = reader.U32();
    const auto stubCount = reader.U32();
    const auto mapOffset = reader.U64();
    const auto mapSize = reader.U64();

    Metadata metadata;
    std::vector<std::tuple<std::size_t, std::size_t, std::uint64_t, std::uint64_t>> pendingSegments;
    for (std::uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
        ImageMeta image;
        image.name = reader.String();
        image.span = reader.U64();
        image.align = reader.U64();
        for (auto count = reader.U32(); count > 0; --count) {
            SegmentMeta segment;
            segment.vaddr = reader.U64();
            const auto fileSize = reader.U64();
            segment.memsz = reader.U64();
            segment.flags = reader.U32();
            const auto offset = reader.U64();
            pendingSegments.emplace_back(metadata.images.size(), image.segments.size(), offset, fileSize);
            image.segments.push_back(segment);
        }
        for (auto count = reader.U32(); count > 0; --count) {
            PhdrMeta phdr;
            phdr.kind = reader.U32();
            phdr.flags = reader.U32();
            phdr.vaddr = reader.U64();
            phdr.memsz = reader.U64();
            image.phdrs.push_back(phdr);
        }
        if (reader.U8() != 0) {
            TlsMeta tls;
            tls.initVaddr = reader.U64();
            tls.initSize = reader.U64();
            tls.memSize = reader.U64();
            tls.align = reader.U64();
            image.tls = tls;
        }
        image.entry = reader.Option();
        image.init = reader.Option();
        image.fini = reader.Option();
        image.initArray = reader.Array();
        image.finiArray = reader.Array();
        for (auto count = reader.U32(); count > 0; --count) image.needed.push_back(reader.String());
        for (auto count = reader.U32(); count > 0; --count) {
            ExportMeta exported;
            exported.nid = reader.String();
            exported.vaddr = reader.U64();
            exported.data = reader.U8() != 0;
            image.exports.push_back(std::move(exported));
        }
        for (auto count = reader.U32(); count > 0; --count) {
            RelocMeta relocation;
            relocation.offset = reader.U64();
            const auto kind = reader.U8();
            if (kind < 1 || kind > 7) throw std::runtime_error("unknown relocation kind " + std::to_string(kind));
            relocation.kind = static_cast<RelocKind>(kind);
            relocation.addend = static_cast<std::int64_t>(reader.U64());
            relocation.target = reader.U32();
            image.relocations.push_back(relocation);
        }
        metadata.images.push_back(std::move(image));
    }
    for (auto count = reader.U32(); count > 0; --count) {
        SymbolRef symbol;
        symbol.image = reader.U32();
        symbol.vaddr = reader.U64();
        metadata.symbols.push_back(symbol);
    }
    for (std::uint32_t index = 0; index < stubCount; ++index) metadata.stubs.push_back(reader.String());
    for (auto count = reader.U32(); count > 0; --count) metadata.hostData.push_back(reader.String());
    const auto blobStart = reader.U64();
    if (blobStart != reader.Position()) throw std::runtime_error("image metadata blob offset does not follow the tables");
    const auto blob = bytes.subspan(blobStart);
    const auto slice = [&](std::uint64_t offset, std::uint64_t length) {
        if (offset > blob.size() || length > blob.size() - offset) throw std::runtime_error("image metadata blob is truncated");
        return blob.subspan(offset, length);
    };
    for (const auto& [image, segment, offset, length] : pendingSegments) metadata.images[image].segments[segment].data = slice(offset, length);
    metadata.addrMap = slice(mapOffset, mapSize);
    return metadata;
}

}  // namespace WasmRuntime
