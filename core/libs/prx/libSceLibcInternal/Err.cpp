#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdarg>

using ErrExit = void (APS5_VABI*)(int);

extern "C" {
extern const char* __progname_nid_postfix;
int APS5_VABI vfprintf_nid_postfix(FileStream* stream, const char* format, VaList* args);
int APS5_VABI fprintf_nid_postfix(FileStream* stream, const char* format, ...);
int APS5_VABI fputs_nid_postfix(const char* str, FileStream* stream);
char* APS5_VABI strerror_nid_postfix(int error);
int* APS5_VABI __error_nid_postfix();
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
int APS5_VABI sceLibcInternalPending3_001_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIcE16do_negative_signEv", sceLibcInternalPending3_001_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_002_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIcE16do_positive_signEv", sceLibcInternalPending3_002_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_003_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIcE16do_thousands_sepEv", sceLibcInternalPending3_003_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_004_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIwE11do_groupingEv", sceLibcInternalPending3_004_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_005_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIwE13do_neg_formatEv", sceLibcInternalPending3_005_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_006_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIwE13do_pos_formatEv", sceLibcInternalPending3_006_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_007_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIwE14do_curr_symbolEv", sceLibcInternalPending3_007_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_008_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIwE14do_frac_digitsEv", sceLibcInternalPending3_008_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_009_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIwE16do_decimal_pointEv", sceLibcInternalPending3_009_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_010_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIwE16do_negative_signEv", sceLibcInternalPending3_010_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_011_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIwE16do_positive_signEv", sceLibcInternalPending3_011_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_012_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7_MpunctIwE16do_thousands_sepEv", sceLibcInternalPending3_012_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_013_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDic9_MbstatetE10do_unshiftERS0_PcS3_RS3_", sceLibcInternalPending3_013_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_014_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDic9_MbstatetE11do_encodingEv", sceLibcInternalPending3_014_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_015_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDic9_MbstatetE13do_max_lengthEv", sceLibcInternalPending3_015_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_016_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDic9_MbstatetE16do_always_noconvEv", sceLibcInternalPending3_016_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_017_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDic9_MbstatetE5do_inERS0_PKcS4_RS4_PDiS6_RS6_", sceLibcInternalPending3_017_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_018_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDic9_MbstatetE6do_outERS0_PKDiS4_RS4_PcS6_RS6_", sceLibcInternalPending3_018_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_019_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m", sceLibcInternalPending3_019_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_020_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDsc9_MbstatetE10do_unshiftERS0_PcS3_RS3_", sceLibcInternalPending3_020_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_021_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDsc9_MbstatetE11do_encodingEv", sceLibcInternalPending3_021_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_022_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDsc9_MbstatetE13do_max_lengthEv", sceLibcInternalPending3_022_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_023_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDsc9_MbstatetE16do_always_noconvEv", sceLibcInternalPending3_023_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_024_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDsc9_MbstatetE5do_inERS0_PKcS4_RS4_PDsS6_RS6_", sceLibcInternalPending3_024_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_025_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDsc9_MbstatetE6do_outERS0_PKDsS4_RS4_PcS6_RS6_", sceLibcInternalPending3_025_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_026_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIDsc9_MbstatetE9do_lengthERS0_PKcS4_m", sceLibcInternalPending3_026_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_027_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIcc9_MbstatetE10do_unshiftERS0_PcS3_RS3_", sceLibcInternalPending3_027_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_028_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIcc9_MbstatetE16do_always_noconvEv", sceLibcInternalPending3_028_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_029_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIcc9_MbstatetE2inERS0_PKcS4_RS4_PcS6_RS6_", sceLibcInternalPending3_029_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_030_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIcc9_MbstatetE3outERS0_PKcS4_RS4_PcS6_RS6_", sceLibcInternalPending3_030_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_031_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIcc9_MbstatetE5do_inERS0_PKcS4_RS4_PcS6_RS6_", sceLibcInternalPending3_031_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_032_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIcc9_MbstatetE6do_outERS0_PKcS4_RS4_PcS6_RS6_", sceLibcInternalPending3_032_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_033_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIcc9_MbstatetE6lengthERS0_PKcS4_m", sceLibcInternalPending3_033_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_034_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIcc9_MbstatetE7unshiftERS0_PcS3_RS3_", sceLibcInternalPending3_034_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_035_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIcc9_MbstatetE9do_lengthERS0_PKcS4_m", sceLibcInternalPending3_035_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_036_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIwc9_MbstatetE10do_unshiftERS0_PcS3_RS3_", sceLibcInternalPending3_036_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_037_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIwc9_MbstatetE11do_encodingEv", sceLibcInternalPending3_037_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_038_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIwc9_MbstatetE13do_max_lengthEv", sceLibcInternalPending3_038_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_039_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIwc9_MbstatetE16do_always_noconvEv", sceLibcInternalPending3_039_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_040_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIwc9_MbstatetE5do_inERS0_PKcS4_RS4_PwS6_RS6_", sceLibcInternalPending3_040_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_041_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIwc9_MbstatetE6do_outERS0_PKwS4_RS4_PcS6_RS6_", sceLibcInternalPending3_041_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_042_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7codecvtIwc9_MbstatetE9do_lengthERS0_PKcS4_m", sceLibcInternalPending3_042_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_043_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIcE10do_compareEPKcS2_S2_S2_", sceLibcInternalPending3_043_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_044_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIcE12do_transformEPKcS2_", sceLibcInternalPending3_044_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_045_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIcE4hashEPKcS2_", sceLibcInternalPending3_045_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_046_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIcE7compareEPKcS2_S2_S2_", sceLibcInternalPending3_046_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_047_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIcE7do_hashEPKcS2_", sceLibcInternalPending3_047_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_048_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIcE9transformEPKcS2_", sceLibcInternalPending3_048_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_049_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIwE10do_compareEPKwS2_S2_S2_", sceLibcInternalPending3_049_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_050_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIwE12do_transformEPKwS2_", sceLibcInternalPending3_050_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_051_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIwE4hashEPKwS2_", sceLibcInternalPending3_051_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_052_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIwE7compareEPKwS2_S2_S2_", sceLibcInternalPending3_052_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_053_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIwE7do_hashEPKwS2_", sceLibcInternalPending3_053_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_054_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7collateIwE9transformEPKwS2_", sceLibcInternalPending3_054_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_055_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv", sceLibcInternalPending3_055_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_056_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERb", sceLibcInternalPending3_056_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_057_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd", sceLibcInternalPending3_057_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_058_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe", sceLibcInternalPending3_058_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_059_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf", sceLibcInternalPending3_059_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_060_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj", sceLibcInternalPending3_060_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_061_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl", sceLibcInternalPending3_061_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_062_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm", sceLibcInternalPending3_062_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_063_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt", sceLibcInternalPending3_063_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_064_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx", sceLibcInternalPending3_064_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_065_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy", sceLibcInternalPending3_065_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_066_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv", sceLibcInternalPending3_066_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_067_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERb", sceLibcInternalPending3_067_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_068_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd", sceLibcInternalPending3_068_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_069_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe", sceLibcInternalPending3_069_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_070_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf", sceLibcInternalPending3_070_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_071_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj", sceLibcInternalPending3_071_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_072_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl", sceLibcInternalPending3_072_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_073_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm", sceLibcInternalPending3_073_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_074_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt", sceLibcInternalPending3_074_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_075_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx", sceLibcInternalPending3_075_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_076_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy", sceLibcInternalPending3_076_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_077_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetffldEPcRS3_S6_RSt8ios_basePi", sceLibcInternalPending3_077_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_078_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale", sceLibcInternalPending3_078_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_079_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE9_GetffldxEPcRS3_S6_RSt8ios_basePi", sceLibcInternalPending3_079_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_080_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv", sceLibcInternalPending3_080_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_081_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERb", sceLibcInternalPending3_081_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_082_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd", sceLibcInternalPending3_082_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_083_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe", sceLibcInternalPending3_083_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_084_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf", sceLibcInternalPending3_084_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_085_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj", sceLibcInternalPending3_085_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_086_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl", sceLibcInternalPending3_086_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_087_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm", sceLibcInternalPending3_087_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_088_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt", sceLibcInternalPending3_088_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_089_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx", sceLibcInternalPending3_089_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_090_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy", sceLibcInternalPending3_090_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_091_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv", sceLibcInternalPending3_091_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_092_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERb", sceLibcInternalPending3_092_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_093_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd", sceLibcInternalPending3_093_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_094_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe", sceLibcInternalPending3_094_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_095_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf", sceLibcInternalPending3_095_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_096_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj", sceLibcInternalPending3_096_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_097_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl", sceLibcInternalPending3_097_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_098_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm", sceLibcInternalPending3_098_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_099_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt", sceLibcInternalPending3_099_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_100_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx", sceLibcInternalPending3_100_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_101_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy", sceLibcInternalPending3_101_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_102_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetffldEPcRS3_S6_RSt8ios_basePi", sceLibcInternalPending3_102_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_103_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale", sceLibcInternalPending3_103_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_104_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE9_GetffldxEPcRS3_S6_RSt8ios_basePi", sceLibcInternalPending3_104_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_105_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecPKv", sceLibcInternalPending3_105_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_106_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecb", sceLibcInternalPending3_106_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_107_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecd", sceLibcInternalPending3_107_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_108_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basece", sceLibcInternalPending3_108_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_109_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecl", sceLibcInternalPending3_109_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_110_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecm", sceLibcInternalPending3_110_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_111_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecx", sceLibcInternalPending3_111_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_112_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecy", sceLibcInternalPending3_112_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_113_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_PKcm", sceLibcInternalPending3_113_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_114_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm", sceLibcInternalPending3_114_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_115_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FfmtEPccNSt5_IosbIiE9_FmtflagsE", sceLibcInternalPending3_115_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_116_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FputES3_RSt8ios_basecPKcm", sceLibcInternalPending3_116_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_117_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FputES3_RSt8ios_basecPKcmmmm", sceLibcInternalPending3_117_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_118_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_IfmtEPcPKcNSt5_IosbIiE9_FmtflagsE", sceLibcInternalPending3_118_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_119_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_IputES3_RSt8ios_basecPcm", sceLibcInternalPending3_119_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_120_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecPKv", sceLibcInternalPending3_120_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_121_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecb", sceLibcInternalPending3_121_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_122_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecd", sceLibcInternalPending3_122_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_123_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basece", sceLibcInternalPending3_123_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_124_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecl", sceLibcInternalPending3_124_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_125_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecm", sceLibcInternalPending3_125_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_126_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecx", sceLibcInternalPending3_126_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_127_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecy", sceLibcInternalPending3_127_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_128_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPKv", sceLibcInternalPending3_128_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_129_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewb", sceLibcInternalPending3_129_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_130_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewd", sceLibcInternalPending3_130_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_131_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewe", sceLibcInternalPending3_131_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_132_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewl", sceLibcInternalPending3_132_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_133_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewm", sceLibcInternalPending3_133_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_134_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewx", sceLibcInternalPending3_134_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_135_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewy", sceLibcInternalPending3_135_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_136_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_PutES3_PKwm", sceLibcInternalPending3_136_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_137_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm", sceLibcInternalPending3_137_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_138_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_FfmtEPccNSt5_IosbIiE9_FmtflagsE", sceLibcInternalPending3_138_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_139_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_FputES3_RSt8ios_basewPKcm", sceLibcInternalPending3_139_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_140_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_FputES3_RSt8ios_basewPKcmmmm", sceLibcInternalPending3_140_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_141_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_IfmtEPcPKcNSt5_IosbIiE9_FmtflagsE", sceLibcInternalPending3_141_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_142_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_IputES3_RSt8ios_basewPcm", sceLibcInternalPending3_142_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_143_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewPKv", sceLibcInternalPending3_143_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_144_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewb", sceLibcInternalPending3_144_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_145_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewd", sceLibcInternalPending3_145_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_146_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewe", sceLibcInternalPending3_146_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_147_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewl", sceLibcInternalPending3_147_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_148_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewm", sceLibcInternalPending3_148_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_149_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewx", sceLibcInternalPending3_149_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_150_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewy", sceLibcInternalPending3_150_nid_no_patch);
}
