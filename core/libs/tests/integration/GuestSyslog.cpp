#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdio>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

extern "C" {
void APS5_VABI syslog_nid_postfix(int, const char*, ...);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int enoent = 2;

int StderrDescriptor() {
#ifdef _WIN32
    return ::_fileno(stderr);
#else
    return ::fileno(stderr);
#endif
}

int DescriptorOf(std::FILE* file) {
#ifdef _WIN32
    return ::_fileno(file);
#else
    return ::fileno(file);
#endif
}

int Duplicate(int descriptor) {
#ifdef _WIN32
    return ::_dup(descriptor);
#else
    return ::dup(descriptor);
#endif
}

bool Redirect(int source, int target) {
#ifdef _WIN32
    return ::_dup2(source, target) == 0;
#else
    return ::dup2(source, target) == target;
#endif
}

bool CloseDescriptor(int descriptor) {
#ifdef _WIN32
    return ::_close(descriptor) == 0;
#else
    return ::close(descriptor) == 0;
#endif
}

class StderrCapture {
public:
    StderrCapture() : file(std::tmpfile()) {
        Require(file != nullptr, "create the capture file");
        std::fflush(stderr);
        saved = Duplicate(StderrDescriptor());
        const bool redirected = saved >= 0 && Redirect(DescriptorOf(file), StderrDescriptor());
        if (!redirected) {
            if (saved >= 0) CloseDescriptor(saved);
            std::fclose(file);
            Testing::Fail("redirect stderr into the capture file");
        }
    }

    ~StderrCapture() {
        Restore();
        std::fclose(file);
    }

    StderrCapture(const StderrCapture&) = delete;
    StderrCapture& operator=(const StderrCapture&) = delete;

    std::string Finish() {
        Require(Restore(), "restore stderr");
        std::rewind(file);
        std::string text;
        char buffer[512];
        std::size_t length = 0;
        while ((length = std::fread(buffer, 1, sizeof(buffer), file)) > 0) text.append(buffer, length);
        return text;
    }

private:
    bool Restore() {
        if (saved < 0) return true;
        std::fflush(stderr);
        const bool restored = Redirect(saved, StderrDescriptor());
        const bool closed = CloseDescriptor(saved);
        saved = -1;
        return restored && closed;
    }

    std::FILE* file;
    int saved = -1;
};

const Case formattedMessage{"Syslog_InfoPriority_WritesUserFacilityPrefixAndFormattedText", [] {
    StderrCapture capture;
    *__error_nid_postfix() = enoent;
    syslog_nid_postfix(6, "station %s at %d kbps", "radio", 128);
    const int error = *__error_nid_postfix();
    RequireEqual(capture.Finish(), std::string("[syslog:14] station radio at 128 kbps\n"), "captured output");
    RequireEqual(error, enoent, "errno");
}};

const Case errorText{"Syslog_PercentM_ExpandsErrnoTextAndKeepsEscapedPercent", [] {
    StderrCapture capture;
    *__error_nid_postfix() = enoent;
    syslog_nid_postfix(16 | 3, "open: %m, %%m kept, %d%%\n", 50);
    const int error = *__error_nid_postfix();
    RequireEqual(capture.Finish(), std::string("[syslog:19] open: No such file or directory, %m kept, 50%\n"), "captured output");
    RequireEqual(error, enoent, "errno");
}};

const Case unknownBits{"Syslog_UnknownPriorityBits_ReportsThemAndLogsMaskedMessage", [] {
    StderrCapture capture;
    *__error_nid_postfix() = enoent;
    syslog_nid_postfix(0x10000 | 4, "masked");
    const int error = *__error_nid_postfix();
    RequireEqual(capture.Finish(),
        std::string("[syslog:35] syslog: unknown facility/priority: 10004\n[syslog:12] masked\n"), "captured output");
    RequireEqual(error, enoent, "errno");
}};

const Case emptyMessage{"Syslog_EmptyMessage_WritesPrefixAndNewline", [] {
    StderrCapture capture;
    *__error_nid_postfix() = enoent;
    syslog_nid_postfix(7, "");
    const int error = *__error_nid_postfix();
    RequireEqual(capture.Finish(), std::string("[syslog:15] \n"), "captured output");
    RequireEqual(error, enoent, "errno");
}};

} // namespace
