#ifndef CORE_SHADER_RECOMPILER_SHADERCACHEDIRECTORY_HPP
#define CORE_SHADER_RECOMPILER_SHADERCACHEDIRECTORY_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <new>
#include <stdexcept>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ShaderRecompiler {

std::filesystem::path ShaderCacheDirectory();

#ifdef _WIN32
inline std::filesystem::path NativeCachePath_nid_no_patch(const std::filesystem::path& path, std::error_code& error) {
    auto native = path;
    native.make_preferred();
    if (native.native().starts_with(L"\\\\?\\") || native.native().starts_with(L"\\\\.\\")) return native;
    const auto capacity = GetFullPathNameW(native.c_str(), 0, nullptr, nullptr);
    if (capacity == 0) { error = std::error_code(GetLastError(), std::system_category()); return {}; }
    std::wstring absolute(capacity, L'\0');
    const auto size = GetFullPathNameW(native.c_str(), capacity, absolute.data(), nullptr);
    if (size == 0 || size >= capacity) {
        error = std::error_code(size == 0 ? GetLastError() : ERROR_INSUFFICIENT_BUFFER, std::system_category());
        return {};
    }
    absolute.resize(size);
    if (absolute.starts_with(L"\\\\")) return std::filesystem::path(L"\\\\?\\UNC\\" + absolute.substr(2));
    return std::filesystem::path(L"\\\\?\\" + absolute);
}
#endif

#ifdef _WIN32
inline DWORD PosixReplaceByHandle(HANDLE source, const std::filesystem::path& destination) {
    struct RenameInfo { DWORD flags; HANDLE root; DWORD nameBytes; WCHAR name[1]; };
    static_assert(sizeof(RenameInfo) == sizeof(FILE_RENAME_INFO) && offsetof(RenameInfo, name) == offsetof(FILE_RENAME_INFO, FileName));
    const auto nameBytes = destination.native().size() * sizeof(wchar_t);
    std::vector<std::byte> renameBuffer(sizeof(RenameInfo) + nameBytes);
    auto* rename = reinterpret_cast<RenameInfo*>(renameBuffer.data());
    constexpr DWORD replaceExisting = 0x00000001, posixSemantics = 0x00000002;
    rename->flags = replaceExisting | posixSemantics;
    rename->nameBytes = static_cast<DWORD>(nameBytes);
    std::memcpy(rename->name, destination.c_str(), nameBytes);
    constexpr auto renameInfoEx = static_cast<FILE_INFO_BY_HANDLE_CLASS>(22);
    return SetFileInformationByHandle(source, renameInfoEx, rename, static_cast<DWORD>(renameBuffer.size())) ? ERROR_SUCCESS : GetLastError();
}
#endif

inline bool WriteFileAtomically(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::error_code error;
#ifdef _WIN32
    const auto destination = NativeCachePath_nid_no_patch(path, error);
    if (error) return false;
    if (destination.native().size() > (MAXDWORD - sizeof(FILE_RENAME_INFO)) / sizeof(wchar_t)) return false;
#else
    const auto& destination = path;
#endif
    std::filesystem::create_directories(destination.parent_path(), error);
    static std::atomic<std::uint64_t> serial{0};
    const auto unique = std::hash<std::thread::id>{}(std::this_thread::get_id()) ^ static_cast<std::size_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    auto temporary = destination;
    temporary += ".tmp." + std::to_string(unique) + "." + std::to_string(serial.fetch_add(1, std::memory_order_relaxed));
    bool written = false;
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        constexpr std::size_t largestWrite = std::size_t{1} << 30u;
        for (std::size_t offset = 0; offset < bytes.size() && file;) {
            const auto chunk = std::min(bytes.size() - offset, largestWrite);
            file.write(reinterpret_cast<const char*>(bytes.data() + offset), static_cast<std::streamsize>(chunk));
            offset += chunk;
        }
        file.close();
        written = !file.fail();
    }
    if (!written) {
        std::filesystem::remove(temporary, error);
        return false;
    }
