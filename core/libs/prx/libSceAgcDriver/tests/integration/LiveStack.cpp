#include <Testing/Test.hpp>
#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#if defined(__linux__)
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Testing::Case;
using Testing::Require;
namespace GuestMemory = AgcDriver::GuestMemory;

void requireLiveFrameAccessible() {
    alignas(16) volatile std::uint64_t packet[2] = {1, 2};
    const auto* address = const_cast<const std::uint64_t*>(packet);
    Require(GuestMemory::Accessible(address, sizeof(packet)) && GuestMemory::Accessible(address, sizeof(packet), true), "a live stack frame is not accessible");
    GuestMemory::CheckRange(address, sizeof(packet), alignof(std::uint64_t), true);
}

const Case liveFrame{"Accessible_LiveStackFrame_IsReadableAndWritable", [] {
    requireLiveFrameAccessible();
}};

#if defined(__linux__)
class GuardedStack {
public:
    static constexpr std::size_t StackBytes = 256 * 1024;

    GuardedStack() : page(static_cast<std::size_t>(sysconf(_SC_PAGESIZE))) {
        auto* mapped = mmap(nullptr, StackBytes + page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "cannot map a test stack");
        block = static_cast<std::byte*>(mapped);
        Require(mprotect(block + StackBytes, page, PROT_NONE) == 0 && mprotect(block, page, PROT_NONE) == 0, "cannot protect the test stack's guard pages");
    }

    ~GuardedStack() {
        if (block != nullptr) munmap(block, StackBytes + page);
    }

    GuardedStack(const GuardedStack&) = delete;
    GuardedStack& operator=(const GuardedStack&) = delete;

    std::size_t Page() const {
        return page;
    }

    std::byte* Block() const {
        return block;
    }

private:
    std::size_t page;
    std::byte* block = nullptr;
};

struct StackProbe {
    std::uintptr_t bottom;
    std::uintptr_t top;
    std::uintptr_t guardEnd;
    std::exception_ptr failure;
};

void probeStack(const StackProbe& probe) {
    requireLiveFrameAccessible();
    Require(GuestMemory::Accessible(reinterpret_cast<const void*>(probe.top - 16), 16, true), "the top of the live stack is not accessible");
    Require(!GuestMemory::Accessible(reinterpret_cast<const void*>(probe.top - 16), 32), "a range past the stack's top counts as accessible");
    Require(!GuestMemory::Accessible(reinterpret_cast<const void*>(probe.top), probe.guardEnd - probe.top), "the page above the stack counts as accessible");
    Require(!GuestMemory::Accessible(reinterpret_cast<const void*>(probe.bottom), 16), "an inaccessible page of the stack below the live frames counts as accessible");
    Testing::RequireThrows<std::runtime_error>([&] { GuestMemory::CheckRange(reinterpret_cast<const void*>(probe.top - 8), 16, 8); }, "CheckRange accepted a range past the stack's top");
}

const Case guardedThreadStack{"Accessible_ThreadStackWithGuardPages_EndsAtTheStackTop", [] {
    GuardedStack stack;
    const auto top = reinterpret_cast<std::uintptr_t>(stack.Block()) + GuardedStack::StackBytes;
    StackProbe probe{reinterpret_cast<std::uintptr_t>(stack.Block()), top, top + stack.Page(), {}};
    pthread_attr_t attributes;
    Require(pthread_attr_init(&attributes) == 0, "cannot initialize the test stack attributes");
    const bool stackSet = pthread_attr_setstack(&attributes, stack.Block(), GuardedStack::StackBytes) == 0;
    pthread_t thread;
    const bool started = stackSet && pthread_create(&thread, &attributes, [](void* raw) -> void* {
        auto& probe = *static_cast<StackProbe*>(raw);
        try {
            probeStack(probe);
        } catch (...) {
            probe.failure = std::current_exception();
        }
        return nullptr;
    }, &probe) == 0;
    if (started) pthread_join(thread, nullptr);
    pthread_attr_destroy(&attributes);
    Require(stackSet, "cannot set the test stack");
    Require(started, "cannot start the stack test thread");
    if (probe.failure) std::rethrow_exception(probe.failure);
    Require(GuestMemory::Accessible(stack.Block() + stack.Page(), 64, true) && !GuestMemory::Accessible(stack.Block(), 16) && !GuestMemory::Accessible(stack.Block() + GuardedStack::StackBytes, 16), "the released test stack is misreported");
}};
#endif

} // namespace
