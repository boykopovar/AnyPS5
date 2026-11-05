#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_MEMORYACCESSSCOPE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_MEMORYACCESSSCOPE_HPP

#include <cstddef>
#include <cstdint>

namespace AgcDriver::GuestMemory {

class MemoryAccessScope {
public:
    using Resolver = void (*)(void*, std::uint64_t, std::size_t, bool);
    MemoryAccessScope(void* context, Resolver resolver) : previousContext(currentContext), previousResolver(currentResolver) {
        currentContext = context;
        currentResolver = resolver;
    }
    ~MemoryAccessScope() {
        currentContext = previousContext;
        currentResolver = previousResolver;
    }
    MemoryAccessScope(const MemoryAccessScope&) = delete;
    MemoryAccessScope& operator=(const MemoryAccessScope&) = delete;
    static void Resolve(std::uint64_t address, std::size_t bytes, bool writable) {
        const auto resolver = currentResolver;
        const auto context = currentContext;
        if (resolver == nullptr || bytes == 0) return;
        const MemoryAccessScope suspended(nullptr, nullptr);
        resolver(context, address, bytes, writable);
    }

private:
    inline static thread_local void* currentContext = nullptr;
    inline static thread_local Resolver currentResolver = nullptr;
    void* previousContext;
    Resolver previousResolver;
};

}

#endif
