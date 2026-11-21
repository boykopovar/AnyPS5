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
    "sceAjmBatchCancel_nid_disambig1",
    "sceAjmBatchErrorDump_nid_disambig1",
    "sceAjmBatchGetLog_nid_disambig1",
    "sceAjmBatchInitialize_nid_disambig1",
    "sceAjmBatchJobClearContext_nid_disambig1",
    "sceAjmBatchJobControl_nid_disambig1",
    "sceAjmBatchJobControlBufferRa_nid_disambig2",
    "sceAjmBatchJobDecode_nid_disambig1",
    "sceAjmBatchJobDecodeSingle_nid_disambig1",
    "sceAjmBatchJobDecodeSplit_nid_disambig1",
    "sceAjmBatchJobEncode_nid_disambig1",
    "sceAjmBatchJobGetCodecInfo_nid_disambig1",
    "sceAjmBatchJobGetGaplessDecode_nid_disambig1",
    "sceAjmBatchJobGetInfo_nid_disambig1",
    "sceAjmBatchJobGetResampleInfo_nid_disambig1",
    "sceAjmBatchJobGetStatistics_nid_disambig1",
    "sceAjmBatchJobInitialize_nid_disambig1",
    "sceAjmBatchJobInlineBuffer_nid_disambig2",
    "sceAjmBatchJobRun_nid_disambig1",
    "sceAjmBatchJobRunBufferRa_nid_disambig2",
    "sceAjmBatchJobRunSplit_nid_disambig1",
    "sceAjmBatchJobRunSplitBufferRa_nid_disambig2",
    "sceAjmBatchJobSetGaplessDecode_nid_disambig1",
    "sceAjmBatchJobSetResampleParameters_nid_disambig1",
    "sceAjmBatchJobSetResampleParametersEx_nid_disambig1",
    "sceAjmBatchStart_nid_disambig1",
    "sceAjmBatchStartBuffer_nid_disambig1",
    "sceAjmBatchWait_nid_disambig1",
    "sceAjmDecAt9ParseConfigData_nid_disambig1",
    "sceAjmDecMp3ParseFrame_nid_disambig1",
    "sceAjmFinalize_nid_disambig1",
    "sceAjmInitialize_nid_disambig1",
    "sceAjmInstanceAvailable_nid_disambig1",
    "sceAjmInstanceCodecType_nid_disambig1",
    "sceAjmInstanceCreate_nid_disambig1",
    "sceAjmInstanceDestroy_nid_disambig1",
    "sceAjmInstanceExtend_nid_disambig1",
    "sceAjmInstanceSwitch_nid_disambig1",
    "sceAjmMemoryRegister_nid_disambig1",
    "sceAjmMemoryUnregister_nid_disambig1",
    "sceAjmModuleRegister_nid_disambig1",
    "sceAjmModuleUnregister_nid_disambig1",
    "sceAjmStrError_nid_disambig1",
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

    const auto strError = Resolve<const char* (APS5_VABI*)(int)>(ajmLibrary, "sceAjmStrError_nid_disambig1");
    for (const int code : {0, 1, static_cast<int>(0x80930005), static_cast<int>(0x8093000C), -1}) {
        Require(std::strcmp(strError(code), sceAjmStrError(code)) == 0, "sceAjmStrError_nid_disambig1 differs from sceAjmStrError");
    }

    const auto initialize = Resolve<int (APS5_VABI*)(std::int64_t, std::uint32_t*)>(ajmLibrary, "sceAjmInitialize_nid_disambig1");
    const auto finalize = Resolve<int (APS5_VABI*)(std::uint32_t)>(ajmLibrary, "sceAjmFinalize_nid_disambig1");
    std::uint32_t context = 0;
    Require(initialize(0, &context) == 0 && context != 0, "sceAjmInitialize_nid_disambig1 did not initialize");
    Require(finalize(context) == 0, "sceAjmFinalize_nid_disambig1 did not finalize");
    return 0;
}
