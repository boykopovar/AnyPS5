#ifndef CORE_LIBS_PRX_LIBSCESAVEDATANATIVE_SAVEDATA_HPP
#define CORE_LIBS_PRX_LIBSCESAVEDATANATIVE_SAVEDATA_HPP

#include <array>
#include <cstdint>
#include <mutex>
#include <string>

constexpr int SAVE_DATA_OK = 0;
// Values follow the SDK error table (0x809F0000 + code): 2 OUT_OF_MEMORY, 3 BUSY, 4 NOT_MOUNTED, 7 EXISTS, 8 NOT_FOUND, 0xB INTERNAL, 0xC MOUNT_FULL.
constexpr int SAVE_DATA_ERROR_PARAMETER = -2137063424;
constexpr int SAVE_DATA_ERROR_NOT_INITIALIZED = -2137063423;
constexpr int SAVE_DATA_ERROR_ALREADY_INITIALIZED = -2137063419;  // 0x809F0005
constexpr int SAVE_DATA_ERROR_OUT_OF_MEMORY = -2137063422;
constexpr int SAVE_DATA_ERROR_BUSY = -2137063421;
constexpr int SAVE_DATA_ERROR_NOT_MOUNTED = -2137063420;
constexpr int SAVE_DATA_ERROR_MOUNT_FULL = -2137063412;
constexpr int SAVE_DATA_ERROR_EXISTS = -2137063417;
constexpr int SAVE_DATA_ERROR_NOT_FOUND = -2137063416;
constexpr int SAVE_DATA_ERROR_INTERNAL = -2137063413;          // 0x809F000B
constexpr int SAVE_DATA_ERROR_MEMORY_NOT_READY = -2137063406;  // 0x809F0012

constexpr std::uint32_t SAVE_DATA_MOUNT_MODE_RDONLY = 1;
constexpr std::uint32_t SAVE_DATA_MOUNT_MODE_RDWR = 2;
constexpr std::uint32_t SAVE_DATA_MOUNT_MODE_CREATE = 4;
constexpr std::uint32_t SAVE_DATA_MOUNT_MODE_CREATE2 = 32;

constexpr std::uint64_t SAVE_DATA_BLOCKS_MAX = 32768;
constexpr std::uint32_t SAVE_DATA_PARAM_TYPE_ALL = 0;
constexpr std::uint32_t SAVE_DATA_PARAM_TYPE_TITLE = 1;
constexpr std::uint32_t SAVE_DATA_PARAM_TYPE_SUB_TITLE = 2;
constexpr std::uint32_t SAVE_DATA_PARAM_TYPE_DETAIL = 3;
constexpr std::uint32_t SAVE_DATA_PARAM_TYPE_USER_PARAM = 4;
constexpr std::uint32_t SAVE_DATA_PARAM_TYPE_MTIME = 5;

constexpr std::size_t SAVE_DATA_MOUNT_SLOTS = 16;

struct MountSlot {
    bool used = false;
    std::string mount_point;
    std::string real_path;
};

inline std::array<MountSlot, SAVE_DATA_MOUNT_SLOTS> g_slots;
inline std::mutex g_slots_mutex;

inline int find_slot_by_mount_point(const char* mount_point) {
    for (int i = 0; i < static_cast<int>(SAVE_DATA_MOUNT_SLOTS); i++) {
        if (g_slots[i].used && g_slots[i].mount_point == mount_point) {
            return i;
        }
    }
    return -1;
}

inline int find_free_slot() {
    for (int i = 0; i < static_cast<int>(SAVE_DATA_MOUNT_SLOTS); i++) {
        if (!g_slots[i].used) {
            return i;
        }
    }
    return -1;
}

inline bool any_slot_used() {
    for (const auto& s : g_slots) {
        if (s.used) {
            return true;
        }
    }
    return false;
}

#endif
