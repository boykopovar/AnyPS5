#include "ControlFlow/RequestSerializer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr char kHistoricalVersion21[] =
    "NVNQQRUAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t3gABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEAAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAABI0VngAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAAAAAF4VjQSAQEBOTAAAAA=";
constexpr char kVersion21WithF16Tail[] =
    "NVNQQRUAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAQA0DAAEB";

constexpr char kVersion22WithoutF16Tail[] =
    "NVNQQRYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAQA0DAAE=";

constexpr char kLayerVersion23[] =
    "NVNQQRcAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t3gAB"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEDAAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAkAAAANAAAAAQUKBQABAAAAAAAAAAAABQAAAAAABAAAAAAAAAABAwUHAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACAAAACwAAABAAAABAAAAAAQYAAAAAAAAAAAAAAAAAADTvzasAAAA5"
    "AAAABAAAAAkAAAAAAXhWNBIBAQE5MAAAAAA=";

void Require(bool condition) {
    if (!condition) std::abort();
}

template <typename TRange>
concept AssignableMemory = requires(ShaderRecompiler::GuestContext& context, TRange&& range) {
    context.memory = std::forward<TRange>(range);
};

using Region = ShaderRecompiler::MemoryRegion;
using Regions = std::vector<Region>;
using RegionArray = std::array<Region, 1>;
using RegionView = ShaderRecompiler::MemoryRegionView;

static_assert(!AssignableMemory<Regions>);
static_assert(!AssignableMemory<const Regions>);
static_assert(!AssignableMemory<RegionArray>);
static_assert(!AssignableMemory<std::initializer_list<Region>>);
static_assert(AssignableMemory<Regions&> && AssignableMemory<const Regions&>);
static_assert(AssignableMemory<RegionArray&> && AssignableMemory<const RegionArray&>);
static_assert(AssignableMemory<std::span<const Region>> && AssignableMemory<RegionView>);
static_assert(sizeof(RegionView) == sizeof(std::span<const Region>));
static_assert(alignof(RegionView) == alignof(std::span<const Region>));
static_assert(std::is_trivially_copyable_v<RegionView>);
static_assert(std::is_nothrow_constructible_v<RegionView, std::span<const Region>>);
static_assert([] {
    const RegionArray owned{{{0x1000u, {}}}};
    const RegionView view{std::span<const Region>(owned)};
    return view.size() == 1u && view.front().guestAddress == 0x1000u;
}());

void VerifyMemoryView() {
    const std::array<std::byte, 4> bytes{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    Regions owned{{0x1000u, bytes}};
    const Regions frozen = owned;
    Region array[]{{0x2000u, bytes}};
    RegionArray standard{{{0x3000u, bytes}}};
    ShaderRecompiler::RecompileRequest request{};
    request.context.memory = owned;
    Require(request.context.memory.data() == owned.data());
    owned.front().guestAddress = 0x4000u;
    Require(request.context.memory.front().guestAddress == 0x4000u);
    request.context.memory = frozen;
    Require(request.context.memory.data() == frozen.data());
    request.context.memory = array;
    Require(request.context.memory.data() == array);
    request.context.memory = standard;
    Require(request.context.memory.data() == standard.data());
    request.context.memory = std::span<const Region>(owned).first(1);
    const auto copy = request;
    Require(copy.context.memory.data() == owned.data() && copy.context.memory.front().bytes.data() == bytes.data());
    request.context.memory = {owned.data(), owned.size()};
    const ShaderRecompiler::RequestSerializer serializer;
    const auto replay = serializer.Deserialize(serializer.Serialize(request));
    Require(replay.request.context.memory.size() == 1u && replay.request.context.memory.front().guestAddress == 0x4000u);
    Require(std::ranges::equal(replay.request.context.memory.front().bytes, bytes));
    request.context.memory = {};
    Require(request.context.memory.empty());
}

void InsertMeshBarycentricFlag(std::vector<std::uint8_t>& bytes) {
    constexpr std::array<std::uint8_t, 20> draw{0x00, 0x34, 0xef, 0xcd, 0xab, 0x00, 0x00, 0x00, 57u, 0u, 0u, 0u, 4u, 0u, 0u, 0u, 9u, 0u, 0u, 0u};
    const auto at = std::search(bytes.begin(), bytes.end(), draw.begin(), draw.end());
    Require(at != bytes.end());
    bytes.insert(at + static_cast<std::ptrdiff_t>(draw.size()), 0u);
}

// Pixel info bytes that follow the stencil operation value flag: the sample order (version 42),
// then the default color target merge (version 46, 40 bytes with samples = 1 at offset 5).
std::vector<std::uint8_t> PixelTailDefaults(bool sampleOrder, bool targetMerge) {
    std::vector<std::uint8_t> tail(sampleOrder ? 16u : 0u, 0u);
    if (targetMerge) {
        tail.resize(tail.size() + 40u, 0u);
        tail[tail.size() - 35u] = 1u;
    }
    return tail;
}

void EraseDefaultTargetMerge(std::vector<std::uint8_t>& bytes, std::ptrdiff_t offset) {
    const auto merge = PixelTailDefaults(false, true);
    const auto at = bytes.begin() + offset;
    Require(bytes.end() - at > static_cast<std::ptrdiff_t>(merge.size()) && std::equal(merge.begin(), merge.end(), at));
    bytes.erase(at, at + static_cast<std::ptrdiff_t>(merge.size()));
}

void InsertPixelDefaultsAfterFrontFace(std::vector<std::uint8_t>& bytes, bool sampleOrder = true, bool targetMerge = true) {
    bytes.insert(bytes.begin() + 261u, {0x0fu, 0xffu, 0xffu, 0u, 0u, 0xffu, 0xffu, 0u, 0u, 0xffu, 0xffu, 0u, 0u, 0xffu, 0xffu, 0u, 0u, 0u, 0u, 0u, 0u});
    bytes.insert(bytes.begin() + 282u, 35u, 0u);
    const auto tail = PixelTailDefaults(sampleOrder, targetMerge);
    bytes.insert(bytes.begin() + 317u, tail.begin(), tail.end());
}

void AppendVertexTail(std::vector<std::uint8_t>& bytes) {
    bytes.insert(bytes.end(), sizeof(std::uint64_t) + 1u + 5u + 1u + 1u, 0u);
    bytes.insert(bytes.end(), {0u, 0u, 0u, 0x80u, 0x3fu, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u});
}

std::vector<std::uint8_t> DecodeBase64(std::string_view text) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<std::uint8_t> bytes;
    std::uint32_t chunk = 0;
    std::size_t bits = 0;
    for (const char symbol : text) {
        if (symbol == '=') break;
        const auto value = alphabet.find(symbol);
        Require(value != std::string_view::npos);
        chunk = ((chunk << 6u) | static_cast<std::uint32_t>(value)) & 0xffffffu;
        bits += 6u;
        if (bits >= 8u) {
            bits -= 8u;
            bytes.push_back(static_cast<std::uint8_t>((chunk >> bits) & 0xffu));
        }
    }
    return bytes;
}

