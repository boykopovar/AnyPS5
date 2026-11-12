#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 61;
constexpr std::uint32_t Height = 3;
constexpr std::uint32_t Levels = 2;
constexpr std::size_t Bytes = 16384;
constexpr std::uint32_t Sentinel = 0xcafe0000u;
constexpr std::uint32_t Identity = 0xfacu;
constexpr std::uint32_t Unwritten = 0xdeadbeefu;
constexpr std::array StoreData{0x9e3779b1u, 0x7f4a7c15u, 0xfe94f82au, 0x7ddf743fu};
alignas(256) std::array<std::array<std::uint8_t, Bytes>, 2> Texels{};
alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(4096) std::array<std::uint32_t, 16384> Output{};
alignas(256) std::array<std::uint32_t, 8 * 4> Table{};
alignas(256) std::array<std::uint32_t, 4> Keys{0u, 1u, 2u, 3u};
alignas(256) std::array<std::uint32_t, 32> Srt{};

struct Format {
    std::uint32_t id;
    std::uint32_t bytes;
    std::uint32_t components;
    bool floating = false;
};

constexpr std::array LoadFormats{
    Format{1u, 1u, 1u}, Format{5u, 1u, 1u}, Format{6u, 1u, 1u},
    Format{7u, 2u, 1u}, Format{11u, 2u, 1u}, Format{12u, 2u, 1u},
    Format{14u, 2u, 2u}, Format{18u, 2u, 2u}, Format{19u, 2u, 2u},
    Format{20u, 4u, 1u}, Format{21u, 4u, 1u}, Format{22u, 4u, 1u, true},
    Format{23u, 4u, 2u}, Format{27u, 4u, 2u}, Format{28u, 4u, 2u},
    Format{56u, 4u, 4u}, Format{60u, 4u, 4u}, Format{61u, 4u, 4u},
    Format{62u, 8u, 2u}, Format{63u, 8u, 2u}, Format{64u, 8u, 2u, true},
    Format{65u, 8u, 4u}, Format{69u, 8u, 4u}, Format{70u, 8u, 4u},
    Format{75u, 16u, 4u}, Format{76u, 16u, 4u}, Format{77u, 16u, 4u, true}
};

std::array<std::uint32_t, 4> Buffer(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> Descriptor(std::uint32_t entry, const Format& format, std::uint32_t swizzle = Identity) {
    const auto address = reinterpret_cast<std::uintptr_t>(Texels[entry].data());
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u) | (format.id << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u), swizzle | ((Levels - 1u) << 16u) | (9u << 28u), 0u, 1u << 4u, 0u, 0u};
}

void SetEntry(std::uint32_t entry, const std::array<std::uint32_t, 8>& descriptor) {
    std::copy(descriptor.begin(), descriptor.end(), Table.begin() + entry * 8u);
}

void FillTexture(std::uint32_t entry, const Format& format) {
    Texels[entry].fill(0xeeu);
    const auto descriptor = Descriptor(entry, format);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(surface.guestBytes <= Bytes && surface.mips.size() == Levels, "image table packed surface size");
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = surface.mips[level];
        for (std::uint32_t row = 0; row < std::max(Height >> level, 1u); ++row) {
            for (std::uint32_t column = 0; column < std::max(Width >> level, 1u); ++column) {
                auto* texel = Texels[entry].data() + mip.tiledOffset + row * mip.pitchBytes + column * format.bytes;
                for (std::uint32_t byte = 0; byte < format.bytes; ++byte) {
                    texel[byte] = static_cast<std::uint8_t>(entry * 97u + level * 101u + row * 53u + column * 37u + byte * 71u);
                    if (column < 4u) texel[byte] = std::array<std::uint8_t, 4>{0u, 255u, 127u, 128u}[column];
                }
                if (column < 4u) {
                    const auto componentBytes = format.bytes / format.components;
                    if (componentBytes == 2u) {
                        const auto value = std::array<std::uint16_t, 4>{0u, 0xffffu, 0x7fffu, 0x8000u}[column];
                        for (std::uint32_t component = 0; component < format.components; ++component) std::memcpy(texel + component * 2u, &value, 2u);
                    } else if (componentBytes == 4u) {
                        const auto value = std::array<std::uint32_t, 4>{0u, 0xffffffffu, 0x7fffffffu, 0x80000000u}[column];
                        for (std::uint32_t component = 0; component < format.components; ++component) std::memcpy(texel + component * 4u, &value, 4u);
                    }
                }
                if (format.floating) {
                    for (std::uint32_t word = 0; word < format.bytes / 4u; ++word) {
                        std::uint32_t bits;
                        std::memcpy(&bits, texel + word * 4u, 4u);
                        bits = (bits & 0x807fffffu) | (0x7eu << 23u);
                        std::memcpy(texel + word * 4u, &bits, 4u);
                    }
                }
            }
        }
    }
    SetEntry(entry, descriptor);
}

void FillInput(bool mip) {
    constexpr std::array xs{0u, 1u, 2u, 3u, 5u, 6u, 7u, Width - 4u, Width - 3u, Width - 2u, Width - 1u, Width, 28u, 29u, 30u, 0xffffffffu};
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Input[lane * 4u] = xs[lane % xs.size()];
        Input[lane * 4u + 1u] = lane < 24u ? 0u : lane % 5u;
        Input[lane * 4u + 2u] = mip ? lane < 16u ? 0u : lane < 28u ? 1u : Levels : 0u;
        Input[lane * 4u + 3u] = 0u;
    }
}

