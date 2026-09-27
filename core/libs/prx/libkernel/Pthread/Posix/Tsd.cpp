#include <climits>
#include <map>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

struct KeyRegistry {
    std::mutex mutex;
    std::map<PthreadKey, pthread_key_destructor_func_t> keys;
    PthreadKey nextKey = 1;
};

// Keep the registry alive through thread-local cleanup at process exit.
KeyRegistry& Registry() {
    static auto* registry = new KeyRegistry;
    return *registry;
}

struct ThreadValues {
    std::unordered_map<PthreadKey, void*> values;

    ~ThreadValues() {
        // POSIX permits destructors to set their key again. Retry up to the
        // required four rounds, clearing each value before invoking its hook.
        for (int round = 0; round < 4; ++round) {
            std::vector<std::pair<pthread_key_destructor_func_t, void*>> callbacks;
            {
                auto& registry = Registry();
                std::lock_guard lock(registry.mutex);
                for (auto& [key, value] : values) {
                    const auto found = registry.keys.find(key);
                    if (value != nullptr && found != registry.keys.end() && found->second != nullptr) {
                        callbacks.emplace_back(found->second, value);
                        value = nullptr;
                    }
                }
            }
            if (callbacks.empty()) break;
            for (const auto& [callback, value] : callbacks) callback(value);
        }
    }
};

thread_local ThreadValues threadValues;

}

extern "C" {

void* APS5_VABI pthread_getspecific_nid_postfix(PthreadKey key) {
    auto& registry = Registry();
    std::lock_guard lock(registry.mutex);
    if (!registry.keys.contains(key)) return nullptr;
    const auto found = threadValues.values.find(key);
    return found == threadValues.values.end() ? nullptr : found->second;
}

int APS5_VABI pthread_setspecific_nid_postfix(PthreadKey key, const void* value) {
    auto& registry = Registry();
    std::lock_guard lock(registry.mutex);
    if (!registry.keys.contains(key)) return 22; // Guest EINVAL.
    threadValues.values[key] = const_cast<void*>(value);
    return 0;
}

int APS5_VABI pthread_key_create_nid_postfix(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    if (key == nullptr) return 22;
    auto& registry = Registry();
    std::lock_guard lock(registry.mutex);
    if (registry.nextKey == INT_MAX) return 35; // Guest EAGAIN.
    const auto newKey = registry.nextKey++;
    registry.keys.emplace(newKey, destructor);
    *key = newKey;
    return 0;
}

int APS5_VABI pthread_key_delete_nid_postfix(PthreadKey key) {
    auto& registry = Registry();
    std::lock_guard lock(registry.mutex);
    if (registry.keys.erase(key) == 0) return 22;
    threadValues.values.erase(key);
    return 0;
}

}
