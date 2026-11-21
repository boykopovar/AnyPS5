#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_ATOMICSHAREDPTR_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_ATOMICSHAREDPTR_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

template <class T>
class AtomicSharedPtrFallback {
public:
    AtomicSharedPtrFallback() noexcept = default;
    AtomicSharedPtrFallback(std::shared_ptr<T> desired) noexcept : value(std::move(desired)) {}
    AtomicSharedPtrFallback(const AtomicSharedPtrFallback&) = delete;
    AtomicSharedPtrFallback& operator=(const AtomicSharedPtrFallback&) = delete;

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
        std::shared_ptr<T> replaced;
        bool matched = false;
        {
            std::lock_guard lock(mutex);
            if (value.get() == expected.get() && !value.owner_before(expected) && !expected.owner_before(value)) {
                replaced = std::move(value);
                value = std::move(desired);
                matched = true;
            } else {
                replaced = std::move(expected);
                expected = value;
            }
        }
        return matched;
    }
    bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired, std::memory_order success, std::memory_order) noexcept {
        return compare_exchange_strong(expected, std::move(desired), success);
    }
    bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
        return compare_exchange_strong(expected, std::move(desired), order);
    }
    bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired, std::memory_order success, std::memory_order failure) noexcept {
        return compare_exchange_strong(expected, std::move(desired), success, failure);
    }
    operator std::shared_ptr<T>() const noexcept { return load(); }
    void operator=(std::shared_ptr<T> desired) noexcept { store(std::move(desired)); }

private:
    mutable std::mutex mutex;
    std::shared_ptr<T> value;
};

#if defined(__cpp_lib_atomic_shared_ptr)
template <class T>
using AtomicSharedPtr = std::atomic<std::shared_ptr<T>>;
#else
template <class T>
using AtomicSharedPtr = AtomicSharedPtrFallback<T>;
#endif

#endif
