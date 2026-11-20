#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/CodecvtUtf.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdarg>

extern "C" {
extern const char* __progname_nid_postfix;

}

namespace {

FileStream* g_errFile = nullptr;
ErrExit g_errExit = nullptr;

FileStream* ErrFile() {
    if (g_errFile == nullptr) g_errFile = &_Stderr_nid_postfix;
    return g_errFile;
}

const char* ProgramName() {
    return __progname_nid_postfix ? __progname_nid_postfix : "(null)";
}

void Report(const char* format, VaList* args, const int* code) {
    FileStream* const stream = ErrFile();
    fprintf_nid_postfix(stream, "%s: ", ProgramName());

    if (format != nullptr) {
        vfprintf_nid_postfix(stream, format, args);

        if (code != nullptr) fputs_nid_postfix(": ", stream);
    }
    if (code != nullptr) fprintf_nid_postfix(stream, "%s\n", strerror_nid_postfix(*code));
    else fputs_nid_postfix("\n", stream);
}

[[noreturn]] void Exit(int status) {
    if (g_errExit != nullptr) g_errExit(status);
    LibcExit_nid_no_patch(status);
}

}

#ifdef _WIN32
#define APS5_ERR_VA_LIST __builtin_sysv_va_list
#define APS5_ERR_VA_START(list, last) __builtin_sysv_va_start(list, last)
#define APS5_ERR_VA_END(list) __builtin_sysv_va_end(list)
#else
#define APS5_ERR_VA_LIST std::va_list
#define APS5_ERR_VA_START(list, last) va_start(list, last)
#define APS5_ERR_VA_END(list) va_end(list)
#endif

extern "C" {

int* APS5_VABI _Geterrno_nid_postfix() {
    return __error_nid_postfix();
}

void APS5_VABI err_set_file_nid_postfix(FileStream* stream) {
    g_errFile = stream != nullptr ? stream : &_Stderr_nid_postfix;
}

void APS5_VABI err_set_exit_nid_postfix(ErrExit function) {
    g_errExit = function;
}

void APS5_VABI vwarnc_nid_postfix(int code, const char* format, VaList* args) {
    Report(format, args, &code);
}

void APS5_VABI vwarn_nid_postfix(const char* format, VaList* args) {
    vwarnc_nid_postfix(*__error_nid_postfix(), format, args);

}

void APS5_VABI vwarnx_nid_postfix(const char* format, VaList* args) {
    Report(format, args, nullptr);
}

[[noreturn]] void APS5_VABI verrc_nid_postfix(int status, int code, const char* format, VaList* args) {
    Report(format, args, &code);
    Exit(status);
}

[[noreturn]] void APS5_VABI verr_nid_postfix(int status, const char* format, VaList* args) {
    verrc_nid_postfix(status, *__error_nid_postfix(), format, args);

}

[[noreturn]] void APS5_VABI verrx_nid_postfix(int status, const char* format, VaList* args) {
    Report(format, args, nullptr);
    Exit(status);
}

void APS5_VABI warnc_nid_postfix(int code, const char* format, ...) {
    APS5_ERR_VA_LIST args;
    APS5_ERR_VA_START(args, format);
    vwarnc_nid_postfix(code, format, reinterpret_cast<VaList*>(args));

    APS5_ERR_VA_END(args);
}

void APS5_VABI warn_nid_postfix(const char* format, ...) {
    const int code = *__error_nid_postfix();
    APS5_ERR_VA_LIST args;
    APS5_ERR_VA_START(args, format);
    vwarnc_nid_postfix(code, format, reinterpret_cast<VaList*>(args));

    APS5_ERR_VA_END(args);
}

void APS5_VABI _warn_nid_postfix(const char* format, ...) {
    APS5_ERR_VA_LIST args;
    APS5_ERR_VA_START(args, format);
    vwarn_nid_postfix(format, reinterpret_cast<VaList*>(args));

    APS5_ERR_VA_END(args);
}

void APS5_VABI warnx_nid_postfix(const char* format, ...) {
    APS5_ERR_VA_LIST args;
    APS5_ERR_VA_START(args, format);
    vwarnx_nid_postfix(format, reinterpret_cast<VaList*>(args));

    APS5_ERR_VA_END(args);
}

[[noreturn]] void APS5_VABI errc_nid_postfix(int status, int code, const char* format, ...) {
    APS5_ERR_VA_LIST args;
    APS5_ERR_VA_START(args, format);
    verrc_nid_postfix(status, code, format, reinterpret_cast<VaList*>(args));

}

[[noreturn]] void APS5_VABI err_nid_postfix(int status, const char* format, ...) {
    const int code = *__error_nid_postfix();
    APS5_ERR_VA_LIST args;
    APS5_ERR_VA_START(args, format);
    verrc_nid_postfix(status, code, format, reinterpret_cast<VaList*>(args));

}

[[noreturn]] void APS5_VABI _err_nid_postfix(int status, const char* format, ...) {
    APS5_ERR_VA_LIST args;
    APS5_ERR_VA_START(args, format);
    verr_nid_postfix(status, format, reinterpret_cast<VaList*>(args));

}

[[noreturn]] void APS5_VABI errx_nid_postfix(int status, const char* format, ...) {
    APS5_ERR_VA_LIST args;
    APS5_ERR_VA_START(args, format);
    verrx_nid_postfix(status, format, reinterpret_cast<VaList*>(args));

}

}

