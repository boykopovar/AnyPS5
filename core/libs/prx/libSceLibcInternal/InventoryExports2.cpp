#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI _Restore_state_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Rint_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Save_state_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Scanf_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Setgloballocale_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Sincos_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Skip_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Strcollx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

char* APS5_VABI _Strerror_nid_postfix(int error, char* buffer) {
    if (const char* text = StrerrorText_nid_no_patch(error)) return const_cast<char*>(text);
    char local[22];
    std::snprintf(local, sizeof(local), "error_code=0x%08x", static_cast<unsigned>(error));
    if (!buffer) {
        thread_local char ownBuffer[sizeof(local)];
        buffer = ownBuffer;
    }
    if (!GuestCopyOut_nid_no_patch(buffer, local, sizeof(local))) throw std::invalid_argument("_Strerror: an unwritable buffer");
    return buffer;
}

int APS5_VABI _Strxfrmx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Tan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Tgamma_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void* APS5_VABI _Ttotm_nid_postfix(void* time, std::int64_t seconds, int isdst) {
    constexpr std::uint64_t SecondsFrom1900To1970 = 2208988800u;
    constexpr std::uint64_t SecondsPerDay = 86400;
    if (time == nullptr) {
        thread_local std::array<std::int32_t, 14> ownTime{};
        time = ownTime.data();
    }
    const std::uint64_t secs = static_cast<std::uint64_t>(seconds) + SecondsFrom1900To1970;
    auto days = static_cast<std::int64_t>(secs / SecondsPerDay);
    const auto weekday = static_cast<std::int32_t>((days + 1) % 7);
    auto year = static_cast<std::int32_t>(static_cast<std::uint32_t>(days / 365));
    std::int64_t yearStart = 0;
    while (days < (yearStart = _Daysto_nid_postfix(year, 0) + 365LL * year)) year = static_cast<std::int32_t>(static_cast<std::uint32_t>(year) - 1u);
    days -= yearStart;
    const int leapDays = _Daysto_nid_postfix(year, 0);
    int month = 12;
    while (days < _Daysto_nid_postfix(year, --month) - leapDays) {}
    const std::uint64_t daySeconds = secs % SecondsPerDay;
    const std::array<std::int32_t, 9> fields{
        static_cast<std::int32_t>(daySeconds % 60),
        static_cast<std::int32_t>(daySeconds % 3600 / 60),
        static_cast<std::int32_t>(daySeconds / 3600),
        static_cast<std::int32_t>(static_cast<std::uint32_t>(days - (_Daysto_nid_postfix(year, month) - leapDays) + 1)),
        month,
        year,
        weekday,
        static_cast<std::int32_t>(static_cast<std::uint32_t>(days)),
        isdst,
    };
    if (!GuestCopyOut_nid_no_patch(time, fields.data(), sizeof(fields))) throw std::invalid_argument("_Ttotm: an unwritable time structure");
    return time;
}

int APS5_VABI _Tzoff_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Unlock_spin_lock_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WFrprep_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WFwprep_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WGenld_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WGetfld_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WGetfloat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WGetint_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WGetstr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WLdtob_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WLitob_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WPrintf_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WPutfld_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WPutstr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WPuttxt_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WScanf_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Wcscollx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Wcsftime_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Wcsxfrmx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Wctomb_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_yc_vi84_2aE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("yc-vi84-2aE", sceLibcInternalUnknown_yc_vi84_2aE_nid_no_patch);

int APS5_VABI _ZNKSbIwSt11char_traitsIwESaIwEE5_XlenEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSbIwSt11char_traitsIwESaIwEE5_XranEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt13bad_exception8_DoraiseEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt19istreambuf_iteratorIcSt11char_traitsIcEE5equalERKS2__nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_jZmLD_ASDto_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("jZmLD-ASDto", sceLibcInternalUnknown_jZmLD_ASDto_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_n9_NJEULZ_0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("n9-NJEULZ-0", sceLibcInternalUnknown_n9_NJEULZ_0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_pSQz254t3ug_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("pSQz254t3ug", sceLibcInternalUnknown_pSQz254t3ug_nid_no_patch);

int APS5_VABI _ZNKSt7_MpunctIcE11do_groupingEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE13do_neg_formatEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE13do_pos_formatEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE14do_curr_symbolEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE14do_frac_digitsEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE16do_decimal_pointEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