std::string EncodeBase64(const std::vector<std::uint8_t>& bytes) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text;
    for (std::size_t index = 0; index < bytes.size(); index += 3u) {
        const std::size_t count = std::min<std::size_t>(3u, bytes.size() - index);
        std::uint32_t chunk = static_cast<std::uint32_t>(bytes[index]) << 16u;
        if (count > 1u) chunk |= static_cast<std::uint32_t>(bytes[index + 1u]) << 8u;
        if (count > 2u) chunk |= bytes[index + 2u];
        for (std::size_t symbol = 0; symbol < 4u; ++symbol) {
            text.push_back(symbol <= count ? alphabet[(chunk >> (18u - 6u * symbol)) & 0x3fu] : '=');
        }
    }
    return text;
}

}

namespace {

void VerifyTessellationInstancePatches() {
    ShaderRecompiler::RecompileRequest request;
    request.graphics.emplace();
    request.graphics->draw.indexCount = 12u;
    request.graphics->draw.instanceCount = 2u;
    ShaderRecompiler::TessellationConfiguration configuration{};
    configuration.inputControlPoints = 0u;
    request.graphics->tessellation = configuration;
    Require(ShaderRecompiler::TessellationInstancePatches(request) == 0u);
    request.graphics->tessellation->inputControlPoints = 3u;
    Require(ShaderRecompiler::TessellationInstancePatches(request) == 4u);
}

}

