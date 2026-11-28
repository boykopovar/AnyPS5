#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "tests/TemporaryFile.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

namespace {

std::FILE* restoreTarget = nullptr;
int failureCall = 0;
int restoreCalls = 0;
int rejectedCalls = 0;
std::FILE* syncFailureTarget = nullptr;
int rejectedSyncCalls = 0;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "stream purge restore: %s\n", message);
        std::abort();
    }
}

}

extern "C" int ungetc(int byte, std::FILE* native) {
    static const auto original = reinterpret_cast<int (*)(int, std::FILE*)>(::dlsym(RTLD_NEXT, "ungetc"));
    Require(original != nullptr, "resolve native ungetc");
    if (native == restoreTarget && ++restoreCalls == failureCall) {
        ++rejectedCalls;
        errno = ENOMEM;
        return EOF;
    }
    return original(byte, native);
}

extern "C" int fflush(std::FILE* native) {
    static const auto original = reinterpret_cast<int (*)(std::FILE*)>(::dlsym(RTLD_NEXT, "fflush"));
    Require(original != nullptr, "resolve native stream synchronization");
    if (syncFailureTarget && native == syncFailureTarget) {
        ++rejectedSyncCalls;
        errno = EIO;
        return EOF;
    }
    return original(native);
}

int main(int argc, char** argv) {
    Require(argc == 2 && std::strlen(argv[1]) == 1 && argv[1][0] >= '0' && argv[1][0] <= '3', "select success, restore rejection or synchronization rejection");
    failureCall = argv[1][0] - '0';
    auto* native = TemporaryFile::Open();
    Require(native != nullptr, "open temporary stream");
    FileStream stream{native};
    Require(std::setvbuf(native, nullptr, _IOFBF, 4096) == 0, "select native buffering before I/O");
    Require(std::fwrite("abcdef", 1, 6, native) == 6 && std::fflush(native) == 0 && std::fseek(native, 0, SEEK_SET) == 0, "prepare buffered input");
    for (const char expected : {'a', 'b', 'c'}) Require(fgetc_nid_postfix(&stream) == expected, "read before pushback");
    Require(ungetc_nid_postfix('X', &stream) == 'X' && ungetc_nid_postfix('Y', &stream) == 'Y', "push back two bytes");

    restoreTarget = native;
    if (failureCall == 3) syncFailureTarget = native;
    errno = 123;
    const int result = fpurge_nid_postfix(&stream);
    const int savedErrno = errno;
    restoreTarget = nullptr;
    syncFailureTarget = nullptr;
    if (failureCall == 3) {
        Require(rejectedSyncCalls == 1, "attempt input synchronization before restoring bytes");
        Require(result == -1 && savedErrno == EIO, "report input synchronization failure");
        Require(restoreCalls == 0 && rejectedCalls == 0, "do not restore bytes after synchronization fails");
        Require(stream.PushedBackBytes().empty() && !stream.PushbackLive(), "do not publish input that was never restored");
        Require(stream.GuestState().position == stream.GuestState().buffer, "discard the guest buffer after synchronization fails");
        Require(stream.Close() == 0, "close stream after synchronization failure");
        std::puts("stream purge synchronization failure test passed");
        return 0;
    }
    Require(rejectedSyncCalls == 0, "do not reject synchronization in restore cases");
    Require(restoreCalls == (failureCall == 1 ? 1 : 2), "reach the selected restore boundary");
    Require(rejectedCalls == (failureCall == 0 ? 0 : 1), "inject exactly the selected failure");
    Require(result == (failureCall == 0 ? 0 : -1), "report incomplete pushback restoration");
    Require(savedErrno == (failureCall == 0 ? 123 : ENOMEM), "preserve success errno or report the restoration error");
    const char* recorded = failureCall == 0 ? "XY" : failureCall == 1 ? "" : "X";
    Require(stream.PushedBackBytes() == recorded, "record only successfully restored bytes");
    Require(stream.PushbackLive() == (failureCall != 1), "publish whether restored input remains");
    Require(std::ftell(native) == (failureCall == 0 ? 1 : failureCall == 1 ? 3 : 2), "retain the correct logical input position");

    Require(fpurge_nid_postfix(&stream) == 0 && fpurge_nid_postfix(&stream) == 0, "purge again after disabling injection");
    char bytes[3]{};
    const char* expected = failureCall == 0 ? "YXd" : failureCall == 1 ? "def" : "Xde";
    Require(fread_nid_postfix(bytes, 1, sizeof(bytes), &stream) == sizeof(bytes) && std::memcmp(bytes, expected, sizeof(bytes)) == 0, "failed restoration never resurrects rejected bytes");
    Require(fpurge_nid_postfix(&stream) == 0, "purge after consuming retained input");
    Require(fgetc_nid_postfix(&stream) == (failureCall == 0 ? 'e' : failureCall == 1 ? EOF : 'f'), "consumed pushback does not return");
    Require(stream.Close() == 0, "close temporary stream");
    std::puts("stream purge restore tests passed");
}
