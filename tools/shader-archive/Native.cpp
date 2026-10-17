#include "Native.hpp"
#include <numeric>
#include <unordered_map>

namespace ShaderArchive {
namespace {
std::size_t Relative(View data, std::size_t field, std::size_t size) {
    const auto relative = U64(data, field);
    Check(relative > 0 && relative <= data.size() && field <= data.size() - relative, "invalid AGC relative pointer");
    const auto offset = field + relative;
    Slice(data, offset, size);
    return offset;
}
View Array(View h, std::size_t field, std::size_t count, std::size_t width) {
    if (!count)
        return {};
    Check(width && count <= h.size() / width, "AGC array size overflow");
    return Slice(h, Relative(h, field, count * width), count * width);
}
std::vector<unsigned> Words(View bytes) {
    Check(bytes.size() % 4 == 0, "unaligned AGC words");
    std::vector<unsigned> words(bytes.size() / 4);
    for (std::size_t i = 0; i < words.size(); ++i)
        words[i] = U32(bytes, i * 4);
    return words;
}
void Put64(Bytes &b, std::size_t at, std::uint64_t value) {
    Slice(b, at, 8);
    for (unsigned i = 0; i < 8; ++i)
        b[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
unsigned Required(const std::map<unsigned, unsigned> &r, unsigned index) {
    auto it = r.find(index);
    Check(it != r.end(), "missing shader register " + std::to_string(index));
    return it->second;
}
} // namespace
std::vector<Native> ParseNative(View blob) {
    std::vector<Native> result;
    static constexpr std::array<std::uint8_t, 4> magic{'1', '2', '3', '4'};
    static constexpr std::array<std::uint8_t, 8> trailer{'b', 'a', 'r', 'e', 'f', 'o', 'o', 't'};
    for (std::size_t at = 0; at < blob.size();) {
        auto begin = std::search(blob.begin() + at, blob.end(), magic.begin(), magic.end());
        if (begin == blob.end())
            break;
        const auto start = std::size_t(begin - blob.begin());
        Check(start >= 20 && U32(blob, start + 4) == 24, "unsupported AGC header");
        Native n;
        n.headerOffset = start;
        n.headerSize = U32(blob, start + 64);
        n.codeSize = U32(blob, start + 68);
        n.codeOffset = start + n.headerSize;
        Check(n.headerSize >= 96 && n.codeSize > 0 && n.codeSize % 4 == 0 && U64(blob, start - 20) == n.headerSize &&
                  U64(blob, start - 12) == n.codeSize,
              "AGC/envelope sizes disagree");
        const auto h = Slice(blob, start, n.headerSize), code = Slice(blob, n.codeOffset, n.codeSize);
        unsigned trailers = 0;
        for (auto it = code.begin(); it != code.end();) {
            it = std::search(it, code.end(), trailer.begin(), trailer.end());
            if (it == code.end())
                break;
            const auto p = std::size_t(it - code.begin());
            if (p + 24 <= code.size()) {
                const auto size = U32(code, p + 20);
                if (size > 0 && size % 4 == 0 && size <= p) {
                    ++trailers;
                    n.executableSize = size;
                }
            }
            ++it;
        }
        Check(trailers == 1, "expected exactly one executable trailer");
        n.type = h[90];
        Check(n.type <= 8, "unknown AGC shader type");
        n.target = U32(h, 76);
        n.scratch = U16(h, 84);
        n.inputs = Words(Array(h, 48, U32(h, 80), 4));
        n.outputs = Words(Array(h, 56, U16(h, 86), 4));
        n.specials = Words(Array(h, 40, U16(h, 88), 1));
        auto registers = [&](auto field, auto count, auto &out) {
            auto data = Array(h, field, count, 8);
            for (std::size_t i = 0; i < data.size(); i += 8)
                out[U32(data, i)] = U32(data, i + 4);
        };
        registers(32, h[92], n.sh);
        registers(24, h[91], n.cx);
        if (U64(h, 8)) {
            const auto ud = Relative(h, 8, 54);
            for (unsigned i = 0; i < 5; ++i) {
                auto values = Array(h, ud + i * 8, U16(h, ud + 44 + i * 2), 2);
                if (!i)
                    for (std::size_t p = 0; p < values.size(); p += 2)
                        n.direct.push_back(U16(values, p));
            }
        }
        n.codeHash = Sha256(code);
        n.headerHash = Sha256(h);
        result.push_back(std::move(n));
        at = start + result.back().headerSize + result.back().codeSize;
    }
    Check(!result.empty(), "no native program");
    Check(blob.size() < 4 || std::memcmp(blob.data(), "AHSP", 4) != 0 || result.size() == 2, "AHSP needs two variants");
    return result;
}
std::vector<Program> Programs(const Archive &archive, unsigned workers) {
    std::vector<std::vector<Native>> parsed(archive.entries.size());
    Parallel(parsed.size(), workers, [&](std::size_t i, unsigned) {
        const auto &e = archive.entries[i];
        parsed[i] = ParseNative(Slice(*archive.groups[e.group].bytes, e.offset, e.size));
    });
    std::vector<Program> out;
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < parsed.size(); ++i)
        for (std::size_t variant = 0; variant < parsed[i].size(); ++variant) {
            auto &n = parsed[i][variant];
            const auto &e = archive.entries[i];
            auto key = n.codeHash + n.headerHash;
            auto [it, added] = index.emplace(key, out.size());
            if (added) {
                Program p;
                p.id = Sha256({reinterpret_cast<const std::uint8_t *>(key.data()), key.size()});
                p.group = archive.groups[e.group].bytes;
                p.offset = e.offset;
                p.size = e.size;
                p.native = std::move(n);
                out.push_back(std::move(p));
            }
            Json::Value a;
            a["library"] = e.library;
            a["shader_index"] = Json::UInt64(e.shaderIndex);
            a["ue_shader_hash"] = e.ueHash;
            a["variant"] = Json::UInt64(variant);
            a["group_file"] = "groups/" + archive.groups[e.group].id + ".bin";
            a["offset"] = Json::UInt64(e.offset);
            a["size"] = Json::UInt64(e.size);
            out[it->second].aliases.append(a);
        }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.id < b.id; });
    return out;
}
void WriteProgramIndex(const fs::path &path, const std::vector<Program> &programs) {
    Bytes bytes(32 + programs.size() * 128);
    std::memcpy(bytes.data(), "APS5IDX1", 8);
    Put64(bytes, 8, programs.size());
    std::uint64_t occurrences = 0;
    auto putHex = [&](std::size_t at, const std::string &text) {
        Check(text.size() % 2 == 0, "index hex width");
        auto digit = [](char c) -> unsigned {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            throw std::runtime_error("invalid index hex");
        };
        for (std::size_t i = 0; i < text.size() / 2; ++i)
            bytes.at(at + i) = static_cast<std::uint8_t>((digit(text[2 * i]) << 4) | digit(text[2 * i + 1]));
    };
    for (std::size_t i = 0; i < programs.size(); ++i) {
        const auto &p = programs[i];
        const auto at = 32 + i * 128;
        occurrences += p.aliases.size();
        putHex(at, p.id);
        putHex(at + 32, p.native.codeHash);
        putHex(at + 64, p.native.headerHash);
        const auto group = fs::path(p.aliases[0]["group_file"].asString()).stem().string();
        Check(group.size() == 24, "index group identity");
        putHex(at + 96, group);
        Put64(bytes, at + 108, p.offset);
        Put64(bytes, at + 116, p.size);
        for (unsigned j = 0; j < 4; ++j)
            bytes[at + 124 + j] = static_cast<std::uint8_t>(p.native.target >> (8 * j));
    }
    Put64(bytes, 16, occurrences);
    WriteFile(path, bytes);
}
std::vector<Program> LoadProgramIndex(const fs::path &index, const fs::path &extracted,
                                      const std::set<std::string> &selection, unsigned target, unsigned sample,
                                      unsigned limit, unsigned workers, Json::Value &report) {
    MappedFile mapping(index);
    const auto bytes = mapping.Data();
    Check(bytes.size() >= 32 && std::memcmp(bytes.data(), "APS5IDX1", 8) == 0, "invalid program index");
    const auto count = U64(bytes, 8);
    Check(count <= (bytes.size() - 32) / 128 && bytes.size() == 32 + count * 128, "truncated program index");
    std::vector<std::size_t> selected;
    std::set<std::string> matched;
    std::set<std::string> codeSelectors;
    for (const auto &id : selection) {
        std::array<std::uint8_t, 32> key{};
        Check(id.size() == 64, "selection must contain full 64-digit identities");
        auto digit = [](char c) -> unsigned {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            throw std::runtime_error("invalid selection hex");
        };
        for (unsigned i = 0; i < 32; ++i)
            key[i] = static_cast<std::uint8_t>((digit(id[2 * i]) << 4) | digit(id[2 * i + 1]));
        std::size_t first = 0, last = count;
        while (first < last) {
            const auto middle = first + (last - first) / 2;
            if (std::memcmp(bytes.data() + 32 + middle * 128, key.data(), 32) < 0)
                first = middle + 1;
            else
                last = middle;
        }
        if (first < count && std::memcmp(bytes.data() + 32 + first * 128, key.data(), 32) == 0 &&
            (!target || U32(bytes, 32 + first * 128 + 124) == target)) {
            selected.push_back(first);
            matched.insert(id);
        } else
            codeSelectors.insert(id);
    }
    if (selection.empty() || !codeSelectors.empty())
        for (std::size_t i = 0; i < count; ++i) {
            const auto at = 32 + i * 128;
            if (target && U32(bytes, at + 124) != target)
                continue;
            if (!selection.empty()) {
                const auto code = Hex(Slice(bytes, at + 32, 32));
                if (!codeSelectors.contains(code))
                    continue;
                matched.insert(code);
            }
            selected.push_back(i);
        }
    std::sort(selected.begin(), selected.end());
    selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
    Check(matched == selection, "selection contains unknown/excluded identities");
    report["planned_jobs"] = Json::UInt64(count);
    report["native_occurrences"] = Json::UInt64(U64(bytes, 16));
    report["eligible_jobs"] = Json::UInt64(selected.size());
    if (sample && sample < selected.size()) {
        std::vector<std::size_t> subset;
        for (std::size_t i = 0; i < sample; ++i)
            subset.push_back(selected[i * selected.size() / sample]);
        selected = std::move(subset);
    }
    if (limit && selected.size() > limit)
        selected.resize(limit);
    std::map<std::string, std::size_t> groups;
    std::vector<std::string> groupIds;
    std::vector<std::size_t> groupForJob;
    for (auto i : selected) {
        auto id = Hex(Slice(bytes, 32 + i * 128 + 96, 12));
        auto [it, added] = groups.emplace(id, groups.size());
        if (added)
            groupIds.push_back(id);
        groupForJob.push_back(it->second);
    }
    std::vector<std::shared_ptr<Bytes>> data(groups.size());
    Parallel(data.size(), workers, [&](auto i, unsigned) {
        data[i] = std::make_shared<Bytes>(ReadFile(extracted / "groups" / (groupIds[i] + ".bin")));
    });
    std::vector<Program> out(selected.size());
    Parallel(selected.size(), workers, [&](auto i, unsigned) {
        const auto at = 32 + selected[i] * 128;
        auto &p = out[i];
        p.id = Hex(Slice(bytes, at, 32));
        p.group = data[groupForJob[i]];
        p.offset = U64(bytes, at + 108);
        p.size = U64(bytes, at + 116);
        const auto code = Hex(Slice(bytes, at + 32, 32)), header = Hex(Slice(bytes, at + 64, 32));
        auto native = ParseNative(Slice(*p.group, p.offset, p.size));
        auto found = std::find_if(native.begin(), native.end(),
                                  [&](const auto &n) { return n.codeHash == code && n.headerHash == header; });
        Check(found != native.end(), "program index does not match extracted bytes");
        const auto identity = code + header;
        Check(p.id == Sha256({reinterpret_cast<const std::uint8_t *>(identity.data()), identity.size()}),
              "program index identity mismatch");
        Check(found->target == U32(bytes, at + 124), "program index target mismatch");
        p.native = std::move(*found);
        Json::Value alias;
        alias["group_file"] = "groups/" + groupIds[groupForJob[i]] + ".bin";
        alias["offset"] = Json::UInt64(p.offset);
        alias["size"] = Json::UInt64(p.size);
        p.aliases.append(alias);
    });
    report["groups_read"] = Json::UInt64(data.size());
    report["index"] = index.string();
    return out;
}
std::unique_ptr<RequestData> MakeRequest(const Program &p, ShaderRecompiler::SpirvTarget target, unsigned subgroup32,
                                         unsigned subgroup64) {
    using namespace ShaderRecompiler;
    auto data = std::make_unique<RequestData>();
    auto &r = data->request;
    const auto &n = p.native;
    auto blob = Slice(*p.group, p.offset, p.size), raw = Slice(blob, n.headerOffset, n.headerSize);
    auto code = Slice(blob, n.codeOffset, n.codeSize);
    data->code = Words(code);
    data->header.assign(raw.begin(), raw.end());
    constexpr std::uint64_t codeAddress = 0x100000000ull, headerAddress = 0x200000000ull;
    std::vector<std::size_t> fields{8, 24, 32, 40, 48, 56};
    if (U64(raw, 8)) {
        const auto ud = Relative(raw, 8, 54);
        for (unsigned i = 0; i < 5; ++i)
            fields.push_back(ud + i * 8);
    }
    for (auto field : fields)
        if (U64(raw, field))
            Put64(data->header, field, headerAddress + Relative(raw, field, 0));
    Put64(data->header, 16, codeAddress);
    auto cx = n.cx;
    for (auto key : {0x1b3u, 0x1b4u, 0x1b6u, 0x1c5u, 0x203u, 0x207u, 0x2d5u})
        cx.try_emplace(key, 0);
    if (n.specials.size() >= 4)
        cx[n.specials[2]] = n.specials[3];
    unsigned rsrc = 0, overflow = 0, first = 0;
    if (n.type == 0) {
        r.shader.stage = ShaderStage::Compute;
        rsrc = 0x212;
        overflow = 26;
    } else if (n.type == 1) {
        r.shader.stage = ShaderStage::Fragment;
        rsrc = 0x00a;
        overflow = 29;
    } else if (n.type == 2) {
        r.shader.stage = ShaderStage::Vertex;
        rsrc = 0x08a;
        overflow = 31;
        first = 8;
    } else
        throw std::runtime_error("needs_configuration: linked geometry/tessellation stages required");
    const auto resources = Required(n.sh, rsrc + 1);
    auto users = (resources >> 1) & 31;
    if (n.type != 0)
        users |= ((resources >> 27) & 1) << 5;
    Check(users <= 32, "user SGPR bank exceeded");
    r.context.waveSize = 64;
    if (auto it = n.sh.find(rsrc); it != n.sh.end()) {
        const auto v = it->second;
        r.context.floatMode =
            ShaderFloatMode{(v >> 12) & 255, bool(v & (1u << 21)), bool(v & (1u << 23)), bool(v & (1u << overflow))};
    }
    if (n.type == 0) {
        Check(n.specials.size() > 4, "missing dispatch modifier");
        const auto dispatch = n.specials[4];
        r.context.waveSize = (dispatch & 0x8000) ? 32 : 64;
        ShaderComputeStageInfo c{};
        for (unsigned i = 0; i < 3; ++i) {
            c.numThreads[i] = Required(n.sh, 0x207 + i);
            Check(c.numThreads[i] > 0, "zero thread dimension");
            c.groupIdEnable[i] = resources & (1u << (i + 7));
        }
        c.ldsSizeDwords = std::min(((resources >> 15) & 511) * 128, 16384u);
        c.tgSizeEnable = resources & (1u << 10);
        c.threadIdComponentCount = ((resources >> 11) & 3) + 1;
        c.scratchDwords = (resources & 1) ? n.scratch : 0;
        Check(!(resources & 1) || c.scratchDwords, "scratch enabled without allocation");
        r.context.compute = c;
    } else if (n.type == 1) {
        ShaderPixelStageInfo pixel{};
        const auto control = cx[0x1b6], addr = cx[0x1b4], active = addr & cx[0x1b3], z = cx[0x203];
        pixel.interpolatorCount = control & 63;
        Check(pixel.interpolatorCount <= 32, "pixel interpolator count");
        Check((active & ((1u << 3) | (1u << 14) | 0xffff0000u)) == 0, "unsupported pixel input mask");
        r.context.waveSize = (control & 0x8000) ? 32 : 64;
        pixel.wave32 = r.context.waveSize == 32;
        pixel.inputAddr = addr;
        std::iota(pixel.interpolatorSettings.begin(), pixel.interpolatorSettings.end(), 0);
        for (unsigned i = 0; i < pixel.interpolatorCount; ++i)
            if (cx.contains(0x191 + i))
                pixel.interpolatorSettings[i] = cx[0x191 + i];
        auto loaded = [&](unsigned bit) { return bool(active & (1u << bit)); };
        pixel.hasPerspectiveCenterVgpr = loaded(1);
        pixel.perspectiveCentroid = loaded(2);
        pixel.posX = loaded(8);
        pixel.posY = loaded(9);
        pixel.posZ = loaded(10);
        pixel.posW = loaded(11);
        pixel.frontFace = loaded(12);
        pixel.ancillary = loaded(13);
        pixel.sampleShading = loaded(0) && loaded(4);
        pixel.noPerspective = loaded(5);
        pixel.linearCentroid = loaded(6);
        pixel.pixelKillEnable = z & 64;
        pixel.depthExportEnable = z & 1;
        pixel.sampleMaskExportEnable = z & 256;
        pixel.earlyZ =
            ((z >> 4) & 3) == 1 && !pixel.pixelKillEnable && !pixel.depthExportEnable && !pixel.sampleMaskExportEnable;
        pixel.executeOnNoop = z & 1024;
        Check(((z >> 13) & 3) != 3, "reserved conservative depth");
        pixel.conservativeZExport = static_cast<ConservativeZExport>((z >> 13) & 3);
        pixel.orderedPixelShader = z & (1u << 16);
        for (unsigned i = 0; i < 8; ++i) {
            pixel.targetOutputMode[i] = (cx[0x1c5] >> (i * 4)) & 15;
            pixel.targetExportMapping[i] = i;
        }
        r.context.pixel = pixel;
    } else {
        const auto routing = cx[0x2d5];
        Check(!(routing & 4), "needs_configuration: tessellation routing");
        r.context.waveSize = (routing & 0x00400000) ? 32 : 64;
        GraphicsCompileContext graphics{};
        graphics.draw.instanceCount = 1;
        static_cast<void>(cx[0x207]);
        if (routing & 0x20) {
            Check(n.specials.size() > 1, "missing GE_CNTL");
            const auto group = n.specials[1], vertices = (group >> 9) & 511, primitives = group & 511,
                       maxVertices = Required(cx, 0x1ff), vpp = Required(cx, 0x2ce), item = Required(cx, 0x2ab);
            const auto output = n.specials.size() > 9 ? n.specials[9] : cx[0x29b];
            Check(output == 2 && vpp >= 3 && vpp <= 256 && vertices >= 1 && maxVertices >= 1 && maxVertices <= 256,
                  "unsupported mesh assembly");
            const auto groupPrimitives = std::min({primitives, vertices, maxVertices / vpp});
            Check(groupPrimitives > 0 && item > 0 && std::uint64_t(item) * vertices <= 65535, "invalid mesh subgroup");
            Check(((Required(n.sh, 0x8a) >> 29) & 3) == 3 && ((resources >> 16) & 3) == 3,
                  "unsupported mesh VGPR allocation");
            const auto maxPrimitives = primitives * (vpp - 2),
                       threads = std::max({groupPrimitives, primitives, maxVertices, maxPrimitives}),
                       wave = r.context.waveSize;
            graphics.mesh = MeshConfiguration{1,
                                              groupPrimitives,
                                              groupPrimitives,
                                              maxVertices,
                                              maxPrimitives,
                                              ((threads + wave - 1) / wave) * wave,
                                              ((resources >> 19) & 255) * 128,
                                              0,
                                              item};
            r.shader.stage = ShaderStage::Mesh;
            first = 0;
            users += 8;
        }
        ShaderVertexStageInfo vertex{};
        Check(n.direct.size() <= 11, "unknown direct resource domain");
        if (n.direct.size() > 10 && n.direct[10] != 65535) {
            const auto attr = n.direct[10], buffer = n.direct[8];
            Check(buffer != 65535 && attr + 1 < users && buffer + 1 < users, "vertex tables exceed user data");
            Check(!n.inputs.empty() && n.inputs.size() <= 32, "vertex semantic count");
            vertex.fetchAttribReg = attr;
            vertex.fetchBufferReg = buffer;
            vertex.fetchEmbedded = true;
            vertex.resourcesNum = n.inputs.size();
            for (std::size_t i = 0; i < n.inputs.size(); ++i) {
                const auto s = n.inputs[i];
                Check(!(s & (3u << 25)), "needs_configuration: bound vertex descriptor");
                vertex.resourcesDst[i] = {int((s >> 8) & 255), int((s >> 16) & 15), int(s & 255), 0};
            }
        }
        r.context.vertex = vertex;
        r.graphics = graphics;
    }
    if (n.type == 1) {
        r.graphics = GraphicsCompileContext{};
        r.graphics->draw.instanceCount = 1;
    }
    if (r.graphics)
        r.graphics->firstUserSgpr = first;
    data->userData.resize(users);
    r.context.userData = data->userData;
    r.context.userDataBaseRegister = first;
    r.shader.codeAddress = codeAddress;
    r.shader.headerAddress = headerAddress;
    r.shader.code = data->code;
    r.shader.header = {reinterpret_cast<const std::byte *>(data->header.data()), data->header.size()};
    data->memory = {
        MemoryRegion{codeAddress, {reinterpret_cast<const std::byte *>(data->code.data()), data->code.size() * 4}},
        MemoryRegion{headerAddress, r.shader.header}};
    r.context.memory = data->memory;
    target.subgroupSize = r.context.waveSize == 32 ? subgroup32 : subgroup64;
    r.target = target;
    r.layout = {0, 0, 0, r.shader.stage == ShaderStage::Mesh ? 104u : 128u};
    r.useCache = false;
    return data;
}
} // namespace ShaderArchive