std::vector<std::uint32_t> Code(std::uint32_t opcode, std::uint32_t dmask, bool table, bool inactive = false) {
    std::vector<std::uint32_t> code{
        0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u,
        0xf4080700u, 0xfa000020u, 0xf4080300u, 0xfa000030u};
    if (table) {
        const std::array select{0x8f108202u, 0xf4200444u, 0x20000000u, 0x8f128511u, 0xf42c0802u, 0x24000000u};
        code.insert(code.end(), select.begin(), select.end());
    } else {
        code.insert(code.end(), {0xf40c0800u, 0xfa000040u});
    }
    code.insert(code.end(), {0x34060084u, 0xe0381000u, 0x80031e03u, 0xbf8c3f70u});
    for (std::uint32_t component = 0; component < 4u; ++component) {
        code.push_back(0x7e0002ffu | ((10u + component) << 17u));
        code.push_back(opcode == 10u || opcode == 11u ? StoreData[component] : Sentinel + component);
    }
    if (inactive) code.insert(code.end(), {0x7da400ffu, 0x00001000u});
    code.insert(code.end(), {0xf0000008u | (opcode << 18u) | (dmask << 8u), 0x00080a1eu, 0xbf8c3f70u,
        0xe0781000u, 0x80070a03u, 0xbf810000u});
    return code;
}

struct Result {
    std::array<std::uint32_t, Threads * 4 * 2> words;
    std::uint64_t artifact;
    std::uint32_t faults;
    std::uint32_t tables;
    bool cacheHit;
    std::string log;
};

class Capture {
public:
    Capture() {
        file = std::tmpfile();
        Require(file != nullptr, "cannot capture packed table shader faults");
        std::fflush(stderr);
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }

    ~Capture() {
        if (file != nullptr) Finish();
    }

    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        std::array<char, 4096> buffer{};
        for (std::size_t count = 0; (count = std::fread(buffer.data(), 1u, buffer.size(), file)) != 0u;) text.append(buffer.data(), count);
        std::fclose(file);
        file = nullptr;
        std::fwrite(text.data(), 1u, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

Result Run(AgcDriver::VulkanDevice& device, const std::vector<std::uint32_t>& code, std::uint32_t selected, std::uint32_t groups = 1u, bool dispatch = true) {
    Output.fill(Unwritten);
    const auto keys = Buffer(Keys.data(), sizeof(Keys));
    const auto table = Buffer(Table.data(), sizeof(Table));
    const auto output = Buffer(Output.data(), sizeof(Output));
    const auto input = Buffer(Input.data(), sizeof(Input));
    std::copy(table.begin(), table.end(), Srt.begin());
    std::copy(keys.begin(), keys.end(), Srt.begin() + 4u);
    std::copy(output.begin(), output.end(), Srt.begin() + 8u);
    std::copy(input.begin(), input.end(), Srt.begin() + 12u);
    std::copy_n(Table.begin() + selected * 8u, 8u, Srt.begin() + 16u);
    Srt[24] = selected * 4u;
    Keys[0] = selected;
    const auto root = reinterpret_cast<std::uintptr_t>(Srt.data());
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(root), static_cast<std::uint32_t>(root >> 32u)};
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 4> memory{{
        {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)},
        {root, std::as_bytes(std::span(Srt))},
        {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
        {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}},
        {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory}, device.Target(), {0u, 0u, 0u, 128u}};
    const auto shader = ShaderRecompiler::Recompile(request);
    std::uint32_t tables = 0;
    for (const auto& binding : shader.bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::ImageTable || binding.role == ShaderRecompiler::DescriptorRole::ImageStorageTable) ++tables;
    }
    if (!dispatch) return {{}, shader.variantId, shader.imageTableFaults, tables, shader.cacheHit, {}};
    Capture capture;
    device.Dispatch(shader, groups, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table packed execution");
    device.WaitIdle();
    Result result{{}, shader.variantId, shader.imageTableFaults, tables, shader.cacheHit, capture.Finish()};
    std::copy_n(Output.begin(), result.words.size(), result.words.begin());
    return result;
}

std::array<std::uint32_t, 4> Raw(const Format& format, std::uint32_t selected, std::uint32_t level, std::uint32_t column, std::uint32_t row) {
    const auto descriptor = Descriptor(selected, format);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    const auto& mip = surface.mips.at(level);
    std::array<std::uint32_t, 4> words{};
    std::memcpy(words.data(), Texels[selected].data() + mip.tiledOffset + row * mip.pitchBytes + column * format.bytes, format.bytes);
    return words;
}

void CheckPacked(const Result& result, const Format& format, std::uint32_t selected, std::uint32_t dmask, bool mip, bool sign, const std::string& name, std::uint32_t baseLevel = 0u) {
    const auto count = static_cast<std::uint32_t>(std::popcount(dmask));
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto column = Input[lane * 4u];
        const auto row = Input[lane * 4u + 1u];
        const auto level = baseLevel + (mip ? Input[lane * 4u + 2u] : 0u);
        const bool inside = level < Levels && column < std::max(Width >> level, 1u) && row < std::max(Height >> level, 1u);
        auto expected = inside ? Raw(format, selected, level, column, row) : std::array<std::uint32_t, 4>{};
        if (sign && format.bytes < 4u) {
            const auto shift = 32u - format.bytes * 8u;
            expected[0] = static_cast<std::uint32_t>(static_cast<std::int32_t>(expected[0] << shift) >> shift);
        }
        std::uint32_t source = 0;
        for (std::uint32_t component = 0; component < 4u; ++component) {
            while (source < 4u && (dmask & (1u << source)) == 0u) ++source;
            const auto wanted = component < count ? expected[source++] : Sentinel + component;
            Require(result.words[lane * 4u + component] == wanted, name + ": reconstructed component " + std::to_string(component) + " lane " + std::to_string(lane) + " actual " + std::to_string(result.words[lane * 4u + component]) + " expected " + std::to_string(wanted));
        }
    }
}

