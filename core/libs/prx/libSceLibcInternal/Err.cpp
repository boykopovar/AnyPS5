#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
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
