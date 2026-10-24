#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/HostStandardStreams.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

extern "C" {
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI dup2_nid_postfix(int, int);
std::int64_t APS5_VABI write_nid_postfix(int, const void*, std::size_t);
int APS5_VABI printf_nid_postfix(const char*, ...);
int APS5_VABI puts_nid_postfix(const char*);
int APS5_VABI fprintf_nid_postfix(FileStream*, const char*, ...);
int APS5_VABI fflush_nid_postfix(FileStream*);
int APS5_VABI setvbuf_nid_postfix(FileStream*, char*, int, std::size_t);
void APS5_VABI setbuf_nid_postfix(FileStream*, char*);
void APS5_VABI flockfile_nid_postfix(FileStream*);
void APS5_VABI funlockfile_nid_postfix(FileStream*);
void APS5_VABI perror_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
extern FileStream _Stdout_nid_postfix;
extern FileStream _Stderr_nid_postfix;
}

static void Require(bool value) { if (!value) std::abort(); }

static DWORD Available(int descriptor, void* buffer, DWORD capacity, DWORD& read) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    DWORD available = 0;
    Require(::PeekNamedPipe(handle, buffer, capacity, &read, &available, nullptr) != 0);
    return available;
}

static void Split(int standard) {
    int capture[2]{-1, -1};
    Require(::_pipe(capture, 65536, _O_BINARY) == 0);
    const int original = ::_dup(standard);
    Require(original >= 0 && ::_dup2(capture[1], standard) == 0);
    Require(std::fputs("host-before\n", standard == 1 ? stdout : stderr) >= 0);

    int guest[2]{-1, -1};
    Require(pipe_nid_postfix(guest) == 0);
    Require(dup2_nid_postfix(guest[1], standard) == standard);
    std::FILE* const stream = standard == 1 ? stdout : stderr;

    std::string host(8192, 'h');
    host.back() = '\n';
    Require(std::fputs(host.c_str(), stream) >= 0);
    Require(std::fflush(stream) == 0);
    host.insert(0, "host-before\n");
    Require(write_nid_postfix(standard, "guest", 5) == 5);

    char guestBytes[8]{};
    DWORD guestRead = 0;
    Require(Available(guest[0], guestBytes, sizeof(guestBytes), guestRead) == 5);
    Require(guestRead == 5 && std::string(guestBytes, 5) == "guest");
    char hostBytes[8]{};
    DWORD hostRead = 0;
    Require(Available(capture[0], hostBytes, sizeof(hostBytes), hostRead) == host.size());
    Require(hostRead == sizeof(hostBytes) && std::string(hostBytes, hostRead) == host.substr(0, hostRead));

    Require(::_dup2(original, standard) == 0);
    ::_close(original);
    ::_close(capture[0]);
    ::_close(capture[1]);
    ::_close(guest[0]);
    ::_close(guest[1]);
}

static void GuestText(int standard, const std::string& text) {
    int guest[2]{-1, -1};
    Require(pipe_nid_postfix(guest) == 0);
    const int original = ::_dup(standard);
    Require(original >= 0);
    Require(dup2_nid_postfix(guest[1], standard) == standard);
    if (standard == 1) {
        Require(printf_nid_postfix("guest-printf\n") >= 0);
        Require(puts_nid_postfix("guest-puts") >= 0);
        Require(fflush_nid_postfix(&_Stdout_nid_postfix) == 0);
    } else {
        Require(fprintf_nid_postfix(&_Stderr_nid_postfix, "guest-fprintf\n") >= 0);
        *__error_nid_postfix() = 2;
        perror_nid_postfix("guest-perror");
        Require(fflush_nid_postfix(&_Stderr_nid_postfix) == 0);
    }
    std::string got(text.size(), '\0');
    DWORD read = 0;
    Require(Available(guest[0], got.data(), static_cast<DWORD>(got.size()), read) == text.size());
    Require(read == text.size() && got == text);
    Require(::_dup2(original, standard) == 0);
    ::_close(original);
    ::_close(guest[0]);
    ::_close(guest[1]);
}

static void Buffering(int standard) {
    int capture[2]{-1, -1};
    Require(::_pipe(capture, 65536, _O_BINARY) == 0);
    const int original = ::_dup(standard);
    Require(original >= 0 && ::_dup2(capture[1], standard) == 0);
    auto* guestStream = standard == 1 ? &_Stdout_nid_postfix : &_Stderr_nid_postfix;
    char buffer[4096]{};
    flockfile_nid_postfix(guestStream);
    if (standard == 1) setbuf_nid_postfix(guestStream, buffer);
    else Require(setvbuf_nid_postfix(guestStream, buffer, 0, sizeof(buffer)) == 0);
    funlockfile_nid_postfix(guestStream);
    Require(fprintf_nid_postfix(guestStream, "guest-buffered") == 14);
    Require(std::fputs("host-immediate", standard == 1 ? stdout : stderr) >= 0);
    char bytes[64]{};
    DWORD read = 0;
    Require(Available(capture[0], bytes, sizeof(bytes), read) == 14);
    Require(read == 14 && std::string(bytes, read) == "host-immediate");
    Require(::_read(capture[0], bytes, 14) == 14);
    Require(Available(capture[0], bytes, sizeof(bytes), read) == 0);
    Require(fflush_nid_postfix(guestStream) == 0);
    Require(Available(capture[0], bytes, sizeof(bytes), read) == 14);
    Require(read == 14 && std::string(bytes, read) == "guest-buffered");
    Require(::_read(capture[0], bytes, 14) == 14);

    int guest[2]{-1, -1};
    Require(pipe_nid_postfix(guest) == 0 && dup2_nid_postfix(guest[1], standard) == standard);
    Require(fprintf_nid_postfix(guestStream, "guest-redirected") == 16 && fflush_nid_postfix(guestStream) == 0);
    Require(std::fputs("host-original", standard == 1 ? stdout : stderr) >= 0);
    Require(Available(capture[0], bytes, sizeof(bytes), read) == 13);
    Require(read == 13 && std::string(bytes, read) == "host-original");
    Require(Available(guest[0], bytes, sizeof(bytes), read) == 16);
    Require(read == 16 && std::string(bytes, read) == "guest-redirected");
    Require(setvbuf_nid_postfix(guestStream, nullptr, 2, 0) == 0);
    Require(::_dup2(original, standard) == 0);
    ::_close(original);
    ::_close(capture[0]);
    ::_close(capture[1]);
    ::_close(guest[0]);
    ::_close(guest[1]);
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--buffering") == 0) {
        Buffering(2);
        Buffering(1);
        return 0;
    }
    Split(2);
    Split(1);
    GuestText(1, "guest-printf\nguest-puts\n");
    GuestText(2, "guest-fprintf\nguest-perror: No such file or directory\n");
    return 0;
}