void PackedLoads(AgcDriver::VulkanDevice& device, std::uint32_t opcode) {
    for (const auto dmask : {1u, 5u, 10u, 15u}) {
        const auto directCode = Code(opcode, dmask, false);
        const auto tableCode = Code(opcode, dmask, true);
        std::uint64_t artifact = 0;
        for (std::size_t index = 0; index < LoadFormats.size(); index += 2u) {
            FillTexture(0u, LoadFormats[index]);
            FillTexture(1u, LoadFormats[(index + 1u) % LoadFormats.size()]);
            FillInput(opcode >= 4u);
            for (const auto selected : {0u, 1u, 0u}) {
                const auto& format = LoadFormats[(index + selected) % LoadFormats.size()];
                const auto name = "table packed opcode " + std::to_string(opcode) + " format " + std::to_string(format.id) + " mask " + std::to_string(dmask) + " entry " + std::to_string(selected);
                const auto direct = Run(device, directCode, selected);
                CheckPacked(direct, format, selected, dmask, opcode >= 4u, (opcode & 1u) != 0u, name + " direct");
                const auto table = Run(device, tableCode, selected);
                Require(table.tables != 0u, name + ": no runtime table");
                Require(table.log.find("image table:") == std::string::npos, name + ": unsupported operation or poisoned valid entry:\n" + table.log);
                CheckPacked(table, format, selected, dmask, opcode >= 4u, (opcode & 1u) != 0u, name);
                Require(direct.words == table.words, name + ": direct/table mismatch");
                if (artifact != 0u) Require(table.cacheHit && table.artifact == artifact, name + ": descriptor selection rebuilt shader");
                artifact = table.artifact;
            }
        }
    }
    std::cout << "table packed opcode " << opcode << " loads and runtime reselection passed\n";
}

std::uint32_t Component(const Format& format, const std::array<std::uint32_t, 4>& raw, std::uint32_t component) {
    const auto bits = format.bytes * 8u / format.components;
    const auto value = (raw[component * bits / 32u] >> (component * bits % 32u)) & (bits == 32u ? 0xffffffffu : (1u << bits) - 1u);
    const bool signedInteger = format.id == 6u || format.id == 12u || format.id == 19u;
    const bool snorm = format.id == 2u || format.id == 8u || format.id == 15u;
    const bool unorm = format.id == 1u || format.id == 7u || format.id == 14u;
    const auto signedValue = static_cast<std::int32_t>(value << (32u - bits)) >> (32u - bits);
    if (signedInteger) return static_cast<std::uint32_t>(signedValue);
    if (unorm) return std::bit_cast<std::uint32_t>(static_cast<float>(value) / static_cast<float>((1u << bits) - 1u));
    if (snorm) return std::bit_cast<std::uint32_t>(std::max(-1.0f, static_cast<float>(signedValue) / static_cast<float>((1u << (bits - 1u)) - 1u)));
    if (format.id == 13u) {
        const auto exponent = (value >> 10u) & 31u;
        const auto fraction = value & 1023u;
        const auto magnitude = exponent == 0u ? std::ldexp(static_cast<float>(fraction), -24) : std::ldexp(1.0f + static_cast<float>(fraction) / 1024.0f, static_cast<int>(exponent) - 15);
        return std::bit_cast<std::uint32_t>((value & 0x8000u) != 0u ? -magnitude : magnitude);
    }
    return value;
}

void CheckBy(const Result& result, const Format& format, std::uint32_t selected, std::uint32_t elements, std::uint32_t dmask, bool mip, bool packed, const std::string& name) {
    const auto count = static_cast<std::uint32_t>(std::popcount(dmask));
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto column = Input[lane * 4u];
        const auto row = Input[lane * 4u + 1u];
        const auto level = mip ? Input[lane * 4u + 2u] : 0u;
        const auto width = level < Levels ? std::max(Width >> level, 1u) : 0u;
        const auto height = level < Levels ? std::max(Height >> level, 1u) : 0u;
        const bool inside = column < width && width - column >= elements && row < height;
        std::array<std::uint32_t, 4> expected{};
        for (std::uint32_t element = 0; inside && element < elements; ++element) {
            const auto raw = Raw(format, selected, level, (column & ~(elements - 1u)) + element, row);
            if (packed) {
                expected[0] |= raw[0] << (element * format.bytes * 8u);
            } else {
                for (std::uint32_t component = 0; component < format.components; ++component) {
                    expected[element * format.components + component] = Component(format, raw, component);
                }
            }
        }
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto wanted = component < count ? expected[component] : Sentinel + component;
            const auto actual = result.words[lane * 4u + component];
            const bool normalized = !packed && inside && component < count && (format.id == 1u || format.id == 2u || format.id == 7u || format.id == 8u || format.id == 14u || format.id == 15u);
            const auto expectedFloat = std::bit_cast<float>(wanted);
            const bool interior = normalized && expectedFloat != 0.0f && std::abs(expectedFloat) != 1.0f;
            const bool matches = interior ? std::abs(std::bit_cast<float>(actual) - expectedFloat) <= 0.00000012f : actual == wanted;
            Require(matches, name + ": BY component order/alignment lane " + std::to_string(lane) + " component " + std::to_string(component) + " actual " + std::to_string(actual) + " expected " + std::to_string(wanted));
        }
    }
}

