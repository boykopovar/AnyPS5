#include "prx/libSceAjm.native/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr const char* kExports[] = {
    "sceAjmBatchCancel_nid_postfix",
    "sceAjmBatchErrorDump_nid_postfix",
    "sceAjmBatchGetLog_nid_postfix",
    "sceAjmBatchInitialize_nid_postfix",
    "sceAjmBatchJobClearContext_nid_postfix",
    "sceAjmBatchJobControl_nid_postfix",
    "sceAjmBatchJobControlBufferRa_nid_postfix",
    "sceAjmBatchJobDecode_nid_postfix",
    "sceAjmBatchJobDecodeSingle_nid_postfix",
    "sceAjmBatchJobDecodeSplit_nid_postfix",
    "sceAjmBatchJobEncode_nid_postfix",
    "sceAjmBatchJobGetCodecInfo_nid_postfix",
    "sceAjmBatchJobGetGaplessDecode_nid_postfix",
    "sceAjmBatchJobGetInfo_nid_postfix",
    "sceAjmBatchJobGetResampleInfo_nid_postfix",
    "sceAjmBatchJobGetStatistics_nid_postfix",
    "sceAjmBatchJobInitialize_nid_postfix",
    "sceAjmBatchJobInlineBuffer_nid_postfix",
    "sceAjmBatchJobRun_nid_postfix",
    "sceAjmBatchJobRunBufferRa_nid_postfix",
    "sceAjmBatchJobRunSplit_nid_postfix",
    "sceAjmBatchJobRunSplitBufferRa_nid_postfix",
    "sceAjmBatchJobSetGaplessDecode_nid_postfix",
    "sceAjmBatchJobSetResampleParameters_nid_postfix",
    "sceAjmBatchJobSetResampleParametersEx_nid_postfix",
    "sceAjmBatchStart_nid_postfix",
    "sceAjmBatchStartBuffer_nid_postfix",
    "sceAjmBatchWait_nid_postfix",
    "sceAjmDecAt9ParseConfigData_nid_postfix",
    "sceAjmDecMp3ParseFrame_nid_postfix",
    "sceAjmFinalize_nid_postfix",
    "sceAjmInitialize_nid_postfix",
    "sceAjmInstanceAvailable_nid_postfix",
    "sceAjmInstanceCodecType_nid_postfix",
    "sceAjmInstanceCreate_nid_postfix",
    "sceAjmInstanceDestroy_nid_postfix",
    "sceAjmInstanceExtend_nid_postfix",
    "sceAjmInstanceSwitch_nid_postfix",
    "sceAjmMemoryRegister_nid_postfix",
    "sceAjmMemoryUnregister_nid_postfix",
    "sceAjmModuleRegister_nid_postfix",
    "sceAjmModuleUnregister_nid_postfix",
    "sceAjmStrError_nid_postfix",
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Ajm module exports: %s\n", message);
    std::abort();
}

void* Find(const char* library, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(LoadLibraryA(library), name));
#else
    void* module = library ? dlopen(library, RTLD_NOW) : dlopen(nullptr, RTLD_NOW);
    return module ? dlsym(module, name) : nullptr;
#endif
}

template <typename Function>
Function Resolve(const char* library, const char* name) {
    void* symbol = Find(library, name);
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by %s\n", name, library ? library : "main");
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    const char* ajmLibrary = argc > 1 ? argv[1] : "libSceAjm.prx";
#else
    const char* ajmLibrary = argc > 1 ? argv[1] : nullptr;
#endif
    for (const char* name : kExports) {
        void* const symbol = Find(ajmLibrary, name);
        if (!symbol) {
            std::fprintf(stderr, "%s is not exported by %s\n", name, ajmLibrary ? ajmLibrary : "main");
            std::abort();
        }
    }

    const auto strError = Resolve<const char* (APS5_VABI*)(int)>(ajmLibrary, "sceAjmStrError_nid_postfix");
    for (const int code : {0, 1, static_cast<int>(0x80930005), static_cast<int>(0x8093000C), -1}) {
        Require(std::strcmp(strError(code), sceAjmStrError(code)) == 0, "sceAjmStrError_nid_postfix differs from sceAjmStrError");
    }

    const auto initialize = Resolve<int (APS5_VABI*)(std::int64_t, std::uint32_t*)>(ajmLibrary, "sceAjmInitialize_nid_postfix");
    const auto finalize = Resolve<int (APS5_VABI*)(std::uint32_t)>(ajmLibrary, "sceAjmFinalize_nid_postfix");
    std::uint32_t context = 0;
    Require(initialize(0, &context) == 0 && context != 0, "sceAjmInitialize_nid_postfix did not initialize");
    Require(finalize(context) == 0, "sceAjmFinalize_nid_postfix did not finalize");
    return 0;
}
