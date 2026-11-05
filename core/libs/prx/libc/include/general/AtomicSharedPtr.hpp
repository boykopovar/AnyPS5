#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_ATOMICSHAREDPTR_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_ATOMICSHAREDPTR_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

#if defined(__cpp_lib_atomic_shared_ptr)
template <class T>
using AtomicSharedPtr = std::atomic<std::shared_ptr<T>>;
#else
template <class T>
class AtomicSharedPtr {
public:
    AtomicSharedPtr() noexcept = default;
    AtomicSharedPtr(std::shared_ptr<T> desired) noexcept : value(std::move(desired)) {}
    AtomicSharedPtr(const AtomicSharedPtr&) = delete;
    AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

    std::shared_ptr<T> load(std::memory_order = std::memory_order_seq_cst) const noexcept {
        std::lock_guard lock(mutex);
        return value;
    }
    void store(std::shared_ptr<T> desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
        exchange(std::move(desired), order);
    }
    std::shared_ptr<T> exchange(std::shared_ptr<T> desired, std::memory_order = std::memory_order_seq_cst) noexcept {
        std::lock_guard lock(mutex);
        return std::exchange(value, std::move(desired));
    }
    bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired, std::memory_order = std::memory_order_seq_cst) noexcept {
        std::lock_guard lock(mutex);
        if (value == expected) {
            value = std::move(desired);
            return true;
        }
        expected = value;
        return false;
    }
    operator std::shared_ptr<T>() const noexcept { return load(); }
    void operator=(std::shared_ptr<T> desired) noexcept { store(std::move(desired)); }

private:
    mutable std::mutex mutex;
    std::shared_ptr<T> value;
};
#endif

#endif
