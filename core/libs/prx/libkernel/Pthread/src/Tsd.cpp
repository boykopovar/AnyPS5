#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <mutex>
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_EAGAIN = 0x80020023;
static constexpr int MAX_KEYS = 512;
static constexpr int DESTRUCTOR_ITERATIONS = 4;

using GuestKeyDestructor = void (APS5_VABI*)(void*);

struct KeySlot {
    bool used = false;
    GuestKeyDestructor destructor = nullptr;
};

static std::mutex g_keyLock;
static std::array<KeySlot, MAX_KEYS> g_keys;

struct ThreadValues {
    std::array<void*, MAX_KEYS> values{};

    ~ThreadValues() {
        for (int round = 0; round < DESTRUCTOR_ITERATIONS; ++round) {
            bool called = false;
            for (int key = 0; key < MAX_KEYS; ++key) {
                void* value = values[key];
                if (!value) continue;
                GuestKeyDestructor destructor;
                {
                    std::lock_guard lock(g_keyLock);
                    destructor = g_keys[key].used ? g_keys[key].destructor : nullptr;
                }
                values[key] = nullptr;
                if (destructor) {
                    destructor(value);
                    called = true;
                }
            }
            if (!called) return;
        }
    }
};

static thread_local ThreadValues g_values;

static bool IsValidKey(PthreadKey key) {
    if (key < 0 || key >= MAX_KEYS) return false;
    std::lock_guard lock(g_keyLock);
    return g_keys[key].used;
}

extern "C" {

int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    if (!key) throw std::runtime_error("scePthreadKeyCreate: null key");
    std::lock_guard lock(g_keyLock);
    for (int index = 0; index < MAX_KEYS; ++index) {
        if (!g_keys[index].used) {
            g_keys[index] = {true, reinterpret_cast<GuestKeyDestructor>(destructor)};
            *key = index;
            return SCE_OK;
        }
    }
    return SCE_KERNEL_ERROR_EAGAIN;
}

int APS5_VABI scePthreadKeyDelete(PthreadKey key) {
    if (key < 0 || key >= MAX_KEYS) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(g_keyLock);
    if (!g_keys[key].used) return SCE_KERNEL_ERROR_EINVAL;
    g_keys[key] = {};
    return SCE_OK;
}

void* APS5_VABI scePthreadGetspecific(PthreadKey key) {
    if (key < 0 || key >= MAX_KEYS) return nullptr;
    return g_values.values[key];
}

int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value) {
    if (!IsValidKey(key)) return SCE_KERNEL_ERROR_EINVAL;
    g_values.values[key] = value;
    return SCE_OK;
}

}
