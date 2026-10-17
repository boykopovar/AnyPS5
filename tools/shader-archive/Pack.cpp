#include "Pack.hpp"
#include "ShaderCacheDirectory.hpp"
#include <spirv-tools/libspirv.h>

namespace ShaderArchive {
namespace Cache = ShaderRecompiler::ShaderDiskCache;
namespace {
void Write(std::ofstream& output, std::span<const std::byte> data) {
    output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    Check(bool(output), "write shader pack");
}
}
PackWriter::PackWriter(const fs::path& path, std::size_t maximumEntries) : path(path), maximum(maximumEntries), offset(sizeof(Cache::PackHeader) + maximumEntries * sizeof(Cache::PackRecord)) {
    static std::atomic_uint64_t sequence{0};
    temporary = path;
    temporary += ".tmp." + std::to_string(Clock::now().time_since_epoch().count()) + "." + std::to_string(sequence++);
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
    output.open(temporary, std::ios::binary | std::ios::trunc);
    Check(bool(output), "create shader pack");
    output.seekp(static_cast<std::streamoff>(offset));
    records.reserve(maximum);
}
PackWriter::~PackWriter() {
    output.close();
    if (!temporary.empty()) { std::error_code error; fs::remove(temporary, error); }
}
void PackWriter::Add(std::span<const std::byte> entry) {
    const auto key = Cache::EntryKey(entry);
    Check(!key.empty(), "invalid shader pack entry");
    const auto hash = Cache::PackKeyHash(key);
    std::lock_guard lock(mutex);
    Check(records.size() < maximum, "shader pack capacity");
    Write(output, entry);
    records.push_back({hash, offset, entry.size()});
    offset += entry.size();
}
Json::Value PackWriter::Finish() {
    Check(!records.empty(), "no compatible artifacts to publish; rebuild with the current compiler");
    std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) { return a.hash < b.hash; });
    const auto index = std::as_bytes(std::span(records));
    const Cache::PackHeader header{Cache::PackMagic, Cache::SourceVersion(), offset, records.size(), ShaderRecompiler::HashBytes(index)};
    output.seekp(0);
    Write(output, std::as_bytes(std::span(&header, 1)));
    Write(output, index);
    output.close();
    Check(bool(output), "finish shader pack");
    PublishAtomic(temporary, path);
    temporary.clear();
    Json::Value result;
    result["path"] = path.string();
    result["entries"] = Json::UInt64(records.size());
    result["bytes"] = Json::UInt64(offset);
    result["index_bytes"] = Json::UInt64(index.size());
    result["compiler_source"] = Json::UInt64(Cache::SourceVersion());
    result["seconds"] = Seconds(started);
    return result;
}
Json::Value PackFolder(const fs::path& source, const fs::path& destination, std::size_t limit) {
    Check(fs::is_directory(source), "pack requires an existing --cache directory");
    std::vector<fs::path> paths;
    for (const auto& item : fs::recursive_directory_iterator(source)) {
        const auto extension = item.path().extension();
        if (item.is_regular_file() && (extension == ".bin" || extension == ".artifact")) paths.push_back(item.path());
    }
    std::sort(paths.begin(), paths.end());
    PackWriter writer(destination, limit ? std::min(limit, paths.size()) : paths.size());
    std::size_t rejected = 0, converted = 0, scanned = 0, accepted = 0;
    const std::string receipt = std::string("archive-validated-v1:") + spvSoftwareVersionDetailsString();
    std::uint64_t bytesRead = 0;
    for (const auto& path : paths) {
        if (limit && accepted == limit) break;
        ++scanned;
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        Check(bool(input), "open cache entry " + path.string());
        const auto size = input.tellg();
        if (size < static_cast<std::streamoff>(Cache::EntryHeaderBytes)) { ++rejected; continue; }
        input.seekg(0);
        std::array<std::byte, Cache::EntryHeaderBytes> header{};
        input.read(reinterpret_cast<char*>(header.data()), header.size());
        bytesRead += header.size();
        if (!input || !Cache::IsCurrentEntryHeader(header, size)) { ++rejected; continue; }
        Bytes bytes(static_cast<std::size_t>(size));
        std::memcpy(bytes.data(), header.data(), header.size());
        input.read(reinterpret_cast<char*>(bytes.data() + header.size()), bytes.size() - header.size());
        Check(bool(input), "read cache entry " + path.string());
        bytesRead += bytes.size() - header.size();
        const auto file = std::as_bytes(std::span(bytes));
        auto key = Cache::EntryKey(file);
        ShaderRecompiler::CompiledVariant variant;
        if (key.empty() || Cache::DecodeEntry(file, key, variant) != Cache::LoadStatus::Loaded) { ++rejected; continue; }
        const std::string_view keyText(reinterpret_cast<const char*>(key.data()), key.size());
        if (keyText.ends_with(receipt)) {
            key = key.first(key.size() - receipt.size());
            writer.Add(Cache::EncodeEntry(key, variant));
            ++converted;
        } else writer.Add(file);
        ++accepted;
    }
    auto result = writer.Finish();
    result["candidates"] = Json::UInt64(paths.size());
    result["scanned"] = Json::UInt64(scanned);
    result["rejected"] = Json::UInt64(rejected);
    result["converted_validation_keys"] = Json::UInt64(converted);
    result["input_bytes_read"] = Json::UInt64(bytesRead);
    return result;
}
}
