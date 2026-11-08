#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
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
APS5_EXPORT("Rqu9OmkKY+M", sceLibcInternalPending3_001_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_002_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ARZszYKvDWg", sceLibcInternalPending3_002_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_003_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("6aFwTNpqTP8", sceLibcInternalPending3_003_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_004_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ckD5sIxo+Co", sceLibcInternalPending3_004_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_005_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("UzmR8lOfyqU", sceLibcInternalPending3_005_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_006_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("zF2GfKzBgqU", sceLibcInternalPending3_006_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_007_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ypq5jFNRIJA", sceLibcInternalPending3_007_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_008_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ij-yZhH9YjY", sceLibcInternalPending3_008_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_009_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("v8P1X84ytFY", sceLibcInternalPending3_009_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_010_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("zkUC74aJxpE", sceLibcInternalPending3_010_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_011_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("PWFePkVdv9w", sceLibcInternalPending3_011_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_012_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("XX+xiPXAN8I", sceLibcInternalPending3_012_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_013_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("c6Lyc6xOp4o", sceLibcInternalPending3_013_nid_no_patch);

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE11do_encodingEv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE13do_max_lengthEv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE16do_always_noconvEv(const void* self) {
    (void)self;
    return false;
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE5do_inERS0_PKcS4_RS4_PDiS6_RS6_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE6do_outERS0_PKDiS4_RS4_PcS6_RS6_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE10do_unshiftERS0_PcS3_RS3_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE11do_encodingEv(const void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE13do_max_lengthEv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE16do_always_noconvEv(const void* self) {
    (void)self;
    return false;
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE5do_inERS0_PKcS4_RS4_PDsS6_RS6_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending3_025_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("0FKwlv9iH1c", sceLibcInternalPending3_025_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_026_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("lynApfiP6Lw", sceLibcInternalPending3_026_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_027_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("iCWgjeqMHvg", sceLibcInternalPending3_027_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_028_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("7tIwDZyyKHo", sceLibcInternalPending3_028_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_029_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("TNexGlwiVEQ", sceLibcInternalPending3_029_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_030_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("14xKj+SV72o", sceLibcInternalPending3_030_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_031_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("P+q1OLiErP0", sceLibcInternalPending3_031_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_032_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Uc+-Sx0UZ3U", sceLibcInternalPending3_032_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_033_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ikBt0lqkxPc", sceLibcInternalPending3_033_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_034_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("N7z+Dnk1cS0", sceLibcInternalPending3_034_nid_no_patch);

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE9do_lengthERS0_PKcS4_m() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI _ZNKSt7collateIwE10do_compareEPKwS2_S2_S2_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7collateIwE12do_transformEPKwS2_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7collateIwE4hashEPKwS2_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7collateIwE7compareEPKwS2_S2_S2_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7collateIwE7do_hashEPKwS2_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7collateIwE9transformEPKwS2_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERb() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending3_067_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("SmtBNDda5qU", sceLibcInternalPending3_067_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_068_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("bNQpG-eKogg", sceLibcInternalPending3_068_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_069_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("uukWbYS6Bn4", sceLibcInternalPending3_069_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_070_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("IntAnFb+tw0", sceLibcInternalPending3_070_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_071_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ywJpNe675zo", sceLibcInternalPending3_071_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_072_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ALEXgLx9fqU", sceLibcInternalPending3_072_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_073_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Pq4PkG0x1fk", sceLibcInternalPending3_073_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_074_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("dRu2RLn4SKM", sceLibcInternalPending3_074_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_075_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("F+AmVDFUyqM", sceLibcInternalPending3_075_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_076_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("TtYifKtVkYA", sceLibcInternalPending3_076_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_077_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("4+y8-2NsDw0", sceLibcInternalPending3_077_nid_no_patch);

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE9_GetffldxEPcRS3_S6_RSt8ios_basePi() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERb() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalPending3_092_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("wEU8oFtBXT8", sceLibcInternalPending3_092_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_093_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("t39dKpPEuVA", sceLibcInternalPending3_093_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_094_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("MCtJ9D7B5Cs", sceLibcInternalPending3_094_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_095_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Gy2iRxp3LGk", sceLibcInternalPending3_095_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_096_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("2bUUbbcqHUo", sceLibcInternalPending3_096_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_097_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("QossXdwWltI", sceLibcInternalPending3_097_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_098_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ig6SRr1GCU4", sceLibcInternalPending3_098_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_099_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("kU7PvJJKUng", sceLibcInternalPending3_099_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_100_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Ou7GV51-ng4", sceLibcInternalPending3_100_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_101_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("rYLrGFoqfi4", sceLibcInternalPending3_101_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_102_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("W5VYncHdreo", sceLibcInternalPending3_102_nid_no_patch);

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE9_GetffldxEPcRS3_S6_RSt8ios_basePi() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecPKv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecb() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basece() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecy() {
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

int APS5_VABI sceLibcInternalPending3_115_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("w11G58-u4p8", sceLibcInternalPending3_115_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_116_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ll99KkqO6ig", sceLibcInternalPending3_116_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_117_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("mNk6FfI8T7I", sceLibcInternalPending3_117_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_118_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("xlgA01CQtBo", sceLibcInternalPending3_118_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_119_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("jykT-VWQVBY", sceLibcInternalPending3_119_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_120_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("0xgFRKf0Lc4", sceLibcInternalPending3_120_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_121_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ke36E2bqNmI", sceLibcInternalPending3_121_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_122_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("F+cp2B3cWNU", sceLibcInternalPending3_122_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_123_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("rLiFc4+HyHw", sceLibcInternalPending3_123_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_124_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("I3+xmBWGPGo", sceLibcInternalPending3_124_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_125_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("nlAk46weq1w", sceLibcInternalPending3_125_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_126_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("H2KGT3vA7yQ", sceLibcInternalPending3_126_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_127_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Vbeoft607aI", sceLibcInternalPending3_127_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_128_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("xfOSCbCiY44", sceLibcInternalPending3_128_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_129_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("mY9FWooxqJY", sceLibcInternalPending3_129_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_130_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("V7aIsVIsIIA", sceLibcInternalPending3_130_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_131_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("vCIFGeI6adI", sceLibcInternalPending3_131_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_132_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("USLhWp7sZoU", sceLibcInternalPending3_132_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_133_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("qtpzdwMMCPc", sceLibcInternalPending3_133_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_134_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ryykbHJ04Cw", sceLibcInternalPending3_134_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_135_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("lmb3oBpMNPU", sceLibcInternalPending3_135_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_136_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("kRGVhisjgMg", sceLibcInternalPending3_136_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_137_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("-b+Avqa2v9k", sceLibcInternalPending3_137_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_138_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("T07KcAOlIeU", sceLibcInternalPending3_138_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_139_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("IdV-tXejEGQ", sceLibcInternalPending3_139_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_140_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("B6JXVOMDdlw", sceLibcInternalPending3_140_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_141_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("WheFSRlZ9JA", sceLibcInternalPending3_141_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_142_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("4pQ3B1BTMgo", sceLibcInternalPending3_142_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_143_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("BDteGj1gqBY", sceLibcInternalPending3_143_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_144_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("1C2-2WB9NN4", sceLibcInternalPending3_144_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_145_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("sX3o6Zmihw0", sceLibcInternalPending3_145_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_146_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("6OYWLisfrB8", sceLibcInternalPending3_146_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_147_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("VpwhOe58wsM", sceLibcInternalPending3_147_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_148_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("jHo78LGEtmU", sceLibcInternalPending3_148_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_149_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("9SSHrlIamto", sceLibcInternalPending3_149_nid_no_patch);

int APS5_VABI sceLibcInternalPending3_150_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("uX0nKsUo8gc", sceLibcInternalPending3_150_nid_no_patch);
}
