#include "BdaTests.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#if defined(__linux__)
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using AgcDriver::Graphics::Require;
namespace GuestMemory = AgcDriver::GuestMemory;

void checkLiveStack(std::uintptr_t stackBottom, std::uintptr_t stackTop, std::uintptr_t guardEnd) {
    alignas(16) volatile std::uint64_t packet[2] = {1, 2};
    const auto* address = const_cast<const std::uint64_t*>(packet);
    Require(GuestMemory::Accessible(address, sizeof(packet)) && GuestMemory::Accessible(address, sizeof(packet), true), "a live stack frame is not accessible");
    GuestMemory::CheckRange(address, sizeof(packet), alignof(std::uint64_t), true);
    if (stackTop == 0) return;
    Require(GuestMemory::Accessible(reinterpret_cast<const void*>(stackTop - 16), 16, true), "the top of the live stack is not accessible");
    Require(!GuestMemory::Accessible(reinterpret_cast<const void*>(stackTop - 16), 32), "a range past the stack's top counts as accessible");
    Require(!GuestMemory::Accessible(reinterpret_cast<const void*>(stackTop), guardEnd - stackTop), "the page above the stack counts as accessible");
    Require(!GuestMemory::Accessible(reinterpret_cast<const void*>(stackBottom), 16), "an inaccessible page of the stack below the live frames counts as accessible");
    bool rejected = false;
    try {
        GuestMemory::CheckRange(reinterpret_cast<const void*>(stackTop - 8), 16, 8);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected, "CheckRange accepted a range past the stack's top");
}

}

void RunLiveStackAccessTests() {
    checkLiveStack(0, 0, 0);
#if defined(__linux__)
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    const std::size_t stackBytes = 256 * 1024;
    auto* block = static_cast<std::byte*>(mmap(nullptr, stackBytes + page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(block != MAP_FAILED, "cannot map a test stack");
    Require(mprotect(block + stackBytes, page, PROT_NONE) == 0 && mprotect(block, page, PROT_NONE) == 0, "cannot protect the test stack's guard pages");
    const auto top = reinterpret_cast<std::uintptr_t>(block) + stackBytes;
    struct Arguments {
        std::uintptr_t bottom;
        std::uintptr_t top;
        std::uintptr_t guardEnd;
        std::string failure;
    } arguments{reinterpret_cast<std::uintptr_t>(block), top, top + page, {}};
    pthread_attr_t attributes;
    Require(pthread_attr_init(&attributes) == 0 && pthread_attr_setstack(&attributes, block, stackBytes) == 0, "cannot set the test stack");
    pthread_t thread;
    Require(pthread_create(&thread, &attributes, [](void* raw) -> void* {
        auto& arguments = *static_cast<Arguments*>(raw);
        try {
            checkLiveStack(arguments.bottom, arguments.top, arguments.guardEnd);
        } catch (const std::exception& error) {
            arguments.failure = error.what();
        }
        return nullptr;
    }, &arguments) == 0, "cannot start the stack test thread");
    pthread_join(thread, nullptr);
    pthread_attr_destroy(&attributes);
    Require(GuestMemory::Accessible(block + page, 64, true) && !GuestMemory::Accessible(block, 16) && !GuestMemory::Accessible(block + stackBytes, 16), "the released test stack is misreported");
    munmap(block, stackBytes + page);
    Require(arguments.failure.empty(), arguments.failure.c_str());
#endif
}
