#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <stdexcept>
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

struct InitEnvParams {
    int argc;
    std::uint32_t pad;
    const char* argv[3];
};

extern "C" {
int APS5_VABI cxa_atexit_nid_postfix(void (APS5_VABI *)(void*), void*, void*);
void APS5_VABI cxa_finalize_nid_postfix(void*);
int APS5_VABI LibcInternalExtCxaThreadAtexit_nid_postfix(void (APS5_VABI *)(void*), void*, void*);
void APS5_VABI init_env_nid_postfix(const InitEnvParams*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

struct ExitRecorder {
    int order = 0;
    int firstAt = -1;
    int secondAt = -1;
    int otherAt = -1;
};

void APS5_VABI First(void* recorder) {
    auto& record = *static_cast<ExitRecorder*>(recorder);
    record.firstAt = record.order++;
}

void APS5_VABI Second(void* recorder) {
    auto& record = *static_cast<ExitRecorder*>(recorder);
    record.secondAt = record.order++;
}

void APS5_VABI Other(void* recorder) {
    auto& record = *static_cast<ExitRecorder*>(recorder);
    record.otherAt = record.order++;
}

class ExitFixture {
public:
    ExitFixture() {
        RequireEqual(cxa_atexit_nid_postfix(First, &recorder, FirstDso()), 0, "register First");
        RequireEqual(cxa_atexit_nid_postfix(Second, &recorder, FirstDso()), 0, "register Second");
        RequireEqual(cxa_atexit_nid_postfix(Other, &recorder, OtherDso()), 0, "register Other");
    }

    ~ExitFixture() {
        cxa_finalize_nid_postfix(FirstDso());
        cxa_finalize_nid_postfix(OtherDso());
    }

    ExitFixture(const ExitFixture&) = delete;
    ExitFixture& operator=(const ExitFixture&) = delete;

    void* FirstDso() { return &firstDso; }
    void* OtherDso() { return &otherDso; }
    void* UnknownDso() { return &unknownDso; }

    ExitRecorder recorder;

private:
    char firstDso = 0;
    char otherDso = 0;
    char unknownDso = 0;
};

struct ThreadRecorder {
    std::atomic<int> cleanups{0};
    std::atomic<void*> objectSeen{nullptr};
};

void APS5_VABI ThreadDone(void* object) {
    auto* recorder = static_cast<ThreadRecorder*>(object);
    recorder->objectSeen.store(object);
    ++recorder->cleanups;
}

const Case finalizeHandle{"CxaFinalize_RegisteredHandle_RunsItsDestructorsInReverseOrder", [] {
    ExitFixture fixture;
    cxa_finalize_nid_postfix(fixture.FirstDso());
    RequireEqual(fixture.recorder.secondAt, 0, "Second runs first");
    RequireEqual(fixture.recorder.firstAt, 1, "First runs second");
    RequireEqual(fixture.recorder.otherAt, -1, "Other does not run");
}};

const Case finalizeUnknown{"CxaFinalize_UnknownHandle_RunsNothing", [] {
    ExitFixture fixture;
    cxa_finalize_nid_postfix(fixture.FirstDso());
    cxa_finalize_nid_postfix(fixture.UnknownDso());
    RequireEqual(fixture.recorder.otherAt, -1, "Other does not run");
    RequireEqual(fixture.recorder.order, 2, "no further destructor runs");
}};

const Case finalizeOther{"CxaFinalize_SecondHandle_RunsRemainingDestructor", [] {
    ExitFixture fixture;
    cxa_finalize_nid_postfix(fixture.FirstDso());
    cxa_finalize_nid_postfix(fixture.UnknownDso());
    cxa_finalize_nid_postfix(fixture.OtherDso());
    RequireEqual(fixture.recorder.otherAt, 2, "Other runs last");
}};

const Case threadAtexit{"CxaThreadAtexit_WorkerThreadExit_RunsDestructorOnceWithObject", [] {
    ThreadRecorder recorder;
    std::atomic<bool> imageFound{false};
    std::atomic<int> registration{-1};
    std::thread worker([&] {
        void* image =
#ifdef _WIN32
            reinterpret_cast<void*>(GetModuleHandleW(nullptr));
#else
            dlopen(nullptr, RTLD_NOW);
#endif
        imageFound = image != nullptr;
        if (image == nullptr) return;
        registration = LibcInternalExtCxaThreadAtexit_nid_postfix(ThreadDone, &recorder, image);
#ifndef _WIN32
        dlclose(image);
#endif
    });
    worker.join();
    Require(imageFound, "acquire the module handle");
    RequireEqual(registration.load(), 0, "registration");
    RequireEqual(recorder.cleanups.load(), 1, "destructor runs once");
    Require(recorder.objectSeen.load() == &recorder, "destructor receives the registered object");
}};

const Case initEnv{"InitEnv_NullParameters_DoesNotReportNotImplemented", [] {
    try {
        init_env_nid_postfix(nullptr);
    } catch (const std::runtime_error& error) {
        Require(std::strstr(error.what(), "not implemented") == nullptr,
            std::string("init_env reported: ") + error.what());
    }
}};

} // namespace
