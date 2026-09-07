#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "SceTypes.hpp"
#include "ModuleTable.hpp"
#include "prx/libc/include/General.hpp"

namespace {

const char* findModuleName(const std::uint32_t id) {
    const auto it = kModuleTable.find(id);
    return it != kModuleTable.end() ? it->second : nullptr;
}

std::mutex gMutex;
std::unordered_map<std::uint32_t, std::int32_t> gLoadCount;

}

extern "C" {

int sceSysmoduleGetModuleInfoForUnwind(std::uint64_t addr, int flags, ModuleInfoForUnwind* info) {
    (void)flags;
    std::ifstream maps("/proc/self/maps");
    if (!maps) {
        throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: failed to open /proc/self/maps");
    }
    std::string line;
    while (std::getline(maps, line)) {
        std::uint64_t start = 0;
        std::uint64_t end = 0;
        char perms[8] = {};
        std::uint64_t offset = 0;
        int devMajor = 0;
        int devMinor = 0;
        std::uint64_t inode = 0;
        char path[4096] = {};
        int parsed = std::sscanf(
            line.c_str(),
            "%llx-%llx %7s %llx %x:%x %llu %4095s",
            (unsigned long long*)&start,
            (unsigned long long*)&end,
            perms,
            (unsigned long long*)&offset,
            &devMajor,
            &devMinor,
            (unsigned long long*)&inode,
            path
        );
        if (parsed < 7 || addr < start || addr >= end) {
            continue;
        }
        info->st_size = sizeof(ModuleInfoForUnwind);
        std::strncpy(info->name, parsed >= 8 ? path : "", sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        info->eh_frame_hdr_addr = 0;
        info->eh_frame_addr = 0;
        info->eh_frame_size = 0;
        info->seg0_addr = start;
        info->seg0_size = end - start;
        return 0;
    }
    throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: address not found in maps");
}

int sceSysmoduleIsLoaded(std::uint16_t id) {
    if (id == 0) {
        throw std::runtime_error("sceSysmoduleIsLoaded: invalid id 0");
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleIsLoaded: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gLoadCount.find(id);
    if (it == gLoadCount.end() || it->second < 1) {
        return 0x80A90002;
    }
    return 0;
}

int sceSysmoduleLoadModule(std::uint16_t id) {
    if (id == 0) {
        throw std::runtime_error("sceSysmoduleLoadModule: invalid id 0");
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleLoadModule: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    gLoadCount[id]++;
    return 0;
}

int sceSysmoduleLoadModuleInternalWithArg(std::uint32_t id, int argc, void* argv, std::uint64_t unk, int* ret) {
    (void)argc;
    (void)argv;
    (void)unk;
    if ((id & 0x7fffffffu) == 0) {
        throw std::runtime_error("sceSysmoduleLoadModuleInternalWithArg: invalid id 0");
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleLoadModuleInternalWithArg: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    gLoadCount[id]++;
    if (ret) {
        *ret = 0;
    }
    return 0;
}

int sceSysmoduleUnloadModule(std::uint16_t id) {
    if (id == 0) {
        throw std::runtime_error("sceSysmoduleUnloadModule: invalid id 0");
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleUnloadModule: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gLoadCount.find(id);
    if (it == gLoadCount.end() || it->second < 1) {
        return 0x80A90003;
    }
    it->second--;
    return 0;
}

}
