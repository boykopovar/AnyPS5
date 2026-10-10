#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using GuestDestructor = void(APS5_VABI *)(void*);

extern "C" {
int APS5_VABI __cxa_thread_atexit_impl_nid_postfix(GuestDestructor, void*, void*);
int APS5_VABI LibcInternalExtCxaThreadAtexit_nid_postfix(GuestDestructor, void*, void*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

struct Call {
    int id = 0;
    void* object = nullptr;
};

struct Recorder;

struct Slot {
    Recorder* owner;
    int id;
};

struct Recorder {
    std::array<Call, 3> calls{};
    std::atomic<std::size_t> callCount{0};
    std::array<Slot, 3> slots{{{this, 1}, {this, 2}, {this, 3}}};
};

void Record(void* object) {
    auto* slot = static_cast<Slot*>(object);
    const auto index = slot->owner->callCount.fetch_add(1, std::memory_order_relaxed);
    if (index < slot->owner->calls.size()) slot->owner->calls[index] = {slot->id, object};
}

void APS5_VABI FirstDestructor(void* object) { Record(object); }
void APS5_VABI SecondDestructor(void* object) { Record(object); }
void APS5_VABI ThirdDestructor(void* object) { Record(object); }

class ModuleHandle {
public:
    ModuleHandle()
#ifdef _WIN32
        : handle(reinterpret_cast<void*>(GetModuleHandleW(nullptr)))
#else
        : handle(dlopen(nullptr, RTLD_NOW))
#endif
    {
        Require(handle != nullptr, "acquire the test module handle");
    }

    ~ModuleHandle() {
#ifndef _WIN32
        dlclose(handle);
#endif
    }

    ModuleHandle(const ModuleHandle&) = delete;
    ModuleHandle& operator=(const ModuleHandle&) = delete;

    void* Get() const { return handle; }

    void* DirectDso() const {
#ifdef _WIN32
        return nullptr;
#else
        return handle;
#endif
    }

private:
    void* handle;
};

struct RegistrationResults {
    int first = -1;
    int second = -1;
    int third = -1;
};

RegistrationResults RegisterOnWorkerThread(Recorder& recorder) {
    const ModuleHandle module;
    RegistrationResults results;
    std::thread worker([&] {
        results.first = __cxa_thread_atexit_impl_nid_postfix(FirstDestructor, &recorder.slots[0], module.DirectDso());
        results.second = __cxa_thread_atexit_impl_nid_postfix(SecondDestructor, &recorder.slots[1], module.DirectDso());
        results.third = LibcInternalExtCxaThreadAtexit_nid_postfix(ThirdDestructor, &recorder.slots[2], module.Get());
    });
    worker.join();
    return results;
}

const Case registration{"ThreadAtexit_RegistrationOnWorkerThread_Succeeds", [] {
    Recorder recorder;
    const auto results = RegisterOnWorkerThread(recorder);
    RequireEqual(results.first, 0, "__cxa_thread_atexit_impl first destructor");
    RequireEqual(results.second, 0, "__cxa_thread_atexit_impl second destructor");
    RequireEqual(results.third, 0, "LibcInternalExtCxaThreadAtexit third destructor");
}};

const Case reverseOrder{"ThreadAtexit_WorkerThreadExit_RunsEachDestructorOnceInReverseOrder", [] {
    Recorder recorder;
    RegisterOnWorkerThread(recorder);
    RequireEqual(recorder.callCount.load(), recorder.calls.size(), "destructor calls");
    const std::array<Call, 3> expected{{{3, &recorder.slots[2]}, {2, &recorder.slots[1]}, {1, &recorder.slots[0]}}};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const std::string position = "call " + std::to_string(index);
        RequireEqual(recorder.calls[index].id, expected[index].id, position + " destructor id");
        RequireEqual(recorder.calls[index].object, expected[index].object, position + " object");
    }
}};

} // namespace
