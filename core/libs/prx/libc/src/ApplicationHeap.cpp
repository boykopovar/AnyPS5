#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <algorithm>

namespace {

using Allocate = void* (APS5_VABI *)(std::size_t);
using Free = void (APS5_VABI *)(void*);
using Reallocate = void* (APS5_VABI *)(void*, std::size_t);
using Calloc = void* (APS5_VABI *)(std::size_t, std::size_t);
using Align = void* (APS5_VABI *)(std::size_t, std::size_t);
using PosixAlign = int (APS5_VABI *)(void**, std::size_t, std::size_t);
using Initialize = void (APS5_VABI *)();

std::mutex heapMutex;
std::array<void*, 10> heapApi{};
std::once_flag heapInitialization;
std::exception_ptr heapFailure;
Initialize heapFinalize = nullptr;
bool heapFinalized = false;
thread_local bool heapCallbackActive = false;

void* APS5_VABI defaultAllocate(std::size_t bytes) { return GuestHeap::GuestHeapAllocate_nid_postfix(bytes); }
void APS5_VABI defaultFree(void* pointer) { GuestHeap::GuestHeapFree_nid_postfix(pointer); }
void* APS5_VABI defaultReallocate(void* pointer, std::size_t bytes) { return GuestHeap::GuestHeapReallocate_nid_postfix(pointer, bytes); }
void* APS5_VABI defaultCalloc(std::size_t count, std::size_t bytes) {
    if (bytes != 0 && count > std::numeric_limits<std::size_t>::max() / bytes) throw std::bad_alloc();
    auto* pointer = defaultAllocate(count * bytes);
    std::memset(pointer, 0, count * bytes);
    return pointer;
}
void* APS5_VABI defaultAlign(std::size_t alignment, std::size_t bytes) { return GuestHeap::GuestHeapAlign_nid_postfix(alignment, bytes); }
int APS5_VABI defaultPosixAlign(void** pointer, std::size_t alignment, std::size_t bytes) {
    if (pointer == nullptr || alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0) return 22;
    try { *pointer = defaultAlign(alignment, bytes); return 0; }
    catch (const std::bad_alloc&) { return 12; }
}

std::array<void*, 10> defaultApi() {
    return {reinterpret_cast<void*>(defaultAllocate), reinterpret_cast<void*>(defaultFree),
        reinterpret_cast<void*>(defaultCalloc), reinterpret_cast<void*>(defaultReallocate),
        reinterpret_cast<void*>(defaultAlign), nullptr, reinterpret_cast<void*>(defaultPosixAlign)};
}

class CallbackScope {
public:
    CallbackScope() {
        if (heapCallbackActive) throw std::runtime_error("application heap: recursive libc allocator callback");
        heapCallbackActive = true;
    }
    ~CallbackScope() { heapCallbackActive = false; }
    CallbackScope(const CallbackScope&) = delete;
    CallbackScope& operator=(const CallbackScope&) = delete;
};

template<typename TValue>
TValue read(const void* pointer, std::size_t offset) {
    if (pointer == nullptr) throw std::invalid_argument("application heap: null metadata");
    TValue value;
    std::memcpy(&value, static_cast<const std::byte*>(pointer) + offset, sizeof(value));
    return value;
}

template<typename TCallback>
TCallback callback(std::size_t index) {
    std::lock_guard lock(heapMutex);
    if (heapFailure) std::rethrow_exception(heapFailure);
    if (heapFinalized) throw std::runtime_error("application heap: allocator has been finalized");
    if (heapApi[index] == nullptr) throw std::runtime_error("application heap: allocator API is not registered");
    static_assert(sizeof(TCallback) == sizeof(void*));
    TCallback result;
    std::memcpy(&result, &heapApi[index], sizeof(result));
    return result;
}

void* requireAllocation(void* pointer) {
    if (pointer == nullptr) throw std::bad_alloc();
    return pointer;
}

void requireAlignment(std::size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) throw std::invalid_argument("application heap: invalid alignment");
}

void finalize() {
    Initialize finalizeCallback;
    {
        std::lock_guard lock(heapMutex);
        if (heapFailure) std::rethrow_exception(heapFailure);
        if (heapFinalized) throw std::runtime_error("application heap: duplicate finalization");
        finalizeCallback = heapFinalize;
    }
    if (finalizeCallback != nullptr) finalizeCallback();
    std::lock_guard lock(heapMutex);
    heapFinalized = true;
}

}

