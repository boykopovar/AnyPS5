#include "prx/libc/include/Api.hpp"
#ifdef _WIN32
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#endif
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
    const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto sinkName = "anyps5-freopen-host-standard-" + stamp + ".txt";
#ifdef _WIN32
    const auto directoryName = "anyps5-freopen-host-standard-" + stamp;
#endif
    FileStream* sink = fopen_nid_postfix(sinkName.c_str(), "wb");
    Check(sink != nullptr);
    const int sinkDescriptor = fileno_nid_postfix(sink);
    const int original = HostDup(1);
    Check(sinkDescriptor >= 0 && original >= 0 && HostDup2(sinkDescriptor, 1) == 0);

#ifdef _WIN32
    Check(std::filesystem::create_directory(directoryName));
    errno = 0;
    Check(freopen_nid_postfix(directoryName.c_str(), "r", __stdoutp_nid_postfix) == __stdoutp_nid_postfix);
    FileStat status{};
    Check(sceKernelFstat(fileno_nid_postfix(__stdoutp_nid_postfix), &status) == 0 && (status.st_mode & 0170000) == 0040000);
    char block[16];
    Check(fread_nid_postfix(block, 1, sizeof(block), __stdoutp_nid_postfix) == 0);
    Check(ferror_nid_postfix(__stdoutp_nid_postfix) != 0 && errno == EISDIR);
    Check(HostDup2(sinkDescriptor, 1) == 0);
#endif

    errno = 0;
    FileStream* const reopened = freopen_nid_postfix("", "w", __stdoutp_nid_postfix);
    const int error = errno;

    const int written = HostWrite(1, "host", 4);
    const bool restored = HostDup2(original, 1) == 0 && HostClose(original) == 0;
    Check(fclose_nid_postfix(sink) == 0);
    Check(restored);
    Check(reopened == nullptr);
    Check(error == ENOENT);
    Check(written == 4);
    Check(Contents(sinkName) == "host");

    errno = 0;
    Check(fputs_nid_postfix("text", __stdoutp_nid_postfix) == -1 && errno == EBADF);
    Check(std::filesystem::remove(sinkName));
#ifdef _WIN32
    Check(std::filesystem::remove(directoryName));
#endif
    return 0;
}
