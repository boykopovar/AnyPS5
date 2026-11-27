#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceContentListController/include/ContentListController.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceUsbStorage/UsbStorage.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" {

APS5_DUMMY_FUN

}

namespace {

constexpr int SceClcErrorNoCallback = static_cast<int>(0x81a30001u);
constexpr int SceClcErrorInvalidArg = static_cast<int>(0x81a30003u);
constexpr int SceClcErrorDeviceNotFound = static_cast<int>(0x81a30004u);
constexpr int SceClcErrorListNotAllocated = static_cast<int>(0x81a30006u);
constexpr int SceClcErrorUnsupportedFormat = static_cast<int>(0x81a30007u);

constexpr std::size_t MaxDevices = 8;
constexpr std::size_t MaxEntries = 4000;
constexpr std::size_t EntryNameSize = 256;
constexpr std::size_t PathBufferSize = 0x200;
constexpr std::uint8_t DirentTypeDir = 4;
constexpr std::uint8_t DirentTypeReg = 8;
constexpr int MaxMp4Depth = 32;

struct GuardedDeviceBuffer {
    std::uint8_t* mapping = nullptr;
    std::size_t pageSize = 0;
    std::uint32_t* ptr = nullptr;

    GuardedDeviceBuffer() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        if (pageSize >= MaxDevices * sizeof(std::uint32_t)) {
            mapping = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
            if (mapping != nullptr) {
                DWORD prev = 0;
                if (VirtualProtect(mapping, pageSize, PAGE_READWRITE, &prev) != 0) {
                    ptr = reinterpret_cast<std::uint32_t*>(mapping + pageSize - MaxDevices * sizeof(std::uint32_t));
                    std::memset(ptr, 0, MaxDevices * sizeof(std::uint32_t));
                } else {
                    VirtualFree(mapping, 0, MEM_RELEASE);
                    mapping = nullptr;
                }
            }
        }
#else
        const long sz = sysconf(_SC_PAGESIZE);
        if (sz > 0) {
            pageSize = static_cast<std::size_t>(sz);
            if (pageSize >= MaxDevices * sizeof(std::uint32_t)) {
                void* raw = mmap(nullptr, pageSize * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (raw != MAP_FAILED) {
                    mapping = static_cast<std::uint8_t*>(raw);
                    if (mprotect(mapping, pageSize, PROT_READ | PROT_WRITE) == 0) {
                        ptr = reinterpret_cast<std::uint32_t*>(mapping + pageSize - MaxDevices * sizeof(std::uint32_t));
                        std::memset(ptr, 0, MaxDevices * sizeof(std::uint32_t));
                    } else {
                        munmap(mapping, pageSize * 2);
                        mapping = nullptr;
                    }
                }
            }
        }
#endif
    }

    ~GuardedDeviceBuffer() {
        if (mapping != nullptr) {
#ifdef _WIN32
            VirtualFree(mapping, 0, MEM_RELEASE);
#else
            munmap(mapping, pageSize * 2);
#endif
        }
    }

    GuardedDeviceBuffer(const GuardedDeviceBuffer&) = delete;
    GuardedDeviceBuffer& operator=(const GuardedDeviceBuffer&) = delete;
};

struct ControllerState {
    SceCLCUsbStorageCallback callbacks{};
    std::mutex mutex;
    std::uint64_t providerGeneration = 0;
    std::int32_t deviceCount = 0;
    std::uint32_t deviceIds[MaxDevices]{};
    std::int32_t fileDeviceId = 0;
    char fileDir[PathBufferSize]{};
    std::int32_t fileFolderCount = 0;
    std::int32_t fileFileCount = 0;
    char* fileEntries = nullptr;
    std::int32_t trackDeviceId = 0;
    char trackDir[PathBufferSize]{};
    std::int32_t trackFolderCount = 0;
    std::int32_t trackFileCount = 0;
    char* trackEntries = nullptr;
    std::uint8_t playbackResults[MaxEntries]{};
    std::int32_t shuffleOrder[MaxEntries]{};
    std::uint8_t shuffleEnabled = 0;
    std::uint64_t rngSeed = 0x123456789abcdef0ULL;
    SceCLCEventCallback eventCallback = nullptr;
    void* eventUserData = nullptr;
    char excludePattern[256]{};

    ControllerState() {
        fileEntries = new char[MaxEntries * EntryNameSize]();
        trackEntries = new char[MaxEntries * EntryNameSize]();
        std::strcpy(excludePattern, "^\\..*");
        KernelTimeval tv{};
        if (sceKernelGettimeofday(&tv) == 0) {
            rngSeed = static_cast<std::uint64_t>(tv.tv_sec) ^ (static_cast<std::uint64_t>(tv.tv_usec) << 16);
        }
    }
};

SceCLCUsbStorageCallback g_callbackShadow{};

ControllerState& State() {
    static ControllerState* state = new ControllerState();
    return *state;
}

template <typename T>
bool SafeCopyIn(T* dst, const void* src, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, bytes);
    errno = savedErrno;
    return ok;
}

template <typename T>
bool SafeCopyOut(void* dst, const T* src, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, bytes);
    errno = savedErrno;
    return ok;
}

bool SafeWritable(void* dst, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(dst, bytes);
    errno = savedErrno;
    return ok;
}

bool SafeWritableArray(void* dst, std::size_t count, std::size_t elemSize) {
    if (dst == nullptr || elemSize == 0) return false;
    if (count > (std::numeric_limits<std::size_t>::max)() / elemSize) return false;
    const std::size_t bytes = count * elemSize;
    const auto addr = reinterpret_cast<std::uintptr_t>(dst);
    if (addr > (std::numeric_limits<std::uintptr_t>::max)() - bytes) return false;
    return SafeWritable(dst, bytes);
}

bool CopyGuestCString(const char* src, char* dst, std::size_t maxBytes) {
    if (src == nullptr || maxBytes == 0) return false;
    std::memset(dst, 0, maxBytes);
    for (std::size_t i = 0; i < maxBytes; ++i) {
        char ch = 0;
        if (!SafeCopyIn(&ch, src + i, 1)) return false;
        dst[i] = ch;
        if (ch == '\0') return true;
    }
    std::memset(dst, 0, maxBytes);
    return false;
}

bool IsDeviceKnown(const ControllerState& state, std::uint32_t deviceId) {
    for (std::int32_t i = 0; i < state.deviceCount && i < static_cast<std::int32_t>(MaxDevices); ++i) {
        if (state.deviceIds[i] == deviceId) return true;
    }
    return false;
}

bool IsSafeGuestPath(std::string_view path) {
    if (path.empty() || path.front() != '/') return false;
    if (path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos) return false;
    std::size_t pos = 0;
    while (pos < path.size()) {
        const std::size_t slash = path.find('/', pos);
        const std::string_view part = slash == std::string_view::npos ? path.substr(pos) : path.substr(pos, slash - pos);
        if (part == "." || part == "..") return false;
        if (slash == std::string_view::npos) break;
        pos = slash + 1;
    }
    return true;
}

std::size_t BoundedStrlen(const char* s, std::size_t maxLen) {
    if (s == nullptr || maxLen == 0) return 0;
    const void* nul = std::memchr(s, '\0', maxLen);
    return nul != nullptr ? static_cast<std::size_t>(static_cast<const char*>(nul) - s) : maxLen;
}

bool CombineEntryPath(const char* dir, const char* entry, char* out, std::size_t outSize) {
    const std::size_t dirLen = BoundedStrlen(dir, PathBufferSize);
    const std::size_t entryLen = BoundedStrlen(entry, EntryNameSize);
    if (dirLen + entryLen >= outSize) return false;
    std::memcpy(out, dir, dirLen);
    std::memcpy(out + dirLen, entry, entryLen);
    out[dirLen + entryLen] = '\0';
    return true;
}

void ClearTrackListLocked(ControllerState& state) {
    state.trackDeviceId = 0;
    std::memset(state.trackDir, 0, sizeof(state.trackDir));
    state.trackFolderCount = 0;
    state.trackFileCount = 0;
    if (state.trackEntries != nullptr) {
        std::memset(state.trackEntries, 0, MaxEntries * EntryNameSize);
    }
    std::memset(state.playbackResults, 0, sizeof(state.playbackResults));
    std::memset(state.shuffleOrder, 0, sizeof(state.shuffleOrder));
}

void APS5_VABI UsbStorageTrampoline(const UsbStorageEvent* event, void* userData) {
    auto* state = static_cast<ControllerState*>(userData);
    if (state == nullptr || event == nullptr) return;
    UsbStorageEvent localEvent{};
    if (!SafeCopyIn(&localEvent, event, sizeof(localEvent))) return;
    if (localEvent.event != 1 && localEvent.event != 2) return;
    SceCLCEventCallback cb = nullptr;
    void* cbUserData = nullptr;
    {
        std::lock_guard lock(state->mutex);
        cb = state->eventCallback;
        cbUserData = state->eventUserData;
    }
    if (cb != nullptr) {
        SceCLCEvent clcEvent{localEvent.event, localEvent.device};
        cb(&clcEvent, cbUserData);
    }
}

