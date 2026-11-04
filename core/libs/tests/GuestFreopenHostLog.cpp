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
#include <mutex>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
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

#ifdef _WIN32
static std::FILE* releaseForOpen = nullptr;
static int openEntryError = 0;
static int openCalls = 0;

static std::FILE* OpenMissingAfterRelease(const std::filesystem::path& filename, const std::string& mode) {
    openEntryError = errno;
    ++openCalls;
    if (releaseForOpen) {
        Require(std::fclose(releaseForOpen) == 0, "cannot release the callback's stream slot");
        releaseForOpen = nullptr;
    }
    const std::wstring wideMode(mode.begin(), mode.end());
    return ::_wfopen(filename.c_str(), wideMode.c_str());
}

static int KeepReturnFailure(int descriptor, const std::string& stamp) {
    const auto missing = std::filesystem::path("anyps5-freopen-host-log-missing-" + stamp) / "output.txt";
    const auto native = __acrt_iob_func(descriptor);
    Require((descriptor == 1 ? stdout : stderr) == native, "the host stream was already kept");
    Require(!std::filesystem::exists(missing.parent_path()), "the missing parent unexpectedly exists");
    if (!failures.empty()) { HostWrite(2, failures); return 2; }
    bool reopened = false;
    bool threw = false;
    int openError = 0;
    {
        struct Cleanup {
            int descriptor;
            int saved;
            int limit;
            std::vector<std::FILE*> fillers;
            ~Cleanup() {
                if (releaseForOpen) {
                    std::fclose(releaseForOpen);
                    releaseForOpen = nullptr;
                }
                for (auto* stream : fillers) std::fclose(stream);
                Require(::_setmaxstdio(limit) == limit, "cannot restore the child stream limit");
                if (saved >= 0) {
                    Require(HostDup2(saved, descriptor) == 0, "cannot restore the child descriptor");
                    Require(HostClose(saved) == 0, "cannot close the saved child descriptor");
                }
            }
        } cleanup{descriptor, HostDup(descriptor), ::_getmaxstdio(), {}};
        if (cleanup.saved < 0 || ::_setmaxstdio(64) != 64) return 3;
        for (int i = 0; i < 64; ++i) {
            auto* stream = std::fopen("NUL", "r");
            if (!stream) break;
            cleanup.fillers.push_back(stream);
        }
        Require(errno == EMFILE && !cleanup.fillers.empty(), "the child stream table was not filled");
        const int duplicate = HostDup(descriptor);
        Require(duplicate >= 0, "low-level descriptors were exhausted instead of stream slots");
        if (duplicate >= 0) HostClose(duplicate);
        errno = 0;
        const int kept = descriptor == 1 ? KeepHostStandardOutput_nid_no_patch() : KeepHostStandardError_nid_no_patch();
        const int keepError = errno;
        Require(kept < 0 && keepError == EMFILE, "the real keep did not return the stream-slot failure");
        Require((descriptor == 1 ? stdout : stderr) == native, "a failed keep replaced the host stream");
        if (!failures.empty()) { HostWrite(2, failures); return 4; }
        releaseForOpen = cleanup.fillers.back();
        cleanup.fillers.pop_back();
        errno = E2BIG;
        try {
            auto* stream = descriptor == 1 ? __stdoutp_nid_postfix : __stderrp_nid_postfix;
            auto lease = AcquireFileStreamLease_nid_no_patch(stream, true);
            Require(lease.Valid(), "cannot lease the standard stream");
            if (lease.Valid()) {
                const std::lock_guard<std::recursive_mutex> gate(*lease.Gate());
                reopened = stream->Reopen(missing, "w", OpenMissingAfterRelease);
            }
        } catch (...) {
            threw = true;
        }
        openError = errno;
    }
    Require(!threw, "a return failure escaped from Reopen");
    Require(openCalls == 1, "the real native-open callback did not run exactly once");
    Require(openEntryError == E2BIG, "a keep failure leaked errno into the native open");
    Require(!reopened && openError == ENOENT, "the actual native-open failure was replaced");
    HostWrite(2, failures);
    return failures.empty() ? 0 : 1;
}
#else
static int NativeClosedStdout(const std::string& stamp, const std::filesystem::path& record) {
    const auto temporary = record.string() + ".tmp";
    std::error_code ignored;
    std::filesystem::remove(record, ignored);
    std::filesystem::remove(temporary, ignored);
    const auto missing = std::filesystem::absolute(std::filesystem::path("anyps5-freopen-host-log-missing-" + stamp) / "output.txt");
    Require(!std::filesystem::exists(missing.parent_path()), "the native baseline parent unexpectedly exists");
    Require(HostFileno(stdout) == 1, "the native baseline stdout descriptor is not 1");
    if (!failures.empty()) { HostWrite(2, failures); return 2; }
    const int saved = HostDup(1);
    if (saved < 0) return 2;
    if (HostClose(1) != 0) { HostClose(saved); return 2; }
    errno = E2BIG;
    const bool reopened = std::freopen(missing.c_str(), "w", stdout) != nullptr;
    const int openError = errno;
    Require(HostDup2(saved, 1) == 0, "cannot restore native baseline stdout");
    Require(HostClose(saved) == 0, "cannot close the native baseline saved descriptor");
    Require(!reopened && openError != 0, "the native baseline did not produce a real open failure");
    if (!failures.empty()) { HostWrite(2, failures); return 2; }
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << "native-closed-stdout-v1\n" << missing.string() << '\n' << reopened << ' ' << openError << '\n';
        output.close();
        Require(!output.fail(), "cannot write native baseline outcome");
    }
    if (failures.empty()) {
        std::filesystem::rename(temporary, record, ignored);
        Require(!ignored, "cannot publish native baseline outcome");
    }
    HostWrite(2, failures);
    return failures.empty() ? 0 : 1;
}

