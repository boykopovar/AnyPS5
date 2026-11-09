#ifndef CORE_SHADER_RECOMPILER_THREADOWNED_HPP
#define CORE_SHADER_RECOMPILER_THREADOWNED_HPP

#include <memory>
#include <pthread.h>
#include <stdexcept>
#include <vector>

namespace ShaderRecompiler {

namespace ThreadOwnedDetail {

struct Owned {
    void* object;
    void (*destroy)(void*);
};

inline void DestroyAll(void* value) {
    auto* owned = static_cast<std::vector<Owned>*>(value);
    for (auto it = owned->rbegin(); it != owned->rend(); ++it) it->destroy(it->object);
    delete owned;
}

inline pthread_key_t Key() {
    static const pthread_key_t key = [] {
        pthread_key_t created{};
        if (pthread_key_create(&created, DestroyAll) != 0) throw std::runtime_error("ThreadOwned: pthread_key_create failed");
        return created;
    }();
    return key;
}

inline void DestroyAtThreadExit(void* object, void (*destroy)(void*)) {
    auto* owned = static_cast<std::vector<Owned>*>(pthread_getspecific(Key()));
    if (owned == nullptr) {
        owned = new std::vector<Owned>();
        if (pthread_setspecific(Key(), owned) != 0) {
            delete owned;
            throw std::runtime_error("ThreadOwned: pthread_setspecific failed");
        }
    }
    owned->push_back({object, destroy});
}

}

template<typename T>
T& ThreadOwned(T*& slot) {
    if (slot == nullptr) {
        auto object = std::make_unique<T>();
        ThreadOwnedDetail::DestroyAtThreadExit(object.get(), [](void* value) { delete static_cast<T*>(value); });
        slot = object.release();
    }
    return *slot;
}

}

#endif
