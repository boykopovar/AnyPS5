#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include "tests/TemporaryFile.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

extern "C" {

extern const char* __progname_nid_postfix;

extern const char* const sys_signame_nid_postfix[32];
extern const char* const sys_siglist_nid_postfix[32];
}

namespace {

struct ExitRequested {
    int status;
};

int g_hookStatus = -1;

void APS5_VABI Hook(int status) {
    g_hookStatus = status;
    throw ExitRequested{status};
}

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

class Capture {
    FileStream stream;
    long consumed = 0;

public:
    Capture() : stream(TemporaryFile::Open()) { err_set_file_nid_postfix(&stream); }
    ~Capture() {
        err_set_file_nid_postfix(nullptr);

        stream.Close();
    }
    std::string Take() {
        std::FILE* handle = stream.GetHandle();
        std::fflush(handle);
        const long end = std::ftell(handle);
        std::fseek(handle, consumed, SEEK_SET);
        std::string text;
        for (int character; (character = std::fgetc(handle)) != EOF;) text.push_back(static_cast<char>(character));
        consumed = end;
        std::fseek(handle, end, SEEK_SET);
        return text;
    }
};

template <typename TCall>
std::string DescriptorTwoOutput(TCall call) {
    std::fflush(stderr);
    std::FILE* capture = TemporaryFile::Open();
    const int saved = dup(2);
    dup2(fileno(capture), 2);
    call();
    dup2(saved, 2);
    close(saved);
    std::rewind(capture);
    std::string text;
    for (int character; (character = std::fgetc(capture)) != EOF;) text.push_back(static_cast<char>(character));
    std::fclose(capture);
    return text;
}

template <typename TCall>
int ExitStatus(TCall call) {
    try {
        call();
    } catch (const ExitRequested& request) {
        return request.status;
    }
    return -1;
}

enum class Variant { Warn, Warnc, Warnx, Err, Errc, Errx, WarnxSkipping };

void APS5_VABI CallVariant(Variant variant, int status, int code, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    if (variant == Variant::WarnxSkipping) (void)__builtin_va_arg(args, int);
#else
    std::va_list args;
    va_start(args, format);
    if (variant == Variant::WarnxSkipping) (void)va_arg(args, int);
#endif
    auto* list = reinterpret_cast<VaList*>(args);
    switch (variant) {
    case Variant::Warn: vwarn_nid_postfix(format, list); break;
    case Variant::Warnc: vwarnc_nid_postfix(code, format, list); break;
    case Variant::Warnx:
    case Variant::WarnxSkipping: vwarnx_nid_postfix(format, list); break;
    case Variant::Err: verr_nid_postfix(status, format, list);
    case Variant::Errc: verrc_nid_postfix(status, code, format, list);
    case Variant::Errx: verrx_nid_postfix(status, format, list);
    }
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
}

}

