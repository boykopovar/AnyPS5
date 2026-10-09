#pragma once

#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
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
#include <sys/file.h>
#include <unistd.h>
#endif

namespace ContentCatalog_nid_no_patch {

struct Entry {
    std::int64_t id = 0;
    std::int64_t generation = 0;
    std::string title;
    std::string mimeType;
    std::string path;
    std::string iconPath;
    std::uint64_t createdTime = 0;
    std::int64_t size = 0;
    std::int32_t contentType = 0;
    std::int32_t generatorType = 2;
    std::int32_t status = 1;
    std::int32_t uploadStatus = 0;
    std::array<std::int32_t, 4> accounts{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    double duration = 0;
    bool hasDimensions = false;
    bool hasDuration = false;
};

struct Snapshot {
    std::vector<Entry> entries;
    std::int64_t updateId = 0;
};

inline std::filesystem::path Root_nid_no_patch() {
    return ResolvePath_nid_no_patch("/data/anyps5-content");
}

inline std::uint64_t CurrentTick_nid_no_patch() {
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return 62135596800000000ull + static_cast<std::uint64_t>(micros);
}

class Lock_nid_no_patch {
public:
    explicit Lock_nid_no_patch(const std::filesystem::path& root) {
        std::filesystem::create_directories(root);
        const auto file = root / ".catalog.lock";
#ifdef _WIN32
        handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("content catalog: lock open failed");
        OVERLAPPED position{};
        if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &position)) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
            throw std::runtime_error("content catalog: lock failed");
        }
#else
        descriptor = ::open(file.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (descriptor < 0) throw std::runtime_error("content catalog: lock open failed");
        int result;
        do { result = ::flock(descriptor, LOCK_EX); } while (result < 0 && errno == EINTR);
        if (result < 0) {
            ::close(descriptor);
            descriptor = -1;
            throw std::runtime_error("content catalog: lock failed");
        }
#endif
    }

    ~Lock_nid_no_patch() {
#ifdef _WIN32
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
#else
        if (descriptor >= 0) ::close(descriptor);
#endif
    }

    Lock_nid_no_patch(const Lock_nid_no_patch&) = delete;
    Lock_nid_no_patch& operator=(const Lock_nid_no_patch&) = delete;
private:
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int descriptor = -1;
#endif
};

template<typename TValue>
inline void WriteScalar_nid_no_patch(std::ostream& stream, const TValue& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    if (!stream) throw std::runtime_error("content catalog: metadata write failed");
}

template<typename TValue>
inline void ReadScalar_nid_no_patch(std::istream& stream, TValue& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (!stream) throw std::runtime_error("content catalog: truncated metadata");
}

inline void WriteString_nid_no_patch(std::ostream& stream, const std::string& value) {
    const auto size = static_cast<std::uint32_t>(value.size());
    WriteScalar_nid_no_patch(stream, size);
    stream.write(value.data(), size);
    if (!stream) throw std::runtime_error("content catalog: metadata write failed");
}

inline void ReadString_nid_no_patch(std::istream& stream, std::string& value, std::uint32_t limit) {
    std::uint32_t size = 0;
    ReadScalar_nid_no_patch(stream, size);
    if (size > limit) throw std::runtime_error("content catalog: invalid metadata string size");
    value.resize(size);
    stream.read(value.data(), size);
    if (!stream || value.find('\0') != std::string::npos)
        throw std::runtime_error("content catalog: invalid metadata string");
}

inline void WriteEntry_nid_no_patch(const std::filesystem::path& directory, const Entry& entry) {
    std::ofstream stream(directory / "entry.bin", std::ios::binary | std::ios::trunc);
    WriteScalar_nid_no_patch(stream, std::uint64_t{0x31544E43535041});
    WriteScalar_nid_no_patch(stream, entry.id);
    WriteScalar_nid_no_patch(stream, entry.generation);
    WriteScalar_nid_no_patch(stream, entry.createdTime);
    WriteScalar_nid_no_patch(stream, entry.size);
    WriteScalar_nid_no_patch(stream, entry.contentType);
    WriteScalar_nid_no_patch(stream, entry.width);
    WriteScalar_nid_no_patch(stream, entry.height);
    WriteScalar_nid_no_patch(stream, entry.duration);
    WriteScalar_nid_no_patch(stream, std::uint8_t{entry.hasDimensions});
    WriteScalar_nid_no_patch(stream, std::uint8_t{entry.hasDuration});
    for (const auto account : entry.accounts) WriteScalar_nid_no_patch(stream, account);
    WriteString_nid_no_patch(stream, entry.title);
    WriteString_nid_no_patch(stream, entry.mimeType);
    WriteString_nid_no_patch(stream, entry.path);
    WriteString_nid_no_patch(stream, entry.iconPath);
    stream.close();
    if (!stream) throw std::runtime_error("content catalog: metadata close failed");
}

