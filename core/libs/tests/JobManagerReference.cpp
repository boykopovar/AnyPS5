#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <string>
#endif

struct MemorySizeQueryParams;
struct ConfigParams;
struct SequenceFactory;

int RunJobManagerCase(const char* name);

template <typename T>
static T Resolve(const char* nid) {
    const bool host = std::getenv("ANYPS5_JOB_MANAGER_HOST") != nullptr;
    const char* library = host ? "libSceJobManager.prx" : "libSceJobManager.prx.guest.prx";
#ifdef _WIN32
    HMODULE module = GetModuleHandleA(library);
    if (module == nullptr && host) module = LoadLibraryA("libs\\libSceJobManager.prx");
    const FARPROC symbol = module != nullptr ? GetProcAddress(module, nid) : nullptr;
    if (symbol == nullptr) {
        std::fprintf(stderr, "JobManager reference: missing %s in %s (Windows error %lu)\n", nid, library, GetLastError());
        std::exit(3);
    }
#else
    static void* module = dlopen(host ? "libs/libSceJobManager.prx" : "app0/sce_module/libSceJobManager.prx.guest.prx", RTLD_NOW | RTLD_LOCAL);
    const std::string name = host ? nid : std::string(nid) + "#guest";
    void* symbol = module != nullptr ? dlsym(module, name.c_str()) : nullptr;
    if (symbol == nullptr) {
        std::fprintf(stderr, "JobManager reference: missing %s in %s (%s)\n", name.c_str(), library, dlerror());
        std::exit(3);
    }
#endif
    return reinterpret_cast<T>(symbol);
}

extern "C" {

void APS5_VABI _ZN3sce3Job10JobManagerC1Ev(void* self) {
    static const auto call = Resolve<decltype(&_ZN3sce3Job10JobManagerC1Ev)>("cwMzzdesLlY");
    call(self);
}

void APS5_VABI _ZN3sce3Job10JobManagerD1Ev(void* self) {
    static const auto call = Resolve<decltype(&_ZN3sce3Job10JobManagerD1Ev)>("4g1ee34TMH8");
    call(self);
}

std::uint64_t APS5_VABI _ZN3sce3Job10JobManager27calculateRequiredMemorySizeEPKNS1_21MemorySizeQueryParamsE(const MemorySizeQueryParams* query) {
    static const auto call = Resolve<decltype(&_ZN3sce3Job10JobManager27calculateRequiredMemorySizeEPKNS1_21MemorySizeQueryParamsE)>("Nm2BhAwYNFk");
    return call(query);
}

int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(void* self, void* memory, std::uint64_t size, std::uint32_t workers, std::uint32_t sequences, std::uint32_t computeSequences, std::uint64_t mask, const ConfigParams* config) {
    static const auto call = Resolve<decltype(&_ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE)>("GS9DSbzC+MY");
    return call(self, memory, size, workers, sequences, computeSequences, mask, config);
}

int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjPKNS1_12ConfigParamsE(void* self, void* memory, std::uint64_t size, std::uint32_t workers, std::uint32_t sequences, const ConfigParams* config) {
    static const auto call = Resolve<decltype(&_ZN3sce3Job10JobManager10initializeEPvmjjPKNS1_12ConfigParamsE)>("d205iUFMyyE");
    return call(self, memory, size, workers, sequences, config);
}

int APS5_VABI _ZN3sce3Job10JobManager8shutdownEv(void* self) {
    static const auto call = Resolve<decltype(&_ZN3sce3Job10JobManager8shutdownEv)>("chxnvsxxfBY");
    return call(self);
}

SequenceFactory* APS5_VABI _ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE(void* self, std::uint32_t priority) {
    static const auto call = Resolve<decltype(&_ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE)>("W-yXgQmaozU");
    return call(self, priority);
}

SequenceFactory* APS5_VABI _ZN3sce3Job10JobManager34getUnifiedSequenceFactoryInterfaceENS1_8PriorityE(void* self, std::uint32_t priority) {
    static const auto call = Resolve<decltype(&_ZN3sce3Job10JobManager34getUnifiedSequenceFactoryInterfaceENS1_8PriorityE)>("L5jTAtk3HKA");
    return call(self, priority);
}

int APS5_VABI _ZN3sce3Job10JobManager24setPersistentThreadCountEj(void* self, std::uint32_t count) {
    static const auto call = Resolve<decltype(&_ZN3sce3Job10JobManager24setPersistentThreadCountEj)>("VTKezfYu-v4");
    return call(self, count);
}

int APS5_VABI Run_nid_no_patch() {
    const char* name = std::getenv("ANYPS5_JOB_MANAGER_CASE");
    const int result = name != nullptr ? RunJobManagerCase(name) : 2;
#ifdef _WIN32
    return result;
#else
    std::exit(result);
#endif
}

}
