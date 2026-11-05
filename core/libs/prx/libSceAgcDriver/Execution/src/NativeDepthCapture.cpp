#include "prx/libSceAgcDriver/Execution/include/NativeDepthCapture.hpp"
#include <zstd.h>
#include <array>
#include <bit>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace AgcDriver {
namespace {

void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("native depth capture: ") + reason);
}

std::pair<std::uint64_t, std::uint64_t> Sizes(const NativeDepthSnapshot& snapshot, bool depth, bool stencil) {
    const auto& target = snapshot.target;
    Require(target.samples == 1u, "multisample snapshots are unsupported");
    Require(target.extent.width != 0u && target.extent.height != 0u, "empty depth extent");
    const bool d16 = target.format == VK_FORMAT_D16_UNORM;
    const bool d16s8 = target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const bool d32 = target.format == VK_FORMAT_D32_SFLOAT;
    const bool d32s8 = target.format == VK_FORMAT_D32_SFLOAT_S8_UINT;
    Require(d16 || d16s8 || d32 || d32s8, "unsupported guest depth format");
    const bool compatible = snapshot.hostFormat == target.format || (d16s8 && (snapshot.hostFormat == VK_FORMAT_D24_UNORM_S8_UINT || snapshot.hostFormat == VK_FORMAT_D32_SFLOAT_S8_UINT));
    Require(compatible, "unsupported host depth format");
    const auto texelBytes = snapshot.hostFormat == VK_FORMAT_D16_UNORM || snapshot.hostFormat == VK_FORMAT_D16_UNORM_S8_UINT ? 2u : 4u;
    const auto pixels = static_cast<std::uint64_t>(target.extent.width) * target.extent.height;
    Require(pixels <= NativeDepthSurfaceLimit / texelBytes, "depth plane exceeds the decoded size limit");
    Require(depth || stencil, "snapshot owns no depth or stencil plane");
    Require(!depth || target.address != 0u, "depth owner has no address");
    Require(!stencil || (target.stencilAddress != 0u && (d16s8 || d32s8)), "stencil owner has no stencil aspect");
    return {depth ? pixels * texelBytes : 0u, stencil ? pixels : 0u};
}

class Writer {
public:
    explicit Writer(const std::filesystem::path& path) : file(path, std::ios::binary | std::ios::trunc) {
        Require(file.is_open(), "cannot create depth file");
    }
    template<typename T>
    void Value(T value) {
        static_assert(std::is_unsigned_v<T>);
        std::array<char, sizeof(T)> bytes{};
        for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<char>(value >> (i * 8u));
        file.write(bytes.data(), bytes.size());
        Require(file.good(), "cannot write depth metadata");
    }
    void Bytes(std::span<const std::byte> bytes) {
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        Require(file.good(), "cannot write depth payload");
    }
    void Finish() { file.flush(); Require(file.good(), "cannot finish depth file"); }
private:
    std::ofstream file;
};

class Reader {
public:
    explicit Reader(const std::filesystem::path& path) : file(path, std::ios::binary), remaining(std::filesystem::file_size(path)) {
        Require(file.is_open(), "cannot open depth file");
    }
    template<typename T>
    T Value() {
        static_assert(std::is_unsigned_v<T>);
        Require(remaining >= sizeof(T), "truncated depth metadata");
        std::array<unsigned char, sizeof(T)> bytes{};
        file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        Require(file.good(), "cannot read depth metadata");
        remaining -= sizeof(T);
        T value = 0;
        for (std::size_t i = 0; i < bytes.size(); ++i) value |= static_cast<T>(bytes[i]) << (i * 8u);
        return value;
    }
    std::vector<std::byte> Bytes(std::size_t count) {
        Require(count <= remaining, "truncated depth payload");
        std::vector<std::byte> bytes(count);
        file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(count));
        Require(file.good(), "cannot read depth payload");
        remaining -= count;
        return bytes;
    }
    std::uint64_t Remaining() const { return remaining; }