#ifdef _WIN32
    DWORD renameError = ERROR_SUCCESS;
    {
        const auto handle = CreateFileW(temporary.c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            std::filesystem::remove(temporary, error);
            return false;
        }
        struct Close { HANDLE handle; ~Close() { CloseHandle(handle); } } close{handle};
        renameError = PosixReplaceByHandle(handle, destination);
    }
    if (renameError == ERROR_SUCCESS) return true;
    if (renameError != ERROR_INVALID_PARAMETER && renameError != ERROR_INVALID_FUNCTION && renameError != ERROR_NOT_SUPPORTED) {
        std::filesystem::remove(temporary, error);
        return false;
    }
#endif
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

#ifdef _WIN32
inline bool PosixReplaceUnsupported(const std::filesystem::path& directory) {
    std::error_code error;
    const auto native = NativeCachePath_nid_no_patch(directory, error);
    if (error) return false;
    const auto source = native / L"posix-replace-probe.source";
    const auto target = native / L"posix-replace-probe.target";
    for (const auto& path : {source, target}) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.put('\0');
    }
    DWORD result = ERROR_SUCCESS;
    const auto handle = CreateFileW(source.c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        result = PosixReplaceByHandle(handle, target);
        CloseHandle(handle);
    }
    std::filesystem::remove(source, error);
    std::filesystem::remove(target, error);
    return result == ERROR_INVALID_PARAMETER;
}
#endif

inline bool ReadWholeFile(const std::filesystem::path& path, std::vector<std::byte>& bytes) {
    bytes.clear();
    try {
#ifdef _WIN32
        std::error_code error;
        const auto native = NativeCachePath_nid_no_patch(path, error);
        if (error) return false;
        const auto file = CreateFileW(native.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        struct Close { HANDLE file; ~Close() { CloseHandle(file); } } close{file};
        LARGE_INTEGER fileSize{};
        if (!GetFileSizeEx(file, &fileSize)) return false;
        const auto size = fileSize.QuadPart;
#else
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return false;
        const auto size = static_cast<std::streamoff>(file.tellg());
#endif
        if (size < 0 || static_cast<std::uintmax_t>(size) > bytes.max_size() ||
            static_cast<std::uintmax_t>(size) > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
            return false;
#ifdef _WIN32
        bytes.resize(static_cast<std::size_t>(size));
        for (std::size_t offset = 0; offset < bytes.size();) {
            const auto left = bytes.size() - offset;
            const auto chunk = static_cast<DWORD>(left > MAXDWORD ? MAXDWORD : left);
            DWORD count = 0;
            if (!ReadFile(file, bytes.data() + offset, chunk, &count, nullptr) || count == 0) {
                bytes.clear();
                return false;
            }
            offset += count;
        }
#else
        if (!file.seekg(0, std::ios::beg)) return false;
        bytes.resize(static_cast<std::size_t>(size));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
            bytes.clear();
            return false;
        }
#endif
        return true;
    } catch (const std::bad_alloc&) {
        bytes.clear();
        return false;
    } catch (const std::length_error&) {
        bytes.clear();
        return false;
    }
}

inline std::uint64_t HashBytes(std::span<const std::byte> bytes, std::uint64_t seed = 0) {
    constexpr std::uint64_t multiplier = 0x9e3779b97f4a7c15ull;
    std::uint64_t hash = seed ^ (static_cast<std::uint64_t>(bytes.size()) * multiplier);
    const auto mix = [&](std::uint64_t chunk) {
        chunk *= 0xff51afd7ed558ccdull;
        chunk ^= chunk >> 32u;
        hash = (hash ^ chunk) * multiplier;
        hash = (hash << 27u) | (hash >> 37u);
    };
    std::size_t index = 0;
    for (; index + 8 <= bytes.size(); index += 8) {
        std::uint64_t chunk;
        std::memcpy(&chunk, bytes.data() + index, sizeof(chunk));
        mix(chunk);
    }
    if (index < bytes.size()) {
        std::uint64_t chunk = 0;
        std::memcpy(&chunk, bytes.data() + index, bytes.size() - index);
        mix(chunk ^ 0x8000000000000000ull);
    }
    hash ^= hash >> 33u;
    hash *= 0xff51afd7ed558ccdull;
    hash ^= hash >> 33u;
    hash *= 0xc4ceb9fe1a85ec53ull;
    hash ^= hash >> 33u;
    return hash;
}

}

#endif