int main() {
    VerifyMemoryView();
    VerifyTessellationInstancePatches();
    const ShaderRecompiler::RequestSerializer serializer;

    const auto historical = serializer.Deserialize(kHistoricalVersion21);
    Require(!historical.request.target.float16DenormPreserve);

    const auto version21WithTail = serializer.Deserialize(kVersion21WithF16Tail);
    Require(version21WithTail.request.target.float16DenormPreserve);

    bool rejectedTruncatedVersion22 = false;
    try {
        static_cast<void>(serializer.Deserialize(kVersion22WithoutF16Tail));
    } catch (const std::runtime_error& error) {
        rejectedTruncatedVersion22 = std::string_view(error.what()) == "RequestSerializer: truncated data";
    }
    Require(rejectedTruncatedVersion22);

    struct CanonicalMain22 {
        bool pixel;
        bool tessellation;
        std::string_view encoded;
    };
    constexpr std::array<CanonicalMain22, 4> canonicalMain22{{
        {true, false,
            "NVNQQRYAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t3gAB"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEDAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAACQAAAA0AAAABBQoFAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAAAEDBQcAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAABBgAAAAAAAAAAAAAAAAAANO/NqwAAADkA"
            "AAAEAAAACQAAAAABeFY0EgEBATkwAAAAAA=="
        },
        {true, true,
            "NVNQQRYAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t3gAB"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEDAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAACQAAAA0AAAABBQoFAAEAAAAAAAAAAAAFAAAAAAAEAAAAAAAAAAEDBQcAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAALAAAAEAAAAEAAAAABBgAAAAAAAAAAAAAAAAEEAAAABAAAAAIA"
            "AAACAAAAAwAAAD8AAAAANO/NqwAAADkAAAAEAAAACQAAAAABeFY0EgEBATkwAAAAAA=="
        },
        {false, false,
            "NVNQQRYAAAABAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t3gAA"
            "AQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACQAAAA0AAAABBQoFAAEAAAAAAAAA"
            "AAAFAAAAAAAEAAAAAAAAAAEDBQcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAIAAAALAAAAEAAAAEAAAAABBgAAAAAAAAAAAAAAAAAANO/NqwAAADkAAAAEAAAACQAAAAABeFY0EgEBATkwAAAAAA=="
        },
        {false, true,
            "NVNQQRYAAAABAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t3gAA"
            "AQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACQAAAA0AAAABBQoFAAEAAAAAAAAA"
            "AAAFAAAAAAAEAAAAAAAAAAEDBQcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAIAAAALAAAAEAAAAEAAAAABBgAAAAAAAAAAAAAAAAEEAAAABAAAAAIAAAACAAAAAwAAAD8AAAAANO/NqwAAADkAAAAEAAAACQAA"
            "AAABeFY0EgEBATkwAAAAAA=="
        },
    }};
    for (const auto& fixture : canonicalMain22) {
        for (const bool preserve : {false, true}) {
            auto bytes = DecodeBase64(fixture.encoded);
            bytes.back() = static_cast<std::uint8_t>(preserve);
            const auto replay = serializer.Deserialize(EncodeBase64(bytes));
            const auto& request = replay.request;
            Require(request.context.pixel.has_value() == fixture.pixel && request.context.vertex.has_value());
            Require(request.context.vertex->fetchAttribReg == 9u && request.context.vertex->fetchBufferReg == 13u && request.context.vertex->fetchEmbedded && request.context.vertex->paClVsOutCntl == 0x00050a05u);
            Require(!fixture.pixel || (request.context.pixel->rasterPrimitive == ShaderRecompiler::RasterPrimitive::Rectangle && request.context.pixel->alphaToCoverage && request.context.pixel->stencilRefExportEnable && request.context.pixel->targetOutputMode[0] == 9u && request.context.pixel->targetExportMapping[0] == 0xe4u && !request.context.pixel->layerExported && !request.context.pixel->ditheredAlphaToCoverage && request.context.pixel->reversedBlend == 0u));
            Require(request.graphics.has_value() && request.graphics->firstUserSgpr == 6u && request.graphics->tessellation.has_value() == fixture.tessellation);
            Require(!fixture.tessellation || (request.graphics->tessellation->patchesPerGroup == 63u && request.graphics->tessellation->domain == 2u && request.graphics->tessellation->outputTopology == 3u));
            Require(request.graphics->draw.indexAddress == 0xabcdef3400u && request.graphics->draw.indexCount == 57u && request.graphics->draw.indexElementBytes == 4u && request.graphics->draw.instanceCount == 9u);
            Require(request.target.float16DenormPreserve == preserve);
            auto upgraded = bytes;
            upgraded[4] = 51u;
            InsertMeshBarycentricFlag(upgraded);
            if (fixture.pixel) upgraded.insert(upgraded.begin() + 258u, {0u, 0u, 0u});
            if (fixture.pixel) InsertPixelDefaultsAfterFrontFace(upgraded);
            if (fixture.tessellation) upgraded.insert(upgraded.end() - 35u, 6u, 0u);
            AppendVertexTail(upgraded);
            upgraded.push_back(0u);
            upgraded.push_back(0u);
            upgraded.insert(upgraded.end(), {0u, 0x10u, 0u, 0u});
            upgraded.push_back(0u);
            upgraded.push_back(1u);
            Require(serializer.Serialize(request) == EncodeBase64(upgraded));
            auto truncated = bytes;
            truncated.pop_back();
            bool rejected = false;
            try {
                static_cast<void>(serializer.Deserialize(EncodeBase64(truncated)));
            } catch (const std::runtime_error& error) {
                rejected = std::string_view(error.what()) == "RequestSerializer: truncated data";
            }
            Require(rejected);
            if (!fixture.pixel) {
                auto vertex23 = bytes;
                vertex23[4] = 23u;
                const auto vertexReplay = serializer.Deserialize(EncodeBase64(vertex23));
                Require(serializer.Serialize(vertexReplay.request) == EncodeBase64(upgraded));
                continue;
            }
            for (const bool layer : {false, true}) {
                auto layered = bytes;
                layered[4] = 23u;
                layered.insert(layered.begin() + 258u, static_cast<std::uint8_t>(layer));
                const auto layerReplay = serializer.Deserialize(EncodeBase64(layered));
                Require(layerReplay.request.context.pixel->layerExported == layer && !layerReplay.request.context.pixel->ditheredAlphaToCoverage && layerReplay.request.context.pixel->reversedBlend == 0u);
                Require(layerReplay.request.target.float16DenormPreserve == preserve && layerReplay.request.graphics->tessellation.has_value() == fixture.tessellation);
                Require(!fixture.tessellation || layerReplay.request.graphics->tessellation->patchesPerGroup == 63u);
                auto truncatedLayer = layered;
                truncatedLayer.pop_back();
                bool layerRejected = false;
                try {
                    static_cast<void>(serializer.Deserialize(EncodeBase64(truncatedLayer)));
                } catch (const std::runtime_error& error) {
                    layerRejected = std::string_view(error.what()) == "RequestSerializer: truncated data";
                }
                Require(layerRejected);
                layered[4] = 51u;
                InsertMeshBarycentricFlag(layered);
                layered.insert(layered.begin() + 259u, {0u, 0u});
                InsertPixelDefaultsAfterFrontFace(layered);
                if (fixture.tessellation) layered.insert(layered.end() - 35u, 6u, 0u);
                AppendVertexTail(layered);
                layered.push_back(0u);
                layered.push_back(0u);
                layered.insert(layered.end(), {0u, 0x10u, 0u, 0u});
                layered.push_back(0u);
                layered.push_back(1u);
                Require(serializer.Serialize(layerReplay.request) == EncodeBase64(layered));
            }
        }
    }

    const auto layer23 = DecodeBase64(kLayerVersion23);
    Require(layer23.size() == 1451u);
    for (const bool layer : {false, true}) {
        for (const bool preserve : {false, true}) {
            auto bytes = layer23;
            bytes[258] = static_cast<std::uint8_t>(layer);
            bytes.back() = static_cast<std::uint8_t>(preserve);
            const auto encoded23 = EncodeBase64(bytes);
            const auto replay = serializer.Deserialize(encoded23);
            Require(replay.request.context.pixel.has_value() && replay.request.context.vertex.has_value());
            Require(replay.request.context.pixel->layerExported == layer);
            Require(!replay.request.context.pixel->ditheredAlphaToCoverage && replay.request.context.pixel->reversedBlend == 0u);
            Require(replay.request.context.pixel->rasterPrimitive == ShaderRecompiler::RasterPrimitive::Rectangle);
            Require(replay.request.context.vertex->paClVsOutCntl == 0x00050a05u);
            Require(replay.request.target.float16DenormPreserve == preserve);
            bytes[4] = 51u;
            InsertMeshBarycentricFlag(bytes);
            bytes.insert(bytes.begin() + 259u, {0u, 0u});
            InsertPixelDefaultsAfterFrontFace(bytes);
            AppendVertexTail(bytes);
            bytes.push_back(0u);
            bytes.push_back(0u);
            bytes.insert(bytes.end(), {0u, 0x10u, 0u, 0u});
            bytes.push_back(0u);
            bytes.push_back(1u);
            Require(serializer.Serialize(replay.request) == EncodeBase64(bytes));

            auto truncated23 = DecodeBase64(encoded23);
            truncated23.pop_back();
            bool rejected = false;
            try {
                static_cast<void>(serializer.Deserialize(EncodeBase64(truncated23)));
            } catch (const std::runtime_error& error) {
                rejected = std::string_view(error.what()) == "RequestSerializer: truncated data";
            }
            Require(rejected);
        }
    }

    struct Main24Fixture {
        bool pixel;
        bool tessellation;
        std::string_view encoded;
    };
    static constexpr std::array<Main24Fixture, 4> canonicalMain24{{
        {true, false,
            "NVNQQRgAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t3gAB"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEDAQEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAkAAAANAAAAAQUKBQABAAAAAAAAAAAABQAAAAAABAAAAAAAAAABAwUHAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACAAAACwAAABAAAABAAAAAAQYAAAAAAAAAAAAAAAAAADTvzasAAAA5"
            "AAAABAAAAAkAAAAAAXhWNBIBAQE5MAAAAAE="
        },
        {true, true,
            "NVNQQRgAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t3gAB"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAJAAAAAAAAAOQAAAAAAAAAAAAAAAAAAAAAAQEDAQEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAkAAAANAAAAAQUKBQABAAAAAAAAAAAABQAAAAAABAAAAAAAAAABAwUHAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACAAAACwAAABAAAABAAAAAAQYAAAAAAAAAAAAAAAABBAAAAAQAAAAC"
            "AAAAAgAAAAMAAAA/AAAAJQAAAAEANO/NqwAAADkAAAAEAAAACQAAAAABeFY0EgEBATkwAAAAAQ=="
        },
        {false, false,
            "NVNQQRgAAAABAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t3gAA"
            "AQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACQAAAA0AAAABBQoFAAEAAAAAAAAA"
            "AAAFAAAAAAAEAAAAAAAAAAEDBQcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAIAAAALAAAAEAAAAEAAAAABBgAAAAAAAAAAAAAAAAAANO/NqwAAADkAAAAEAAAACQAAAAABeFY0EgEBATkwAAAAAQ=="
        },
        {false, true,
            "NVNQQRgAAAABAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAgAAAADAAAAAAAAAN+bVxPgrGgk776t3gAA"
            "AQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACQAAAA0AAAABBQoFAAEAAAAAAAAA"
            "AAAFAAAAAAAEAAAAAAAAAAEDBQcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAIAAAALAAAAEAAAAEAAAAABBgAAAAAAAAAAAAAAAAEEAAAABAAAAAIAAAACAAAAAwAAAD8AAAAlAAAAAQA0782rAAAAOQAAAAQA"
            "AAAJAAAAAAF4VjQSAQEBOTAAAAAB"
        },
    }};
    for (const auto& fixture : canonicalMain24) {
        for (const bool preserve : {false, true}) {
            for (const bool layer : {false, true}) {
                if (!fixture.pixel && layer) continue;
                auto bytes = DecodeBase64(fixture.encoded);
                bytes.back() = static_cast<std::uint8_t>(preserve);
                if (fixture.pixel) bytes[258] = static_cast<std::uint8_t>(layer);
                const auto replay = serializer.Deserialize(EncodeBase64(bytes));
                const auto& request = replay.request;
                Require(request.context.pixel.has_value() == fixture.pixel && request.context.vertex.has_value());
                Require(!fixture.pixel || (request.context.pixel->layerExported == layer && !request.context.pixel->ditheredAlphaToCoverage && request.context.pixel->reversedBlend == 0u));
                Require(request.graphics.has_value() && request.graphics->tessellation.has_value() == fixture.tessellation);
                Require(!fixture.tessellation || (request.graphics->tessellation->patchesPerGroup == 63u && request.graphics->tessellation->primitiveGroupSize == 37u && request.graphics->tessellation->breakAtInstance));
                Require(request.graphics->draw.indexAddress == 0xabcdef3400u && request.graphics->draw.indexCount == 57u && request.graphics->draw.indexElementBytes == 4u && request.graphics->draw.instanceCount == 9u);
                Require(request.target.float16DenormPreserve == preserve);
                auto main38 = bytes;
                main38[4] = 38u;
                InsertMeshBarycentricFlag(main38);
                if (fixture.pixel) main38.insert(main38.begin() + 259u, {0u, 0u});
                if (fixture.pixel) InsertPixelDefaultsAfterFrontFace(main38, false, false);
                auto upgraded = main38;
                upgraded[4] = 51u;
                if (fixture.pixel) {
                    const auto tail = PixelTailDefaults(true, true);
                    upgraded.insert(upgraded.begin() + 317u, tail.begin(), tail.end());
                }
                if (fixture.tessellation) upgraded.insert(upgraded.end() - 35u, 1u, 0u);
                main38.insert(main38.end(), sizeof(std::uint64_t) + 1u + 5u + 1u, 0u);
                AppendVertexTail(upgraded);
                upgraded.push_back(0u);
                upgraded.push_back(0u);
                upgraded.insert(upgraded.end(), {0u, 0x10u, 0u, 0u});
                upgraded.push_back(0u);
                upgraded.push_back(1u);
                Require(serializer.Serialize(request) == EncodeBase64(upgraded));
                const auto replay38 = serializer.Deserialize(EncodeBase64(main38));
                Require(replay38.request.graphics.has_value() && replay38.request.graphics->tessellation.has_value() == fixture.tessellation);
                Require(!fixture.tessellation || (!replay38.request.graphics->tessellation->mergedHull && replay38.request.graphics->tessellation->primitiveGroupSize == 37u && replay38.request.graphics->tessellation->breakAtInstance));
                Require(replay38.request.graphics->draw.indexAddress == 0xabcdef3400u && replay38.request.graphics->draw.indexCount == 57u && replay38.request.graphics->draw.indexElementBytes == 4u && replay38.request.graphics->draw.instanceCount == 9u);
                Require(replay38.request.target.float16DenormPreserve == preserve);
                Require(serializer.Serialize(replay38.request) == EncodeBase64(upgraded));
                if (fixture.tessellation) {
                    for (const bool merged : {false, true}) {
                        auto pointed = request;
                        pointed.graphics->tessellation->mergedHull = merged;
                        pointed.context.vertex->pointList = true;
                        pointed.context.vertex->pointSize = 3.0f;
                        pointed.context.vertex->pointSizeMin = 1.0f;
                        pointed.context.vertex->pointSizeMax = 8.0f;
                        auto version51 = DecodeBase64(serializer.Serialize(pointed));
                        Require(version51.size() > 17u && version51[4] == 51u);
                        auto version48 = version51;
                        version48.pop_back();
                        version48[4] = 48u;
                        auto version46 = version48;
                        version46.pop_back();
                        version46[4] = 46u;
                        const auto replay46 = serializer.Deserialize(EncodeBase64(version46));
                        Require(replay46.request.graphics->tessellation->mergedHull == merged && replay46.request.context.vertex->pointList && replay46.request.context.vertex->pointSize == 3.0f && replay46.request.context.vertex->pointSizeMin == 1.0f && replay46.request.context.vertex->pointSizeMax == 8.0f);
                        auto version45 = version46;
                        if (fixture.pixel) EraseDefaultTargetMerge(version45, 333);
                        version45[4] = 45u;
                        auto version44 = version45;
                        version44.resize(version44.size() - 4u);
                        version44.pop_back();
                        version44[4] = 44u;
                        auto version43 = version44;
                        version43.pop_back();
                        version43[4] = 43u;
                        const auto replay43 = serializer.Deserialize(EncodeBase64(version43));
                        Require(serializer.Serialize(replay43.request) == EncodeBase64(version51));
                        Require(replay43.request.graphics->tessellation->mergedHull == merged && replay43.request.context.vertex->pointList && replay43.request.context.vertex->pointSize == 3.0f && replay43.request.context.vertex->pointSizeMin == 1.0f && replay43.request.context.vertex->pointSizeMax == 8.0f);
                        auto version42 = version43;
                        version42.resize(version42.size() - 13u);
                        version42[4] = 42u;
                        const auto replay42 = serializer.Deserialize(EncodeBase64(version42));
                        Require(replay42.request.graphics->tessellation->mergedHull == merged && !replay42.request.context.vertex->pointList && replay42.request.context.vertex->pointSize == 1.0f && replay42.request.context.vertex->pointSizeMin == 0.0f && replay42.request.context.vertex->pointSizeMax == 0.0f);
                        auto version41 = version42;
                        if (fixture.pixel) version41.erase(version41.begin() + 317u, version41.begin() + 333u);
                        version41[4] = 41u;
                        const auto replay41 = serializer.Deserialize(EncodeBase64(version41));
                        Require(replay41.request.graphics->tessellation->mergedHull == merged && !replay41.request.context.vertex->pointList && replay41.request.context.vertex->pointSize == 1.0f && replay41.request.context.vertex->pointSizeMin == 0.0f && replay41.request.context.vertex->pointSizeMax == 0.0f);
                        auto version39 = version41;
                        version39.pop_back();
                        version39[4] = 39u;
                        const auto replay39 = serializer.Deserialize(EncodeBase64(version39));
                        Require(replay39.request.graphics->tessellation->mergedHull == merged && !replay39.request.context.vertex->pointList && replay39.request.context.vertex->pointSize == 1.0f && replay39.request.context.vertex->pointSizeMin == 0.0f && replay39.request.context.vertex->pointSizeMax == 0.0f);
                    }
                }
                for (const bool dither : {false, true}) {
                    if (!fixture.pixel && dither) continue;
                    auto main25 = bytes;
                    main25[4] = 25u;
                    if (fixture.pixel) main25.insert(main25.begin() + 259u, static_cast<std::uint8_t>(dither));
                    const auto replay25 = serializer.Deserialize(EncodeBase64(main25));
                    Require(!fixture.pixel || (replay25.request.context.pixel->layerExported == layer && replay25.request.context.pixel->ditheredAlphaToCoverage == dither && replay25.request.context.pixel->reversedBlend == 0u));
                    Require(replay25.request.target.float16DenormPreserve == preserve && (!fixture.tessellation || replay25.request.graphics->tessellation->primitiveGroupSize == 37u));
                    main25[4] = 51u;
                    InsertMeshBarycentricFlag(main25);
                    if (fixture.pixel) {
                        main25.insert(main25.begin() + 260u, 0u);
                        InsertPixelDefaultsAfterFrontFace(main25);
                    }
                    if (fixture.tessellation) main25.insert(main25.end() - 35u, 1u, 0u);
                    AppendVertexTail(main25);
                    main25.push_back(0u);
                    main25.push_back(0u);
                    main25.insert(main25.end(), {0u, 0x10u, 0u, 0u});
                    main25.push_back(0u);
                    main25.push_back(1u);
                    Require(serializer.Serialize(replay25.request) == EncodeBase64(main25));
                }
            }
        }
    }

    ShaderRecompiler::RecompileRequest request{};
    for (const bool preserve : {false, true}) {
        request.target.float16DenormPreserve = preserve;
        request.target.float32FlushToZero = !preserve;
        request.target.float32DenormPreserve = preserve;
        request.target.largePixelFunctionLdsBuffer = !preserve;
        request.target.pixelFunctionStorageDwords = preserve ? 2048u : 4096u;
        const auto encoded = serializer.Serialize(request);
        const auto replay = serializer.Deserialize(encoded);
        Require(replay.request.target.float16DenormPreserve == preserve && replay.request.target.float32FlushToZero == !preserve && replay.request.target.float32DenormPreserve == preserve && replay.request.target.largePixelFunctionLdsBuffer == !preserve && replay.request.target.pixelFunctionStorageDwords == request.target.pixelFunctionStorageDwords);
        auto bytes = DecodeBase64(encoded);
        Require(bytes.size() >= 8u && bytes[4] == 51u && bytes[5] == 0u && bytes[6] == 0u && bytes[7] == 0u);
        auto version40 = bytes;
        version40.resize(version40.size() - sizeof(std::uint32_t));
        version40.pop_back();
        version40.pop_back();
        version40.pop_back();
        version40.pop_back();
        version40.pop_back();
        version40[4] = 40u;
        const auto replay40 = serializer.Deserialize(EncodeBase64(version40));
        Require(!replay40.request.target.float32DenormPreserve && replay40.request.target.float32FlushToZero == !preserve);
        auto future = bytes;
        future[4] = 52u;
        bool futureRejected = false;
        try {
            static_cast<void>(serializer.Deserialize(EncodeBase64(future)));
        } catch (const std::runtime_error& error) {
            futureRejected = std::string_view(error.what()) == "unsupported recompile request serialization version";
        }
        Require(futureRejected);
        bytes.pop_back();
        bool rejected = false;
        try {
            static_cast<void>(serializer.Deserialize(EncodeBase64(bytes)));
        } catch (const std::runtime_error& error) {
            rejected = std::string_view(error.what()) == "RequestSerializer: truncated data";
        }
        Require(rejected);
    }

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.iteratesSamples = true;
    pixel.positionLocation = ShaderRecompiler::PixelPositionLocation::UpperLeft;
    pixel.sampleLocations = {0xa5u, 0x5au, 0xc3u, 0x3cu};
    pixel.sampleLocations.back() = 0x77u;
    request.context.pixel = pixel;
    const auto pixelEncoded = serializer.Serialize(request);
    const auto pixelReplay = serializer.Deserialize(pixelEncoded);
    Require(pixelReplay.request.context.pixel.has_value() && pixelReplay.request.context.pixel->iteratesSamples && pixelReplay.request.context.pixel->positionLocation == ShaderRecompiler::PixelPositionLocation::UpperLeft && pixelReplay.request.context.pixel->sampleLocations == pixel.sampleLocations);
    auto pixelBytes = DecodeBase64(pixelEncoded);
    const auto marker = std::search(pixelBytes.begin(), pixelBytes.end(), pixel.sampleLocations.begin(), pixel.sampleLocations.begin() + 4);
    Require(marker != pixelBytes.end() && marker - pixelBytes.begin() >= 2 && *(marker - 1) == 3u && *(marker - 2) == 1u);
    *(marker - 1) = 4u;
    bool locationRejected = false;
    try {
        static_cast<void>(serializer.Deserialize(EncodeBase64(pixelBytes)));
    } catch (const std::runtime_error& error) {
        locationRejected = std::string_view(error.what()) == "RequestSerializer: invalid PixelPositionLocation value";
    }
    Require(locationRejected);
    pixel.sampleOrder = {0x76543210u, 0x01234567u, 0x10325476u, 0x67452301u};
    request.context.pixel = pixel;
    const auto orderEncoded = serializer.Serialize(request);
    Require(serializer.Deserialize(orderEncoded).request.context.pixel->sampleOrder == pixel.sampleOrder);
    auto version42 = DecodeBase64(orderEncoded);
    const auto orderOffset = std::search(version42.begin(), version42.end(), pixel.sampleLocations.begin(), pixel.sampleLocations.end()) - version42.begin() + static_cast<std::ptrdiff_t>(pixel.sampleLocations.size() + 1u);
    Require(version42.end() - version42.begin() > orderOffset + static_cast<std::ptrdiff_t>(sizeof(pixel.sampleOrder)) && version42[static_cast<std::size_t>(orderOffset)] == 0x10u);
    EraseDefaultTargetMerge(version42, orderOffset + static_cast<std::ptrdiff_t>(sizeof(pixel.sampleOrder)));
    version42.resize(version42.size() - 4u);
    version42.pop_back();
    version42.pop_back();
    version42.pop_back();
    version42.pop_back();
    version42[4] = 42u;
    const auto replay42 = serializer.Deserialize(EncodeBase64(version42));
    Require(replay42.request.context.pixel.has_value() && replay42.request.context.pixel->sampleOrder == pixel.sampleOrder && replay42.request.context.pixel->targetMerges.empty());
    auto upgraded42 = replay42.request;
    upgraded42.target = request.target;
    Require(serializer.Serialize(upgraded42) == orderEncoded);
    auto version41 = version42;
    version41.erase(version41.begin() + orderOffset, version41.begin() + orderOffset + static_cast<std::ptrdiff_t>(sizeof(pixel.sampleOrder)));
    version41[4] = 41u;
    const auto replay41 = serializer.Deserialize(EncodeBase64(version41));
    Require(replay41.request.context.pixel.has_value() && replay41.request.context.pixel->sampleLocations == pixel.sampleLocations && replay41.request.context.pixel->sampleOrder == std::array<std::uint32_t, 4>{});
    pixel.sampleOrder = {};
    request.context.pixel = pixel;
    for (const bool spanned : {false, true}) {
        request.target.spannedWaves = spanned;
        Require(serializer.Deserialize(serializer.Serialize(request)).request.target.spannedWaves == spanned);
        request.target.float32FlushToZero = true;
        auto version35 = DecodeBase64(serializer.Serialize(request));
        version35.resize(version35.size() - sizeof(std::uint32_t));
        version35.pop_back();
        version35.pop_back();
        version35.pop_back();
        const auto stencilOperationValue = std::search(version35.begin(), version35.end(), pixel.sampleLocations.begin(), pixel.sampleLocations.begin() + 4) + pixel.sampleLocations.size();
        const auto pixelTail = static_cast<std::ptrdiff_t>(1u + sizeof(pixel.sampleOrder));
        Require(version35.end() - stencilOperationValue > pixelTail && std::all_of(stencilOperationValue, stencilOperationValue + pixelTail, [](std::uint8_t value) { return value == 0u; }));
        EraseDefaultTargetMerge(version35, stencilOperationValue - version35.begin() + pixelTail);
        version35.erase(stencilOperationValue, stencilOperationValue + pixelTail);
        version35[4] = 35u;
        version35.pop_back();
        version35.pop_back();
        version35.pop_back();
        const auto replay35 = serializer.Deserialize(EncodeBase64(version35));
        Require(replay35.request.target.spannedWaves == spanned && !replay35.request.target.float32FlushToZero);
    }
    ShaderRecompiler::RecompileRequest pointRequest{};
    pointRequest.context.vertex = ShaderRecompiler::ShaderVertexStageInfo{};
    pointRequest.context.vertex->paClVsOutCntl = 0x01210000u;
    pointRequest.context.vertex->pointList = true;
    pointRequest.context.vertex->pointSize = 4.0f;
    pointRequest.context.vertex->pointSizeMin = 0.5f;
    pointRequest.context.vertex->pointSizeMax = 8.0f;
    const auto pointEncoded = serializer.Serialize(pointRequest);
    const auto pointReplay = serializer.Deserialize(pointEncoded);
    Require(pointReplay.request.context.vertex.has_value() && pointReplay.request.context.vertex->paClVsOutCntl == 0x01210000u && pointReplay.request.context.vertex->pointList);
    Require(pointReplay.request.context.vertex->pointSize == 4.0f && pointReplay.request.context.vertex->pointSizeMin == 0.5f && pointReplay.request.context.vertex->pointSizeMax == 8.0f);
    Require(serializer.Serialize(pointReplay.request) == pointEncoded);
    auto pointBytes = DecodeBase64(pointEncoded);
    pointBytes.pop_back();
    pointBytes.pop_back();
    pointBytes.pop_back();
    bool pointRejected = false;
    try {
        static_cast<void>(serializer.Deserialize(EncodeBase64(pointBytes)));
    } catch (const std::runtime_error& error) {
        pointRejected = std::string_view(error.what()) == "RequestSerializer: truncated data";
    }
    Require(pointRejected);

    pointRequest.target.depthRangeRemap = true;
    pointRequest.target.float32FlushToZero = true;
    pointRequest.context.vertex->depthRemapMode = 2u;
    const auto remappedPoint = serializer.Deserialize(serializer.Serialize(pointRequest));
    Require(remappedPoint.request.target.depthRangeRemap && remappedPoint.request.context.vertex->depthRemapMode == 2u);
    Require(serializer.Serialize(remappedPoint.request) == serializer.Serialize(pointRequest));

    for (const bool spanned : {false, true}) {
        for (const std::uint8_t version : {32u, 33u, 34u, 35u, 36u, 37u, 38u}) {
            pointRequest.target.spannedWaves = spanned;
            auto legacy = DecodeBase64(serializer.Serialize(pointRequest));
            legacy.resize(legacy.size() - (version >= 36u ? 22u : version >= 35u ? 23u : 28u));
            legacy[4] = version;
            const auto replayLegacy = serializer.Deserialize(EncodeBase64(legacy));
            Require(replayLegacy.request.target.spannedWaves == spanned);
            Require(replayLegacy.request.target.float32FlushToZero == (version >= 36u));
            Require(replayLegacy.request.target.depthRangeRemap == (version >= 35u));
            Require(replayLegacy.request.context.vertex.has_value());
            Require(replayLegacy.request.context.vertex->depthRemapMode == (version >= 35u ? 2u : 0u));
            Require(replayLegacy.request.context.vertex->paClVsOutCntl == pointRequest.context.vertex->paClVsOutCntl);
            Require(!replayLegacy.request.context.vertex->pointList && replayLegacy.request.context.vertex->pointSize == 1.0f);
            Require(replayLegacy.request.context.vertex->pointSizeMin == 0.0f && replayLegacy.request.context.vertex->pointSizeMax == 0.0f);
            const auto upgradedLegacy = DecodeBase64(serializer.Serialize(replayLegacy.request));
            Require(upgradedLegacy[4] == 51u && upgradedLegacy.size() == legacy.size() + (version >= 36u ? 22u : version >= 35u ? 23u : 28u));
        }
    }

    pixel.sampleOrder = {0x76543210u, 0x01234567u, 0x10325476u, 0x67452301u};
    pointRequest.context.pixel = pixel;
    const auto sampledPoint = serializer.Deserialize(serializer.Serialize(pointRequest));
    Require(sampledPoint.request.context.vertex->pointList && sampledPoint.request.context.pixel->sampleOrder == pixel.sampleOrder);
    auto legacy43 = DecodeBase64(serializer.Serialize(pointRequest));
    const auto mergeOffset = std::search(legacy43.begin(), legacy43.end(), pixel.sampleLocations.begin(), pixel.sampleLocations.end()) - legacy43.begin() + static_cast<std::ptrdiff_t>(pixel.sampleLocations.size() + 1u + sizeof(pixel.sampleOrder));
    EraseDefaultTargetMerge(legacy43, mergeOffset);
    legacy43.resize(legacy43.size() - 4u);
    legacy43.pop_back();
    legacy43.pop_back();
    legacy43.pop_back();
    legacy43.pop_back();
    legacy43[4] = 43u;
    const auto pointReplay43 = serializer.Deserialize(EncodeBase64(legacy43));
    Require(pointReplay43.request.context.pixel->sampleOrder == pixel.sampleOrder && pointReplay43.request.context.vertex->pointList && pointReplay43.request.context.vertex->pointSize == 4.0f);
    Require(serializer.Serialize(pointReplay43.request) == serializer.Serialize(pointRequest));
    auto legacy42 = legacy43;
    legacy42.resize(legacy42.size() - 13u);
    legacy42[4] = 42u;
    const auto pointReplay42 = serializer.Deserialize(EncodeBase64(legacy42));
    Require(pointReplay42.request.context.pixel->sampleOrder == pixel.sampleOrder && !pointReplay42.request.context.vertex->pointList && pointReplay42.request.context.vertex->pointSize == 1.0f);
    pixel.sampleOrder = {};
    pointRequest.context.pixel = pixel;
    auto legacy34 = DecodeBase64(serializer.Serialize(pointRequest));
    legacy34.resize(legacy34.size() - 28u);
    const auto stencil34 = std::search(legacy34.begin(), legacy34.end(), pixel.sampleLocations.begin(), pixel.sampleLocations.begin() + 4) + pixel.sampleLocations.size();
    const auto legacyPixelTail = static_cast<std::ptrdiff_t>(1u + sizeof(pixel.sampleOrder));
    Require(legacy34.end() - stencil34 > legacyPixelTail && std::all_of(stencil34, stencil34 + legacyPixelTail, [](std::uint8_t value) { return value == 0u; }));
    EraseDefaultTargetMerge(legacy34, stencil34 - legacy34.begin() + legacyPixelTail);
    legacy34.erase(stencil34, stencil34 + legacyPixelTail);
    legacy34[4] = 34u;
    const auto replay34 = serializer.Deserialize(EncodeBase64(legacy34));
    Require(replay34.request.context.pixel.has_value() && replay34.request.context.pixel->iteratesSamples && replay34.request.context.pixel->positionLocation == pixel.positionLocation && replay34.request.context.pixel->sampleLocations == pixel.sampleLocations);
    Require(replay34.request.context.vertex.has_value() && !replay34.request.context.vertex->pointList && replay34.request.context.vertex->pointSize == 1.0f);

    for (const bool fused : {false, true}) {
        request.target.fusedExactArithmetic = fused;
        request.target.largePixelFunctionLdsBuffer = false;
        auto bytes = DecodeBase64(serializer.Serialize(request));
        Require(serializer.Deserialize(EncodeBase64(bytes)).request.target.fusedExactArithmetic == fused && bytes[bytes.size() - 8u] == (fused ? 1u : 0u) && bytes[bytes.size() - 7u] == 0u);
        bytes.pop_back();
        bytes.pop_back();
        const auto fusedMerge = std::search(bytes.begin(), bytes.end(), pixel.sampleLocations.begin(), pixel.sampleLocations.end()) - bytes.begin() + static_cast<std::ptrdiff_t>(pixel.sampleLocations.size() + 1u + sizeof(pixel.sampleOrder));
        EraseDefaultTargetMerge(bytes, fusedMerge);
        bytes.resize(bytes.size() - 4u);
        bytes.pop_back();
        bytes[4] = 44u;
        Require(serializer.Deserialize(EncodeBase64(bytes)).request.target.fusedExactArithmetic == fused);
        bytes[4] = 43u;
        bytes.pop_back();
        Require(!serializer.Deserialize(EncodeBase64(bytes)).request.target.fusedExactArithmetic);
    }

    Require(request.context.pixel.has_value());
    request.context.pixel->ditheredAlphaToCoverage = true;
    request.context.pixel->ditheredAlphaSamples = 2u;
    auto twoSample = DecodeBase64(serializer.Serialize(request));
    const auto twoReplay = serializer.Deserialize(EncodeBase64(twoSample));
    Require(twoReplay.request.context.pixel->ditheredAlphaToCoverage && twoReplay.request.context.pixel->ditheredAlphaSamples == 2u);
    request.context.pixel->ditheredAlphaSamples = 8u;
    const auto eightSample = DecodeBase64(serializer.Serialize(request));
    const auto eightReplay = serializer.Deserialize(EncodeBase64(eightSample));
    Require(eightReplay.request.context.pixel->ditheredAlphaToCoverage && eightReplay.request.context.pixel->ditheredAlphaSamples == 8u);
    Require(twoSample.size() == eightSample.size());
    std::size_t ditherByte = 0u;
    std::size_t differences = 0u;
    for (std::size_t index = 0; index < twoSample.size(); ++index) {
        if (twoSample[index] == eightSample[index]) continue;
        ditherByte = index;
        ++differences;
    }
    Require(differences == 1u && twoSample[ditherByte] == 2u && eightSample[ditherByte] == 1u);
    const auto rejectsDither = [&](std::vector<std::uint8_t> bytes) {
        try {
            static_cast<void>(serializer.Deserialize(EncodeBase64(bytes)));
        } catch (const std::runtime_error& error) {
            return std::string_view(error.what()) == "RequestSerializer: invalid alpha-to-coverage dither value";
        }
        return false;
    };
    auto version46Dither = twoSample;
    version46Dither.pop_back();
    version46Dither.pop_back();
    version46Dither[4] = 46u;
    Require(rejectsDither(version46Dither));
    version46Dither[ditherByte] = 1u;
    Require(serializer.Deserialize(EncodeBase64(version46Dither)).request.context.pixel->ditheredAlphaSamples == 8u);
    auto unknownDither = twoSample;
    unknownDither[ditherByte] = 3u;
    Require(rejectsDither(unknownDither));

    ShaderRecompiler::RecompileRequest domain{};
    domain.shader.stage = ShaderRecompiler::ShaderStage::TessellationEvaluation;
    for (const bool subgroups : {false, true}) {
        domain.target.tessellationEvaluationSubgroups = subgroups;
        const auto encoded = serializer.Serialize(domain);
        auto bytes = DecodeBase64(encoded);
        Require(bytes.size() > 8u && bytes[4] == 51u && bytes.back() == (subgroups ? 1u : 0u));
        Require(serializer.Deserialize(encoded).request.target.tessellationEvaluationSubgroups == subgroups);
        bytes.pop_back();
        bool truncatedRejected = false;
        try {
            static_cast<void>(serializer.Deserialize(EncodeBase64(bytes)));
        } catch (const std::runtime_error& error) {
            truncatedRejected = std::string_view(error.what()) == "RequestSerializer: truncated data";
        }
        Require(truncatedRejected);
        bytes[4] = 49u;
        const auto replay49 = serializer.Deserialize(EncodeBase64(bytes));
        Require(replay49.request.shader.stage == ShaderRecompiler::ShaderStage::TessellationEvaluation && replay49.request.target.tessellationEvaluationSubgroups);
        auto upgraded = bytes;
        upgraded[4] = 51u;
        upgraded.push_back(1u);
        Require(serializer.Serialize(replay49.request) == EncodeBase64(upgraded));
        for (auto skipped : {bytes, upgraded}) {
            skipped[4] = 50u;
            bool skippedRejected = false;
            try {
                static_cast<void>(serializer.Deserialize(EncodeBase64(skipped)));
            } catch (const std::runtime_error& error) {
                skippedRejected = std::string_view(error.what()) == "unsupported recompile request serialization version";
            }
            Require(skippedRejected);
        }
    }

    return 0;
}