std::uint32_t NextShuffleIndex(std::uint64_t& seed, std::int32_t count) {
    seed = seed * 0x41c64e6dULL + 0x3039ULL;
    return static_cast<std::uint32_t>((seed >> 8) & 0xffffu) % static_cast<std::uint32_t>(count);
}

void CopyFixedString(char* dst, std::size_t dstSize, const char* src, std::size_t srcMax) {
    std::memset(dst, 0, dstSize);
    if (src == nullptr || dstSize == 0) return;
    std::size_t len = 0;
    while (len < srcMax && len + 1 < dstSize && src[len] != '\0') {
        dst[len] = src[len];
        ++len;
    }
    dst[len] = '\0';
}

void CopyFixedUtf8String(char* dst, std::size_t dstSize, const char* src, std::size_t srcMax) {
    std::memset(dst, 0, dstSize);
    if (src == nullptr || dstSize == 0) return;
    std::size_t len = 0;
    while (len < srcMax && len + 1 < dstSize && src[len] != '\0') {
        ++len;
    }
    if (len < srcMax && src[len] != '\0') {
        while (len > 0 && (static_cast<unsigned char>(src[len]) & 0xc0u) == 0x80u) {
            --len;
        }
    }
    std::memcpy(dst, src, len);
    dst[len] = '\0';
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

int DetectAudioFormat(std::string_view path) {
    const std::size_t dot = path.rfind('.');
    if (dot == std::string_view::npos) return 0;
    const std::string_view ext = path.substr(dot);
    if (EqualsIgnoreCase(ext, ".mp3")) return 1;
    if (EqualsIgnoreCase(ext, ".m4a") || EqualsIgnoreCase(ext, ".3gp")) return 2;
    if (EqualsIgnoreCase(ext, ".flac")) return 3;
    return 0;
}

struct ScannedEntry {
    char name[EntryNameSize]{};
    bool isFile = false;
};

bool MatchesExcludePattern(const char* pattern, std::string_view name) {
    if (pattern[0] == '\0') return true;
    try {
        std::regex re(pattern, std::regex::ECMAScript);
        return std::regex_match(name.begin(), name.end(), re);
    } catch (const std::regex_error&) {
        return false;
    }
}

void AppendDirentsFromBuffer(
    const char* buf,
    std::size_t bytes,
    std::size_t dirPathLen,
    const char* excludePattern,
    std::vector<ScannedEntry>& entries) {
    std::size_t offset = 0;
    while (offset + 8 <= bytes && entries.size() < MaxEntries) {
        std::uint16_t reclen = 0;
        std::memcpy(&reclen, buf + offset + 4, sizeof(reclen));
        if (reclen < 8 || offset + reclen > bytes) break;
        const std::uint8_t type = static_cast<std::uint8_t>(buf[offset + 6]);
        const std::uint8_t namlen = static_cast<std::uint8_t>(buf[offset + 7]);
        const std::size_t maxName = std::min<std::size_t>(namlen, reclen - 8);
        std::string_view name(buf + offset + 8, maxName);
        if (const std::size_t nul = name.find('\0'); nul != std::string_view::npos) {
            name = name.substr(0, nul);
        }
        if (!name.empty() && name != "." && name != ".." && name.size() < EntryNameSize &&
            dirPathLen + name.size() < PathBufferSize) {
            if (type == DirentTypeDir) {
                if (name.front() != '.') {
                    ScannedEntry entry{};
                    CopyFixedString(entry.name, sizeof(entry.name), name.data(), name.size());
                    entry.isFile = false;
                    entries.push_back(entry);
                }
            } else if (type == DirentTypeReg || type == 0) {
                if (DetectAudioFormat(name) != 0 && !MatchesExcludePattern(excludePattern, name)) {
                    ScannedEntry entry{};
                    CopyFixedString(entry.name, sizeof(entry.name), name.data(), name.size());
                    entry.isFile = true;
                    entries.push_back(entry);
                }
            }
        }
        offset += reclen;
    }
}

void SortScannedEntries(std::vector<ScannedEntry>& entries) {
    if (entries.size() < 2) return;
    for (std::size_t i = 0; i + 1 < entries.size(); ++i) {
        for (std::size_t j = 0; j + 1 + i < entries.size(); ++j) {
            const ScannedEntry& a = entries[j];
            const ScannedEntry& b = entries[j + 1];
            bool swapNeeded = false;
            if (a.isFile != b.isFile) {
                swapNeeded = a.isFile && !b.isFile;
            } else {
                swapNeeded = std::strncmp(a.name, b.name, EntryNameSize) > 0;
            }
            if (swapNeeded) {
                std::swap(entries[j], entries[j + 1]);
            }
        }
    }
}

std::uint16_t ReadBe16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

std::uint32_t ReadBe24(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 16) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           static_cast<std::uint32_t>(p[2]);
}

std::uint32_t ReadBe32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

std::uint64_t ReadBe64(const std::uint8_t* p) {
    return (static_cast<std::uint64_t>(ReadBe32(p)) << 32) | ReadBe32(p + 4);
}

std::uint32_t ReadLe32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint32_t ReadSynchsafe32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0] & 0x7f) << 21) |
           (static_cast<std::uint32_t>(p[1] & 0x7f) << 14) |
           (static_cast<std::uint32_t>(p[2] & 0x7f) << 7) |
           static_cast<std::uint32_t>(p[3] & 0x7f);
}

bool ReadExactAt(int fd, std::int64_t offset, void* dst, std::size_t size) {
    if (sceKernelLseek(fd, offset, 0) < 0) return false;
    std::size_t total = 0;
    auto* out = static_cast<std::uint8_t*>(dst);
    while (total < size) {
        const std::int64_t n = sceKernelRead(fd, out + total, size - total);
        if (n <= 0) return false;
        total += static_cast<std::size_t>(n);
    }
    return true;
}

std::string DecodeId3Text(const std::uint8_t* data, std::size_t size) {
    if (size == 0) return {};
    const std::uint8_t encoding = data[0];
    const std::uint8_t* p = data + 1;
    std::size_t len = size - 1;
    std::string result;
    if (encoding == 0 || encoding == 3) {
        while (len > 0 && p[len - 1] == 0) --len;
        result.assign(reinterpret_cast<const char*>(p), len);
        return result;
    }
    if (encoding == 1 || encoding == 2) {
        bool littleEndian = false;
        if (encoding == 1 && len >= 2) {
            if (p[0] == 0xff && p[1] == 0xfe) {
                littleEndian = true;
                p += 2;
                len -= 2;
            } else if (p[0] == 0xfe && p[1] == 0xff) {
                p += 2;
                len -= 2;
            }
        }
        auto readUnit = [&](std::size_t offset) -> std::uint16_t {
            return littleEndian
                ? static_cast<std::uint16_t>(p[offset] | (p[offset + 1] << 8))
                : static_cast<std::uint16_t>((p[offset] << 8) | p[offset + 1]);
        };
        for (std::size_t i = 0; i + 1 < len; i += 2) {
            const std::uint16_t ch = readUnit(i);
            if (ch == 0) break;
            std::uint32_t cp = ch;
            if (ch >= 0xd800u && ch <= 0xdbffu && i + 3 < len) {
                const std::uint16_t low = readUnit(i + 2);
                if (low >= 0xdc00u && low <= 0xdfffu) {
                    cp = 0x10000u + ((static_cast<std::uint32_t>(ch - 0xd800u) << 10) |
                                    static_cast<std::uint32_t>(low - 0xdc00u));
                    i += 2;
                } else {
                    cp = 0xfffdu;
                }
            } else if (ch >= 0xd800u && ch <= 0xdfffu) {
                cp = 0xfffdu;
            }
            if (cp < 0x80u) {
                result.push_back(static_cast<char>(cp));
            } else if (cp < 0x800u) {
                result.push_back(static_cast<char>(0xc0u | (cp >> 6)));
                result.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
            } else if (cp < 0x10000u) {
                result.push_back(static_cast<char>(0xe0u | (cp >> 12)));
                result.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3fu)));
                result.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
            } else {
                result.push_back(static_cast<char>(0xf0u | (cp >> 18)));
                result.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3fu)));
                result.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3fu)));
                result.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
            }
        }
        return result;
    }
    while (len > 0 && p[len - 1] == 0) --len;
    result.assign(reinterpret_cast<const char*>(p), len);
    return result;
}

void SetFieldIfEmpty(char* dst, std::size_t dstSize, std::string_view value) {
    if (dst[0] != '\0' || value.empty()) return;
    CopyFixedUtf8String(dst, dstSize, value.data(), value.size());
}

