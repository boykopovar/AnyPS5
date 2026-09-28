#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
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
    std::uint64_t generation = 0;
};

static std::mutex g_keyLock;
static std::array<KeySlot, MAX_KEYS> g_keys;

struct ThreadValues {
    std::array<void*, MAX_KEYS> values{};
    std::array<std::uint64_t, MAX_KEYS> generations{};

    ~ThreadValues() {
        for (int round = 0; round < DESTRUCTOR_ITERATIONS; ++round) {
            bool called = false;
            for (int key = 0; key < MAX_KEYS; ++key) {
                void* value = values[key];
                if (!value) continue;
                GuestKeyDestructor destructor;
                {
                    std::lock_guard lock(g_keyLock);
                    const auto& slot = g_keys[key];
                    destructor = slot.used && slot.generation == generations[key] ? slot.destructor : nullptr;
                }
                values[key] = nullptr;
                generations[key] = 0;
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

extern "C" {
int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    if (!key) throw std::runtime_error("scePthreadKeyCreate: null key");
    std::lock_guard lock(g_keyLock);
    for (int index = 0; index < MAX_KEYS; ++index) {
        if (!g_keys[index].used) {
            auto generation = g_keys[index].generation + 1;
            if (generation == 0) ++generation;
            g_keys[index] = {true, destructor, generation};
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
    g_keys[key].used = false;
    g_keys[key].destructor = nullptr;
    return SCE_OK;
}

void* APS5_VABI scePthreadGetspecific(PthreadKey key) {
    if (key < 0 || key >= MAX_KEYS) return nullptr;
    std::lock_guard lock(g_keyLock);
    if (!g_keys[key].used || g_values.generations[key] != g_keys[key].generation) return nullptr;
    return g_values.values[key];
}

int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value) {
    if (key < 0 || key >= MAX_KEYS) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(g_keyLock);
    if (!g_keys[key].used) return SCE_KERNEL_ERROR_EINVAL;
    g_values.values[key] = value;
    g_values.generations[key] = value == nullptr ? 0 : g_keys[key].generation;
    return SCE_OK;
}

}
