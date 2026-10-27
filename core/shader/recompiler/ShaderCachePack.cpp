#include "ShaderCachePack.hpp"
#include "ShaderCacheDirectory.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <limits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ShaderRecompiler::ShaderDiskCache {
namespace {
using Clock = std::chrono::steady_clock;
std::uint64_t elapsed(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}
}

std::array<std::uint64_t, 2> PackKeyHash(std::span<const std::byte> key) {
    return {HashBytes(key, 0x5eed0001ull), HashBytes(key, 0x5eed0002ull)};
}

std::vector<std::filesystem::path> PrebuiltCachePaths(const std::filesystem::path& cacheRoot) {
    if (cacheRoot.empty()) return {};
    return {cacheRoot / "prebuilt"};
}

CachePack::CachePack(const std::filesystem::path& path) {
    static_assert(std::endian::native == std::endian::little);
#ifdef _WIN32
    std::error_code pathError;
    const auto filename = NativeCachePath_nid_no_patch(path, pathError);
    if (pathError) { error = "cannot resolve"; return; }
    const auto handle = CreateFileW(filename.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { error = "cannot open"; return; }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size) || size.QuadPart < static_cast<LONGLONG>(sizeof(PackHeader)) || static_cast<std::uint64_t>(size.QuadPart) > std::numeric_limits<std::size_t>::max()) {
        CloseHandle(handle); error = "invalid size"; return;
    }
    mapping = CreateFileMappingW(handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    fileHandle = handle;
    if (!mapping) { error = "cannot map"; return; }
    const auto* data = static_cast<const std::byte*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    if (!data) { error = "cannot map view"; return; }
    file = {data, static_cast<std::size_t>(size.QuadPart)};
#else
    const auto fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) { error = "cannot open"; return; }
    struct stat size{};
    if (fstat(fd, &size) != 0 || size.st_size < static_cast<off_t>(sizeof(PackHeader)) || static_cast<std::uint64_t>(size.st_size) > std::numeric_limits<std::size_t>::max()) {
        close(fd); error = "invalid size"; return;
    }
    auto* data = mmap(nullptr, size.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (data == MAP_FAILED) { error = "cannot map"; return; }
    file = {static_cast<const std::byte*>(data), static_cast<std::size_t>(size.st_size)};
#endif
    Validate();
}

CachePack::~CachePack() {
#ifdef _WIN32
    if (!file.empty()) UnmapViewOfFile(file.data());
    if (mapping) CloseHandle(mapping);
    if (fileHandle) CloseHandle(fileHandle);
#else
    if (!file.empty()) munmap(const_cast<std::byte*>(file.data()), file.size());
#endif
}

void CachePack::Validate() {
    PackHeader header;
    std::memcpy(&header, file.data(), sizeof(header));
    if (header.magic != PackMagic) { error = "unknown pack format"; return; }
    if (header.sourceVersion != SourceVersion()) { error = "different compiler source version"; return; }
    if (header.fileBytes != file.size() || header.entryCount > (file.size() - sizeof(header)) / sizeof(PackRecord)) { error = "truncated index"; return; }
    const auto indexBytes = static_cast<std::size_t>(header.entryCount) * sizeof(PackRecord);
    const auto index = file.subspan(sizeof(header), indexBytes);
    if (HashBytes(index) != header.indexHash) { error = "index checksum mismatch"; return; }
    const std::span<const PackRecord> checked(reinterpret_cast<const PackRecord*>(index.data()), header.entryCount);
    std::array<std::uint64_t, 2> previous{};
    for (const auto& record : checked) {
        if (record.hash < previous || record.offset < sizeof(header) + indexBytes || record.offset > file.size() || record.size > file.size() - record.offset || record.size < 48) { error = "invalid index record"; return; }
        previous = record.hash;
    }
    records = checked;
}

LoadStatus CachePack::Load(std::span<const std::byte> key, CompiledVariant& variant, PackLoadTimings& timings) const {
    const auto start = Clock::now();
    const auto hash = PackKeyHash(key);
    const auto first = std::lower_bound(records.begin(), records.end(), hash, [](const auto& record, const auto& value) { return record.hash < value; });
    timings.lookupNanoseconds += elapsed(start);
    auto status = LoadStatus::Absent;
    for (auto it = first; it != records.end() && it->hash == hash; ++it) {
        const auto data = file.subspan(it->offset, it->size);
        const auto decode = Clock::now();
        const auto result = DecodeEntry(data, key, variant);
        timings.decodeNanoseconds += elapsed(decode);
        timings.bytes += data.size();
        if (result == LoadStatus::Loaded) return result;
        if (result == LoadStatus::Rejected) status = result;
    }
    return status;
}
}
