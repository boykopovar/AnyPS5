#pragma once
#include "prx/libSceIcu/Status.hpp"
#include <cstdint>
#include <mutex>
#include <new>
#include <unordered_set>

namespace Icu {

constexpr std::int32_t MemoryAllocationError = 7;

template<class THandle>
class ResultHandles {
public:
    THandle* Open(std::int32_t* status) {
        if (Failure(status)) return nullptr;
        auto* result = new (std::nothrow) Result();
        if (!result) {
            *status = MemoryAllocationError;
            return nullptr;
        }
        try {
            std::lock_guard lock(mutex);
            live.insert(result);
        } catch (const std::bad_alloc&) {
            delete result;
            *status = MemoryAllocationError;
            return nullptr;
        }
        return reinterpret_cast<THandle*>(result);
    }

    void Close(THandle* handle) {
        auto* result = reinterpret_cast<Result*>(handle);
        {
            std::lock_guard lock(mutex);
            if (live.erase(result) == 0) return;
        }
        delete result;
    }

private:
    struct Result {};

    std::mutex mutex;
    std::unordered_set<const Result*> live;
};

}