static int KeepReturnFailure(const std::filesystem::path& record) {
    std::string version;
    std::string nativePath;
    bool nativeReopened = true;
    int nativeError = 0;
    {
        std::ifstream input(record, std::ios::binary);
        std::getline(input, version);
        std::getline(input, nativePath);
        input >> nativeReopened >> nativeError;
        Require(!input.fail() && version == "native-closed-stdout-v1" && !nativePath.empty() && !nativeReopened && nativeError != 0,
                "the independent native baseline is missing or invalid");
    }
    std::error_code ignored;
    Require(std::filesystem::remove(record, ignored) && !ignored, "cannot consume the native baseline outcome");
    const std::filesystem::path missing(nativePath);
    Require(!std::filesystem::exists(missing.parent_path()), "the missing parent unexpectedly exists");
    Require(HostFileno(stdout) == 1, "the Reopen stdout descriptor is not 1");
    if (!failures.empty()) { HostWrite(2, failures); return 2; }
    const int saved = HostDup(1);
    if (saved < 0) return 2;
    auto* const original = stdout;
    Require(HostClose(1) == 0, "cannot close the child's stdout descriptor");
    errno = 0;
    int kept = 0;
    int keepError = 0;
    bool keepThrew = false;
    try {
        kept = KeepHostStandardOutput_nid_no_patch();
        keepError = errno;
    } catch (...) {
        keepThrew = true;
    }
    if (keepThrew || kept >= 0 || keepError != EBADF) {
        HostDup2(saved, 1);
        HostClose(saved);
        Require(false, "the real keep did not return EBADF");
        HostWrite(2, failures);
        return 3;
    }
    bool threw = false;
    bool reopened = true;
    try {
        auto lease = AcquireFileStreamLease_nid_no_patch(__stdoutp_nid_postfix, true);
        Require(lease.Valid(), "cannot lease stdout for the real Reopen branch");
        if (lease.Valid()) {
            const std::lock_guard<std::recursive_mutex> gate(*lease.Gate());
            Require(__stdoutp_nid_postfix->IsOpen(), "the standard FILE was detached before Reopen");
            if (__stdoutp_nid_postfix->IsOpen()) {
                errno = E2BIG;
                reopened = __stdoutp_nid_postfix->Reopen(missing, "w");
            }
        }
    } catch (...) {
        threw = true;
    }
    const int openError = errno;
    Require(HostDup2(saved, 1) == 0, "cannot restore the child's stdout descriptor");
    Require(HostClose(saved) == 0, "cannot close the saved child descriptor");
    Require(kept < 0 && keepError == EBADF, "the real keep did not return EBADF");
    Require(stdout == original, "a failed keep replaced host stdout");
    if (threw || reopened != nativeReopened || openError != nativeError) {
        const int savedError = errno;
        std::fprintf(stderr, "Reopen outcome: reopened=%d threw=%d errno=%d; native reopened=%d errno=%d\n",
                     reopened, threw, openError, nativeReopened, nativeError);
        errno = savedError;
    }
    Require(!threw && reopened == nativeReopened && openError == nativeError, "the real Reopen outcome differs from native freopen");
    HostWrite(2, failures);
    return failures.empty() ? 0 : 1;
}
#endif

int main(int argc, char** argv) {
    const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
#ifndef _WIN32
    if (argc > 1 && std::string(argv[1]) == "keep-failure") return KeepFailure(stamp);
    if (argc > 2 && std::string(argv[1]) == "native-closed-stdout") return NativeClosedStdout(stamp, argv[2]);
    if (argc > 2 && std::string(argv[1]) == "keep-return-failure") return KeepReturnFailure(argv[2]);
    if (argc > 1 && (std::string(argv[1]) == "native-closed-stdout" || std::string(argv[1]) == "keep-return-failure")) return 2;
#else
    if (argc > 1 && std::string(argv[1]) == "keep-return-stdout") return KeepReturnFailure(1, stamp);
    if (argc > 1 && std::string(argv[1]) == "keep-return-stderr") return KeepReturnFailure(2, stamp);
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