void ByLoads(AgcDriver::VulkanDevice& device) {
    constexpr std::array formats{
        Format{1u, 1u, 1u}, Format{5u, 1u, 1u}, Format{6u, 1u, 1u}, Format{2u, 1u, 1u},
        Format{7u, 2u, 1u}, Format{11u, 2u, 1u}, Format{12u, 2u, 1u}, Format{8u, 2u, 1u}, Format{13u, 2u, 1u},
        Format{14u, 2u, 2u}, Format{18u, 2u, 2u}, Format{19u, 2u, 2u}, Format{15u, 2u, 2u}};
    for (const auto opcode : {0x42u, 0x43u, 0x4au, 0x4bu, 0x70u, 0x71u, 0x73u, 0x74u}) {
        const bool packed = opcode >= 0x70u;
        const auto elements = packed ? opcode == 0x71u || opcode == 0x74u ? 4u : 2u : (opcode & 1u) != 0u ? 4u : 2u;
        const bool mip = packed ? opcode >= 0x73u : (opcode & 8u) != 0u;
        std::map<std::uint32_t, std::uint64_t> artifacts;
        std::map<std::uint32_t, std::vector<std::uint32_t>> tableCodes;
        for (const auto& format : formats) {
            if (elements == 4u && format.bytes != 1u) continue;
            if (packed && (format.id == 2u || format.id == 8u || format.id == 13u || format.id == 15u)) continue;
            const auto count = packed ? 1u : elements * format.components;
            const auto dmask = (1u << count) - 1u;
            auto [code, inserted] = tableCodes.try_emplace(dmask, Code(opcode, dmask, true));
            const auto directCode = Code(opcode, dmask, false);
            const Format other = format.bytes == 1u ? Format{5u, 1u, 1u} : format.components == 2u ? Format{18u, 2u, 2u} : Format{11u, 2u, 1u};
            FillTexture(0u, format);
            FillTexture(1u, other);
            if (format.id == 13u) {
                const auto descriptor = Descriptor(0u, format);
                const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
                for (const auto& level : surface.mips) {
                    for (std::uint32_t row = 0; row < level.height; ++row) {
                        for (std::uint32_t column = 0; column < level.width; ++column) {
                            const std::uint16_t value = static_cast<std::uint16_t>(0x3800u + (column * 17u + row * 31u) % 0x400u);
                            std::memcpy(Texels[0].data() + level.tiledOffset + row * level.pitchBytes + column * 2u, &value, 2u);
                        }
                    }
                }
            }
            FillInput(mip);
            const auto initial = Texels;
            for (const auto selected : {0u, 1u, 0u}) {
                const auto& selectedFormat = selected == 0u ? format : other;
                const auto name = "table BY/PCK opcode " + std::to_string(opcode) + " format " + std::to_string(selectedFormat.id) + " entry " + std::to_string(selected);
                const auto direct = Run(device, directCode, selected);
                CheckBy(direct, selectedFormat, selected, elements, dmask, mip, packed, name + " direct");
                const auto table = Run(device, code->second, selected);
                Require(table.tables != 0u && table.log.find("image table:") == std::string::npos, name + ": valid entry rejected:\n" + table.log);
                CheckBy(table, selectedFormat, selected, elements, dmask, mip, packed, name);
                Require(table.words == direct.words, name + ": direct/table raw-bit mismatch");
                auto& artifact = artifacts[dmask];
                if (artifact != 0u) Require(table.cacheHit && table.artifact == artifact, name + ": runtime format rebuilt BY shader");
                artifact = table.artifact;
            }
            Require(Texels == initial, "table BY/PCK load wrote texture storage");
        }
    }
    std::cout << "table BY2/BY4 and PCK2/PCK4 direct comparisons passed\n";
}

void CheckStored(const Format& format, std::uint32_t selected, std::uint32_t dmask, const std::array<std::uint8_t, Bytes>& before, const std::string& name) {
    auto expected = before;
    std::array<std::uint32_t, 8> descriptor{};
    std::copy_n(Table.begin() + selected * 8u, 8u, descriptor.begin());
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    const auto level = (descriptor[3] >> 12u) & 15u;
    const auto& mip = surface.mips[level];
    std::array<std::uint32_t, 4> data{};
    std::uint32_t source = 0;
    for (std::uint32_t word = 0; word < 4u; ++word) {
        if ((dmask & (1u << word)) != 0u) data[word] = StoreData[source++];
    }
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto column = Input[lane * 4u];
        const auto row = Input[lane * 4u + 1u];
        if (column >= std::max(Width >> level, 1u) || row >= std::max(Height >> level, 1u)) continue;
        std::memcpy(expected.data() + mip.tiledOffset + row * mip.pitchBytes + column * format.bytes, data.data(), format.bytes);
    }
    Require(Texels[selected] == expected, name + ": packed store bits, dmask order or untouched texels differ");
}

