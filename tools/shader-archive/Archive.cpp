#include "Archive.hpp"
#include "Oodle.hpp"
#include <map>
#include <set>
#include <unordered_map>

namespace ShaderArchive {
Bytes Decompress(View input, std::size_t size) {
    return DecompressOodle(input, size);
}
namespace {
struct Chunk {
    std::string id;
    std::uint64_t offset;
    std::size_t size;
};
struct Block {
    std::uint64_t offset;
    std::size_t size, raw;
    unsigned method;
};
struct Toc {
    fs::path path;
    std::shared_ptr<MappedFile> data;
    std::vector<Chunk> chunks;
    std::vector<Block> blocks;
    std::unordered_map<std::string, std::size_t> lookup;
    std::map<std::size_t, std::string> names;
    std::vector<std::string> methods{"None"};
    std::size_t blockSize = 0, blockTable = 0;
    explicit Toc(const fs::path &p) : path(p) {
        auto raw = ReadFile(p);
        View b(raw);
        Check(b.size() >= 144 && std::memcmp(b.data(), "-==--==--==--==-", 16) == 0, "bad IoStore magic");
        Check(b[16] == 8 && U32(b, 20) == 144 && U32(b, 32) == 12, "unsupported IoStore layout");
        Check(U32(b, 52) == 1 && !(b[80] & 2), "partitioned/encrypted IoStore is unsupported");
        auto ucas = p;
        ucas.replace_extension("ucas");
        data = std::make_shared<MappedFile>(ucas);
        const auto count = U32(b, 24), blockCount = U32(b, 28), methodCount = U32(b, 36), methodWidth = U32(b, 40);
        blockSize = U32(b, 44);
        Check(blockSize > 0, "zero IoStore block size");
        Reader r{b, 144};
        auto ids = r.Take(std::size_t(count) * 12), ranges = r.Take(std::size_t(count) * 10);
        for (std::size_t i = 0; i < count; ++i) {
            Chunk c{Hex(Slice(ids, i * 12, 12)), ReadInt(ranges, i * 10, 5, true),
                    static_cast<std::size_t>(ReadInt(ranges, i * 10 + 5, 5, true))};
            Check(c.offset <= std::uint64_t(blockCount) * blockSize &&
                      c.size <= std::uint64_t(blockCount) * blockSize - c.offset,
                  "chunk outside logical container");
            Check(lookup.emplace(c.id, i).second, "duplicate chunk ID");
            chunks.push_back(std::move(c));
        }
        r.Take((std::size_t(U32(b, 84)) + U32(b, 96)) * 4);
        blockTable = r.at;
        for (std::size_t i = 0; i < blockCount; ++i) {
            auto row = r.Take(12);
            Block x{ReadInt(row, 0, 5), static_cast<std::size_t>(ReadInt(row, 5, 3)),
                    static_cast<std::size_t>(ReadInt(row, 8, 3)), row[11]};
            Check(x.raw && x.raw <= blockSize && x.method <= methodCount, "invalid compression block");
            Slice(data->Data(), x.offset, x.size);
            Check(x.method || x.size == x.raw, "stored block length mismatch");
            blocks.push_back(x);
        }
        for (unsigned i = 0; i < methodCount; ++i) {
            auto s = r.Take(methodWidth);
            auto end = std::find(s.begin(), s.end(), 0);
            methods.emplace_back(s.begin(), end);
        }
        if (b[80] & 4) {
            auto n = r.Word();
            r.Take(std::size_t(n) * 2 + std::size_t(blockCount) * 20);
        }
        if (auto dirSize = U32(b, 48)) {
            Check(b[80] & 8, "directory without indexed flag");
            Reader dir{r.Take(dirSize)};
            dir.String();
            dir.Array(16);
            auto files = dir.Array(12);
            const auto n = dir.Word();
            std::vector<std::string> strings;
            Check(n <= dir.data.size() / 4, "invalid string count");
            for (unsigned i = 0; i < n; ++i)
                strings.push_back(dir.String());
            for (std::size_t i = 0; i < files.size(); i += 12) {
                auto name = U32(files, i), chunk = U32(files, i + 8);
                Check(name < strings.size() && chunk < chunks.size(), "invalid directory index");
                names[chunk] = strings[name];
            }
            auto trailing = dir.Take(dir.data.size() - dir.at);
            Check(std::all_of(trailing.begin(), trailing.end(), [](auto v) { return v == 0; }),
                  "directory trailing data");
        }
        r.Take(std::size_t(count) * 24);
        Check(r.at == b.size(), "TOC trailing data");
    }
    Bytes Read(std::size_t index) const {
        const auto &c = chunks.at(index);
        Check(c.size <= 512ull * 1024 * 1024, "shader chunk too large");
        Bytes out(c.size);
        std::size_t written = 0;
        auto pos = c.offset;
        while (written < c.size) {
            const auto &block = blocks.at(pos / blockSize);
            const auto skip = std::size_t(pos % blockSize);
            Check(skip < block.raw, "chunk overlaps block padding");
            const auto n = std::min(block.raw - skip, c.size - written);
            auto source = Slice(data->Data(), block.offset, block.size);
            if (!block.method)
                std::memcpy(out.data() + written, source.data() + skip, n);
            else {
                Check(methods.at(block.method) == "Oodle", "unsupported IoStore compression");
                auto raw = Decompress(source, block.raw);
                std::memcpy(out.data() + written, raw.data() + skip, n);
            }
            pos += n;
            written += n;
        }
        return out;
    }
};
struct Pending {
    std::size_t toc, chunk, raw, compressed;
};
} // namespace
Archive Extract(const fs::path &paks, unsigned workers, const fs::path &output) {
    const auto started = Clock::now();
    Archive archive;
    if (!output.empty()) {
        Check(!fs::exists(output), "output already exists");
        fs::create_directories(output / "groups");
        fs::create_directories(output / "libraries");
    }
    std::vector<fs::path> paths;
    for (const auto &e : fs::directory_iterator(paks))
        if (e.path().extension() == ".utoc")
            paths.push_back(e.path());
    std::sort(paths.begin(), paths.end());
    Check(!paths.empty(), "no IoStore containers");
    std::vector<Toc> tocs;
    for (const auto &p : paths)
        tocs.emplace_back(p);
    const auto libraryStart = Clock::now();
    archive.report["toc_seconds"] = Seconds(started);
    std::unordered_map<std::string, std::size_t> groupLookup;
    std::vector<Pending> pending;
    std::set<std::string> shaderHashes;
    Json::Value containers(Json::arrayValue), libraries(Json::arrayValue);
    std::size_t groupReferences = 0;
    for (std::size_t t = 0; t < tocs.size(); ++t) {
        const auto &toc = tocs[t];
        Json::Value container;
        container["path"] = toc.path.string();
        container["chunks"] = Json::UInt64(toc.chunks.size());
        container["blocks"] = Json::UInt64(toc.blocks.size());
        container["block_table_offset"] = Json::UInt64(toc.blockTable);
        containers.append(container);
        std::set<std::string> used;
        for (std::size_t k = 0; k < toc.chunks.size(); ++k) {
            if (!toc.chunks[k].id.ends_with("08"))
                continue;
            const auto name = toc.names.contains(k) ? toc.names.at(k) : toc.chunks[k].id + ".ushaderbytecode";
            const auto bytes = toc.Read(k);
            Reader r{bytes};
            Check(r.Word() == 1, "unsupported shader archive version");
            auto mapHashes = r.Array(20), hashes = r.Array(20), ids = r.Array(12), maps = r.Array(8),
                 entries = r.Array(8), groups = r.Array(16), indices = r.Array(4);
            Check(r.at == bytes.size() && maps.size() / 8 == mapHashes.size() / 20 &&
                      entries.size() / 8 == hashes.size() / 20 && groups.size() / 16 == ids.size() / 12,
                  "library table mismatch");
            const auto shaderCount = entries.size() / 8;
            std::vector<bool> seen(shaderCount);
            for (std::size_t i = 0; i < indices.size(); i += 4)
                Check(U32(indices, i) < shaderCount, "invalid shader index");
            for (std::size_t i = 0; i < maps.size(); i += 8)
                Slice(indices, std::size_t(U32(maps, i)) * 4, std::size_t(U32(maps, i + 4)) * 4);
            for (std::size_t g = 0; g < groups.size() / 16; ++g) {
                const auto id = Hex(Slice(ids, g * 12, 12));
                const auto raw = U32(groups, g * 16 + 8), compressed = U32(groups, g * 16 + 12);
                Check(id.ends_with("09") && raw > 0 && raw <= 512ull * 1024 * 1024 && compressed > 0 &&
                          compressed <= raw,
                      "invalid shader group");
                auto it = toc.lookup.find(id);
                Check(it != toc.lookup.end() && toc.chunks[it->second].size == compressed,
                      "missing or inconsistent shader group");
                auto [where, inserted] = groupLookup.emplace(id, pending.size());
                const auto group = where->second;
                if (inserted) {
                    pending.push_back({t, it->second, raw, compressed});
                    archive.groups.push_back({id, {}});
                } else {
                    const auto &prev = pending[group];
                    Check(prev.raw == raw && prev.compressed == compressed &&
                              tocs[prev.toc].Read(prev.chunk) == toc.Read(it->second),
                          "conflicting group identity");
                }
                used.insert(id);
                ++groupReferences;
                auto members =
                    Slice(indices, std::size_t(U32(groups, g * 16)) * 4, std::size_t(U32(groups, g * 16 + 4)) * 4);
                Check(!members.empty(), "empty group");
                std::vector<std::size_t> sorted;
                for (std::size_t i = 0; i < members.size(); i += 4)
                    sorted.push_back(U32(members, i));
                std::sort(sorted.begin(), sorted.end(),
                          [&](auto a, auto b) { return (U64(entries, a * 8) >> 34) < (U64(entries, b * 8) >> 34); });
                for (std::size_t j = 0; j < sorted.size(); ++j) {
                    const auto s = sorted[j];
                    const auto packed = U64(entries, s * 8);
                    const auto offset = std::size_t(packed >> 34),
                               end = j + 1 == sorted.size() ? raw : std::size_t(U64(entries, sorted[j + 1] * 8) >> 34);
                    Check(!seen[s] && ((packed >> 4) & 0x3fffffff) == g && offset < end && end <= raw &&
                              (j || offset == 0),
                          "inconsistent shader group membership");
                    seen[s] = true;
                    auto hash = Hex(Slice(hashes, s * 20, 20));
                    shaderHashes.insert(hash);
                    archive.entries.push_back({name, toc.path.filename().string(), std::move(hash), s, group, offset,
                                               end - offset, unsigned(packed & 15)});
                }
            }
            Check(std::all_of(seen.begin(), seen.end(), [](bool v) { return v; }), "unaccounted shaders");
            if (!output.empty())
                WriteFile(output / "libraries" / (toc.chunks[k].id + ".bin"), bytes);
            Json::Value library;
            library["name"] = name;
            library["id"] = toc.chunks[k].id;
            library["shaders"] = Json::UInt64(shaderCount);
            library["groups"] = Json::UInt64(groups.size() / 16);
            library["maps"] = Json::UInt64(maps.size() / 8);
            libraries.append(library);
        }
        for (const auto &c : toc.chunks)
            if (c.id.ends_with("09"))
                Check(used.contains(c.id), "unreferenced shader code chunk");
    }
    archive.report["library_seconds"] = Seconds(libraryStart);
    struct WorkerTimes {
        double read = 0, decompress = 0, write = 0;
    };
    std::vector<WorkerTimes> workerTimes(workers);
    const auto groupStart = Clock::now();
    Parallel(pending.size(), workers, [&](std::size_t i, unsigned worker) {
        const auto &p = pending[i];
        const auto readStart = Clock::now();
        auto compressed = tocs[p.toc].Read(p.chunk);
        workerTimes[worker].read += Seconds(readStart);
        const auto decompressStart = Clock::now();
        auto bytes = std::make_shared<Bytes>(Decompress(compressed, p.raw));
        workerTimes[worker].decompress += Seconds(decompressStart);
        const auto writeStart = Clock::now();
        if (!output.empty())
            WriteFile(output / "groups" / (archive.groups[i].id + ".bin"), *bytes);
        workerTimes[worker].write += Seconds(writeStart);
        archive.groups[i].bytes = std::move(bytes);
    });
    std::uint64_t rawBytes = 0, compressedBytes = 0;
    for (const auto &p : pending) {
        rawBytes += p.raw;
        compressedBytes += p.compressed;
    }
    auto &report = archive.report;
    report["group_seconds"] = Seconds(groupStart);
    for (const auto &w : workerTimes) {
        report["worker_read_seconds"] = report["worker_read_seconds"].asDouble() + w.read;
        report["worker_decompress_seconds"] = report["worker_decompress_seconds"].asDouble() + w.decompress;
        report["worker_write_seconds"] = report["worker_write_seconds"].asDouble() + w.write;
    }
    report["schema"] = 1;
    report["containers"] = containers;
    report["libraries"] = libraries;
    report["shader_entries"] = Json::UInt64(archive.entries.size());
    report["unique_shader_hashes"] = Json::UInt64(shaderHashes.size());
    report["unique_groups"] = Json::UInt64(pending.size());
    report["group_references"] = Json::UInt64(groupReferences);
    report["uncompressed_group_bytes"] = Json::UInt64(rawBytes);
    report["compressed_group_bytes"] = Json::UInt64(compressedBytes);
    const auto manifestStart = Clock::now();
    if (!output.empty()) {
        std::vector<std::string> lines(archive.entries.size());
        Parallel(lines.size(), workers, [&](std::size_t i, unsigned) {
            const auto &e = archive.entries[i];
            Json::Value v;
            v["library"] = e.library;
            v["container"] = e.container;
            v["shader_index"] = Json::UInt64(e.shaderIndex);
            v["ue_shader_hash"] = e.ueHash;
            v["frequency"] = e.frequency;
            v["group_file"] = "groups/" + archive.groups[e.group].id + ".bin";
            v["offset"] = Json::UInt64(e.offset);
            v["size"] = Json::UInt64(e.size);
            v["sha256"] = Sha256(Slice(*archive.groups[e.group].bytes, e.offset, e.size));
            lines[i] = JsonText(v) + "\n";
        });
        std::ofstream manifest(output / "shaders.jsonl.partial", std::ios::binary);
        for (const auto &line : lines)
            manifest << line;
        manifest.close();
        Check(bool(manifest), "write shader manifest");
        fs::rename(output / "shaders.jsonl.partial", output / "shaders.jsonl");
    }
    report["seconds"] = Seconds(started);
    report["manifest_seconds"] = Seconds(manifestStart);
    report["process"] = ProcessUsage();
    if (!output.empty())
        WriteJson(output / "report.json", report);
    return archive;
}
Archive LoadExtracted(const fs::path &root, unsigned workers) {
    Archive archive;
    std::ifstream manifest(root / "shaders.jsonl");
    Check(bool(manifest), "open extracted manifest");
    std::unordered_map<std::string, std::size_t> groups;
    std::vector<fs::path> paths;
    for (std::string line; std::getline(manifest, line);) {
        auto v = ParseJson(line);
        const auto file = v["group_file"].asString();
        fs::path path(file);
        Check(!path.is_absolute() && path.parent_path() == "groups" && path.extension() == ".bin",
              "invalid group path");
        auto [it, added] = groups.emplace(file, groups.size());
        if (added) {
            paths.push_back(root / path);
            archive.groups.push_back({path.stem().string(), {}});
        }
        archive.entries.push_back({v["library"].asString(), v["container"].asString(), v["ue_shader_hash"].asString(),
                                   v["shader_index"].asUInt64(), it->second, v["offset"].asUInt64(),
                                   v["size"].asUInt64(), v["frequency"].asUInt()});
    }
    Check(manifest.eof(), "read extracted manifest");
    Parallel(paths.size(), workers,
             [&](std::size_t i, unsigned) { archive.groups[i].bytes = std::make_shared<Bytes>(ReadFile(paths[i])); });
    for (const auto &e : archive.entries)
        Slice(*archive.groups[e.group].bytes, e.offset, e.size);
    return archive;
}
} // namespace ShaderArchive