void SetFieldOverwrite(char* dst, std::size_t dstSize, std::string_view value) {
    if (value.empty()) return;
    CopyFixedUtf8String(dst, dstSize, value.data(), value.size());
}

std::size_t SkipNullTerminatedString(const std::uint8_t* p, std::size_t size, std::uint8_t encoding) {
    if (encoding == 1 || encoding == 2) {
        std::size_t pos = 0;
        while (pos + 1 < size) {
            if (p[pos] == 0 && p[pos + 1] == 0) return pos + 2;
            pos += 2;
        }
        return size;
    }
    std::size_t pos = 0;
    while (pos < size) {
        if (p[pos] == 0) return pos + 1;
        ++pos;
    }
    return size;
}

std::uint64_t ParseId3v2(int fd, std::int64_t fileSize, const char* guestPath, SceCLCFileInfo& info) {
    std::uint8_t hdr[10]{};
    if (fileSize < 10 || !ReadExactAt(fd, 0, hdr, 10)) return 0;
    if (hdr[0] != 'I' || hdr[1] != 'D' || hdr[2] != '3') return 0;
    const std::uint8_t major = hdr[3];
    const std::uint8_t flags = hdr[5];
    const std::uint32_t tagSize = ReadSynchsafe32(hdr + 6);
    const std::uint64_t tagEnd = std::min<std::uint64_t>(static_cast<std::uint64_t>(fileSize), 10ULL + tagSize);

    std::size_t pos = 10;
    if ((flags & 0x40) != 0 && pos + 4 <= tagEnd) {
        std::uint8_t ext[4]{};
        if (ReadExactAt(fd, static_cast<std::int64_t>(pos), ext, 4)) {
            const std::uint32_t extSize = major >= 4 ? ReadSynchsafe32(ext) : ReadBe32(ext);
            const std::uint64_t extBytes = major >= 4 ? extSize : (4ULL + extSize);
            if (extBytes > tagEnd - pos) return tagEnd;
            pos += static_cast<std::size_t>(extBytes);
        }
    }

    while (pos < tagEnd) {
        if (major == 2) {
            if (tagEnd - pos < 6) break;
            std::uint8_t fhdr[6]{};
            if (!ReadExactAt(fd, static_cast<std::int64_t>(pos), fhdr, 6)) break;
            if (fhdr[0] == 0) break;
            const std::uint32_t fsize = ReadBe24(fhdr + 3);
            pos += 6;
            if (fsize == 0 || fsize > tagEnd - pos) break;
            std::string_view fid(reinterpret_cast<const char*>(fhdr), 3);
            if (fid == "TT2" || fid == "TAL" || fid == "TP1") {
                std::vector<std::uint8_t> payload(fsize);
                if (ReadExactAt(fd, static_cast<std::int64_t>(pos), payload.data(), fsize)) {
                    const std::string text = DecodeId3Text(payload.data(), payload.size());
                    if (fid == "TT2") SetFieldOverwrite(info.title, sizeof(info.title), text);
                    else if (fid == "TAL") SetFieldOverwrite(info.album, sizeof(info.album), text);
                    else if (fid == "TP1") SetFieldOverwrite(info.artist, sizeof(info.artist), text);
                }
            } else if (fid == "PIC" && fsize > 6) {
                const std::size_t readLen = std::min<std::size_t>(fsize, 512);
                std::vector<std::uint8_t> payload(readLen);
                if (ReadExactAt(fd, static_cast<std::int64_t>(pos), payload.data(), readLen)) {
                    const std::uint8_t enc = payload[0];
                    std::size_t off = 5;
                    off += SkipNullTerminatedString(payload.data() + off, payload.size() - off, enc);
                    if (off < fsize) {
                        info.coverArtOffset = pos + off;
                        info.coverArtSize = fsize - off;
                        CopyFixedString(info.coverArtPath, sizeof(info.coverArtPath), guestPath, std::strlen(guestPath));
                    }
                }
            }
            pos += fsize;
        } else {
            if (tagEnd - pos < 10) break;
            std::uint8_t fhdr[10]{};
            if (!ReadExactAt(fd, static_cast<std::int64_t>(pos), fhdr, 10)) break;
            if (fhdr[0] == 0) break;
            const std::uint32_t fsize = major >= 4 ? ReadSynchsafe32(fhdr + 4) : ReadBe32(fhdr + 4);
            pos += 10;
            if (fsize == 0 || fsize > tagEnd - pos) break;
            std::string_view fid(reinterpret_cast<const char*>(fhdr), 4);
            if (fid == "TIT2" || fid == "TALB" || fid == "TPE1" || fid == "TLEN") {
                std::vector<std::uint8_t> payload(fsize);
                if (ReadExactAt(fd, static_cast<std::int64_t>(pos), payload.data(), fsize)) {
                    const std::string text = DecodeId3Text(payload.data(), payload.size());
                    if (fid == "TIT2") SetFieldOverwrite(info.title, sizeof(info.title), text);
                    else if (fid == "TALB") SetFieldOverwrite(info.album, sizeof(info.album), text);
                    else if (fid == "TPE1") SetFieldOverwrite(info.artist, sizeof(info.artist), text);
                    else if (fid == "TLEN" && !text.empty()) {
                        const std::uint64_t ms = std::strtoull(text.c_str(), nullptr, 10);
                        if (ms > 0) {
                            const std::uint64_t secs = ms / 1000ULL;
                            info.duration = secs == 0 ? 1ULL : secs;
                        }
                    }
                }
            } else if (fid == "APIC" && fsize > 4) {
                const std::size_t readLen = std::min<std::size_t>(fsize, 512);
                std::vector<std::uint8_t> payload(readLen);
                if (ReadExactAt(fd, static_cast<std::int64_t>(pos), payload.data(), readLen)) {
                    const std::uint8_t enc = payload[0];
                    std::size_t off = 1;
                    off += SkipNullTerminatedString(payload.data() + off, payload.size() - off, 0);
                    if (off < payload.size()) {
                        off += 1;
                        off += SkipNullTerminatedString(payload.data() + off, payload.size() - off, enc);
                        if (off < fsize) {
                            info.coverArtOffset = pos + off;
                            info.coverArtSize = fsize - off;
                            CopyFixedString(info.coverArtPath, sizeof(info.coverArtPath), guestPath, std::strlen(guestPath));
                        }
                    }
                }
            }
            pos += fsize;
        }
    }
    return tagEnd;
}

std::string TrimId3v1Field(const std::uint8_t* p, std::size_t len) {
    while (len > 0 && (p[len - 1] == 0 || p[len - 1] == ' ')) --len;
    return std::string(reinterpret_cast<const char*>(p), len);
}

void ParseMp3Metadata(int fd, std::int64_t fileSize, const char* guestPath, SceCLCFileInfo& info) {
    const std::uint64_t audioStart = ParseId3v2(fd, fileSize, guestPath, info);
    bool hasId3v1 = false;
    if (fileSize >= 128) {
        std::uint8_t tail[128]{};
        if (ReadExactAt(fd, fileSize - 128, tail, 128) && tail[0] == 'T' && tail[1] == 'A' && tail[2] == 'G') {
            hasId3v1 = true;
            SetFieldIfEmpty(info.title, sizeof(info.title), TrimId3v1Field(tail + 3, 30));
            SetFieldIfEmpty(info.artist, sizeof(info.artist), TrimId3v1Field(tail + 33, 30));
            SetFieldIfEmpty(info.album, sizeof(info.album), TrimId3v1Field(tail + 63, 30));
        }
    }

    if (info.duration != 0) return;

    const std::uint64_t audioEnd = hasId3v1 ? static_cast<std::uint64_t>(fileSize - 128) : static_cast<std::uint64_t>(fileSize);
    if (audioStart >= audioEnd) return;

    const std::size_t scanSize = static_cast<std::size_t>(std::min<std::uint64_t>(audioEnd - audioStart, 16384));
    std::vector<std::uint8_t> buf(scanSize);
    if (!ReadExactAt(fd, static_cast<std::int64_t>(audioStart), buf.data(), scanSize)) return;

    static constexpr int Mpeg1Layer3Bitrates[16] = {
        0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
    };
    static constexpr int Mpeg2Layer3Bitrates[16] = {
        0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0
    };
    static constexpr int Mpeg1SampleRates[4] = {44100, 48000, 32000, 0};
    static constexpr int Mpeg2SampleRates[4] = {22050, 24000, 16000, 0};
    static constexpr int Mpeg25SampleRates[4] = {11025, 12000, 8000, 0};

    for (std::size_t i = 0; i + 4 <= buf.size(); ++i) {
        if (buf[i] != 0xff || (buf[i + 1] & 0xe0) != 0xe0) continue;
        const std::uint8_t versionBits = (buf[i + 1] >> 3) & 0x03;
        const std::uint8_t layerBits = (buf[i + 1] >> 1) & 0x03;
        const std::uint8_t bitrateIdx = (buf[i + 2] >> 4) & 0x0f;
        const std::uint8_t srIdx = (buf[i + 2] >> 2) & 0x03;
        if (versionBits == 1 || layerBits != 1 || bitrateIdx == 0 || bitrateIdx == 15 || srIdx == 3) continue;

        const int bitrateKbps = (versionBits == 3) ? Mpeg1Layer3Bitrates[bitrateIdx] : Mpeg2Layer3Bitrates[bitrateIdx];
        const int sampleRate = (versionBits == 3) ? Mpeg1SampleRates[srIdx]
                             : (versionBits == 2) ? Mpeg2SampleRates[srIdx]
                             : Mpeg25SampleRates[srIdx];
        if (bitrateKbps <= 0 || sampleRate <= 0) continue;

        const std::size_t channelMode = (buf[i + 3] >> 6) & 0x03;
        const std::size_t xingOffset = i + 4 + ((versionBits == 3) ? (channelMode == 3 ? 17 : 32) : (channelMode == 3 ? 9 : 17));
        if (xingOffset + 12 <= buf.size() &&
            ((std::memcmp(buf.data() + xingOffset, "Xing", 4) == 0) ||
             (std::memcmp(buf.data() + xingOffset, "Info", 4) == 0))) {
            const std::uint32_t xflags = ReadBe32(buf.data() + xingOffset + 4);
            if ((xflags & 0x01) != 0) {
                const std::uint32_t frames = ReadBe32(buf.data() + xingOffset + 8);
                const std::uint64_t samplesPerFrame = (versionBits == 3) ? 1152ULL : 576ULL;
                const std::uint64_t secs = (static_cast<std::uint64_t>(frames) * samplesPerFrame) / static_cast<std::uint64_t>(sampleRate);
                info.duration = secs == 0 ? 1ULL : secs;
                return;
            }
        }

        const std::uint64_t audioBytes = audioEnd - (audioStart + i);
        const std::uint64_t secs = (audioBytes * 8ULL) / (static_cast<std::uint64_t>(bitrateKbps) * 1000ULL);
        info.duration = secs == 0 ? 1ULL : secs;
        return;
    }
}

