#ifndef CORE_LIBS_PRX_LIBSCESAVEDATANATIVE_SAVEDATA_HPP
#define CORE_LIBS_PRX_LIBSCESAVEDATANATIVE_SAVEDATA_HPP

#include <array>
#include <cstdint>
#include <string>

constexpr int SAVE_DATA_OK = 0;
constexpr int SAVE_DATA_ERROR_PARAMETER = -2137063424;
constexpr int SAVE_DATA_ERROR_NOT_INITIALIZED = -2137063423;
constexpr int SAVE_DATA_ERROR_ALREADY_INITIALIZED = -2137063422;
constexpr int SAVE_DATA_ERROR_OUT_OF_MEMORY = -2137063421;
constexpr int SAVE_DATA_ERROR_BUSY = -2137063420;
constexpr int SAVE_DATA_ERROR_NOT_MOUNTED = -2137063419;
constexpr int SAVE_DATA_ERROR_MOUNT_FULL = -2137063418;
constexpr int SAVE_DATA_ERROR_EXISTS = -2137063414;
constexpr int SAVE_DATA_ERROR_NOT_FOUND = -2137063413;

constexpr std::uint32_t SAVE_DATA_MOUNT_MODE_RDONLY = 1;
constexpr std::uint32_t SAVE_DATA_MOUNT_MODE_RDWR = 2;
constexpr std::uint32_t SAVE_DATA_MOUNT_MODE_CREATE = 4;
constexpr std::uint32_t SAVE_DATA_MOUNT_MODE_CREATE2 = 32;

constexpr std::uint64_t SAVE_DATA_BLOCKS_MAX = 32768;

constexpr std::size_t SAVE_DATA_MOUNT_SLOTS = 16;

struct MountSlot {
    bool used = false;
    std::string mount_point;
    std::string real_path;
};

inline std::array<MountSlot, SAVE_DATA_MOUNT_SLOTS> g_slots;

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