void PackedStores(AgcDriver::VulkanDevice& device) {
    constexpr std::array formats{
        Format{5u, 1u, 1u}, Format{11u, 2u, 1u}, Format{18u, 2u, 2u},
        Format{20u, 4u, 1u}, Format{21u, 4u, 1u}, Format{22u, 4u, 1u, true},
        Format{27u, 4u, 2u}, Format{60u, 4u, 4u}, Format{62u, 8u, 2u},
        Format{63u, 8u, 2u}, Format{64u, 8u, 2u, true}, Format{69u, 8u, 4u},
        Format{75u, 16u, 4u}, Format{76u, 16u, 4u}, Format{77u, 16u, 4u, true}};
    for (const auto dmask : {1u, 5u, 10u, 15u}) {
        const auto directCode = Code(10u, dmask, false);
        const auto tableCode = Code(10u, dmask, true);
        const auto readCode = Code(2u, 15u, true);
        std::uint64_t artifact = 0;
        for (const auto& format : formats) {
            FillInput(false);
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                Input[lane * 4u] = lane;
                Input[lane * 4u + 1u] = lane % Height;
            }
            for (const auto selected : {0u, 1u}) {
                FillTexture(selected, format);
                FillTexture(1u - selected, formats[(static_cast<std::size_t>(&format - formats.data()) + 1u) % formats.size()]);
                const auto initial = Texels;
                const auto name = "table packed store format " + std::to_string(format.id) + " mask " + std::to_string(dmask) + " entry " + std::to_string(selected);
                Run(device, directCode, selected);
                CheckStored(format, selected, dmask, initial[selected], name + " direct");
                const auto direct = Texels[selected];
                Texels = initial;
                const auto table = Run(device, tableCode, selected);
                Require(table.tables != 0u && table.log.find("image table:") == std::string::npos, name + ": valid storage entry rejected:\n" + table.log);
                CheckStored(format, selected, dmask, initial[selected], name);
                Require(Texels[selected] == direct && Texels[1u - selected] == initial[1u - selected], name + ": wrong destination table entry");
                const auto read = Run(device, readCode, selected);
                CheckPacked(read, format, selected, 15u, false, false, name + " round trip");
                if (artifact != 0u) Require(table.cacheHit && table.artifact == artifact, name + ": storage format rebuilt shader");
                artifact = table.artifact;
            }
        }
    }
    std::cout << "table packed stores, masks and round trips passed\n";
}

void Guards(AgcDriver::VulkanDevice& device) {
    constexpr Format good{5u, 1u, 1u};
    FillTexture(0u, good);
    FillTexture(1u, good);
    FillInput(false);
    SetEntry(2u, {});
    const auto unsupported = Descriptor(1u, Format{78u, 1u, 1u});
    SetEntry(3u, unsupported);
    for (const auto opcode : {2u, 0x42u, 0x70u, 10u}) {
        const auto dmask = opcode == 0x42u ? 3u : 1u;
        const auto code = Code(opcode, dmask, true);
        const auto inactiveCode = Code(opcode, dmask, true, true);
        const auto initial = Texels;
        const auto inactive = Run(device, inactiveCode, 3u);
        Require(inactive.log.find("image table:") == std::string::npos, "inactive packed table operation reported a fault");
        Require(std::all_of(inactive.words.begin(), inactive.words.end(), [](auto word) { return word == Unwritten; }), "inactive packed table operation wrote output");
        Require(Texels == initial, "inactive packed table operation wrote image");
        const auto null = Run(device, code, 2u);
        Require(null.log.find("image table:") == std::string::npos, "null packed table operation reported a fault");
        Require(Texels == initial, "null packed table entry wrote image");
        if (opcode != 10u) {
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                for (std::uint32_t component = 0; component < 4u; ++component) {
                    const auto expected = component < static_cast<std::uint32_t>(std::popcount(dmask)) ? 0u : Sentinel + component;
                    Require(null.words[lane * 4u + component] == expected, "null packed table load did not return zero");
                }
            }
        }
        const auto poisoned = Run(device, code, 3u);
        Require(poisoned.faults != 0u && poisoned.log.find("image table:") != std::string::npos, "unrecoverable packed table format was not poisoned");
        Require(Texels == initial, "poisoned packed table operation wrote image");
        if (opcode != 10u) {
            for (std::size_t word = 0; word < null.words.size(); ++word) {
                Require(poisoned.words[word] == null.words[word], "poisoned packed table opcode " + std::to_string(opcode) + " word " + std::to_string(word) + " actual " + std::to_string(poisoned.words[word]) + " expected " + std::to_string(null.words[word]));
            }
        }
    }
    std::cout << "table packed inactive, null and poisoned entries passed\n";
}

std::vector<std::uint32_t> GroupCode(std::uint32_t opcode, std::uint32_t dmask) {
    auto code = Code(opcode, dmask, true);
    const std::array address{0x7e120202u, 0x34101285u, 0x4a101100u, 0x34101084u};
    code.insert(code.end() - 3, address.begin(), address.end());
    code[code.size() - 2u] = 0x80070a08u;
    return code;
}

Result Group(const Result& result, std::uint32_t group) {
    auto selected = result;
    std::copy_n(result.words.begin() + group * Threads * 4u, Threads * 4u, selected.words.begin());
    return selected;
}

