#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <string_view>
#include <system_error>

namespace {
constexpr std::size_t PathMax = 1024;
constexpr std::string_view Separators = "/\\";
constexpr int NoEntry = 2;
constexpr int InputOutput = 5;
constexpr int NoMemory = 12;
constexpr int PermissionDenied = 13;
constexpr int NotDirectory = 20;
constexpr int InvalidArgument = 22;
constexpr int TooManyLinks = 62;
constexpr int NameTooLong = 63;
constexpr unsigned MaxSymbolicLinks = 32;

int StatusError(const std::error_code& error) {
    if (error == std::errc::no_such_file_or_directory) return NoEntry;
    if (error == std::errc::not_a_directory) return NotDirectory;
    if (error == std::errc::permission_denied) return PermissionDenied;
    if (error == std::errc::filename_too_long) return NameTooLong;
    if (error == std::errc::too_many_symbolic_link_levels) return TooManyLinks;
    if (error == std::errc::not_enough_memory) return NoMemory;
    return InputOutput;
}

void LeaveTrouble(char* resolved, const std::string& trouble) {
    if (resolved == nullptr) return;
    const auto length = trouble.size() < PathMax ? trouble.size() : PathMax - 1;
    std::memcpy(resolved, trouble.data(), length);
    resolved[length] = '\0';
}

void DropLastComponent(std::string& result) {
    if (result.size() <= 1) return;
    const auto slash = result.rfind('/');
    result.erase(slash == 0 ? 1 : slash);
}

bool IsSeparator(char character) { return Separators.find(character) != std::string_view::npos; }

bool ReadGuestLink(const std::filesystem::path& host, std::string& target, bool& isLink, bool& absolute) {
    isLink = false;
    absolute = false;
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(host.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) return true;
    const HANDLE handle = CreateFileW(host.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return true;
    struct ReparseHeader {
        ULONG tag;
        USHORT dataLength;
        USHORT reserved;
        USHORT substituteNameOffset;
        USHORT substituteNameLength;
        USHORT printNameOffset;
        USHORT printNameLength;
        ULONG flags;
    } header{};
    alignas(8) std::array<std::byte, MAXIMUM_REPARSE_DATA_BUFFER_SIZE> data{};
    DWORD size = 0;
    const BOOL read = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, data.data(), static_cast<DWORD>(data.size()), &size, nullptr);
    CloseHandle(handle);
    if (!read || size < sizeof(header)) return true;
    std::memcpy(&header, data.data(), sizeof(header));
    if (header.tag != IO_REPARSE_TAG_SYMLINK) return true;
    constexpr ULONG relativeFlag = 1;
    if ((header.flags & relativeFlag) == 0) {
        absolute = true;
        return true;
    }
    isLink = true;
    const std::size_t begin = sizeof(header) + header.substituteNameOffset;
    if (header.substituteNameLength % sizeof(wchar_t) != 0 || begin + header.substituteNameLength > size) return false;
    std::wstring name(header.substituteNameLength / sizeof(wchar_t), L'\0');
    if (!name.empty()) std::memcpy(name.data(), data.data() + begin, header.substituteNameLength);
    if (!name.empty()) {
        const int length = WideCharToMultiByte(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), nullptr, 0, nullptr, nullptr);
        target.assign(static_cast<std::size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), target.data(), length, nullptr, nullptr);
    }
#else
    std::error_code code;
    if (!std::filesystem::is_symlink(std::filesystem::symlink_status(host, code))) return true;
    isLink = true;
    target = std::filesystem::read_symlink(host, code).generic_string();
    if (code) return false;
#endif
    absolute = !target.empty() && IsSeparator(target.front());
#ifdef _WIN32
    absolute = absolute || (target.size() > 1 && target[1] == ':');
#endif
    if (absolute) {
        isLink = false;
        target.clear();
    }
    return true;
}

bool ResolveGuest(const char* path, std::string& result, int& error) {
    std::string left(path);
    if (IsSeparator(left.front())) {
        result = "/";
        left.erase(0, 1);
    } else {
        char current[PathMax];
        if (getcwd_nid_postfix(current, sizeof(current)) == nullptr) {
            result = ".";
            error = errno;
            return false;
        }
        result = current;
    }
    if (left.size() >= PathMax || result.size() >= PathMax) {
        error = NameTooLong;
        return false;
    }
    unsigned links = 0;
    bool expandLinks = true;
    while (!left.empty()) {
        const auto separator = left.find_first_of(Separators);
        const std::string token = left.substr(0, separator);
        const bool moreFollows = separator != std::string::npos;
        left = moreFollows ? left.substr(separator + 1) : std::string();
        if (result.size() > 1 && result.size() + 1 >= PathMax) {
            error = NameTooLong;
            return false;
        }
        if (token.empty() || token == ".") continue;
        if (token == "..") {
            DropLastComponent(result);
            continue;
        }
        std::string candidate = result;
        if (candidate.size() > 1) candidate += '/';
        candidate += token;
        result = candidate;
        if (candidate.size() >= PathMax) {
            error = NameTooLong;
            return false;
        }
        const auto host = ResolvePath_nid_no_patch(candidate.c_str());
        std::string target;
        bool isLink = false;
        bool absolute = false;
        if (expandLinks && !ReadGuestLink(host, target, isLink, absolute)) {
            error = InputOutput;
            return false;
        }
        if (absolute) expandLinks = false;
        if (isLink) {
            if (links++ > MaxSymbolicLinks) {
                error = TooManyLinks;
                return false;
            }
            if (target.empty()) {
                error = NoEntry;
                return false;
            }
            if (target.size() >= PathMax) {
                error = NameTooLong;
                return false;
            }
            DropLastComponent(result);
            if (moreFollows) {
                if (!IsSeparator(target.back())) {
                    if (target.size() + 1 >= PathMax) {
                        error = NameTooLong;
                        return false;
                    }
                    target += '/';
                }
                target += left;
                if (target.size() >= PathMax) {
                    error = NameTooLong;
                    return false;
                }
            }
            left = target;
            continue;
        }
        std::error_code code;
        const auto status = std::filesystem::status(host, code);
        if (code && code != std::errc::no_such_file_or_directory && code != std::errc::not_a_directory) {
            error = StatusError(code);
            return false;
        }
        if (!std::filesystem::exists(status)) {
            error = NoEntry;
            return false;
        }
        if (moreFollows && !std::filesystem::is_directory(status)) {
            error = NotDirectory;
            return false;
        }
    }
    return true;
}
}

extern "C" char* APS5_VABI realpath_nid_postfix(const char* path, char* resolved) {
    if (path == nullptr) {
        errno = InvalidArgument;
        return nullptr;
    }
    if (*path == '\0') {
        errno = NoEntry;
        return nullptr;
    }
    std::string result;
    int error = 0;
    try {
        if (!ResolveGuest(path, result, error)) {
            LeaveTrouble(resolved, result);
            errno = error;
            return nullptr;
        }
        if (resolved == nullptr) {
            resolved = static_cast<char*>(ApplicationHeapAllocate_nid_no_patch(PathMax));
            if (resolved == nullptr) {
                errno = NoMemory;
                return nullptr;
            }
        }
        std::memcpy(resolved, result.c_str(), result.size() + 1);
        return resolved;
    } catch (const std::bad_alloc&) {
        errno = NoMemory;
        return nullptr;
    } catch (const std::filesystem::filesystem_error& failure) {
        LeaveTrouble(resolved, result);
        errno = StatusError(failure.code());
        return nullptr;
    }
}
