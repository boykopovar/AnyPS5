#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <string_view>
#include <system_error>

extern "C" char* APS5_VABI getcwd_nid_postfix(char* buffer, std::size_t size);

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

bool ResolveGuest(const char* path, std::string& result, int& error) {
    std::string_view left(path);
    if (IsSeparator(left.front())) {
        result = "/";
        left.remove_prefix(1);
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
    while (!left.empty()) {
        const auto separator = left.find_first_of(Separators);
        const auto token = left.substr(0, separator);
        const bool moreFollows = separator != std::string_view::npos;
        left = moreFollows ? left.substr(separator + 1) : std::string_view();
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
        std::error_code code;
        const auto status = std::filesystem::status(ResolvePath_nid_no_patch(candidate.c_str()), code);
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