void ParseMp4IlstBox(int fd, std::uint64_t start, std::uint64_t end, const char* guestPath, SceCLCFileInfo& info) {
    std::uint64_t pos = start;
    while (pos <= end && end - pos >= 8) {
        std::uint8_t bhdr[8]{};
        if (!ReadExactAt(fd, static_cast<std::int64_t>(pos), bhdr, 8)) break;
        std::uint64_t boxSize = ReadBe32(bhdr);
        std::uint64_t hdrSize = 8;
        if (boxSize == 1 && end - pos >= 16) {
            std::uint8_t ext[8]{};
            if (!ReadExactAt(fd, static_cast<std::int64_t>(pos + 8), ext, 8)) break;
            boxSize = ReadBe64(ext);
            hdrSize = 16;
        } else if (boxSize == 0) {
            boxSize = end - pos;
        }
        if (boxSize < hdrSize || boxSize > end - pos) break;

        std::uint64_t inner = pos + hdrSize;
        const std::uint64_t boxEnd = pos + boxSize;
        while (inner <= boxEnd && boxEnd - inner >= 8) {
            std::uint8_t dhdr[8]{};
            if (!ReadExactAt(fd, static_cast<std::int64_t>(inner), dhdr, 8)) break;
            const std::uint64_t dsize = ReadBe32(dhdr);
            if (dsize < 8 || dsize > boxEnd - inner) break;
            if (std::memcmp(dhdr + 4, "data", 4) == 0 && dsize > 16) {
                const std::uint64_t payloadOff = inner + 16;
                const std::uint64_t payloadSize = dsize - 16;
                if (bhdr[4] == 0xa9 && std::memcmp(bhdr + 5, "nam", 3) == 0) {
                    const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(payloadSize, sizeof(info.title) - 1));
                    std::string buf(n, '\0');
                    if (ReadExactAt(fd, static_cast<std::int64_t>(payloadOff), buf.data(), n)) {
                        SetFieldOverwrite(info.title, sizeof(info.title), buf);
                    }
                } else if (bhdr[4] == 0xa9 && std::memcmp(bhdr + 5, "alb", 3) == 0) {
                    const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(payloadSize, sizeof(info.album) - 1));
                    std::string buf(n, '\0');
                    if (ReadExactAt(fd, static_cast<std::int64_t>(payloadOff), buf.data(), n)) {
                        SetFieldOverwrite(info.album, sizeof(info.album), buf);
                    }
                } else if (bhdr[4] == 0xa9 && std::memcmp(bhdr + 5, "ART", 3) == 0) {
                    const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(payloadSize, sizeof(info.artist) - 1));
                    std::string buf(n, '\0');
                    if (ReadExactAt(fd, static_cast<std::int64_t>(payloadOff), buf.data(), n)) {
                        SetFieldOverwrite(info.artist, sizeof(info.artist), buf);
                    }
                } else if (std::memcmp(bhdr + 4, "covr", 4) == 0) {
                    info.coverArtOffset = payloadOff;
                    info.coverArtSize = payloadSize;
                    CopyFixedString(info.coverArtPath, sizeof(info.coverArtPath), guestPath, std::strlen(guestPath));
                }
            }
            inner += dsize;
        }
        pos += boxSize;
    }
}

void ParseMp4Container(int fd, std::uint64_t start, std::uint64_t end, const char* guestPath, SceCLCFileInfo& info, int depth = 0) {
    if (depth >= MaxMp4Depth) return;
    std::uint64_t pos = start;
    while (pos <= end && end - pos >= 8) {
        std::uint8_t hdr[8]{};
        if (!ReadExactAt(fd, static_cast<std::int64_t>(pos), hdr, 8)) break;
        std::uint64_t boxSize = ReadBe32(hdr);
        std::uint64_t hdrSize = 8;
        if (boxSize == 1 && end - pos >= 16) {
            std::uint8_t ext[8]{};
            if (!ReadExactAt(fd, static_cast<std::int64_t>(pos + 8), ext, 8)) break;
            boxSize = ReadBe64(ext);
            hdrSize = 16;
        } else if (boxSize == 0) {
            boxSize = end - pos;
        }
        if (boxSize < hdrSize || boxSize > end - pos) break;

        const std::uint64_t bodyStart = pos + hdrSize;
        const std::uint64_t boxEnd = pos + boxSize;
        if (std::memcmp(hdr + 4, "moov", 4) == 0 || std::memcmp(hdr + 4, "udta", 4) == 0) {
            ParseMp4Container(fd, bodyStart, boxEnd, guestPath, info, depth + 1);
        } else if (std::memcmp(hdr + 4, "meta", 4) == 0 && boxEnd - bodyStart >= 4) {
            ParseMp4Container(fd, bodyStart + 4, boxEnd, guestPath, info, depth + 1);
        } else if (std::memcmp(hdr + 4, "ilst", 4) == 0) {
            ParseMp4IlstBox(fd, bodyStart, boxEnd, guestPath, info);
        } else if (std::memcmp(hdr + 4, "mvhd", 4) == 0 && boxEnd - bodyStart >= 20) {
            std::uint8_t mvhd[32]{};
            const std::size_t toRead = static_cast<std::size_t>(std::min<std::uint64_t>(boxEnd - bodyStart, sizeof(mvhd)));
            if (ReadExactAt(fd, static_cast<std::int64_t>(bodyStart), mvhd, toRead)) {
                const std::uint8_t version = mvhd[0];
                if (version == 0 && toRead >= 20) {
                    const std::uint32_t timescale = ReadBe32(mvhd + 12);
                    const std::uint32_t duration = ReadBe32(mvhd + 16);
                    if (timescale > 0) {
                        const std::uint64_t secs = static_cast<std::uint64_t>(duration) / timescale;
                        info.duration = secs == 0 ? 1ULL : secs;
                    }
                } else if (version == 1 && toRead >= 32) {
                    const std::uint32_t timescale = ReadBe32(mvhd + 20);
                    const std::uint64_t duration = ReadBe64(mvhd + 24);
                    if (timescale > 0) {
                        const std::uint64_t secs = duration / timescale;
                        info.duration = secs == 0 ? 1ULL : secs;
                    }
                }
            }
        }
        pos += boxSize;
    }
}

