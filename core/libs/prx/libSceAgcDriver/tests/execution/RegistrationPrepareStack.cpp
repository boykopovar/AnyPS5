#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace {

constexpr std::uint64_t PoolStackBytes = 64ull << 20u;
constexpr std::uint64_t MostCommittedBytes = 8ull << 20u;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("registration prepare stack: " + message);
}

struct StackUse {
    std::thread::id thread;
    std::uint64_t reserved = 0;
    std::uint64_t committed = 0;
};

StackUse MeasureCurrentStack() {
    StackUse use;
    use.thread = std::this_thread::get_id();
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION here{};
    const int local = 0;
    Require(VirtualQuery(&local, &here, sizeof(here)) != 0, "VirtualQuery of the stack failed");
    auto cursor = reinterpret_cast<std::uintptr_t>(here.AllocationBase);
    for (;;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0 || info.AllocationBase != here.AllocationBase) break;
        use.reserved += info.RegionSize;
        if (info.State == MEM_COMMIT) use.committed += info.RegionSize;
        cursor = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    }
#elif defined(__APPLE__)
    use.reserved = pthread_get_stacksize_np(pthread_self());
#else
    pthread_attr_t attributes;
    Require(pthread_getattr_np(pthread_self(), &attributes) == 0, "pthread_getattr_np failed");
    void* base = nullptr;
    std::size_t size = 0;
    pthread_attr_getstack(&attributes, &base, &size);
    pthread_attr_destroy(&attributes);
    use.reserved = size;
#endif
    return use;
}

StackUse MeasureOn(const std::function<void(std::function<void()>)>& submit) {
    std::mutex mutex;
    std::condition_variable done;
    bool measured = false;
    StackUse use;
    submit([&] {
        const auto result = MeasureCurrentStack();
        std::lock_guard lock(mutex);
        use = result;
        measured = true;
        done.notify_all();
    });
    std::unique_lock lock(mutex);
    done.wait(lock, [&] { return measured; });
    return use;
}

void CheckPool(const char* name, const std::function<void(std::function<void()>)>& submit) {
    const auto use = MeasureOn(submit);
    Require(use.thread != std::this_thread::get_id(), std::string(name) + " ran the task on the caller's thread");
    Require(use.reserved >= PoolStackBytes, std::string(name) + " thread reserves " + std::to_string(use.reserved >> 20u) + " MiB of stack, less than 64 MiB");
#ifdef _WIN32
    Require(use.committed <= MostCommittedBytes, std::string(name) + " thread commits " + std::to_string(use.committed >> 20u) + " MiB of its " + std::to_string(use.reserved >> 20u) + " MiB stack up front");
#endif
}

}

int main() {
    try {
        AgcDriver::DriverDetail::Driver::Get();
        CheckPool("registration preparation", [](std::function<void()> task) { AgcDriver::DriverDetail::SubmitRegistrationPreparation(std::move(task)); });
        CheckPool("library prebuild", [](std::function<void()> task) { AgcDriver::DriverDetail::SubmitLibraryPrebuild(std::move(task)); });
        const char* prebuildThreads = std::getenv("APS5_PREBUILD_THREADS");
        const auto started = std::max(2u, std::thread::hardware_concurrency() / 2u) + (prebuildThreads != nullptr ? std::max(1u, static_cast<unsigned>(std::strtoul(prebuildThreads, nullptr, 10))) : std::max(2u, std::thread::hardware_concurrency() / 2u));
        const auto live = AgcDriver::DriverDetail::RegistrationPreparationThreads();
        Require(live == started, "the preparation and prebuild pools count " + std::to_string(live) + " live threads of the " + std::to_string(started) + " they started");
        AgcDriver::DriverDetail::CloseRegistrationPreparation();
        Require(AgcDriver::DriverDetail::RegistrationPreparationThreads() == 0, "closing the preparation and prebuild pools left " + std::to_string(AgcDriver::DriverDetail::RegistrationPreparationThreads()) + " live threads");
        std::puts("registration prepare stack tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