void ApplicationHeapRegister_nid_no_patch(void* const* api) {
    if (api == nullptr) throw std::invalid_argument("application heap: null allocator API");
    std::array<void*, 10> replacement;
    std::memcpy(replacement.data(), api, sizeof(replacement));
    if (std::all_of(replacement.begin(), replacement.end(), [](const void* entry) { return entry == nullptr; })) {
        replacement = defaultApi();
    } else {
        for (std::size_t index = 0; index < 7; ++index) {
            if (replacement[index] == nullptr) throw std::invalid_argument("application heap: incomplete allocator API");
        }
    }
    std::lock_guard lock(heapMutex);
    if (heapFailure) std::rethrow_exception(heapFailure);
    if (heapFinalized) throw std::runtime_error("application heap: allocator has been finalized");
    if (heapApi[0] != nullptr && heapApi != replacement) throw std::runtime_error("application heap: cannot replace an active allocator");
    heapApi = replacement;
}

void ApplicationHeapInitialize_nid_no_patch(const void* processParameters) {
    std::call_once(heapInitialization, [processParameters] {
        try {
            if (read<std::uint64_t>(processParameters, 0) < 0x40 || read<std::uint32_t>(processParameters, 8) != 0x4942524f) throw std::runtime_error("application heap: invalid process parameters");
            const auto* libcParameters = read<const void*>(processParameters, 0x38);
            if (read<std::uint64_t>(libcParameters, 0) < 0x38) throw std::runtime_error("application heap: invalid libc parameters");
            const auto* replacement = read<const void*>(libcParameters, 0x30);
            if (read<std::uint64_t>(replacement, 0) != 0x78 || read<std::uint64_t>(replacement, 8) != 2) throw std::runtime_error("application heap: unsupported allocator replacement table");
            std::array<void*, 10> api;
            std::memcpy(api.data(), static_cast<const std::byte*>(replacement) + 0x20, sizeof(api));
            ApplicationHeapRegister_nid_no_patch(api.data());
            const auto initialize = read<Initialize>(replacement, 0x10);
            if (initialize != nullptr) initialize();
            {
                std::lock_guard lock(heapMutex);
                heapFinalize = read<Initialize>(replacement, 0x18);
            }
            if (std::atexit(finalize) != 0) throw std::runtime_error("application heap: cannot register finalization");
        } catch (...) {
            std::lock_guard lock(heapMutex);
            heapFailure = std::current_exception();
        }
    });
    std::lock_guard lock(heapMutex);
    if (heapFailure) std::rethrow_exception(heapFailure);
}

void* ApplicationHeapAllocate_nid_no_patch(std::size_t bytes) {
    const auto allocate = callback<Allocate>(0);
    CallbackScope scope;
    return requireAllocation(allocate(bytes));
}

void ApplicationHeapFree_nid_no_patch(void* pointer) {
    if (pointer == nullptr) return;
    const auto free = callback<Free>(1);
    CallbackScope scope;
    free(pointer);
}

void* ApplicationHeapReallocate_nid_no_patch(void* pointer, std::size_t bytes) {
    if (bytes == 0) {
        ApplicationHeapFree_nid_no_patch(pointer);
        return nullptr;
    }
    const auto reallocate = callback<Reallocate>(3);
    CallbackScope scope;
    return requireAllocation(reallocate(pointer, bytes));
}

void* ApplicationHeapAlign_nid_no_patch(std::size_t alignment, std::size_t bytes) {
    requireAlignment(alignment);
    const auto align = callback<Align>(4);
    CallbackScope scope;
    void* pointer = requireAllocation(align(alignment, bytes));
    if (reinterpret_cast<std::uintptr_t>(pointer) % alignment != 0) throw std::runtime_error("application heap: allocator returned a misaligned pointer");
    return pointer;
}

void* ApplicationHeapCalloc_nid_no_patch(std::size_t count, std::size_t bytes) {
    if (bytes != 0 && count > std::numeric_limits<std::size_t>::max() / bytes) throw std::length_error("application heap: calloc size overflow");
    const auto calloc = callback<Calloc>(2);
    CallbackScope scope;
    return requireAllocation(calloc(count, bytes));
}

int ApplicationHeapPosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t bytes) {
    if (pointer == nullptr) throw std::invalid_argument("application heap: null allocation output");
    requireAlignment(alignment);
    if (alignment < sizeof(void*)) throw std::invalid_argument("application heap: invalid POSIX alignment");
    const auto align = callback<PosixAlign>(6);
    CallbackScope scope;
    void* result = nullptr;
    if (align(&result, alignment, bytes) != 0) throw std::runtime_error("application heap: posix_memalign failed");
    requireAllocation(result);
    if (reinterpret_cast<std::uintptr_t>(result) % alignment != 0) throw std::runtime_error("application heap: allocator returned a misaligned pointer");
    *pointer = result;
    return 0;
}
