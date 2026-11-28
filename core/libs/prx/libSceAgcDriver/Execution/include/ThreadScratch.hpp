#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_THREADSCRATCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_THREADSCRATCH_HPP

#include "prx/libc/include/HostThreadSlot.hpp"
#include "ThreadOwned.hpp"
#include <memory>

namespace AgcDriver {

template<typename T, typename Tag>
T& ThreadScratch() {
    auto* object = HostThreadSlot<T*, Tag>::Get();
    if (object == nullptr) {
        ShaderRecompiler::ThreadOwned(object);
        HostThreadSlot<T*, Tag>::Set(object);
    }
    return *object;
}

template<typename T, typename Tag>
class ExclusiveScratch {
public:
    ExclusiveScratch() {
        auto& slot = Slot();
        if (slot.busy) {
            owned = std::make_unique<Held>();
            held = owned.get();
        } else {
            held = &slot;
        }
        held->busy = true;
    }

    ~ExclusiveScratch() {
        held->value.clear();
        held->busy = false;
    }

    ExclusiveScratch(const ExclusiveScratch&) = delete;
    ExclusiveScratch& operator=(const ExclusiveScratch&) = delete;

    T* operator->() const { return &held->value; }
    T& operator*() const { return held->value; }
    bool Nested() const { return owned != nullptr; }

    static bool Busy() { return Slot().busy; }
    static const T& Resting() { return Slot().value; }

private:
    struct Held {
        T value;
        bool busy = false;
    };

    static Held& Slot() { return ThreadScratch<Held, Tag>(); }

    std::unique_ptr<Held> owned;
    Held* held = nullptr;
};

}

#endif
