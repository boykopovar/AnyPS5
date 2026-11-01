#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/HostStandardStreams.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>
#endif

extern "C" {

extern FileStream* __stdoutp_nid_postfix;
extern FileStream* __stderrp_nid_postfix;
}

static std::string failures;
static void Require(bool value, const char* what) {
    if (!value) failures += std::string("freopen host log: ") + what + "\n";
}

#ifdef _WIN32
static int HostDup(int descriptor) { return ::_dup(descriptor); }
static int HostDup2(int from, int to) { return ::_dup2(from, to); }
static int HostClose(int descriptor) { return ::_close(descriptor); }
static int HostFileno(std::FILE* stream) { return ::_fileno(stream); }
static void HostWrite(int descriptor, const std::string& text) { ::_write(descriptor, text.data(), static_cast<unsigned>(text.size())); }
#else
static int HostDup(int descriptor) { return ::dup(descriptor); }
static int HostDup2(int from, int to) { return ::dup2(from, to) == to ? 0 : -1; }
static int HostClose(int descriptor) { return ::close(descriptor); }
static int HostFileno(std::FILE* stream) { return ::fileno(stream); }
static void HostWrite(int descriptor, const std::string& text) { [[maybe_unused]] const auto written = ::write(descriptor, text.data(), text.size()); }
#endif

static std::string Contents(const std::string& name) {
    std::ifstream stream(name, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

static bool Has(const std::string& text, const char* needle) { return text.find(needle) != std::string::npos; }

#ifndef _WIN32
static int KeepFailure(const std::string& stamp) {
    const auto outName = "anyps5-freopen-host-log-keep-failure-" + stamp + ".txt";
    rlimit original{};
    if (::getrlimit(RLIMIT_NOFILE, &original) != 0 || original.rlim_cur < 64) return 2;
    std::fflush(stdout);
    const int savedOut = ::dup(1);
    const int savedIn = ::dup(0);
    if (savedOut < 0 || savedIn < 0) return 3;
    rlimit limited = original;
    limited.rlim_cur = 64;
    if (::setrlimit(RLIMIT_NOFILE, &limited) != 0) return 4;
    std::vector<int> fillers;
    for (int descriptor; fillers.size() < 64 && (descriptor = ::open("/dev/null", O_RDONLY)) >= 0;) fillers.push_back(descriptor);
    const bool full = errno == EMFILE;
    ::close(0);

    bool threw = false;
    bool reopened = false;
    try {
        reopened = freopen_nid_postfix(outName.c_str(), "w", __stdoutp_nid_postfix) == __stdoutp_nid_postfix;
    } catch (...) {
        threw = true;
    }
    const bool kept = reopened && stdout != __stdoutp_nid_postfix->GetHandle();
    for (const int descriptor : fillers) ::close(descriptor);
    ::dup2(savedIn, 0);
    ::close(savedIn);
    ::setrlimit(RLIMIT_NOFILE, &original);
    if (reopened) {
        fputs_nid_postfix("guest-out\n", __stdoutp_nid_postfix);
        fflush_nid_postfix(__stdoutp_nid_postfix);
    }
    std::fflush(stdout);
    ::dup2(savedOut, 1);
    ::close(savedOut);

    Require(full, "the descriptor table was not filled");
    Require(!threw, "a failure to keep the host standard output escaped from freopen");
    Require(reopened, "freopen failed although only keeping the host standard output failed");
    Require(Has(Contents(outName), "guest-out"), "the guest's stdout line is not in its file");
    Require(!kept, "the host standard output was replaced although keeping it failed");
    std::error_code ignored;
    std::filesystem::remove(outName, ignored);
    HostWrite(2, failures);
    return failures.empty() ? 0 : 1;
}
#endif

int main(int argc, char** argv) {
    const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
#ifndef _WIN32
    if (argc > 1 && std::string(argv[1]) == "keep-failure") return KeepFailure(stamp);
#else
    static_cast<void>(argc);
    static_cast<void>(argv);
#endif
    const auto consoleName = "anyps5-freopen-host-log-console-" + stamp + ".txt";
    const auto outName = "anyps5-freopen-host-log-out-" + stamp + ".txt";
    const auto errName = "anyps5-freopen-host-log-err-" + stamp + ".txt";
    std::FILE* console = std::fopen(consoleName.c_str(), "wb");
    if (console == nullptr) return 2;
    std::fflush(stdout);
    std::fflush(stderr);
    const int savedOut = HostDup(1);
    const int savedErr = HostDup(2);
    if (savedOut < 0 || savedErr < 0 || HostDup2(HostFileno(console), 1) != 0 || HostDup2(HostFileno(console), 2) != 0) return 3;

    const bool outReopened = freopen_nid_postfix(outName.c_str(), "w", __stdoutp_nid_postfix) == __stdoutp_nid_postfix;
    const bool errReopened = freopen_nid_postfix(errName.c_str(), "w", __stderrp_nid_postfix) == __stderrp_nid_postfix;
    fputs_nid_postfix("guest-out\n", __stdoutp_nid_postfix);
    fputs_nid_postfix("guest-err\n", __stderrp_nid_postfix);
    fflush_nid_postfix(__stdoutp_nid_postfix);
    fflush_nid_postfix(__stderrp_nid_postfix);

    std::fputs("host-out\n", stdout);
    std::fflush(stdout);
    std::fputs("host-err\n", stderr);
    std::fflush(stderr);

    fclose_nid_postfix(__stdoutp_nid_postfix);
    fclose_nid_postfix(__stderrp_nid_postfix);
    std::fflush(stdout);
    std::fflush(stderr);
    HostDup2(savedOut, 1);
    HostDup2(savedErr, 2);
    HostClose(savedOut);
    HostClose(savedErr);
    std::fclose(console);

    const auto consoleText = Contents(consoleName);
    const auto outText = Contents(outName);
    const auto errText = Contents(errName);
    Require(outReopened && errReopened, "the guest standard streams were not reopened");
    Require(Has(outText, "guest-out"), "the guest's stdout line is not in its file");
    Require(Has(errText, "guest-err"), "the guest's stderr line is not in its file");
    Require(!Has(outText, "host-out"), "a host stdout line went into the guest's stdout file");
    Require(!Has(errText, "host-err"), "a host stderr line went into the guest's stderr file");
    Require(Has(consoleText, "host-out"), "a host stdout line did not reach the host's standard output");
    Require(Has(consoleText, "host-err"), "a host stderr line did not reach the host's standard error");
    std::error_code ignored;
    for (const auto& name : {consoleName, outName, errName}) std::filesystem::remove(name, ignored);
    HostWrite(2, failures);
    return failures.empty() ? 0 : 1;
}