void ParseFlacMetadata(int fd, std::int64_t fileSize, const char* guestPath, SceCLCFileInfo& info) {
    const std::uint64_t fileBytes = fileSize > 0 ? static_cast<std::uint64_t>(fileSize) : 0ULL;
    std::uint64_t pos = ParseId3v2(fd, fileSize, guestPath, info);
    if (pos > fileBytes || fileBytes - pos < 4) return;
    std::uint8_t magic[4]{};
    if (!ReadExactAt(fd, static_cast<std::int64_t>(pos), magic, 4) || std::memcmp(magic, "fLaC", 4) != 0) {
        return;
    }
    pos += 4;
    while (pos <= fileBytes && fileBytes - pos >= 4) {
        std::uint8_t bhdr[4]{};
        if (!ReadExactAt(fd, static_cast<std::int64_t>(pos), bhdr, 4)) break;
        const bool isLast = (bhdr[0] & 0x80) != 0;
        const std::uint8_t blockType = bhdr[0] & 0x7f;
        const std::uint32_t blockSize = ReadBe24(bhdr + 1);
        pos += 4;
        if (blockSize > fileBytes - pos) break;

        if (blockType == 0 && blockSize >= 18) {
            std::uint8_t si[18]{};
            if (ReadExactAt(fd, static_cast<std::int64_t>(pos), si, 18)) {
                const std::uint32_t sampleRate = (static_cast<std::uint32_t>(si[10]) << 12) |
                                                 (static_cast<std::uint32_t>(si[11]) << 4) |
                                                 ((static_cast<std::uint32_t>(si[12]) >> 4) & 0x0f);
                const std::uint64_t totalSamples = (static_cast<std::uint64_t>(si[13] & 0x0f) << 32) |
                                                   ReadBe32(si + 14);
                if (sampleRate > 0) {
                    info.duration = totalSamples / sampleRate;
                }
            }
        } else if (blockType == 4 && blockSize >= 8 && blockSize <= 1024 * 1024) {
            std::vector<std::uint8_t> vc(blockSize);
            if (ReadExactAt(fd, static_cast<std::int64_t>(pos), vc.data(), blockSize)) {
                std::size_t off = 0;
                const std::size_t vendorLen = static_cast<std::size_t>(ReadLe32(vc.data() + off));
                off += 4;
                if (vendorLen <= vc.size() - off && vc.size() - off - vendorLen >= 4) {
                    off += vendorLen;
                    const std::uint32_t count = ReadLe32(vc.data() + off);
                    off += 4;
                    for (std::uint32_t i = 0; i < count && vc.size() - off >= 4; ++i) {
                        const std::size_t clen = static_cast<std::size_t>(ReadLe32(vc.data() + off));
                        off += 4;
                        if (clen > vc.size() - off) break;
                        std::string_view comment(reinterpret_cast<const char*>(vc.data() + off), clen);
                        if (const std::size_t eq = comment.find('='); eq != std::string_view::npos) {
                            const std::string_view key = comment.substr(0, eq);
                            const std::string_view val = comment.substr(eq + 1);
                            if (EqualsIgnoreCase(key, "TITLE")) SetFieldOverwrite(info.title, sizeof(info.title), val);
                            else if (EqualsIgnoreCase(key, "ALBUM")) SetFieldOverwrite(info.album, sizeof(info.album), val);
                            else if (EqualsIgnoreCase(key, "ARTIST")) SetFieldOverwrite(info.artist, sizeof(info.artist), val);
                        }
                        off += clen;
                    }
                }
            }
        } else if (blockType == 6 && blockSize >= 32) {
            std::uint8_t hdr8[8]{};
            if (ReadExactAt(fd, static_cast<std::int64_t>(pos), hdr8, 8)) {
                const std::size_t blockLen = static_cast<std::size_t>(blockSize);
                const std::size_t mimeLen = static_cast<std::size_t>(ReadBe32(hdr8 + 4));
                std::size_t off = 8;
                if (mimeLen <= blockLen - off && blockLen - off - mimeLen >= 24) {
                    off += mimeLen;
                    std::uint8_t dlenBuf[4]{};
                    if (ReadExactAt(fd, static_cast<std::int64_t>(pos + off), dlenBuf, 4)) {
                        const std::size_t descLen = static_cast<std::size_t>(ReadBe32(dlenBuf));
                        off += 4;
                        if (descLen <= blockLen - off && blockLen - off - descLen >= 20) {
                            off += descLen + 16;
                            std::uint8_t picLenBuf[4]{};
                            if (ReadExactAt(fd, static_cast<std::int64_t>(pos + off), picLenBuf, 4)) {
                                const std::size_t picLen = static_cast<std::size_t>(ReadBe32(picLenBuf));
                                off += 4;
                                if (picLen <= blockLen - off) {
                                    info.coverArtOffset = pos + off;
                                    info.coverArtSize = picLen;
                                    CopyFixedString(info.coverArtPath, sizeof(info.coverArtPath), guestPath, std::strlen(guestPath));
                                }
                            }
                        }
                    }
                }
            }
        }
        pos += blockSize;
        if (isLast) break;
    }
}

int GetFileInfoUnlocked(const SceCLCUsbStorageCallback& callbacks, std::uint32_t deviceId, const char* guestPath, SceCLCFileInfo& info) {
    const int format = DetectAudioFormat(guestPath);
    if (format == 0) return SceClcErrorUnsupportedFormat;
    if (!IsSafeGuestPath(guestPath)) return SceClcErrorInvalidArg;
    if (callbacks.requestMap == nullptr || callbacks.requestUnmap == nullptr) return SceClcErrorNoCallback;

    const char* secondSlash = std::strchr(guestPath + 1, '/');
    char topDir[PathBufferSize]{};
    const char* suffix = nullptr;
    if (secondSlash == nullptr) {
        topDir[0] = '/';
        topDir[1] = '\0';
        suffix = guestPath;
    } else {
        const std::size_t topLen = std::min<std::size_t>(static_cast<std::size_t>(secondSlash - guestPath), sizeof(topDir) - 1);
        std::memcpy(topDir, guestPath, topLen);
        topDir[topLen] = '\0';
        suffix = secondSlash;
    }

    char mountPoint[PathBufferSize]{};
    std::uint64_t mountId = 0;
    const int mapResult = callbacks.requestMap(deviceId, topDir, 3, 0, mountPoint, &mountId);
    if (mapResult < 0) return mapResult;

    const std::size_t mountLen = BoundedStrlen(mountPoint, sizeof(mountPoint));
    if (mountLen >= sizeof(mountPoint)) {
        return SceClcErrorInvalidArg;
    }
    if (mountLen > 0 && mountPoint[mountLen - 1] == '/' && suffix[0] == '/') {
        ++suffix;
    }

    char mountedPath[PathBufferSize * 2]{};
    const int written = std::snprintf(mountedPath, sizeof(mountedPath), "%s%s", mountPoint, suffix);
    if (written < 0 || static_cast<std::size_t>(written) >= sizeof(mountedPath)) {
        const int unmapResult = callbacks.requestUnmap(deviceId, mountPoint);
        return unmapResult < 0 ? unmapResult : SceClcErrorInvalidArg;
    }

    std::memset(&info, 0, sizeof(info));
    info.index = 0;
    info.deviceId = deviceId;
    CopyFixedString(info.path, sizeof(info.path), guestPath, std::strlen(guestPath));

    const int fd = sceKernelOpen(mountedPath, SCE_KERNEL_O_RDONLY, 0);
    if (fd >= 0) {
        FileStat st{};
        if (sceKernelFstat(fd, &st) >= 0 && st.st_size > 0) {
            if (format == 1) {
                ParseMp3Metadata(fd, st.st_size, guestPath, info);
            } else if (format == 2) {
                ParseMp4Container(fd, 0, static_cast<std::uint64_t>(st.st_size), guestPath, info);
            } else if (format == 3) {
                ParseFlacMetadata(fd, st.st_size, guestPath, info);
            }
        }
        sceKernelClose(fd);
    }

    const int unmapResult = callbacks.requestUnmap(deviceId, mountPoint);
    return unmapResult < 0 ? unmapResult : 0;
}

}