void MixedGroups(AgcDriver::VulkanDevice& device) {
    Table.fill(0u);
    constexpr std::array formats{Format{1u, 1u, 1u}, Format{6u, 1u, 1u}, Format{7u, 2u, 1u}, Format{18u, 2u, 2u}};
    for (const auto opcode : {2u, 3u, 4u, 5u, 0x42u, 0x43u, 0x4au, 0x4bu, 0x70u, 0x71u, 0x73u, 0x74u}) {
        const bool by = opcode >= 0x42u;
        const bool packedBy = opcode >= 0x70u;
        const auto elements = packedBy ? opcode == 0x71u || opcode == 0x74u ? 4u : 2u : (opcode & 1u) != 0u ? 4u : 2u;
        const auto dmask = !by ? 15u : packedBy ? 1u : (1u << elements) - 1u;
        const bool mip = !by ? opcode >= 4u : packedBy ? opcode >= 0x73u : opcode >= 0x4au;
        const auto code = GroupCode(opcode, dmask);
        std::uint64_t artifact = 0;
        for (std::uint32_t pair = 0; pair < 2u; ++pair) {
            const auto first = formats[pair * 2u];
            const auto second = formats[pair * 2u + 1u];
            if (by && (elements == 4u || !packedBy) && pair != 0u) continue;
            FillTexture(0u, first);
            FillTexture(1u, second);
            FillInput(mip);
            const auto initial = Texels;
            for (const auto selected : {0u, 1u, 0u}) {
                Keys[1] = 1u - selected;
                const auto result = Run(device, code, selected, 2u);
                Require(result.tables != 0u && result.log.find("image table:") == std::string::npos, "mixed-group packed image table faulted");
                for (std::uint32_t group = 0; group < 2u; ++group) {
                    const auto entry = group == 0u ? selected : 1u - selected;
                    const auto& format = entry == 0u ? first : second;
                    const auto words = Group(result, group);
                    const auto name = "mixed-group opcode " + std::to_string(opcode) + " group " + std::to_string(group) + " entry " + std::to_string(entry);
                    if (by) CheckBy(words, format, entry, elements, dmask, mip, packedBy, name);
                    else CheckPacked(words, format, entry, dmask, mip, (opcode & 1u) != 0u, name);
                }
                if (artifact != 0u) Require(result.cacheHit && result.artifact == artifact, "mixed-group descriptor change rebuilt shader");
                artifact = result.artifact;
            }
            Require(Texels == initial, "mixed-group load changed a source image");
        }
    }
    Keys[1] = 1u;
    std::cout << "table packed mixed-format groups and reselection passed\n";
}

void IsolatedFaults(AgcDriver::VulkanDevice& device) {
    Table.fill(0u);
    constexpr Format format{5u, 1u, 1u};
    FillTexture(0u, format);
    FillTexture(1u, format);
    FillInput(false);
    const auto initial = Texels;
    SetEntry(2u, {});
    SetEntry(3u, Descriptor(1u, Format{2u, 1u, 1u}));
    const auto code = GroupCode(2u, 15u);
    for (const auto other : {2u, 3u}) {
        Keys[1] = other;
        const auto result = Run(device, code, 0u, 2u);
        CheckPacked(Group(result, 0u), format, 0u, 15u, false, false, "valid group next to null/poison group");
        for (std::uint32_t word = Threads * 4u; word < Threads * 8u; ++word) {
            Require(result.words[word] == 0u, "null/poison group did not return all four zero words");
        }
        const bool fault = result.log.find("image table:") != std::string::npos;
        Require(fault == (other == 3u), "mixed-group null/poison fault isolation");
        Require(Texels == initial, "mixed-group rejected load wrote an image");
    }
    for (const auto opcode : {2u, 3u, 4u, 5u, 0x42u, 0x43u, 0x4au, 0x4bu, 0x70u, 0x71u, 0x73u, 0x74u}) {
        const bool by = opcode >= 0x42u && opcode < 0x70u;
        const auto dmask = by ? (opcode & 1u) != 0u ? 15u : 3u : 1u;
        auto oneLane = Code(opcode, dmask, true, true);
        const auto compare = std::find(oneLane.begin(), oneLane.end(), 0x7da400ffu);
        Require(compare != oneLane.end(), "partial EXEC program lost its comparison");
        *(compare + 1) = 16u;
        const auto selected = Run(device, oneLane, 0u);
        Require(selected.log.find("image table:") == std::string::npos, "partial EXEC packed table access faulted on unselected poison");
        const auto control = Run(device, Code(opcode, dmask, false), 0u);
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto index = lane * 4u + component;
                const auto expected = lane == 16u ? control.words[index] : Unwritten;
                Require(selected.words[index] == expected, "partial EXEC packed table load wrote an inactive lane or lost the active lane");
            }
        }
        Require(Texels == initial, "partial EXEC load changed an image");
    }
    Keys[1] = 1u;
    std::cout << "table packed mixed-group faults and partial EXEC passed\n";
}

