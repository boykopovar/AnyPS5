#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr int ThreadNameInvalid = 22;
constexpr std::size_t ThreadNameCapacity = 32;

constexpr int FnmNomatch = 1;
constexpr int FnmNoescape = 0x01;
constexpr int FnmPathname = 0x02;
constexpr int FnmPeriod = 0x04;
constexpr int FnmLeadingDir = 0x08;
constexpr int FnmCasefold = 0x10;

enum class RangeResult { Match, NoMatch, Error };

unsigned char FoldCase(unsigned char c, int flags) {
    return (flags & FnmCasefold) && c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c - 'A' + 'a') : c;
}

RangeResult MatchRange(const char* pattern, unsigned char test, int flags, const char** next) {
    const bool negate = *pattern == '!' || *pattern == '^';
    if (negate) ++pattern;
    test = FoldCase(test, flags);
    bool ok = false;
    const char* const origin = pattern;
    for (;;) {
        if (*pattern == ']' && pattern > origin) {
            ++pattern;
            break;
        }
        if (*pattern == '\0') return RangeResult::Error;
        if (*pattern == '/' && (flags & FnmPathname)) return RangeResult::NoMatch;
        if (*pattern == '\\' && !(flags & FnmNoescape)) ++pattern;
        const unsigned char low = FoldCase(static_cast<unsigned char>(*pattern), flags);
        if (*pattern != '\0') ++pattern;
        if (*pattern == '-' && pattern[1] != '\0' && pattern[1] != ']') {
            if (*++pattern == '\\' && !(flags & FnmNoescape)) ++pattern;
            const unsigned char high = static_cast<unsigned char>(*pattern);
            if (high == '\0') return RangeResult::Error;
            ++pattern;
            if (low <= test && test <= FoldCase(high, flags)) ok = true;
        } else if (low == test) {
            ok = true;
        }
    }
    *next = pattern;
    return ok == negate ? RangeResult::NoMatch : RangeResult::Match;
}

int MatchPattern(const char* pattern, const char* string, int flags) {
    const char* const start = string;
    const char* backtrackPattern = nullptr;
    const char* backtrackString = nullptr;
    const auto leadingPeriod = [&](const char* at) {
        return *at == '.' && (flags & FnmPeriod) && (at == start || ((flags & FnmPathname) && at[-1] == '/'));
    };
    for (;;) {
        const unsigned char pc = static_cast<unsigned char>(*pattern);
        if (pc != '\0') ++pattern;
        const unsigned char sc = static_cast<unsigned char>(*string);
        bool mismatch = false;
        bool literal = false;
        unsigned char expected = pc;
        switch (pc) {
            case '\0':
                if ((flags & FnmLeadingDir) && sc == '/') return 0;
                if (sc == '\0') return 0;
                mismatch = true;
                break;
            case '?':
                if (sc == '\0') return FnmNomatch;
                if ((sc == '/' && (flags & FnmPathname)) || leadingPeriod(string)) {
                    mismatch = true;
                    break;
                }
                ++string;
                break;
            case '*': {
                char c = *pattern;
                while (c == '*') c = *++pattern;
                if (leadingPeriod(string)) {
                    mismatch = true;
                    break;
                }
                if (c == '\0') {
                    if (flags & FnmPathname) return (flags & FnmLeadingDir) || std::strchr(string, '/') == nullptr ? 0 : FnmNomatch;
                    return 0;
                }
                if (c == '/' && (flags & FnmPathname)) {
                    string = std::strchr(string, '/');
                    if (string == nullptr) return FnmNomatch;
                    break;
                }
                backtrackPattern = pattern;
                backtrackString = string;
                break;
            }
            case '[': {
                if (sc == '\0') return FnmNomatch;
                if ((sc == '/' && (flags & FnmPathname)) || leadingPeriod(string)) {
                    mismatch = true;
                    break;
                }
                const char* next = nullptr;
                const RangeResult result = MatchRange(pattern, sc, flags, &next);
                if (result == RangeResult::Error) {
                    literal = true;
                } else if (result == RangeResult::Match) {
                    pattern = next;
                    ++string;
                } else {
                    mismatch = true;
                }
                break;
            }
            case '\\':
                if (!(flags & FnmNoescape)) {
                    expected = static_cast<unsigned char>(*pattern);
                    if (expected == '\0') return FnmNomatch;
                    ++pattern;
                }
                literal = true;
                break;
            default:
                literal = true;
                break;
        }
        if (literal) {
            if (sc != '\0') ++string;
            if (FoldCase(expected, flags) != FoldCase(sc, flags)) mismatch = true;
        }
        if (mismatch) {
            if (backtrackPattern == nullptr) return FnmNomatch;
            const unsigned char bc = static_cast<unsigned char>(*backtrackString);
            if (bc == '\0') return FnmNomatch;
            if (bc == '/' && (flags & FnmPathname)) return FnmNomatch;
            ++backtrackString;
            pattern = backtrackPattern;
            string = backtrackString;
        }
    }
}

}