private:
    std::ifstream file;
    std::uint64_t remaining;
};

}

std::uint64_t ValidateNativeDepthSnapshots(std::span<const NativeDepthSnapshot> snapshots) {
    Require(snapshots.size() <= 65536u, "too many native surface records");
    std::uint64_t bytes = 0;
    std::set<std::pair<std::uint64_t, unsigned>> owners;
    for (const auto& snapshot : snapshots) {
        const auto [depthBytes, stencilBytes] = Sizes(snapshot, !snapshot.depth.empty(), !snapshot.stencil.empty());
        Require(snapshot.depth.size() == depthBytes && snapshot.stencil.size() == stencilBytes, "plane size does not match its extent");
        Require(depthBytes == 0u || owners.emplace(snapshot.target.address, 0u).second, "duplicate depth owner");
        Require(stencilBytes == 0u || owners.emplace(snapshot.target.stencilAddress, 1u).second, "duplicate stencil owner");
        const auto added = depthBytes + stencilBytes;
        Require(added <= NativeDepthCaptureLimit - bytes, "capture exceeds the decoded size limit");
        bytes += added;
    }
    return bytes;
}

void WriteNativeDepthCapture(const std::filesystem::path& path, std::span<const NativeDepthSnapshot> snapshots) {
    static_cast<void>(ValidateNativeDepthSnapshots(snapshots));
    Writer out(path);
    out.Bytes(std::as_bytes(std::span(NativeDepthCaptureMagic.data(), NativeDepthCaptureMagic.size())));
    out.Value(static_cast<std::uint32_t>(snapshots.size()));
    for (const auto& snapshot : snapshots) {
        const auto& t = snapshot.target;
        out.Value(t.address); out.Value(t.stencilAddress); out.Value(t.htileAddress);
        for (const auto value : {t.extent.width, t.extent.height, static_cast<std::uint32_t>(t.format), static_cast<std::uint32_t>(snapshot.hostFormat), std::bit_cast<std::uint32_t>(t.clearDepth), static_cast<std::uint32_t>(t.clearStencil), t.samples, t.depthSwizzle, t.pipeBankXor, t.firstSlice, t.lastSlice, t.htileVrsEncoding}) out.Value(value);
        const std::uint32_t flags = static_cast<std::uint32_t>(t.htileStencil) | (static_cast<std::uint32_t>(t.iterate256) << 1u) | (static_cast<std::uint32_t>(t.htilePipeAligned) << 2u) | (static_cast<std::uint32_t>(t.htileDepthOnly) << 3u) | (static_cast<std::uint32_t>(t.gcn) << 4u);
        out.Value(flags);
        out.Value(static_cast<std::uint32_t>(!snapshot.depth.empty()) | (static_cast<std::uint32_t>(!snapshot.stencil.empty()) << 1u));
        out.Value(static_cast<std::uint64_t>(snapshot.depth.size())); out.Value(static_cast<std::uint64_t>(snapshot.stencil.size()));
        std::vector<std::byte> payload = snapshot.depth;
        payload.insert(payload.end(), snapshot.stencil.begin(), snapshot.stencil.end());
        std::vector<std::byte> compressed(ZSTD_compressBound(payload.size()));
        const auto size = ZSTD_compress(compressed.data(), compressed.size(), payload.data(), payload.size(), 1);
        Require(!ZSTD_isError(size), "cannot compress depth payload");
        out.Value(static_cast<std::uint64_t>(size));
        out.Bytes(std::span(compressed).first(size));
    }
    out.Finish();
}