void FixedMipStores(AgcDriver::VulkanDevice& device) {
    Table.fill(0u);
    constexpr std::array formats{
        Format{5u, 1u, 1u}, Format{18u, 2u, 2u}, Format{21u, 4u, 1u},
        Format{63u, 8u, 2u}, Format{76u, 16u, 4u}, Format{77u, 16u, 4u, true}};
    for (const auto dmask : {1u, 5u, 10u, 15u}) {
        const auto directCode = Code(10u, dmask, false);
        const auto tableCode = Code(10u, dmask, true);
        const auto readCode = Code(2u, 15u, true);
        std::uint64_t artifact = 0;
        for (const auto& format : formats) {
            for (const auto selected : {0u, 1u}) {
                for (const auto swizzle : {Identity, 0x977u, 0u}) {
                    FillTexture(selected, format);
                    FillTexture(1u - selected, Format{20u, 4u, 1u});
                    auto descriptor = Descriptor(selected, format, swizzle);
                    descriptor[3] |= 1u << 12u;
                    SetEntry(selected, descriptor);
                    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                        Input[lane * 4u] = lane < 30u ? lane : lane == 30u ? Width : 0xffffffffu;
                        Input[lane * 4u + 1u] = 0u;
                        Input[lane * 4u + 2u] = 0u;
                        Input[lane * 4u + 3u] = 0u;
                    }
                    const auto initial = Texels;
                    const auto name = "fixed mip table store format " + std::to_string(format.id) + " entry " + std::to_string(selected) + " mask " + std::to_string(dmask) + " swizzle " + std::to_string(swizzle);
                    Run(device, directCode, selected);
                    CheckStored(format, selected, dmask, initial[selected], name + " direct");
                    const auto direct = Texels[selected];
                    Texels = initial;
                    const auto table = Run(device, tableCode, selected);
                    Require(table.tables != 0u && table.log.find("image table:") == std::string::npos, name + ": fixed nonzero descriptor mip was rejected");
                    CheckStored(format, selected, dmask, initial[selected], name);
                    Require(Texels[selected] == direct && Texels[1u - selected] == initial[1u - selected], name + ": fixed mip store modified another entry or mip");
                    descriptor[3] = (descriptor[3] & ~0xfffu) | Identity;
                    SetEntry(selected, descriptor);
                    const auto read = Run(device, readCode, selected);
                    CheckPacked(read, format, selected, 15u, false, false, name + " readback", 1u);
                    if (artifact != 0u) Require(table.cacheHit && table.artifact == artifact, name + ": base mip or swizzle rebuilt shader");
                    artifact = table.artifact;
                }
            }
        }
    }
    std::cout << "table packed fixed nonzero storage mip and ignored store swizzles passed\n";
}

void EmptyTables(AgcDriver::VulkanDevice& device) {
    Table.fill(0u);
    FillInput(false);
    const auto initial = Texels;
    for (const auto opcode : {2u, 3u, 4u, 5u, 0x42u, 0x43u, 0x4au, 0x4bu, 0x70u, 0x71u, 0x73u, 0x74u, 10u}) {
        const bool by = opcode >= 0x42u && opcode < 0x70u;
        const auto dmask = by ? (opcode & 1u) != 0u ? 15u : 3u : 1u;
        const auto code = Code(opcode, dmask, true);
        std::uint64_t artifact = 0;
        for (const auto selected : {0u, 1u, 3u}) {
            const auto result = Run(device, code, selected);
            Require(result.log.find("image table:") == std::string::npos, "all-null packed image table faulted");
            if (opcode != 10u) {
                for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                    for (std::uint32_t component = 0; component < 4u; ++component) {
                        const auto expected = component < static_cast<std::uint32_t>(std::popcount(dmask)) ? 0u : Sentinel + component;
                        Require(result.words[lane * 4u + component] == expected, "all-null table load returned data or damaged sentinel registers");
                    }
                }
            }
            Require(Texels == initial, "all-null table store changed an image");
            if (artifact != 0u) Require(result.cacheHit && result.artifact == artifact, "all-null table reselection rebuilt shader");
            artifact = result.artifact;
        }
    }
    FillTexture(0u, Format{5u, 1u, 1u});
    const auto code = Code(2u, 15u, true);
    const auto valid = Run(device, code, 0u);
    CheckPacked(valid, Format{5u, 1u, 1u}, 0u, 15u, false, false, "table populated after an all-null snapshot");
    SetEntry(0u, {});
    const auto null = Run(device, code, 0u);
    Require(null.cacheHit && valid.artifact == null.artifact, "removing the selected image rebuilt shader");
    Require(null.log.find("image table:") == std::string::npos, "removing the selected image faulted");
    for (std::uint32_t word = 0; word < Threads * 4u; ++word) Require(null.words[word] == 0u, "removed table entry retained stale image bits");
    std::cout << "table packed all-null snapshots and descriptor removal passed\n";
}

void UnsupportedStores(AgcDriver::VulkanDevice& device) {
    Table.fill(0u);
    FillTexture(0u, Format{5u, 1u, 1u});
    FillTexture(1u, Format{11u, 2u, 1u});
    FillInput(false);
    const auto initial = Texels;
    for (const auto opcode : {0x52u, 0x53u, 0x5au, 0x5bu, 0x76u, 0x77u, 0x79u, 0x7au}) {
        for (const auto table : {false, true}) {
            const auto dmask = opcode >= 0x76u ? 1u : (opcode & 1u) != 0u ? 15u : 3u;
            const auto code = Code(opcode, dmask, table);
            std::string refusal;
            try {
                Run(device, code, 0u, 1u, false);
            } catch (const std::exception& error) {
                refusal = error.what();
            }
            Require(refusal.find("stores are not implemented") != std::string::npos, "unsupported BY/PCK store opcode " + std::to_string(opcode) + " was not rejected before GPU submission: " + refusal);
            Require(Texels == initial, "unsupported BY/PCK store compilation modified an image");
        }
    }
    std::cout << "BY stores and unmeasured PCK stores rejected without GPU submission\n";
}

struct Rejection {
    std::uint32_t opcode;
    std::uint32_t dmask;
    Format format;
    std::uint32_t swizzle;
};