extern "C" {

int APS5_VABI sceCLCSetUsbStorageCallback(const SceCLCUsbStorageCallback* callback) {
    if (callback == nullptr) return SceClcErrorInvalidArg;
    SceCLCUsbStorageCallback local{};
    if (!SafeCopyIn(&local, callback, sizeof(local))) return SceClcErrorInvalidArg;
    auto& state = State();
    {
        std::lock_guard lock(state.mutex);
        g_callbackShadow = local;
        state.callbacks = local;
        state.deviceCount = 0;
        std::memset(state.deviceIds, 0, sizeof(state.deviceIds));
        ++state.providerGeneration;
    }
    if (local.registerCallback != nullptr) {
        local.registerCallback(1, &UsbStorageTrampoline, &state);
        local.registerCallback(2, &UsbStorageTrampoline, &state);
    }
    return 0;
}

int APS5_VABI sceCLCGetUsbStorageCallback(SceCLCUsbStorageCallback* callback) {
    if (callback == nullptr) return SceClcErrorInvalidArg;
    auto& state = State();
    SceCLCUsbStorageCallback snapshot{};
    {
        std::lock_guard lock(state.mutex);
        snapshot = g_callbackShadow;
    }
    if (!SafeCopyOut(callback, &snapshot, sizeof(snapshot))) return SceClcErrorInvalidArg;
    return 0;
}

int APS5_VABI sceCLCGetDeviceList(std::uint32_t* devices, std::int32_t* count) {
    if (devices == nullptr || count == nullptr) return SceClcErrorInvalidArg;
    if (!SafeWritable(count, sizeof(*count)) || !SafeWritable(devices, sizeof(*devices))) {
        return SceClcErrorInvalidArg;
    }

    auto& state = State();
    std::int32_t (APS5_VABI *getDeviceList)(std::uint32_t*, std::int32_t*) = nullptr;
    std::uint64_t snapshotGeneration = 0;
    {
        std::lock_guard lock(state.mutex);
        getDeviceList = state.callbacks.getDeviceList;
        snapshotGeneration = state.providerGeneration;
    }

    if (getDeviceList == nullptr) {
        const std::int32_t zero = 0;
        if (!SafeCopyOut(count, &zero, sizeof(zero))) return SceClcErrorInvalidArg;
        return SceClcErrorNoCallback;
    }

    GuardedDeviceBuffer guardedDevices;
    if (guardedDevices.ptr == nullptr) return SceClcErrorListNotAllocated;

    std::int32_t localCount = 0;
    const int res = getDeviceList(guardedDevices.ptr, &localCount);
    if (res < 0) return res;

    if (localCount < 0) localCount = 0;
    if (localCount > static_cast<std::int32_t>(MaxDevices)) localCount = static_cast<std::int32_t>(MaxDevices);

    if (!SafeWritable(count, sizeof(*count))) return SceClcErrorInvalidArg;
    if (localCount > 0 && !SafeWritableArray(devices, static_cast<std::size_t>(localCount), sizeof(std::uint32_t))) {
        return SceClcErrorInvalidArg;
    }

    if (localCount > 0 && !SafeCopyOut(devices, guardedDevices.ptr, static_cast<std::size_t>(localCount) * sizeof(std::uint32_t))) {
        return SceClcErrorInvalidArg;
    }
    if (!SafeCopyOut(count, &localCount, sizeof(localCount))) return SceClcErrorInvalidArg;

    {
        std::lock_guard lock(state.mutex);
        if (state.providerGeneration == snapshotGeneration) {
            state.deviceCount = localCount;
            std::memset(state.deviceIds, 0, sizeof(state.deviceIds));
            for (std::int32_t i = 0; i < localCount; ++i) {
                state.deviceIds[i] = guardedDevices.ptr[i];
            }
        }
    }
    return 0;
}

int APS5_VABI sceCLCGetDeviceInfo(std::uint32_t deviceId, SceCLCDeviceInfo* outInfo) {
    if (outInfo == nullptr) return SceClcErrorInvalidArg;
    if (!SafeWritable(outInfo, sizeof(*outInfo))) return SceClcErrorInvalidArg;

    auto& state = State();
    std::int32_t (APS5_VABI *getDeviceInfo)(std::uint32_t, UsbStorageDeviceInfo*) = nullptr;
    {
        std::lock_guard lock(state.mutex);
        if (!IsDeviceKnown(state, deviceId)) return SceClcErrorDeviceNotFound;
        if (state.callbacks.getDeviceInfo == nullptr) return SceClcErrorNoCallback;
        getDeviceInfo = state.callbacks.getDeviceInfo;
    }

    UsbStorageDeviceInfo rawInfo{};
    const int res = getDeviceInfo(deviceId, &rawInfo);
    if (res < 0) return res;

    SceCLCDeviceInfo formatted{};
    CopyFixedString(formatted.vendor, sizeof(formatted.vendor), rawInfo.vendor, sizeof(rawInfo.vendor));
    CopyFixedString(formatted.product, sizeof(formatted.product), rawInfo.product, sizeof(rawInfo.product));
    CopyFixedString(formatted.serial, sizeof(formatted.serial), rawInfo.serialNumber, sizeof(rawInfo.serialNumber));

    if (!SafeCopyOut(outInfo, &formatted, sizeof(formatted))) return SceClcErrorInvalidArg;
    return 0;
}

int APS5_VABI sceCLCGetFolderInfo(std::uint32_t deviceId, const char* path, std::uint64_t* outCounts) {
    if (path == nullptr || outCounts == nullptr) return SceClcErrorInvalidArg;
    char localPath[PathBufferSize]{};
    if (!CopyGuestCString(path, localPath, sizeof(localPath))) return SceClcErrorInvalidArg;
    if (!SafeWritable(outCounts, sizeof(*outCounts))) return SceClcErrorInvalidArg;

    std::size_t pathLen = std::strlen(localPath);
    if (!IsSafeGuestPath(localPath)) return SceClcErrorInvalidArg;
    if (localPath[pathLen - 1] != '/') {
        if (pathLen + 1 >= sizeof(localPath)) return SceClcErrorInvalidArg;
        localPath[pathLen] = '/';
        localPath[pathLen + 1] = '\0';
        ++pathLen;
    }

    const bool isRoot = std::strcmp(localPath, "/") == 0;
    const char* secondSlash = isRoot ? nullptr : std::strchr(localPath + 1, '/');
    if (!isRoot && secondSlash == nullptr) return SceClcErrorInvalidArg;

    auto& state = State();
    SceCLCUsbStorageCallback callbacks{};
    std::uint64_t snapshotGeneration = 0;
    char excludePattern[256]{};
    {
        std::lock_guard lock(state.mutex);
        if (!IsDeviceKnown(state, deviceId)) return SceClcErrorDeviceNotFound;
        if (state.fileEntries == nullptr) return SceClcErrorListNotAllocated;
        callbacks = state.callbacks;
        snapshotGeneration = state.providerGeneration;
        std::memcpy(excludePattern, state.excludePattern, sizeof(excludePattern));
    }

    std::vector<ScannedEntry> scanned;
    if (isRoot) {
        if (callbacks.getdentsOpen == nullptr ||
            callbacks.getdentsRead == nullptr ||
            callbacks.getdentsClose == nullptr) {
            return SceClcErrorNoCallback;
        }
        std::int32_t fd = -1;
        std::uint64_t bufferSize = 0;
        const int openRes = callbacks.getdentsOpen(deviceId, &fd, &bufferSize);
        if (openRes < 0) return openRes;
        if (bufferSize == 0) bufferSize = 0x8000;
        std::vector<char> buffer(static_cast<std::size_t>(bufferSize));
        for (;;) {
            std::uint64_t bytesRead = 0;
            const int readRes = callbacks.getdentsRead(fd, buffer.data(), bufferSize, &bytesRead);
            if (readRes < 0) {
                callbacks.getdentsClose(fd);
                return readRes;
            }
            if (bytesRead == 0) break;
            if (bytesRead > bufferSize) {
                callbacks.getdentsClose(fd);
                return SceClcErrorInvalidArg;
            }
            AppendDirentsFromBuffer(buffer.data(), static_cast<std::size_t>(bytesRead), pathLen, excludePattern, scanned);
            if (scanned.size() >= MaxEntries) break;
        }
        callbacks.getdentsClose(fd);
    } else {
        if (callbacks.requestMap == nullptr || callbacks.requestUnmap == nullptr) return SceClcErrorNoCallback;

        char topDir[PathBufferSize]{};
        const std::size_t topLen = std::min<std::size_t>(static_cast<std::size_t>(secondSlash - localPath), sizeof(topDir) - 1);
        std::memcpy(topDir, localPath, topLen);

        char mountPoint[PathBufferSize]{};
        std::uint64_t mountId = 0;
        const int mapRes = callbacks.requestMap(deviceId, topDir, 3, 0, mountPoint, &mountId);
        if (mapRes < 0) return mapRes;

        const std::size_t mountLen = BoundedStrlen(mountPoint, sizeof(mountPoint));
        if (mountLen >= sizeof(mountPoint)) {
            return SceClcErrorInvalidArg;
        }
        const char* suffix = (mountLen > 0 && mountPoint[mountLen - 1] == '/' && secondSlash[0] == '/')
            ? (secondSlash + 1)
            : secondSlash;

        char mountedPath[PathBufferSize * 2]{};
        const int written = std::snprintf(mountedPath, sizeof(mountedPath), "%s%s", mountPoint, suffix);
        if (written < 0 || static_cast<std::size_t>(written) >= sizeof(mountedPath)) {
            const int unmapRes = callbacks.requestUnmap(deviceId, mountPoint);
            return unmapRes < 0 ? unmapRes : SceClcErrorInvalidArg;
        }

        const int fd = sceKernelOpen(mountedPath, SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
        if (fd < 0) {
            const int unmapRes = callbacks.requestUnmap(deviceId, mountPoint);
            return unmapRes < 0 ? unmapRes : fd;
        }

        FileStat st{};
        int bufSize = 4096;
        if (sceKernelFstat(fd, &st) >= 0 && st.st_blksize >= 4096 && st.st_blksize <= 65536) {
            bufSize = static_cast<int>(st.st_blksize);
        }
        std::vector<char> buffer(static_cast<std::size_t>(bufSize));
        for (;;) {
            const int readBytes = sceKernelGetdents(fd, buffer.data(), bufSize);
            if (readBytes < 0) {
                sceKernelClose(fd);
                const int unmapRes = callbacks.requestUnmap(deviceId, mountPoint);
                return unmapRes < 0 ? unmapRes : readBytes;
            }
            if (readBytes == 0) break;
            AppendDirentsFromBuffer(buffer.data(), static_cast<std::size_t>(readBytes), pathLen, excludePattern, scanned);
            if (scanned.size() >= MaxEntries) break;
        }
        sceKernelClose(fd);
        const int unmapRes = callbacks.requestUnmap(deviceId, mountPoint);
        if (unmapRes < 0) return unmapRes;
    }

    SortScannedEntries(scanned);

    std::int32_t folderCount = 0;
    std::int32_t fileCount = 0;
    for (std::size_t i = 0; i < scanned.size() && i < MaxEntries; ++i) {
        if (scanned[i].isFile) {
            ++fileCount;
        } else {
            ++folderCount;
        }
    }

    {
        std::lock_guard lock(state.mutex);
        if (state.fileEntries == nullptr) return SceClcErrorListNotAllocated;
        if (state.providerGeneration != snapshotGeneration) return SceClcErrorInvalidArg;
        std::memset(state.fileEntries, 0, MaxEntries * EntryNameSize);
        for (std::size_t i = 0; i < scanned.size() && i < MaxEntries; ++i) {
            std::memcpy(state.fileEntries + i * EntryNameSize, scanned[i].name, EntryNameSize);
        }
        state.fileDeviceId = static_cast<std::int32_t>(deviceId);
        CopyFixedString(state.fileDir, sizeof(state.fileDir), localPath, pathLen);
        state.fileFolderCount = folderCount;
        state.fileFileCount = fileCount;
    }

    const std::uint64_t packedCounts =
        static_cast<std::uint64_t>(static_cast<std::uint32_t>(folderCount)) |
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(fileCount)) << 32);
    if (!SafeCopyOut(outCounts, &packedCounts, sizeof(packedCounts))) return SceClcErrorInvalidArg;
    return 0;
}

