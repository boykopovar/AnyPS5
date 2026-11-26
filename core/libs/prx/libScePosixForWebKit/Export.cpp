#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
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
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <limits>
#include <new>
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
#include <unistd.h>
#endif

namespace {

constexpr int ThreadNameInvalid = 22;
constexpr std::size_t ThreadNameCapacity = 32;
constexpr int PosixEnomem = 12;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr std::size_t EnvTableCapacity = 128;
constexpr std::size_t MaxEnvStringLength = 4096;
constexpr std::uint64_t DefaultTotalMemoryAssumption = 0x80000000ULL;

struct ModuleEnvEntry {
    bool used = false;
    std::string name;
    std::string value;
};

std::mutex& ModuleEnvMutex() {
    static std::mutex mutex;
    return mutex;
}

std::array<ModuleEnvEntry, EnvTableCapacity>& ModuleEnvTable() {
    static std::array<ModuleEnvEntry, EnvTableCapacity> table{};
    return table;
}

bool ReadGuestCString(const char* source, std::string& out) {
    out.clear();
    if (source == nullptr) return false;
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    const int savedErrno = errno;
    for (std::size_t index = 0; index < MaxEnvStringLength; ++index) {
        if (base > std::numeric_limits<std::uintptr_t>::max() - index) {
            errno = savedErrno;
            return false;
        }
        char ch = 0;
        if (!GuestCopyIn_nid_no_patch(&ch, reinterpret_cast<const void*>(base + index), sizeof(ch))) {
            errno = savedErrno;
            return false;
        }
        if (ch == '\0') {
            errno = savedErrno;
            return true;
        }
        out.push_back(ch);
    }
    errno = savedErrno;
    return false;
}

bool LookupModuleEnv(const std::string& name, std::string& value) {
    std::lock_guard lock(ModuleEnvMutex());
    for (const auto& entry : ModuleEnvTable()) {
        if (entry.used && entry.name == name) {
            value = entry.value;
            return true;
        }
    }
    return false;
}

std::uint64_t TotalMemorySizeAssumption() {
    static const std::uint64_t cached = []() -> std::uint64_t {
        std::string text;
        if (!LookupModuleEnv("TOTAL_MEMORY_SIZE_ASSUMPTION", text) || text.empty()) {
            return DefaultTotalMemoryAssumption;
        }
        const unsigned long long parsed = std::strtoull(text.c_str(), nullptr, 10);
        return static_cast<std::int64_t>(parsed) > 0 ? static_cast<std::uint64_t>(parsed) : DefaultTotalMemoryAssumption;
    }();
    return cached;
}

void SetVirtualRangeNamePrefixed(void* address, std::size_t length, const std::string& raw) {
    if (raw.rfind("SceNK", 0) == 0) {
        sceKernelSetVirtualRangeName(address, length, raw.c_str());
        return;
    }
    std::string prefixed = "SceNK" + raw;
    if (prefixed.size() > 31) prefixed.resize(31);
    sceKernelSetVirtualRangeName(address, length, prefixed.c_str());
}

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

std::size_t HostPageSize() {
    static const std::size_t size = [] {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        return info.dwPageSize != 0 ? static_cast<std::size_t>(info.dwPageSize) : static_cast<std::size_t>(4096);
#else
        const long value = ::sysconf(_SC_PAGESIZE);
        return value > 0 ? static_cast<std::size_t>(value) : static_cast<std::size_t>(4096);
#endif
    }();
    return size;
}

bool CopyGuestBytesPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool CopyGuestStringUnbounded(const char* source, std::vector<char>& scratch, std::string& out) {
    out.clear();
    if (source == nullptr) return false;
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    const std::size_t pageSize = scratch.size();
    std::size_t offset = 0;
    for (;;) {
        if (base > std::numeric_limits<std::uintptr_t>::max() - offset) return false;
        const std::uintptr_t current = base + offset;
        const std::size_t toPageEnd = pageSize - static_cast<std::size_t>(current % pageSize);
        if (toPageEnd > std::numeric_limits<std::uintptr_t>::max() - current) return false;
        if (!CopyGuestBytesPreservingErrno(scratch.data(), reinterpret_cast<const void*>(current), toPageEnd)) {
            return false;
        }
        const auto* nul = static_cast<const char*>(std::memchr(scratch.data(), '\0', toPageEnd));
        const std::size_t pieceLength = nul != nullptr ? static_cast<std::size_t>(nul - scratch.data()) : toPageEnd;
        out.append(scratch.data(), pieceLength);
        if (nul != nullptr) return true;
        if (offset > std::numeric_limits<std::size_t>::max() - toPageEnd) return false;
        offset += toPageEnd;
    }
}

}

