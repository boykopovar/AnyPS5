#ifndef PRX_LIBC_INCLUDE_SPECIFICS_LINUX_HOSTMAPPING_HPP
#define PRX_LIBC_INCLUDE_SPECIFICS_LINUX_HOSTMAPPING_HPP

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
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

inline int QueryFd() {
    static const int fd = [] {
        if (std::getenv("APS5_NO_PROCMAP_QUERY") != nullptr) return -1;
        const int opened = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
        if (opened < 0) return -1;
        procmap_query probe{};
        probe.size = sizeof(probe);
        probe.query_flags = PROCMAP_QUERY_COVERING_OR_NEXT_VMA;
        if (ioctl(opened, PROCMAP_QUERY, &probe) == 0 || errno == ENOENT) return opened;
        close(opened);
        return -1;
    }();
    return fd;
}

inline std::optional<Mapping> ParseMapsLine(const std::string& line) {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    char perms[8] = {};
    std::uint64_t offset = 0;
    unsigned int devMajor = 0;
    unsigned int devMinor = 0;
    std::uint64_t inode = 0;
    char path[4096] = {};
    const int parsed = std::sscanf(line.c_str(), "%llx-%llx %7s %llx %x:%x %llu %4095s",
        (unsigned long long*)&start, (unsigned long long*)&end, perms,
        (unsigned long long*)&offset, &devMajor, &devMinor, (unsigned long long*)&inode, path);
    if (parsed < 7 || std::strlen(perms) < 4) return std::nullopt;
    return Mapping{start, end, parsed >= 8 ? path : "", perms[0] == 'r', perms[1] == 'w', perms[2] == 'x', perms[3] == 's', inode};
}

inline std::optional<Mapping> ScanMaps(std::uint64_t address, const char* caller) {
    std::ifstream maps("/proc/self/maps");
    if (!maps) throw std::runtime_error(std::string(caller) + ": failed to open /proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        auto mapping = ParseMapsLine(line);
        if (!mapping || address < mapping->start || address >= mapping->end) continue;
        return mapping;
    }
    return std::nullopt;
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
    std::ifstream maps("/proc/self/maps");
    if (!maps) throw std::runtime_error(std::string(caller) + ": failed to open /proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        const auto mapping = ParseMapsLine(line);
        if (!mapping) throw std::runtime_error(std::string(caller) + ": unexpected /proc/self/maps line: " + line);
        if (mapping->end <= begin) continue;
        if (mapping->start >= end) return;
        visit(*mapping);
    }
}

}

#endif