void RejectedEntries(AgcDriver::VulkanDevice& device) {
    constexpr std::array cases{
        Rejection{2u, 1u, {2u, 1u, 1u}, Identity},
        Rejection{2u, 1u, {8u, 2u, 1u}, Identity},
        Rejection{2u, 1u, {13u, 2u, 1u}, Identity},
        Rejection{2u, 1u, {57u, 4u, 4u}, Identity},
        Rejection{2u, 1u, {54u, 4u, 4u}, Identity},
        Rejection{2u, 1u, {5u, 1u, 1u}, 0xf2eu},
        Rejection{0x42u, 3u, {20u, 4u, 1u}, Identity},
        Rejection{0x43u, 15u, {11u, 2u, 1u}, Identity},
        Rejection{0x42u, 3u, {5u, 1u, 1u}, 0xf2eu},
        Rejection{0x70u, 1u, {2u, 1u, 1u}, Identity},
        Rejection{0x70u, 1u, {13u, 2u, 1u}, Identity},
        Rejection{0x70u, 1u, {20u, 4u, 1u}, Identity},
        Rejection{0x71u, 1u, {11u, 2u, 1u}, Identity},
        Rejection{0x74u, 1u, {18u, 2u, 2u}, Identity},
        Rejection{0x70u, 1u, {5u, 1u, 1u}, 0xf2eu},
        Rejection{10u, 1u, {1u, 1u, 1u}, Identity},
        Rejection{10u, 1u, {6u, 1u, 1u}, Identity},
        Rejection{10u, 1u, {12u, 2u, 1u}, Identity},
        Rejection{10u, 1u, {13u, 2u, 1u}, Identity},
        Rejection{10u, 1u, {56u, 4u, 4u}, Identity},
        Rejection{10u, 1u, {72u, 12u, 3u}, Identity},
        Rejection{10u, 1u, {54u, 4u, 4u}, Identity}};
    for (const auto& rejected : cases) {
        Table.fill(0u);
        FillTexture(0u, Format{5u, 1u, 1u});
        FillTexture(1u, Format{5u, 1u, 1u});
        FillInput(false);
        SetEntry(1u, Descriptor(1u, rejected.format, rejected.swizzle));
        const auto initial = Texels;
        const auto directCode = Code(rejected.opcode, rejected.dmask, false);
        const auto tableCode = Code(rejected.opcode, rejected.dmask, true);
        const auto inactiveCode = Code(rejected.opcode, rejected.dmask, true, true);
        const auto name = "rejected table opcode " + std::to_string(rejected.opcode) + " format " + std::to_string(rejected.format.id);
        std::string refusal;
        try {
            Run(device, directCode, 1u);
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        Require(!refusal.empty(), name + ": direct descriptor unexpectedly accepted");
        const auto inactive = Run(device, inactiveCode, 1u);
        Require(inactive.log.find("image table:") == std::string::npos, name + ": inactive invalid descriptor faulted");
        Require(std::all_of(inactive.words.begin(), inactive.words.end(), [](auto word) { return word == Unwritten; }), name + ": inactive invalid descriptor wrote output");
        const auto active = Run(device, tableCode, 1u);
        Require(active.log.find("image table:") != std::string::npos, name + ": active invalid descriptor failed to report poison");
        Require(active.faults != 0u, name + ": invalid descriptor absent from snapshot poison");
        if (rejected.opcode != 10u) {
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                for (std::uint32_t component = 0; component < 4u; ++component) {
                    const auto expected = component < static_cast<std::uint32_t>(std::popcount(rejected.dmask)) ? 0u : Sentinel + component;
                    Require(active.words[lane * 4u + component] == expected, name + ": invalid descriptor returned nonzero bits or changed destination order");
                }
            }
        }
        Require(Texels == initial, name + ": invalid descriptor modified texture bytes");
    }
    Table.fill(0u);
    Keys[1] = 1u;
    std::cout << "table packed measured format and swizzle rejections passed\n";
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestAllocations::Mutation().Add(Texels.data(), sizeof(Texels), true, true, true);
        GuestAllocations::Mutation().Add(Table.data(), sizeof(Table), true, true, true);
        GuestAllocations::Mutation().Add(Output.data(), sizeof(Output), true, true, true);
        const std::string mode = argc > 1 ? argv[1] : "loads";
        Require(mode == "all" || mode == "loads" || mode == "load_sgn" || mode == "load_mip" || mode == "load_mip_sgn" || mode == "by" || mode == "stores" || mode == "guards" || mode == "mixed" || mode == "isolated" || mode == "rejected" || mode == "mips" || mode == "empty" || mode == "unmeasured", "unknown image table packed test mode");
        if (mode == "all" || mode == "loads") PackedLoads(*device, 2u);
        if (mode == "all" || mode == "load_sgn") PackedLoads(*device, 3u);
        if (mode == "all" || mode == "load_mip") PackedLoads(*device, 4u);
        if (mode == "all" || mode == "load_mip_sgn") PackedLoads(*device, 5u);
        if (mode == "all" || mode == "by") ByLoads(*device);
        if (mode == "all" || mode == "stores") PackedStores(*device);
        if (mode == "all" || mode == "guards") Guards(*device);
        if (mode == "all" || mode == "mixed") MixedGroups(*device);
        if (mode == "all" || mode == "isolated") IsolatedFaults(*device);
        if (mode == "all" || mode == "rejected") RejectedEntries(*device);
        if (mode == "all" || mode == "mips") FixedMipStores(*device);
        if (mode == "all" || mode == "empty") EmptyTables(*device);
        if (mode == "all" || mode == "unmeasured") UnsupportedStores(*device);
        GuestAllocations::Mutation().Remove(Table.data());
        GuestAllocations::Mutation().Remove(Output.data());
        GuestAllocations::Mutation().Remove(Texels.data());
        std::cout << "image table packed execution tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