inline Entry ReadEntry_nid_no_patch(const std::filesystem::path& directory, std::int64_t id) {
    std::ifstream stream(directory / "entry.bin", std::ios::binary);
    std::uint64_t magic = 0;
    ReadScalar_nid_no_patch(stream, magic);
    if (magic != 0x31544E43535041) throw std::runtime_error("content catalog: unknown metadata version");
    Entry entry;
    ReadScalar_nid_no_patch(stream, entry.id);
    ReadScalar_nid_no_patch(stream, entry.generation);
    ReadScalar_nid_no_patch(stream, entry.createdTime);
    ReadScalar_nid_no_patch(stream, entry.size);
    ReadScalar_nid_no_patch(stream, entry.contentType);
    ReadScalar_nid_no_patch(stream, entry.width);
    ReadScalar_nid_no_patch(stream, entry.height);
    ReadScalar_nid_no_patch(stream, entry.duration);
    std::uint8_t dimensions = 0;
    std::uint8_t duration = 0;
    ReadScalar_nid_no_patch(stream, dimensions);
    ReadScalar_nid_no_patch(stream, duration);
    if (entry.id != id || entry.generation <= 0 || entry.size < 0 || dimensions > 1 || duration > 1)
        throw std::runtime_error("content catalog: invalid metadata");
    entry.hasDimensions = dimensions != 0;
    entry.hasDuration = duration != 0;
    for (auto& account : entry.accounts) ReadScalar_nid_no_patch(stream, account);
    ReadString_nid_no_patch(stream, entry.title, 256);
    ReadString_nid_no_patch(stream, entry.mimeType, 64);
    ReadString_nid_no_patch(stream, entry.path, 1024);
    ReadString_nid_no_patch(stream, entry.iconPath, 1024);
    if (stream.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("content catalog: trailing metadata");
    const auto content = ResolvePath_nid_no_patch(entry.path.c_str());
    if (content.parent_path() != directory || content.filename().string().find("content.") != 0)
        throw std::runtime_error("content catalog: invalid content path");
    std::error_code error;
    const auto size = std::filesystem::file_size(content, error);
    if (error || size != static_cast<std::uint64_t>(entry.size)) entry.status = 0;
    if (!entry.iconPath.empty()) {
        const auto icon = ResolvePath_nid_no_patch(entry.iconPath.c_str());
        if (icon.parent_path() != directory || icon.filename().string().find("thumbnail.") != 0)
            throw std::runtime_error("content catalog: invalid icon path");
        if (!std::filesystem::is_regular_file(icon, error) || error) entry.status = 0;
    }
    return entry;
}

inline Snapshot ReadUnlocked_nid_no_patch(const std::filesystem::path& root) {
    Snapshot snapshot;
    std::error_code error;
    if (!std::filesystem::exists(root, error)) {
        if (error) throw std::filesystem::filesystem_error("content catalog", root, error);
        return snapshot;
    }
    for (const auto& item : std::filesystem::directory_iterator(root)) {
        const auto name = item.path().filename().string();
        std::int64_t id = 0;
        const auto parsed = std::from_chars(name.data(), name.data() + name.size(), id);
        if (parsed.ec != std::errc{} || parsed.ptr != name.data() + name.size() || id <= 0) continue;
        if (!item.is_directory()) throw std::runtime_error("content catalog: invalid entry directory");
        snapshot.entries.push_back(ReadEntry_nid_no_patch(item.path(), id));
        snapshot.updateId = std::max(snapshot.updateId, snapshot.entries.back().generation);
    }
    std::ranges::sort(snapshot.entries, {}, &Entry::id);
    return snapshot;
}

inline Snapshot Read_nid_no_patch() {
    const auto root = Root_nid_no_patch();
    Lock_nid_no_patch lock(root);
    return ReadUnlocked_nid_no_patch(root);
}

class Transaction_nid_no_patch {
public:
    Transaction_nid_no_patch() {
        const auto root = Root_nid_no_patch();
        Lock_nid_no_patch lock(root);
        const auto snapshot = ReadUnlocked_nid_no_patch(root);
        const auto previous = snapshot.entries.empty() ? std::int64_t{0} : snapshot.entries.back().id;
        if (previous == std::numeric_limits<std::int64_t>::max())
            throw std::overflow_error("content catalog: content ids exhausted");
        id = std::max(static_cast<std::int64_t>(CurrentTick_nid_no_patch()), previous + 1);
        for (;;) {
            directory = root / (".pending-" + std::to_string(id));
            finalDirectory = root / std::to_string(id);
            if (!std::filesystem::exists(finalDirectory) && std::filesystem::create_directory(directory)) break;
            if (id == std::numeric_limits<std::int64_t>::max())
                throw std::overflow_error("content catalog: content ids exhausted");
            ++id;
        }
    }

    ~Transaction_nid_no_patch() {
        if (!committed) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
    }

    Transaction_nid_no_patch(const Transaction_nid_no_patch&) = delete;
    Transaction_nid_no_patch& operator=(const Transaction_nid_no_patch&) = delete;

    std::int64_t Id_nid_no_patch() const { return id; }
    const std::filesystem::path& Directory_nid_no_patch() const { return directory; }

    std::string GuestPath_nid_no_patch(const std::string& file) const {
        return "/data/anyps5-content/" + std::to_string(id) + "/" + file;
    }

    void Commit_nid_no_patch(Entry entry) {
        const auto root = Root_nid_no_patch();
        Lock_nid_no_patch lock(root);
        const auto snapshot = ReadUnlocked_nid_no_patch(root);
        if (snapshot.updateId == std::numeric_limits<std::int64_t>::max())
            throw std::overflow_error("content catalog: generations exhausted");
        entry.generation = snapshot.updateId + 1;
        WriteEntry_nid_no_patch(directory, entry);
        std::filesystem::rename(directory, finalDirectory);
        committed = true;
    }

private:
    std::int64_t id = 0;
    std::filesystem::path directory;
    std::filesystem::path finalDirectory;
    bool committed = false;
};

}