extern "C" {
int APS5_VABI _ZNKSt7_MpunctIcE16do_negative_signEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE16do_positive_signEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE16do_thousands_sepEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIwE11do_groupingEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIwE13do_neg_formatEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIwE13do_pos_formatEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIwE14do_curr_symbolEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIwE14do_frac_digitsEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIwE16do_decimal_pointEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIwE16do_negative_signEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIwE16do_positive_signEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIwE16do_thousands_sepEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE16do_always_noconvEv(const void* self) {
    (void)self;
    return false;
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE5do_inERS0_PKcS4_RS4_PDiS6_RS6_(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char32_t* first2, char32_t* last2, char32_t** mid2) {
    return CodecvtUtf32In_nid_no_patch(self, state, first1, last1, mid1, first2, last2, mid2);
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE6do_outERS0_PKDiS4_RS4_PcS6_RS6_(const void* self, void* state, const char32_t* first1, const char32_t* last1, const char32_t** mid1, char* first2, char* last2, char** mid2) {
    return CodecvtUtf32Out_nid_no_patch(self, state, first1, last1, mid1, first2, last2, mid2);
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m(const void* self, void* state, const char* first, const char* last, std::size_t count) {
    return CodecvtUtf32Length_nid_no_patch(self, state, first, last, count);
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE11do_encodingEv(const void* self) {
    (void)self;
    return 0;
}

bool APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE16do_always_noconvEv(const void* self) {
    (void)self;
    return false;
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE5do_inERS0_PKcS4_RS4_PDsS6_RS6_(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char16_t* first2, char16_t* last2, char16_t** mid2) {
    return CodecvtUtf16In_nid_no_patch(self, state, first1, last1, mid1, first2, last2, mid2);
}

int APS5_VABI sceLibcInternalPending3_025_nid_no_patch(const void* self, void* state, const char16_t* first1, const char16_t* last1, const char16_t** mid1, char* first2, char* last2, char** mid2) {
    return CodecvtUtf16Out_nid_no_patch(self, state, first1, last1, mid1, first2, last2, mid2);
}
APS5_EXPORT("0FKwlv9iH1c", sceLibcInternalPending3_025_nid_no_patch);

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix(const void* self, void* state, const char* first, const char* last, std::size_t count) {
    return CodecvtUtf16Length_nid_no_patch(self, state, first, last, count);
}

int APS5_VABI _ZNKSt7codecvtIwc9_MbstatetE10do_unshiftERS0_PcS3_RS3_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIwc9_MbstatetE11do_encodingEv(const void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIwc9_MbstatetE13do_max_lengthEv(const void* self) {
    (void)self;
    return 6;
}

bool APS5_VABI _ZNKSt7codecvtIwc9_MbstatetE16do_always_noconvEv(const void* self) {
    (void)self;
    return false;
}

int APS5_VABI _ZNKSt7codecvtIwc9_MbstatetE5do_inERS0_PKcS4_RS4_PwS6_RS6_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIwc9_MbstatetE6do_outERS0_PKwS4_RS4_PcS6_RS6_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIwc9_MbstatetE9do_lengthERS0_PKcS4_m() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


int APS5_VABI sceLibcInternalPending3_066_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("VKdXFE7ualw", sceLibcInternalPending3_066_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_067_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("SmtBNDda5qU", sceLibcInternalPending3_067_nid_no_patch);

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending3_079_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("J-0I2PtiZc4", sceLibcInternalPending3_079_nid_no_patch);

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending3_092_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("wEU8oFtBXT8", sceLibcInternalPending3_092_nid_no_patch);

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE9_GetffldxEPcRS3_S6_RSt8ios_basePi() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_PKcm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending3_114_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("tCihLs4UJxQ", sceLibcInternalPending3_114_nid_no_patch);

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FfmtEPccNSt5_IosbIiE9_FmtflagsE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FputES3_RSt8ios_basecPKcm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FputES3_RSt8ios_basecPKcmmmm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_IfmtEPcPKcNSt5_IosbIiE9_FmtflagsE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_IputES3_RSt8ios_basecPcm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecPKv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecb_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecd_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basece_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecl_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPKv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewb_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewd_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewe_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewl_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_PutES3_PKwm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_FfmtEPccNSt5_IosbIiE9_FmtflagsE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_FputES3_RSt8ios_basewPKcm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_FputES3_RSt8ios_basewPKcmmmm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_IfmtEPcPKcNSt5_IosbIiE9_FmtflagsE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_IputES3_RSt8ios_basewPcm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewPKv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewb_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewd_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewe_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewl_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