extern "C" {

int APS5_VABI fnmatch_nid_postfix(const char* pattern, const char* string, int flags) {
    if (pattern == nullptr || string == nullptr) throw std::invalid_argument("fnmatch: null pattern or string");
    std::vector<char> scratch(HostPageSize());
    std::string patternCopy;
    std::string stringCopy;
    if (!CopyGuestStringUnbounded(pattern, scratch, patternCopy) || !CopyGuestStringUnbounded(string, scratch, stringCopy)) {
        throw std::invalid_argument("fnmatch: unreadable pattern or string");
    }
    return MatchPattern(patternCopy.c_str(), stringCopy.c_str(), flags);
}

char* APS5_VABI strcasestr_nid_postfix(const char* text, const char* needle) {
    if (text == nullptr || needle == nullptr) return nullptr;
    try {
        const std::size_t pageSize = HostPageSize();
        std::vector<char> scratch(pageSize);
        std::string foldedNeedle;
        if (!CopyGuestStringUnbounded(needle, scratch, foldedNeedle)) return nullptr;
        if (foldedNeedle.empty()) {
            char first = 0;
            return CopyGuestBytesPreservingErrno(&first, text, 1) ? const_cast<char*>(text) : nullptr;
        }
        for (char& ch : foldedNeedle) {
            ch = static_cast<char>(FoldCase(static_cast<unsigned char>(ch), FnmCasefold));
        }
        const std::size_t needleSize = foldedNeedle.size();
        const std::size_t overlap = needleSize - 1;
        const std::size_t compactThreshold = std::max(pageSize, overlap);
        std::string window;
        std::size_t baseOffset = 0;
        std::size_t searchOffset = 0;
        std::size_t readOffset = 0;
        const auto textBase = reinterpret_cast<std::uintptr_t>(text);
        for (;;) {
            if (textBase > std::numeric_limits<std::uintptr_t>::max() - readOffset) return nullptr;
            const std::uintptr_t current = textBase + readOffset;
            const std::size_t toPageEnd = pageSize - static_cast<std::size_t>(current % pageSize);
            if (toPageEnd > std::numeric_limits<std::uintptr_t>::max() - current) return nullptr;
            if (!CopyGuestBytesPreservingErrno(scratch.data(), reinterpret_cast<const void*>(current), toPageEnd)) {
                return nullptr;
            }
            const auto* nul = static_cast<const char*>(std::memchr(scratch.data(), '\0', toPageEnd));
            const std::size_t pieceLength = nul != nullptr ? static_cast<std::size_t>(nul - scratch.data()) : toPageEnd;
            const std::size_t previousSize = window.size();
            window.resize(previousSize + pieceLength);
            for (std::size_t index = 0; index < pieceLength; ++index) {
                window[previousSize + index] =
                    static_cast<char>(FoldCase(static_cast<unsigned char>(scratch[index]), FnmCasefold));
            }
            if (window.size() >= needleSize) {
                const std::size_t found = window.find(foldedNeedle, searchOffset);
                if (found != std::string::npos) {
                    return const_cast<char*>(text) + baseOffset + found;
                }
                searchOffset = window.size() - overlap;
                if (searchOffset >= compactThreshold) {
                    window.erase(0, searchOffset);
                    baseOffset += searchOffset;
                    searchOffset = 0;
                }
            }
            if (nul != nullptr) return nullptr;
            if (readOffset > std::numeric_limits<std::size_t>::max() - toPageEnd) return nullptr;
            readOffset += toPageEnd;
        }
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
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

int GetenvNp_nid_no_patch(const char* name, char* buffer, std::size_t size) {
    if (!name || !buffer)
        throw std::invalid_argument("getenv_np requires a name and a buffer");
    std::string nameCopy;
    if (!ReadGuestCString(name, nameCopy))
        throw std::invalid_argument("getenv_np requires a readable name");
    std::string moduleValue;
    const char* value = LookupModuleEnv(nameCopy, moduleValue) ? moduleValue.c_str() : getenv_nid_postfix(nameCopy.c_str());
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

int APS5_VABI getenv_np(const char* name, char* buffer, std::size_t size) {
    return GetenvNp_nid_no_patch(name, buffer, size);
}

APS5_EXPORT("dt8ermYp-Wc", scePosixForWebKit_gethostbyname_r);

void* APS5_VABI mmap_np(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset, const char* name) {
    if ((flags & 0x1001) == 0x1000) {
        if ((flags & 0x10) != 0 && address == nullptr) {
            *__error_nid_postfix() = PosixEinval;
            return reinterpret_cast<void*>(-1);
        }
        const unsigned int align = static_cast<unsigned int>(flags) >> 24;
        if ((flags & 0x10) == 0 && align >= 2 && align <= 13) {
            *__error_nid_postfix() = PosixEinval;
            return reinterpret_cast<void*>(-1);
        }
        NotImplemented_nid_no_patch(__func__);
    }
    std::string resolvedName;
    if (name != nullptr) {
        if (!ReadGuestCString(name, resolvedName)) {
            throw std::invalid_argument("mmap_np: unreadable name");
        }
    } else if ((flags & 1) != 0) {
        resolvedName = "SharedMemory";
    } else if (descriptor >= -1) {
        resolvedName = "Unknown";
    } else {
        static constexpr const char* kVmTags[] = {
            "Unknown", "FastMalloc", "IsoHeap", "JITCode", "Gigacage", "GraphicsSurface", "OSAllocator", "InvalidVMTag",
        };
        const std::uint32_t tag = static_cast<std::uint32_t>(descriptor) ^ 0xdb609b7au;
        descriptor = -1;
        resolvedName = kVmTags[tag < 7 ? tag : 7];
    }
    void* mapped = mmap_nid_postfix(address ? address : reinterpret_cast<void*>(std::uintptr_t{0x1000000000ULL}),
                                    length, protection, flags, descriptor, offset);
    if (mapped == reinterpret_cast<void*>(-1)) {
        if (*__error_nid_postfix() == PosixEnomem) {
            sceKernelDebugRaiseException(static_cast<int>(0xa0020405u), 0);
            *__error_nid_postfix() = PosixEnomem;
        }
        return mapped;
    }
    SetVirtualRangeNamePrefixed(mapped, length, resolvedName);
    return mapped;
}

int APS5_VABI msetname(void* address, std::size_t length, const char* name) {
    if (name == nullptr) return 0;
    std::string raw;
    if (!ReadGuestCString(name, raw)) {
        throw std::invalid_argument("msetname: unreadable name");
    }
    SetVirtualRangeNamePrefixed(address, length, raw);
    return 0;
}

int APS5_VABI pthread_setschedparam_np(Pthread thread, int policy, const KernelSchedParam* param) {
    if (policy != 2) return pthread_setschedparam_nid_postfix(thread, policy, param);
    KernelSchedParam requested{};
    if (!GuestCopyIn_nid_no_patch(&requested, param, sizeof(requested))) return PosixEfault;
    return pthread_setprio_nid_postfix(thread, requested.sched_priority);
}

int APS5_VABI setenv_np(const char* name, const char* value, int overwrite) {
    if (name == nullptr || value == nullptr) {
        *__error_nid_postfix() = PosixEinval;
        return -1;
    }
    std::string nameCopy;
    std::string valueCopy;
    if (!ReadGuestCString(name, nameCopy) || !ReadGuestCString(value, valueCopy)) {
        throw std::invalid_argument("setenv_np: unreadable name or value");
    }
    std::lock_guard lock(ModuleEnvMutex());
    for (auto& entry : ModuleEnvTable()) {
        if (entry.used && entry.name == nameCopy) {
            if (overwrite != 0) entry.value = std::move(valueCopy);
            return 0;
        }
    }
    for (auto& entry : ModuleEnvTable()) {
        if (!entry.used) {
            entry.used = true;
            entry.name = std::move(nameCopy);
            entry.value = std::move(valueCopy);
            return 0;
        }
    }
    *__error_nid_postfix() = PosixEnomem;
    return -1;
}

APS5_EXPORT("9Vqxt-qBPB4", scePosixForWebKit_sysinfo);
int APS5_VABI scePosixForWebKit_sysinfo(PosixForWebKitSysinfo* info) {
    if (info == nullptr) {
        *__error_nid_postfix() = PosixEfault;
        return -1;
    }
    const std::uint64_t total = TotalMemorySizeAssumption();
    const std::uint32_t one = 1;
    const int savedErrno = errno;
    const bool wroteTotal = GuestCopyOut_nid_no_patch(info, &total, sizeof(total));
    const bool wroteOne = wroteTotal && GuestCopyOut_nid_no_patch(reinterpret_cast<char*>(info) + offsetof(PosixForWebKitSysinfo, one), &one, sizeof(one));
    errno = savedErrno;
    if (!wroteTotal || !wroteOne) {
        throw std::invalid_argument("scePosixForWebKit_sysinfo: unwritable output");
    }
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