extern "C" {

int APS5_VABI fnmatch_nid_postfix(const char* pattern, const char* string, int flags) {
    if (pattern == nullptr || string == nullptr) throw std::invalid_argument("fnmatch: null pattern or string");
    return MatchPattern(pattern, string, flags);
}

char* APS5_VABI strcasestr_nid_postfix(const char* text, const char* needle) {
    if (text == nullptr || needle == nullptr) return nullptr;
    std::string hostText;
    if (CopyGuestString_nid_no_patch(text, 4096, hostText)) return nullptr;
    std::string hostNeedle;
    if (CopyGuestString_nid_no_patch(needle, 4096, hostNeedle)) return nullptr;
    for (std::size_t offset = 0; offset + hostNeedle.size() <= hostText.size(); ++offset) {
        bool match = true;
        for (std::size_t index = 0; index < hostNeedle.size(); ++index) {
            if (FoldCase(static_cast<unsigned char>(hostText[offset + index]), FnmCasefold) !=
                FoldCase(static_cast<unsigned char>(hostNeedle[index]), FnmCasefold)) {
                match = false;
                break;
            }
        }
        if (match) return const_cast<char*>(text) + offset;
    }
    return nullptr;
}

void APS5_VABI arc4random_buf_nid_postfix(void* buffer, std::size_t size) {
    if (!buffer || size == 0) return;
    std::array<std::uint8_t, 256> chunk;
    std::size_t offset = 0;
    const int savedErrno = errno;
    while (offset < size) {
        const std::size_t current = std::min(size - offset, chunk.size());
        HostEntropy_nid_no_patch(chunk.data(), current);
        if (!GuestCopyOut_nid_no_patch(static_cast<char*>(buffer) + offset, chunk.data(), current)) {
            errno = savedErrno;
            return;
        }
        offset += current;
    }
    errno = savedErrno;
}

std::uint32_t APS5_VABI arc4random_nid_postfix() {
    std::uint32_t value;
    arc4random_buf_nid_postfix(&value, sizeof(value));

    return value;
}

std::uint32_t APS5_VABI arc4random_uniform_nid_postfix(std::uint32_t upper_bound) {
    if (upper_bound < 2) return 0;
    const std::uint32_t min = -upper_bound % upper_bound;
    std::uint32_t value;
    do {
        value = arc4random_nid_postfix();
    } while (value < min);
    return value % upper_bound;
}

int APS5_VABI __wrap_access(const char* path, int mode) {
    return access_nid_postfix(path, mode);
}

int APS5_VABI __wrap_clock_gettime(int clockId, KernelTimespec* tp) {
    return clock_gettime_nid_postfix(clockId, tp);
}


char* APS5_VABI __wrap_getcwd(char* buffer, std::size_t size) {
    return getcwd_nid_postfix(buffer, size);
}

char* APS5_VABI __wrap_getenv(const char* name) {
    return getenv_nid_postfix(name);
}


int APS5_VABI __wrap_lstat(const char* path, FileStat* sb) {
    return lstat_nid_postfix(path, sb);
}

void* APS5_VABI __wrap_mmap(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset) {
    return mmap_nid_postfix(address, length, protection, flags, descriptor, offset);
}




int APS5_VABI clock_gettime_np(int clockId, KernelTimespec* tp) {
    return clock_gettime_nid_postfix(clockId, tp);
}

int APS5_VABI getenv_np(const char* name, char* buffer, std::size_t size) {
    if (!name || !buffer)
        throw std::invalid_argument("getenv_np requires a name and a buffer");
    const char* value = getenv_nid_postfix(name);
    if (!value)
        return -1;
    const std::size_t length = std::strlen(value);
    if (length >= size)
        throw std::runtime_error("getenv_np: the console's handling of a value longer than the buffer is unknown");
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(buffer, value, length + 1);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument("getenv_np requires a writable buffer");
    return 0;
}

APS5_EXPORT("dt8ermYp-Wc", scePosixForWebKit_gethostbyname_r);

int APS5_VABI mmap_np() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI msetname() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_setschedparam_np() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI setenv_np() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("9Vqxt-qBPB4", scePosixForWebKit_sysinfo);
int APS5_VABI scePosixForWebKit_sysinfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_setname_np_nid_postfix(Pthread thread, const char* name) {
    if (thread == nullptr || name == nullptr) return ThreadNameInvalid;
    std::string copy;
    if (const int error = CopyGuestString_nid_no_patch(name, ThreadNameCapacity, copy)) return error;
    std::lock_guard lock(thread->nameLock);
    thread->name = std::move(copy);
    return 0;
}

void* APS5_VABI opendir_nid_disambig1_nid_postfix(const char* path) {
    return OpenDirectory_nid_no_patch(path);
}

int APS5_VABI closedir_nid_disambig1_nid_postfix(void* handle) {
    return CloseDirectory_nid_no_patch(handle);
}

GuestDirectoryEntry* APS5_VABI readdir_nid_disambig1_nid_postfix(void* handle) {
    return ReadDirectory_nid_no_patch(handle);
}

int APS5_VABI readdir_r_nid_disambig1_nid_postfix(void* handle, GuestDirectoryEntry* entry, GuestDirectoryEntry** result) {
    return ReadDirectoryEntry_nid_no_patch(handle, entry, result);
}
}
