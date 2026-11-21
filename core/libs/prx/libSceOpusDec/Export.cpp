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

int APS5_VABI sceOpusDecCancel(void* argument1) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecCreate(void* argument1, std::int32_t argument2, std::int32_t argument3) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    if (argument2 != 8000 && argument2 != 12000 && argument2 != 16000 && argument2 != 24000 && argument2 != 48000) return static_cast<int>(ErrorBase + 0x2);
    if (argument3 != 1 && argument3 != 2) return static_cast<int>(ErrorBase + 0x3);
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

int APS5_VABI sceOpusDecGetAjmContext(const void* state, std::uint32_t* ajmContext) {
    if (state == nullptr) return static_cast<int>(ErrorBase);
    if (ajmContext == nullptr) return 0;
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libSceOpusDecUnknown__minus_NcPFjTj5zU_nid_no_patch(void* argument1, const void* argument2, std::uint32_t argument3, void* argument4, std::int32_t argument5, std::int32_t argument6) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    if (argument2 == nullptr && argument3 != 0) return static_cast<int>(ErrorBase + 0x6);
    if (argument3 > 0x1f4b) return static_cast<int>(ErrorBase + 0x7);
    if (argument4 == nullptr) return static_cast<int>(ErrorBase + 0x8);
    if (argument5 <= 0) return static_cast<int>(ErrorBase + 0x9);
    if (argument6 < 20 || argument6 > 100) return static_cast<int>(ErrorBase + 0xa);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("-NcPFjTj5zU", libSceOpusDecUnknown__minus_NcPFjTj5zU_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_EF6FftzF33M_nid_no_patch(void* argument1) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("EF6FftzF33M", libSceOpusDecUnknown_EF6FftzF33M_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_I1ugumHd0_plus_A_nid_no_patch(void* argument1, const void* argument2, std::uint32_t argument3, void* argument4, std::int32_t argument5, std::int32_t argument6) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    if (argument2 == nullptr && argument3 != 0) return static_cast<int>(ErrorBase + 0x6);
    if (argument3 > 0x1f4b) return static_cast<int>(ErrorBase + 0x7);
    if (argument4 == nullptr) return static_cast<int>(ErrorBase + 0x8);
    if (argument5 <= 0) return static_cast<int>(ErrorBase + 0x9);
    if (argument6 < 20 || argument6 > 100) return static_cast<int>(ErrorBase + 0xa);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("I1ugumHd0+A", libSceOpusDecUnknown_I1ugumHd0_plus_A_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_JZapTsLx0j0_nid_no_patch(const void* argument1, void* argument2, std::int32_t argument3, std::uint32_t argument4, std::uint32_t argument5, std::uint32_t argument6, std::uint32_t argument7) {
    if (argument1 == nullptr || argument2 == nullptr) return static_cast<int>(ErrorBase);
    if (argument3 != 8000 && argument3 != 12000 && argument3 != 16000 && argument3 != 24000 && argument3 != 48000) return static_cast<int>(ErrorBase + 0x2);
    if (argument4 - 1 >= 12) return static_cast<int>(ErrorBase + 0x3);
    if (argument6 - 1 >= 12) return static_cast<int>(ErrorBase + 0x16);
    if (argument7 > argument6) return static_cast<int>(ErrorBase + 0x17);
    if (argument6 + argument7 > 12) return static_cast<int>(ErrorBase + 0x3);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("JZapTsLx0j0", libSceOpusDecUnknown_JZapTsLx0j0_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_VsujeTF0_plus_xk_nid_no_patch(void* argument1, const void* argument2, std::uint32_t argument3, void* argument4, std::int32_t argument5) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    if (argument2 == nullptr && argument3 != 0) return static_cast<int>(ErrorBase + 0x6);
    if (argument3 > 0x1f4b) return static_cast<int>(ErrorBase + 0x7);
    if (argument4 == nullptr) return static_cast<int>(ErrorBase + 0x8);
    if (argument5 <= 0) return static_cast<int>(ErrorBase + 0x9);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("VsujeTF0+xk", libSceOpusDecUnknown_VsujeTF0_plus_xk_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_ZBx02KtHz2Y_nid_no_patch(void* argument1, const void* argument2, std::uint32_t argument3, void* argument4, std::int32_t argument5, std::int32_t argument6) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    if (argument2 == nullptr && argument3 != 0) return static_cast<int>(ErrorBase + 0x6);
    if (argument3 > 0x1f4b) return static_cast<int>(ErrorBase + 0x7);
    if (argument4 == nullptr) return static_cast<int>(ErrorBase + 0x8);
    if (argument5 <= 0) return static_cast<int>(ErrorBase + 0x9);
    if (argument6 < 20 || argument6 > 100) return static_cast<int>(ErrorBase + 0xa);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ZBx02KtHz2Y", libSceOpusDecUnknown_ZBx02KtHz2Y_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_ark0tPj4meI_nid_no_patch(std::int32_t first, std::int32_t second, std::int32_t third) {
    if (first < 1 || first > 12) return static_cast<int>(ErrorBase + 0x03);
    if (second < 1 || second > 12) return static_cast<int>(ErrorBase + 0x16);
    if (static_cast<std::uint32_t>(third) > static_cast<std::uint32_t>(second)) return static_cast<int>(ErrorBase + 0x17);
    if (static_cast<std::uint32_t>(second) + static_cast<std::uint32_t>(third) >= 13) return static_cast<int>(ErrorBase + 0x03);
    return 0x290;
}
APS5_EXPORT("ark0tPj4meI", libSceOpusDecUnknown_ark0tPj4meI_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_ct42RpB2CaM_nid_no_patch(void* argument1, const void* argument2, std::uint32_t argument3, void* argument4, std::int32_t argument5) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    if (argument2 == nullptr && argument3 != 0) return static_cast<int>(ErrorBase + 0x6);
    if (argument3 > 0x1f4b) return static_cast<int>(ErrorBase + 0x7);
    if (argument4 == nullptr) return static_cast<int>(ErrorBase + 0x8);
    if (argument5 <= 0) return static_cast<int>(ErrorBase + 0x9);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ct42RpB2CaM", libSceOpusDecUnknown_ct42RpB2CaM_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_eJQx7Emrwb0_nid_no_patch(void* argument1, const void* argument2, std::uint32_t argument3, void* argument4, std::int32_t argument5, std::int32_t argument6) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    if (argument2 == nullptr && argument3 != 0) return static_cast<int>(ErrorBase + 0x6);
    if (argument3 > 0x1f4b) return static_cast<int>(ErrorBase + 0x7);
    if (argument4 == nullptr) return static_cast<int>(ErrorBase + 0x8);
    if (argument5 <= 0) return static_cast<int>(ErrorBase + 0x9);
    if (argument6 < 20 || argument6 > 100) return static_cast<int>(ErrorBase + 0xa);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("eJQx7Emrwb0", libSceOpusDecUnknown_eJQx7Emrwb0_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_ewfq1d5iJhU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ewfq1d5iJhU", libSceOpusDecUnknown_ewfq1d5iJhU_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_gMazl3u8Y40_nid_no_patch(const void* state, std::uint32_t* value) {
    if (state == nullptr) return static_cast<int>(ErrorBase);
    if (value == nullptr) return 0;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gMazl3u8Y40", libSceOpusDecUnknown_gMazl3u8Y40_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_jXVNwfmd1II_nid_no_patch(std::uint32_t* context) {
    if (context == nullptr) return static_cast<int>(ErrorBase + 0x0d);
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jXVNwfmd1II", libSceOpusDecUnknown_jXVNwfmd1II_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_lFTdHt_minus_AuKU_nid_no_patch(void* argument1) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("lFTdHt-AuKU", libSceOpusDecUnknown_lFTdHt_minus_AuKU_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_sr25irrwGBI_nid_no_patch(void* argument1, std::int32_t argument2, std::uint32_t argument3, std::uint32_t argument4, std::uint32_t argument5, std::uint32_t argument6) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    if (argument2 != 8000 && argument2 != 12000 && argument2 != 16000 && argument2 != 24000 && argument2 != 48000) return static_cast<int>(ErrorBase + 0x2);
    if (argument3 - 1 >= 12) return static_cast<int>(ErrorBase + 0x3);
    if (argument5 - 1 >= 12) return static_cast<int>(ErrorBase + 0x16);
    if (argument6 > argument5) return static_cast<int>(ErrorBase + 0x17);
    if (argument5 + argument6 > 12) return static_cast<int>(ErrorBase + 0x3);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("sr25irrwGBI", libSceOpusDecUnknown_sr25irrwGBI_nid_no_patch);

int APS5_VABI libSceOpusDecUnknown_ygCoF49g3F0_nid_no_patch(void* argument1) {
    if (argument1 == nullptr) return static_cast<int>(ErrorBase);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ygCoF49g3F0", libSceOpusDecUnknown_ygCoF49g3F0_nid_no_patch);

}