int APS5_VABI sceCLCGetFileListInfo(SceCLCListInfo* outInfo) {
    if (outInfo == nullptr) return SceClcErrorInvalidArg;
    auto& state = State();
    SceCLCListInfo local{};
    {
        std::lock_guard lock(state.mutex);
        local.deviceId = state.fileDeviceId;
        CopyFixedString(local.dir, sizeof(local.dir), state.fileDir, sizeof(state.fileDir));
        local.folderCount = state.fileFolderCount;
        local.fileCount = state.fileFileCount;
    }
    if (!SafeCopyOut(outInfo, &local, sizeof(local))) return SceClcErrorInvalidArg;
    return 0;
}

int APS5_VABI sceCLCGetTrackListInfo(SceCLCTrackListInfo* outInfo) {
    if (outInfo == nullptr) return SceClcErrorInvalidArg;
    auto& state = State();
    SceCLCTrackListInfo local{};
    {
        std::lock_guard lock(state.mutex);
        local.deviceId = state.trackDeviceId;
        CopyFixedString(local.dir, sizeof(local.dir), state.trackDir, sizeof(state.trackDir));
        local.fileCount = state.trackFileCount;
    }
    if (!SafeCopyOut(outInfo, &local, sizeof(local))) return SceClcErrorInvalidArg;
    return 0;
}

int APS5_VABI sceCLCGetFileList(std::uint32_t, std::uint32_t, std::int32_t listMode, std::uint32_t startIndex, std::int32_t maxCount, SceCLCListOutput* outList) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.fileEntries == nullptr) return SceClcErrorListNotAllocated;
    if (outList == nullptr || startIndex > 0xf9f || maxCount < 1 || maxCount > 0xfa0) return SceClcErrorInvalidArg;

    SceCLCListOutput localList{};
    if (!SafeCopyIn(&localList, outList, sizeof(localList))) return SceClcErrorInvalidArg;

    std::int32_t total = 0;
    std::uint32_t baseOffset = 0;
    if (listMode == 1) {
        total = state.fileFileCount;
        baseOffset = static_cast<std::uint32_t>(state.fileFolderCount);
    } else if (listMode == 2) {
        total = state.fileFolderCount;
        baseOffset = 0;
    } else {
        total = state.fileFolderCount + state.fileFileCount;
        baseOffset = 0;
    }

    if (static_cast<std::int32_t>(startIndex) > total) return SceClcErrorInvalidArg;

    const std::int32_t clampedTotal = (std::min)(total, static_cast<std::int32_t>(0xfa0));
    const std::int32_t remaining = clampedTotal - static_cast<std::int32_t>(startIndex);
    const std::int32_t plannedCount = (std::min)(maxCount, remaining > 0 ? remaining : 0);
    const std::size_t requiredEntries = static_cast<std::size_t>(plannedCount > 0 ? plannedCount : 1);
    if (!SafeWritable(outList, sizeof(SceCLCListOutput)) ||
        !SafeWritableArray(localList.items, requiredEntries, sizeof(SceCLCListEntry))) {
        return SceClcErrorInvalidArg;
    }

    for (std::int32_t i = 0; i < plannedCount; ++i) {
        const std::uint32_t entryIdx = baseOffset + startIndex + static_cast<std::uint32_t>(i);
        char checkPath[PathBufferSize]{};
        if (!CombineEntryPath(state.fileDir, state.fileEntries + static_cast<std::size_t>(entryIdx) * EntryNameSize, checkPath, sizeof(checkPath))) {
            return SceClcErrorInvalidArg;
        }
    }

    const bool dirMatchesTrack =
        state.fileDeviceId == state.trackDeviceId &&
        std::strcmp(state.fileDir, state.trackDir) == 0 &&
        state.trackEntries != nullptr;
    std::unordered_map<std::string_view, std::uint8_t> playbackByTrack;
    if (dirMatchesTrack && listMode != 2 && plannedCount > 0) {
        for (std::int32_t t = 0; t < state.trackFileCount && t < static_cast<std::int32_t>(MaxEntries); ++t) {
            const char* tname = state.trackEntries + static_cast<std::size_t>(t) * EntryNameSize;
            playbackByTrack.emplace(std::string_view(tname, BoundedStrlen(tname, EntryNameSize)), state.playbackResults[t]);
        }
    }

    std::int32_t count = 0;
    for (std::int32_t i = 0; i < plannedCount; ++i) {
        const std::int32_t idx = static_cast<std::int32_t>(startIndex) + i;
        const std::uint32_t entryIdx = baseOffset + static_cast<std::uint32_t>(idx);
        const char* entryName = state.fileEntries + static_cast<std::size_t>(entryIdx) * EntryNameSize;
        SceCLCListEntry item{};
        item.deviceId = state.fileDeviceId;
        CombineEntryPath(state.fileDir, entryName, item.name, sizeof(item.name));
        const bool isFile = static_cast<std::int32_t>(entryIdx) >= state.fileFolderCount;
        item.isFile = isFile ? 1 : 0;
        if (isFile && dirMatchesTrack) {
            const auto found = playbackByTrack.find(std::string_view(entryName, BoundedStrlen(entryName, EntryNameSize)));
            if (found != playbackByTrack.end()) {
                item.state = found->second;
            }
        }
        if (!SafeCopyOut(localList.items + i, &item, sizeof(item))) return SceClcErrorInvalidArg;
        ++count;
    }

    localList.totalCount = total;
    localList.returnedCount = count;
    localList.startIndex = static_cast<std::int32_t>(startIndex);
    if (!SafeCopyOut(outList, &localList, sizeof(localList))) return SceClcErrorInvalidArg;
    return 0;
}

