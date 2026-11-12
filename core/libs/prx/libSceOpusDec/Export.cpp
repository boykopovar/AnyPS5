#include "prx/libSceOpusDec/include/InventoryExports.hpp"
#include "prx/libSceOpusDec/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusDec/include/OpusDecoder.hpp"
#include "prx/libSceOpusDec/include/OpusDecodeVariants.hpp"
#include <cstdarg>
#include <cstdint>
#include "prx/libc/include/General.hpp"

namespace {

OpusDecoderLibrary library("OpusDec", false);

constexpr std::uint32_t ErrorBase = 0x81740000u;

} // namespace

extern "C" {

int APS5_VABI sceOpusDecInitialize(std::uint32_t* context) { return library.Initialize(context); }

int APS5_VABI sceOpusDecTerminate(const std::uint32_t* context) { return library.Terminate(context); }

int APS5_VABI sceOpusDecGetSize(int channels) { return library.GetSize(channels); }

int APS5_VABI sceOpusDecCreateEx(const std::uint32_t* context, void* state, int sampleRate, int channels) {
    return library.CreateEx(context, state, sampleRate, channels);
}

int APS5_VABI sceOpusDecDecode(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity) {
    return library.Decode(state, data, bytes, pcm, capacity);
}

int APS5_VABI sceOpusDecDestroy(void* state) { return library.Destroy(state); }

int APS5_VABI sceOpusDecCancel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecCreate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecCtl(void* state, int request, ...) {
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

int APS5_VABI sceOpusDecDecodeFloat(void* state, const std::uint8_t* data, int bytes, float* pcm, int capacity) {
    return OpusDecodeVariant(library, ErrorBase, state, data, bytes, pcm, capacity, OpusDefaultDecodePriority, true);
}

int APS5_VABI sceOpusDecDecodeFloatWithPriority(void* state, const std::uint8_t* data, int bytes, float* pcm, int capacity, int priority) {
    return OpusDecodeVariant(library, ErrorBase, state, data, bytes, pcm, capacity, priority, true);
}

int APS5_VABI sceOpusDecDecodeFloatWithPriorityAndTimeout(void* state, const std::uint8_t* data, int bytes, float* pcm, int capacity, int priority,
                                                         std::uint32_t timeout, std::uint32_t*) {
    return OpusDecodeVariantWithTimeout(library, ErrorBase, state, data, bytes, pcm, capacity, priority, true, timeout, __func__);
}

int APS5_VABI sceOpusDecDecodeWithPriority(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity, int priority) {
    return OpusDecodeVariant(library, ErrorBase, state, data, bytes, pcm, capacity, priority, false);
}

int APS5_VABI sceOpusDecDecodeWithPriorityAndTimeout(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity, int priority,
                                                    std::uint32_t timeout, std::uint32_t*) {
    return OpusDecodeVariantWithTimeout(library, ErrorBase, state, data, bytes, pcm, capacity, priority, false, timeout, __func__);
}

}

extern "C" {

int APS5_VABI sceOpusDecGetAjmContext() {
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libSceOpusDecUnknown__minus_NcPFjTj5zU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-NcPFjTj5zU", libSceOpusDecUnknown__minus_NcPFjTj5zU_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_EF6FftzF33M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("EF6FftzF33M", libSceOpusDecUnknown_EF6FftzF33M_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_I1ugumHd0_plus_A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("I1ugumHd0+A", libSceOpusDecUnknown_I1ugumHd0_plus_A_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_JZapTsLx0j0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("JZapTsLx0j0", libSceOpusDecUnknown_JZapTsLx0j0_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_VsujeTF0_plus_xk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VsujeTF0+xk", libSceOpusDecUnknown_VsujeTF0_plus_xk_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_ZBx02KtHz2Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZBx02KtHz2Y", libSceOpusDecUnknown_ZBx02KtHz2Y_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_ark0tPj4meI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ark0tPj4meI", libSceOpusDecUnknown_ark0tPj4meI_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_ct42RpB2CaM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ct42RpB2CaM", libSceOpusDecUnknown_ct42RpB2CaM_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_eJQx7Emrwb0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("eJQx7Emrwb0", libSceOpusDecUnknown_eJQx7Emrwb0_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_ewfq1d5iJhU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ewfq1d5iJhU", libSceOpusDecUnknown_ewfq1d5iJhU_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_gMazl3u8Y40_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gMazl3u8Y40", libSceOpusDecUnknown_gMazl3u8Y40_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_jXVNwfmd1II_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jXVNwfmd1II", libSceOpusDecUnknown_jXVNwfmd1II_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_lFTdHt_minus_AuKU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lFTdHt-AuKU", libSceOpusDecUnknown_lFTdHt_minus_AuKU_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_sr25irrwGBI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("sr25irrwGBI", libSceOpusDecUnknown_sr25irrwGBI_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_ygCoF49g3F0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ygCoF49g3F0", libSceOpusDecUnknown_ygCoF49g3F0_nid_no_patch);

}
