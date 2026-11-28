#ifndef PRX_LIBC_INCLUDE_SPECIFICS_LINUX_HOSTMAPPING_HPP
#define PRX_LIBC_INCLUDE_SPECIFICS_LINUX_HOSTMAPPING_HPP

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>
#if __has_include(<linux/fs.h>)
#include <linux/fs.h>
#endif
#if !defined(PROCMAP_QUERY)
struct procmap_query {
    std::uint64_t size;
    std::uint64_t query_flags;
    std::uint64_t query_addr;
    std::uint64_t vma_start;
    std::uint64_t vma_end;
    std::uint64_t vma_flags;
    std::uint64_t vma_page_size;
    std::uint64_t vma_offset;
    std::uint64_t inode;
    std::uint32_t dev_major;
    std::uint32_t dev_minor;
    std::uint32_t vma_name_size;
    std::uint32_t build_id_size;
    std::uint64_t vma_name_addr;
    std::uint64_t build_id_addr;
};
static_assert(sizeof(procmap_query) == 104);
enum : std::uint64_t {
    PROCMAP_QUERY_VMA_READABLE = 0x01,
    PROCMAP_QUERY_VMA_WRITABLE = 0x02,
    PROCMAP_QUERY_VMA_EXECUTABLE = 0x04,
    PROCMAP_QUERY_VMA_SHARED = 0x08,
    PROCMAP_QUERY_COVERING_OR_NEXT_VMA = 0x10,
};
#define PROCMAP_QUERY _IOWR('f', 17, struct procmap_query)
#endif

