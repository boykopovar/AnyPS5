#include "prx/libSceOpusCeltDec/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusDec/include/OpusDecoder.hpp"
#include <cstdarg>
#include <cstdint>
#include "prx/libc/include/General.hpp"

namespace {

OpusDecoderLibrary library("OpusCeltDec", true);

} // namespace

extern "C" {

int APS5_VABI sceOpusCeltDecInitialize(std::uint32_t* context) { return library.Initialize(context); }

int APS5_VABI sceOpusCeltDecTerminate(const std::uint32_t* context) { return library.Terminate(context); }

int APS5_VABI sceOpusCeltDecGetSize(int channels) { return library.GetSize(channels); }

int APS5_VABI sceOpusCeltDecCreateEx(const std::uint32_t* context, void* state, int sampleRate, int channels) {
    return library.CreateEx(context, state, sampleRate, channels);
}

int APS5_VABI sceOpusCeltDecDecode(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity) {
    return library.Decode(state, data, bytes, pcm, capacity);
}

int APS5_VABI sceOpusCeltDecDestroy(void* state) { return library.Destroy(state); }

int APS5_VABI sceOpusCeltDecCreate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecCtl(void* state, int request, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list list;
    __builtin_sysv_va_start(list, request);
    const auto arguments = OpusDecoderLibrary::ReadCtlArguments(request, [&] { return __builtin_va_arg(list, int); }, [&] { return __builtin_va_arg(list, std::int32_t*); });
    __builtin_sysv_va_end(list);
#else
    std::va_list list;
    va_start(list, request);
    const auto arguments = OpusDecoderLibrary::ReadCtlArguments(request, [&] { return va_arg(list, int); }, [&] { return va_arg(list, std::int32_t*); });
    va_end(list);
#endif
    return library.Ctl(state, request, arguments);
}

int APS5_VABI sceOpusCeltDecDecodeFloat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecDecodeFloatWithPriority() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecDecodeWithPriority() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
