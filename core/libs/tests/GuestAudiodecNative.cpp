#include "prx/libSceAudiodec.native/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/ExportLookup.hpp"
#include <cstddef>
#include <cstdio>

int main(int argc, char** argv) {
    struct Export {
        int (APS5_VABI* function)(void);
        const char* name;
    };
    const Export exports[] = {
        {sceAudiodecClearContextEx, "sceAudiodecClearContextEx"},
        {sceAudiodecCreateDecoderEx, "sceAudiodecCreateDecoderEx"},
        {sceAudiodecDecode2, "sceAudiodecDecode2"},
        {sceAudiodecDecode2Ex, "sceAudiodecDecode2Ex"},
        {sceAudiodecDecode2WithPriority, "sceAudiodecDecode2WithPriority"},
        {sceAudiodecDecode2WithPriorityEx, "sceAudiodecDecode2WithPriorityEx"},
        {sceAudiodecDecodeEx, "sceAudiodecDecodeEx"},
        {sceAudiodecDecodeWithPriority, "sceAudiodecDecodeWithPriority"},
        {sceAudiodecDecodeWithPriorityEx, "sceAudiodecDecodeWithPriorityEx"},
        {sceAudiodecDeleteDecoderEx, "sceAudiodecDeleteDecoderEx"},
        {sceAudiodecGetContext, "sceAudiodecGetContext"},
        {sceAudiodecInitialize, "sceAudiodecInitialize"},
        {sceAudiodecRegisterCodec, "sceAudiodecRegisterCodec"},
        {sceAudiodecTerminate, "sceAudiodecTerminate"},
    };
    constexpr std::size_t count = sizeof(exports) / sizeof(exports[0]);
    for (std::size_t i = 0; i < count; ++i) {
        if (exports[i].function == nullptr) return 1;
        for (std::size_t j = i + 1; j < count; ++j) {
            if (exports[i].function == exports[j].function) return 1;
        }
    }
    if (!ExportsResolveByName(argc > 1 ? argv[1] : nullptr, exports, "sceAudiodecNotExported")) return 1;
    std::puts("libSceAudiodec.native pending exports are exported");
    return 0;
}