namespace HostMapping {

struct Mapping {
    std::uint64_t start;
    std::uint64_t end;
    std::string path;
    bool readable = false;
    bool writable = false;
    bool executable = false;
    bool shared = false;
    std::uint64_t inode = 0;
};

inline int MapsFd() {
    static std::atomic<int> cached{-1};
    int fd = cached.load(std::memory_order_acquire);
    if (fd >= 0) return fd;
    const int opened = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (opened < 0) return -1;
    if (cached.compare_exchange_strong(fd, opened, std::memory_order_acq_rel, std::memory_order_acquire)) return opened;
    close(opened);
    return fd;
}

inline int QueryFd() {
    static const int fd = [] {
        if (std::getenv("APS5_NO_PROCMAP_QUERY") != nullptr) return -1;
        const int opened = MapsFd();
        if (opened < 0) return -1;
        procmap_query probe{};
        probe.size = sizeof(probe);
        probe.query_flags = PROCMAP_QUERY_COVERING_OR_NEXT_VMA;
        if (ioctl(opened, PROCMAP_QUERY, &probe) == 0 || errno == ENOENT) return opened;
        return -1;
    }();
    return fd;
}

inline std::mutex& ScanLock() {
    static std::mutex lock;
    return lock;
}

inline constexpr std::size_t ScanBufferBytes = 16384;

inline char* ScanBuffer() {
    static char buffer[ScanBufferBytes];
    return buffer;
}

template <typename Line>
void ScanLines(const char* caller, Line&& line) {
    const std::lock_guard guard(ScanLock());
    const int fd = MapsFd();
    if (fd < 0) throw std::runtime_error(std::string(caller) + ": failed to open /proc/self/maps");
    char* const buffer = ScanBuffer();
    std::size_t held = 0;
    for (off_t offset = 0;;) {
        if (held == ScanBufferBytes - 1) throw std::runtime_error(std::string(caller) + ": /proc/self/maps line longer than the scan buffer");
        const ssize_t got = pread(fd, buffer + held, ScanBufferBytes - 1 - held, offset);
        if (got < 0 && errno == EINTR) continue;
        if (got < 0) throw std::runtime_error(std::string(caller) + ": failed to read /proc/self/maps");
        if (got == 0) break;
        offset += got;
        held += static_cast<std::size_t>(got);
        std::size_t begin = 0;
        while (auto* newline = static_cast<char*>(std::memchr(buffer + begin, '\n', held - begin))) {
            const auto bytes = static_cast<std::size_t>(newline - (buffer + begin));
            *newline = '\0';
            if (!line(buffer + begin, bytes)) return;
            begin += bytes + 1;
        }
        std::memmove(buffer, buffer + begin, held - begin);
        held -= begin;
    }
    if (held != 0) {
        buffer[held] = '\0';
        line(buffer, held);
    }
}

struct MapsLine {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    char perms[8] = {};
    std::uint64_t inode = 0;
    const char* path = nullptr;
    std::size_t pathBytes = 0;
};

inline bool ParseMapsLine(const char* text, std::size_t bytes, MapsLine& line) {
    std::uint64_t offset = 0;
    unsigned int devMajor = 0;
    unsigned int devMinor = 0;
    int prefix = 0;
    const int parsed = std::sscanf(text, "%llx-%llx %7s %llx %x:%x %llu%n",
        (unsigned long long*)&line.start, (unsigned long long*)&line.end, line.perms,
        (unsigned long long*)&offset, &devMajor, &devMinor, (unsigned long long*)&line.inode, &prefix);
    if (parsed < 7 || std::strlen(line.perms) < 4) return false;
    auto begin = static_cast<std::size_t>(prefix > 0 ? prefix : 0);
    while (begin < bytes && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
    if (prefix > 0 && begin < bytes) {
        line.path = text + begin;
        line.pathBytes = std::min<std::size_t>(bytes - begin, 4095);
    }
    return true;
}

inline Mapping FromLine(const MapsLine& line) {
    return Mapping{line.start, line.end, line.path != nullptr ? std::string(line.path, line.pathBytes) : std::string(), line.perms[0] == 'r',
        line.perms[1] == 'w', line.perms[2] == 'x', line.perms[3] == 's', line.inode};
}

inline std::optional<Mapping> ScanMaps(std::uint64_t address, const char* caller) {
    std::optional<Mapping> found;
    ScanLines(caller, [&](const char* text, std::size_t bytes) {
        MapsLine line;
        if (!ParseMapsLine(text, bytes, line) || address < line.start || address >= line.end) return true;
        found = FromLine(line);
        return false;
    });
    return found;
}

inline Mapping FromQuery(const procmap_query& query, const char* name) {
    return Mapping{query.vma_start, query.vma_end, query.vma_name_size != 0 ? name : "", (query.vma_flags & PROCMAP_QUERY_VMA_READABLE) != 0,
        (query.vma_flags & PROCMAP_QUERY_VMA_WRITABLE) != 0, (query.vma_flags & PROCMAP_QUERY_VMA_EXECUTABLE) != 0,
        (query.vma_flags & PROCMAP_QUERY_VMA_SHARED) != 0, query.inode};
}

inline std::optional<Mapping> Find(std::uint64_t address, const char* caller) {
    const int fd = QueryFd();
    if (fd < 0) return ScanMaps(address, caller);
    char name[4096] = {};
    procmap_query query{};
    query.size = sizeof(query);
    query.query_addr = address;
    query.vma_name_size = sizeof(name);
    query.vma_name_addr = reinterpret_cast<std::uint64_t>(name);
    if (ioctl(fd, PROCMAP_QUERY, &query) != 0) {
        if (errno == ENOENT) return std::nullopt;
        throw std::runtime_error(std::string(caller) + ": PROCMAP_QUERY failed");
    }
    return FromQuery(query, name);
}

template <typename Visit>
void Walk(std::uint64_t begin, std::uint64_t end, const char* caller, Visit&& visit) {
    if (begin >= end) return;
    if (const int fd = QueryFd(); fd >= 0) {
        for (auto cursor = begin; cursor < end;) {
            procmap_query query{};
            query.size = sizeof(query);
            query.query_flags = PROCMAP_QUERY_COVERING_OR_NEXT_VMA;
            query.query_addr = cursor;
            if (ioctl(fd, PROCMAP_QUERY, &query) != 0) {
                if (errno == ENOENT) return;
                throw std::runtime_error(std::string(caller) + ": PROCMAP_QUERY failed");
            }
            if (query.vma_start >= end) return;
            if (query.vma_end <= cursor) throw std::runtime_error(std::string(caller) + ": PROCMAP_QUERY went backwards");
            visit(FromQuery(query, ""));
            cursor = query.vma_end;
        }
        return;
    }
    std::vector<Mapping> mappings;
    ScanLines(caller, [&](const char* text, std::size_t bytes) {
        MapsLine line;
        if (!ParseMapsLine(text, bytes, line)) throw std::runtime_error(std::string(caller) + ": unexpected /proc/self/maps line: " + text);
        if (line.end <= begin) return true;
        if (line.start >= end) return false;
        mappings.push_back(FromLine(line));
        return true;
    });
    for (const auto& mapping : mappings) visit(mapping);
}

}

#endif
