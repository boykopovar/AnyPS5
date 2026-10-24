#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <fcntl.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

extern "C" {
FileStream* APS5_VABI freopen_nid_postfix(const char* filename, const char* mode, FileStream* stream);
int APS5_VABI fputs_nid_postfix(const char* str, FileStream* stream);
extern FileStream* __stdoutp_nid_postfix;
}

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "freopen host standard check failed at line %d, errno %d\n", line, errno);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

#ifdef _WIN32
static int HostDup(int descriptor) { return ::_dup(descriptor); }
static int HostDup2(int from, int to) { return ::_dup2(from, to); }
static int HostClose(int descriptor) { return ::_close(descriptor); }
static int HostWrite(int descriptor, const char* text, unsigned size) { return ::_write(descriptor, text, size); }
static int HostFileno(std::FILE* stream) { return ::_fileno(stream); }
#else
static int HostDup(int descriptor) { return ::dup(descriptor); }
static int HostDup2(int from, int to) { return ::dup2(from, to) == to ? 0 : -1; }
static int HostClose(int descriptor) { return ::close(descriptor); }
static int HostWrite(int descriptor, const char* text, unsigned size) { return static_cast<int>(::write(descriptor, text, size)); }
static int HostFileno(std::FILE* stream) { return ::fileno(stream); }
#endif

static std::string Contents(const std::string& name) {
    std::ifstream stream(name, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

int main() {
    const auto sinkName = "anyps5-freopen-host-standard-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt";
    std::FILE* sink = std::fopen(sinkName.c_str(), "wb");
    Check(sink != nullptr);
    const int original = HostDup(1);
    Check(original >= 0 && HostDup2(HostFileno(sink), 1) == 0);

    errno = 0;
    FileStream* const reopened = freopen_nid_postfix("", "w", __stdoutp_nid_postfix);
    const int error = errno;

    const int written = HostWrite(1, "host", 4);
    const bool restored = HostDup2(original, 1) == 0 && HostClose(original) == 0;
    Check(std::fclose(sink) == 0);
    Check(restored);
    Check(reopened == nullptr);
    Check(error == ENOENT);
    Check(written == 4);
    Check(Contents(sinkName) == "host");

    errno = 0;
    Check(fputs_nid_postfix("text", __stdoutp_nid_postfix) == -1 && errno == EBADF);
    Check(std::filesystem::remove(sinkName));
    return 0;
}