std::vector<NativeDepthSnapshot> ReadNativeDepthCapture(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return {};
    Reader in(path);
    const auto magic = in.Bytes(NativeDepthCaptureMagic.size());
    Require(std::memcmp(magic.data(), NativeDepthCaptureMagic.data(), magic.size()) == 0, "unknown depth file format");
    const auto count = in.Value<std::uint32_t>();
    Require(count <= 65536u && count <= in.Remaining() / 104u, "invalid native surface count");
    std::vector<NativeDepthSnapshot> snapshots;
    snapshots.reserve(count);
    std::uint64_t total = 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        NativeDepthSnapshot snapshot;
        auto& t = snapshot.target;
        t.address = in.Value<std::uint64_t>(); t.stencilAddress = in.Value<std::uint64_t>(); t.htileAddress = in.Value<std::uint64_t>();
        t.extent.width = in.Value<std::uint32_t>(); t.extent.height = in.Value<std::uint32_t>();
        t.format = static_cast<VkFormat>(in.Value<std::uint32_t>()); snapshot.hostFormat = static_cast<VkFormat>(in.Value<std::uint32_t>());
        t.clearDepth = std::bit_cast<float>(in.Value<std::uint32_t>());
        const auto clearStencil = in.Value<std::uint32_t>();
        Require(clearStencil <= 255u, "invalid stencil clear value");
        t.clearStencil = static_cast<std::uint8_t>(clearStencil);
        t.samples = in.Value<std::uint32_t>(); t.depthSwizzle = in.Value<std::uint32_t>(); t.pipeBankXor = in.Value<std::uint32_t>();
        t.firstSlice = in.Value<std::uint32_t>(); t.lastSlice = in.Value<std::uint32_t>(); t.htileVrsEncoding = in.Value<std::uint32_t>();
        const auto flags = in.Value<std::uint32_t>();
        Require((flags & ~31u) == 0u, "unknown target flags");
        t.htileStencil = (flags & 1u) != 0u; t.iterate256 = (flags & 2u) != 0u; t.htilePipeAligned = (flags & 4u) != 0u; t.htileDepthOnly = (flags & 8u) != 0u; t.gcn = (flags & 16u) != 0u;
        const auto aspects = in.Value<std::uint32_t>();
        Require(aspects != 0u && (aspects & ~3u) == 0u, "unknown owner aspects");
        const auto [depthBytes, stencilBytes] = Sizes(snapshot, (aspects & 1u) != 0u, (aspects & 2u) != 0u);
        Require(in.Value<std::uint64_t>() == depthBytes && in.Value<std::uint64_t>() == stencilBytes, "plane size does not match its extent");
        const auto bytes = depthBytes + stencilBytes;
        Require(bytes <= NativeDepthCaptureLimit - total, "capture exceeds the decoded size limit");
        total += bytes;
        const auto compressedBytes = in.Value<std::uint64_t>();
        Require(compressedBytes != 0u && compressedBytes <= ZSTD_compressBound(static_cast<std::size_t>(bytes)) && compressedBytes <= in.Remaining(), "invalid compressed depth size");
        const auto compressed = in.Bytes(static_cast<std::size_t>(compressedBytes));
        Require(ZSTD_getFrameContentSize(compressed.data(), compressed.size()) == bytes, "compressed depth size differs from its metadata");
        Require(ZSTD_findFrameCompressedSize(compressed.data(), compressed.size()) == compressed.size(), "depth payload is not one compressed frame");
        std::vector<std::byte> payload(static_cast<std::size_t>(bytes));
        Require(ZSTD_decompress(payload.data(), payload.size(), compressed.data(), compressed.size()) == bytes, "invalid compressed depth payload");
        snapshot.depth.assign(payload.begin(), payload.begin() + static_cast<std::ptrdiff_t>(depthBytes));
        snapshot.stencil.assign(payload.begin() + static_cast<std::ptrdiff_t>(depthBytes), payload.end());
        snapshots.push_back(std::move(snapshot));
    }
    Require(in.Remaining() == 0u, "trailing depth file data");
    static_cast<void>(ValidateNativeDepthSnapshots(snapshots));
    return snapshots;
}

}
