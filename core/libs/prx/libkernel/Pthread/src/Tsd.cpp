#include "prx/libkernel/include/Api.hpp"
#include "../include/Pthread.hpp"
#include "../include/ThreadLifecycle.hpp"
#include "../include/WindowsThreadLocal.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_EAGAIN = 0x80020023;
static constexpr int MAX_KEYS = 512;
static constexpr int DESTRUCTOR_ITERATIONS = 4;
static constexpr std::uint64_t FREE_KEY = 0;

using GuestKeyDestructor = void (APS5_VABI*)(void*);

struct KeySlot {
    std::atomic<std::uint64_t> sequence{FREE_KEY};
    GuestKeyDestructor destructor = nullptr;
};

struct ThreadValue {
    void* value = nullptr;
    std::uint64_t sequence = FREE_KEY;
};

static std::mutex g_keyLock;
static std::array<KeySlot, MAX_KEYS> g_keys;
static std::uint64_t g_lastSequence = FREE_KEY;

struct ThreadValues {
    std::array<ThreadValue, MAX_KEYS> values{};
    bool destroyed = false;

    ~ThreadValues() {
        RunDestructors();
    }

    void RunDestructors() {
        if (destroyed) return;
        destroyed = true;
        for (int round = 0; round < DESTRUCTOR_ITERATIONS; ++round) {
            bool called = false;
            for (int key = 0; key < MAX_KEYS; ++key) {
                const ThreadValue entry = values[key];
                if (!entry.value) continue;
                GuestKeyDestructor destructor;
                {
                    std::lock_guard lock(g_keyLock);
                    destructor = g_keys[key].sequence.load(std::memory_order_relaxed) == entry.sequence ? g_keys[key].destructor : nullptr;
                }
                values[key] = {};
                if (destructor) {
                    destructor(entry.value);
                    called = true;
                }
            }
            if (!called) return;
        }
        values = {};
    }
};

static int KeySlot(PthreadKey key) {
    const unsigned slot = static_cast<unsigned>(key) - 1u;
    return slot < static_cast<unsigned>(MAX_KEYS) ? static_cast<int>(slot) : -1;
}

static ThreadValues& Values() {
#ifdef _WIN32
    return WindowsThreadLocal<ThreadValues>::Get();
#else
    static thread_local ThreadValues values;
    return values;
#endif
}

void ThreadLifecycle::RunKeyDtors() {
    Values().RunDestructors();
}

extern "C" {

int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    if (!key) throw std::runtime_error("scePthreadKeyCreate: null key");
    std::lock_guard lock(g_keyLock);
    for (int index = 0; index < MAX_KEYS; ++index) {
        if (g_keys[index].sequence.load(std::memory_order_relaxed) == FREE_KEY) {
            g_keys[index].destructor = reinterpret_cast<GuestKeyDestructor>(destructor);
            g_keys[index].sequence.store(++g_lastSequence, std::memory_order_release);
            *key = index + 1;
            return SCE_OK;
        }
    }
    return SCE_KERNEL_ERROR_EAGAIN;
}

int APS5_VABI scePthreadKeyDelete(PthreadKey key) {
    const int slot = KeySlot(key);
    if (slot < 0) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(g_keyLock);
    if (g_keys[slot].sequence.load(std::memory_order_relaxed) == FREE_KEY) return SCE_KERNEL_ERROR_EINVAL;
    g_keys[slot].sequence.store(FREE_KEY, std::memory_order_release);
    g_keys[slot].destructor = nullptr;
    return SCE_OK;
}

void* APS5_VABI scePthreadGetspecific(PthreadKey key) {
    const int slot = KeySlot(key);
    if (slot < 0) return nullptr;
    const ThreadValue& entry = Values().values[slot];
    return entry.sequence == g_keys[slot].sequence.load(std::memory_order_acquire) ? entry.value : nullptr;
}

int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value) {
    const int slot = KeySlot(key);
    if (slot < 0) return SCE_KERNEL_ERROR_EINVAL;
    const std::uint64_t sequence = g_keys[slot].sequence.load(std::memory_order_acquire);
    if (sequence == FREE_KEY) return SCE_KERNEL_ERROR_EINVAL;
    Values().values[slot] = {value, sequence};
    return SCE_OK;
}

}