int APS5_VABI sceCLCGetTrackList(std::uint32_t startIndex, std::int32_t maxCount, SceCLCListOutput* outList) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.trackEntries == nullptr) return SceClcErrorListNotAllocated;
    if (outList == nullptr || startIndex > 0xf9f || maxCount < 1 || maxCount > 0xfa0) return SceClcErrorInvalidArg;
    if (static_cast<std::int32_t>(startIndex) > state.trackFileCount) return SceClcErrorInvalidArg;

    SceCLCListOutput localList{};
    if (!SafeCopyIn(&localList, outList, sizeof(localList))) return SceClcErrorInvalidArg;

    const std::int32_t clampedTotal = (std::min)(state.trackFileCount, static_cast<std::int32_t>(0xfa0));
    const std::int32_t remaining = clampedTotal - static_cast<std::int32_t>(startIndex);
    const std::int32_t plannedCount = (std::min)(maxCount, remaining > 0 ? remaining : 0);
    const std::size_t requiredEntries = static_cast<std::size_t>(plannedCount > 0 ? plannedCount : 1);
    if (!SafeWritable(outList, sizeof(SceCLCListOutput)) ||
        !SafeWritableArray(localList.items, requiredEntries, sizeof(SceCLCListEntry))) {
        return SceClcErrorInvalidArg;
    }

    for (std::int32_t i = 0; i < plannedCount; ++i) {
        const std::int32_t idx = static_cast<std::int32_t>(startIndex) + i;
        char checkPath[PathBufferSize]{};
        if (!CombineEntryPath(state.trackDir, state.trackEntries + static_cast<std::size_t>(idx) * EntryNameSize, checkPath, sizeof(checkPath))) {
            return SceClcErrorInvalidArg;
        }
    }

    std::int32_t count = 0;
    for (std::int32_t i = 0; i < plannedCount; ++i) {
        const std::int32_t idx = static_cast<std::int32_t>(startIndex) + i;
        SceCLCListEntry item{};
        item.deviceId = state.trackDeviceId;
        CombineEntryPath(state.trackDir, state.trackEntries + static_cast<std::size_t>(idx) * EntryNameSize, item.name, sizeof(item.name));
        item.isFile = 1;
        item.state = state.playbackResults[idx];
        if (!SafeCopyOut(localList.items + i, &item, sizeof(item))) return SceClcErrorInvalidArg;
        ++count;
    }

    localList.totalCount = state.trackFileCount;
    localList.returnedCount = count;
    localList.startIndex = static_cast<std::int32_t>(startIndex);
    if (!SafeCopyOut(outList, &localList, sizeof(localList))) return SceClcErrorInvalidArg;
    return 0;
}

int APS5_VABI sceCLCClearFileList() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.fileDeviceId = 0;
    std::memset(state.fileDir, 0, sizeof(state.fileDir));
    state.fileFolderCount = 0;
    state.fileFileCount = 0;
    if (state.fileEntries != nullptr) {
        std::memset(state.fileEntries, 0, MaxEntries * EntryNameSize);
    }
    return 0;
}

int APS5_VABI sceCLCClearTrackList() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    ClearTrackListLocked(state);
    return 0;
}

int APS5_VABI sceCLCSetTrackList() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.fileEntries == nullptr || state.trackEntries == nullptr) return SceClcErrorListNotAllocated;
    if (state.fileFileCount == 0) {
        ClearTrackListLocked(state);
        return 0;
    }
    if (state.fileDeviceId == state.trackDeviceId && std::strcmp(state.fileDir, state.trackDir) == 0) {
        return 0;
    }

    state.trackDeviceId = state.fileDeviceId;
    CopyFixedString(state.trackDir, sizeof(state.trackDir), state.fileDir, sizeof(state.fileDir));
    state.trackFolderCount = 0;
    state.trackFileCount = state.fileFileCount;
    std::memcpy(
        state.trackEntries,
        state.fileEntries + static_cast<std::size_t>(state.fileFolderCount) * EntryNameSize,
        static_cast<std::size_t>(state.fileFileCount) * EntryNameSize);
    std::memset(state.playbackResults, 0, sizeof(state.playbackResults));
    std::memset(state.shuffleOrder, 0, sizeof(state.shuffleOrder));

    for (std::int32_t i = 0; i < state.trackFileCount; ++i) {
        state.shuffleOrder[i] = i;
    }

    if (state.shuffleEnabled == 1) {
        for (std::int32_t i = 0; i < state.trackFileCount; ++i) {
            const std::uint32_t j = NextShuffleIndex(state.rngSeed, state.trackFileCount);
            std::swap(state.shuffleOrder[i], state.shuffleOrder[j]);
        }
    } else {
        state.shuffleEnabled = 0;
    }
    return 0;
}

int APS5_VABI sceCLCSetPlaybackResult(std::int32_t trackIndex, std::uint32_t result) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (result > 1 || trackIndex < 0 || trackIndex >= state.trackFileCount) return SceClcErrorInvalidArg;
    state.playbackResults[trackIndex] = static_cast<std::uint8_t>(result);
    return 0;
}

int APS5_VABI sceCLCSetShuffle(std::int32_t enable, std::int32_t currentTrackIndex) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.shuffleEnabled = (enable != 0) ? 1 : 0;
    for (std::int32_t i = 0; i < state.trackFileCount; ++i) {
        state.shuffleOrder[i] = i;
    }
    if (state.shuffleEnabled != 0 && state.trackFileCount > 0) {
        for (std::int32_t i = 0; i < state.trackFileCount; ++i) {
            const std::uint32_t j = NextShuffleIndex(state.rngSeed, state.trackFileCount);
            std::swap(state.shuffleOrder[i], state.shuffleOrder[j]);
        }
        if (static_cast<std::uint32_t>(currentTrackIndex) < static_cast<std::uint32_t>(state.trackFileCount)) {
            for (std::int32_t i = 0; i < state.trackFileCount; ++i) {
                if (state.shuffleOrder[i] == currentTrackIndex) {
                    std::swap(state.shuffleOrder[0], state.shuffleOrder[i]);
                    break;
                }
            }
        }
    }
    return 0;
}

std::uint8_t APS5_VABI sceCLCGetShuffle() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return state.shuffleEnabled;
}

int APS5_VABI sceCLCRegisterEventCallback(SceCLCEventCallback callback, void* userData) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.eventCallback = callback;
    state.eventUserData = userData;
    return 0;
}

int APS5_VABI sceCLCGetFileInfo(std::uint32_t deviceId, const char* path, SceCLCFileInfo* outInfo) {
    if (path == nullptr || outInfo == nullptr) return SceClcErrorInvalidArg;
    char localPath[PathBufferSize]{};
    if (!CopyGuestCString(path, localPath, sizeof(localPath))) return SceClcErrorInvalidArg;
    if (!SafeWritable(outInfo, sizeof(*outInfo))) return SceClcErrorInvalidArg;

    auto& state = State();
    SceCLCUsbStorageCallback callbacks{};
    {
        std::lock_guard lock(state.mutex);
        callbacks = state.callbacks;
    }

    SceCLCFileInfo localInfo{};
    const int res = GetFileInfoUnlocked(callbacks, deviceId, localPath, localInfo);
    if (res < 0) return res;
    if (!SafeCopyOut(outInfo, &localInfo, sizeof(localInfo))) return SceClcErrorInvalidArg;
    return 0;
}

int APS5_VABI sceCLCGetTrackInfo(std::int32_t trackIndex, SceCLCFileInfo* outInfo) {
    if (outInfo == nullptr) return SceClcErrorInvalidArg;
    if (!SafeWritable(outInfo, sizeof(*outInfo))) return SceClcErrorInvalidArg;

    auto& state = State();
    SceCLCUsbStorageCallback callbacks{};
    std::uint32_t deviceId = 0;
    std::int32_t origIdx = 0;
    char fullPath[PathBufferSize]{};
    {
        std::lock_guard lock(state.mutex);
        if (trackIndex < 0 || trackIndex >= state.trackFileCount || state.trackEntries == nullptr) {
            return SceClcErrorInvalidArg;
        }
        origIdx = state.shuffleOrder[trackIndex];
        if (origIdx < 0 || origIdx >= state.trackFileCount) return SceClcErrorInvalidArg;
        if (!CombineEntryPath(state.trackDir, state.trackEntries + static_cast<std::size_t>(origIdx) * EntryNameSize, fullPath, sizeof(fullPath))) {
            return SceClcErrorInvalidArg;
        }
        deviceId = static_cast<std::uint32_t>(state.trackDeviceId);
        callbacks = state.callbacks;
    }

    SceCLCFileInfo localInfo{};
    const int res = GetFileInfoUnlocked(callbacks, deviceId, fullPath, localInfo);
    if (res < 0) return res;
    localInfo.index = origIdx;
    if (!SafeCopyOut(outInfo, &localInfo, sizeof(localInfo))) return SceClcErrorInvalidArg;
    return 0;
}

int APS5_VABI libSceContentListControllerUnknown_SoBk_minus_kuNLsE_nid_no_patch(const char* pattern) {
    if (pattern == nullptr) return SceClcErrorInvalidArg;
    char localPattern[257]{};
    if (!CopyGuestCString(pattern, localPattern, sizeof(localPattern))) return SceClcErrorInvalidArg;
    const std::size_t len = std::strlen(localPattern);
    if (len > 0xff) return SceClcErrorInvalidArg;

    auto& state = State();
    std::lock_guard lock(state.mutex);
    std::memset(state.excludePattern, 0, sizeof(state.excludePattern));
    std::memcpy(state.excludePattern, localPattern, len);
    return 0;
}
APS5_EXPORT("SoBk-kuNLsE", libSceContentListControllerUnknown_SoBk_minus_kuNLsE_nid_no_patch);

}