int main() {
    const char* const savedName = __progname_nid_postfix;
    __progname_nid_postfix = "prog";
    const std::string noEntry = strerror_nid_postfix(2);
    const std::string denied = strerror_nid_postfix(13);
    {
        Capture capture;
        *__error_nid_postfix() = 2;
        warn_nid_postfix("open %s", "a.txt");

        Require(capture.Take() == "prog: open a.txt: " + noEntry + "\n", "warn");
        *__error_nid_postfix() = 2;
        warn_nid_postfix(nullptr);

        Require(capture.Take() == "prog: " + noEntry + "\n", "warn without format");
        *__error_nid_postfix() = 13;
        _warn_nid_postfix("open %s", "b.txt");

        Require(capture.Take() == "prog: open b.txt: " + denied + "\n", "_warn");
        warnc_nid_postfix(13, "%d-%s", 7, "x");

        Require(capture.Take() == "prog: 7-x: " + denied + "\n", "warnc");
        warnx_nid_postfix("plain %c", 'q');

        Require(capture.Take() == "prog: plain q\n", "warnx");
        warnx_nid_postfix(nullptr);

        Require(capture.Take() == "prog: \n", "warnx without format");

        err_set_exit_nid_postfix(Hook);
        Require(ExitStatus([] { errx_nid_postfix(3, "bad %d", 1); }) == 3 && g_hookStatus == 3, "errx status");
        Require(capture.Take() == "prog: bad 1\n", "errx text");
        Require(ExitStatus([] { errc_nid_postfix(4, 13, "c"); }) == 4, "errc status");
        Require(capture.Take() == "prog: c: " + denied + "\n", "errc text");
        *__error_nid_postfix() = 2;
        Require(ExitStatus([] { err_nid_postfix(5, "e"); }) == 5, "err status");
        Require(capture.Take() == "prog: e: " + noEntry + "\n", "err text");
        *__error_nid_postfix() = 13;
        Require(ExitStatus([] { _err_nid_postfix(7, "%s %d", "f", 8); }) == 7 && g_hookStatus == 7, "_err status");
        Require(capture.Take() == "prog: f 8: " + denied + "\n", "_err text");
        *__error_nid_postfix() = 2;
        Require(ExitStatus([] { _warn_nid_postfix(nullptr); }) == -1 && g_hookStatus == 7, "_warn does not exit");
        Require(capture.Take() == "prog: " + noEntry + "\n", "_warn without format");

        *__error_nid_postfix() = 2;
        CallVariant(Variant::Warn, 0, 0, "open %s", "a.txt");
        Require(capture.Take() == "prog: open a.txt: " + noEntry + "\n", "vwarn");
        *__error_nid_postfix() = 2;
        CallVariant(Variant::Warnc, 0, 13, "%d-%s", 7, "x");
        Require(capture.Take() == "prog: 7-x: " + denied + "\n", "vwarnc uses its code");
        CallVariant(Variant::Warnc, 0, 13, nullptr);
        Require(capture.Take() == "prog: " + denied + "\n", "vwarnc without format");
        CallVariant(Variant::Warnx, 0, 0, "%d %d %d %d %d %d %d %d %.2f %s %ld", 1, 2, 3, 4, 5, 6, 7, 8, 2.5, "s", 123456789LL);
        Require(capture.Take() == "prog: 1 2 3 4 5 6 7 8 2.50 s 123456789\n", "vwarnx stack and floating arguments");
        CallVariant(Variant::WarnxSkipping, 0, 0, "after %d", 1, 2);
        Require(capture.Take() == "prog: after 2\n", "vwarnx continues an advanced list");
        *__error_nid_postfix() = 16;
        Require(ExitStatus([] { CallVariant(Variant::Err, 6, 0, nullptr); }) == 6, "verr status");
        Require(capture.Take() == "prog: " + std::string(strerror_nid_postfix(16)) + "\n", "verr without format");
        *__error_nid_postfix() = 2;
        Require(ExitStatus([] { CallVariant(Variant::Errc, 4, 13, "c%s", "!"); }) == 4, "verrc status");
        Require(capture.Take() == "prog: c!: " + denied + "\n", "verrc text");
        Require(ExitStatus([] { CallVariant(Variant::Errx, 0, 0, nullptr); }) == 0, "verrx status 0");
        Require(capture.Take() == "prog: \n", "verrx without format");
        err_set_exit_nid_postfix(nullptr);

    }
    __progname_nid_postfix = savedName;

    Require(std::string(sys_signame_nid_postfix[0]) == "Signal 0" && std::string(sys_signame_nid_postfix[15]) == "TERM", "sys_signame");
    Require(std::string(sys_signame_nid_postfix[26]) == "VTALRM" && std::string(sys_signame_nid_postfix[31]) == "USR2", "sys_signame tail");
    Require(DescriptorTwoOutput([] { psignal_nid_postfix(2, "tag"); }) == std::string("tag: ") + sys_siglist_nid_postfix[2] + "\n", "psignal with prefix");
    Require(DescriptorTwoOutput([] { psignal_nid_postfix(11, ""); }) == std::string(sys_siglist_nid_postfix[11]) + "\n", "psignal empty prefix");
    Require(DescriptorTwoOutput([] { psignal_nid_postfix(32, nullptr); }) == "Unknown signal\n", "psignal unknown signal");
    return 0;
}
