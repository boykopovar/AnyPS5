#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <source_location>
#include <string>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

extern "C" {
int APS5_VABI mkstemp_nid_postfix(char*);
int APS5_VABI isatty_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI close_nid_postfix(int);
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI socket_nid_postfix(int, int, int);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::size_t);
std::int64_t APS5_VABI write_nid_postfix(int, const void*, std::size_t);
std::int64_t APS5_VABI lseek_nid_postfix(int, std::int64_t, int);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "POSIX filesystem check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

int main() {
    std::random_device random;
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-posix-test-" + std::to_string(random()) + "-" + std::to_string(random()));
    Require(std::filesystem::create_directory(root));
    AddPathAlias_nid_no_patch("/anyps5-posix-test", root.string().c_str());
    const std::string prefix = "/anyps5-posix-test/file-";
    std::string pattern = prefix + "XXXXXXXXX";
    const int descriptor = mkstemp_nid_postfix(pattern.data());
    Require(descriptor >= 0);
    Require(pattern.starts_with(prefix) && pattern.size() == prefix.size() + 9);
    Require(pattern.substr(prefix.size()).find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::string::npos);
    Require(std::filesystem::file_size(ResolvePath_nid_no_patch(pattern.c_str())) == 0);
#ifndef _WIN32
    struct stat status{};
    Require(::fstat(descriptor, &status) == 0 && (status.st_mode & 0777) == 0600);
#endif
    const char payload[] = {'a', '\0', '\r', '\n', '\x1a', 'z'};
    Require(write_nid_postfix(descriptor, payload, sizeof(payload)) == sizeof(payload));
    Require(lseek_nid_postfix(descriptor, 0, SEEK_SET) == 0);
    std::array<char, sizeof(payload)> received{};
    Require(read_nid_postfix(descriptor, received.data(), received.size()) == static_cast<std::int64_t>(received.size()));
    Require(std::memcmp(payload, received.data(), sizeof(payload)) == 0);
    Require(isatty_nid_postfix(descriptor) == 0 && *__error_nid_postfix() == 25);
    Require(close_nid_postfix(descriptor) == 0);
    Require(isatty_nid_postfix(descriptor) == 0 && *__error_nid_postfix() == 9);
    std::string second = prefix + "XXXXXXXXX";
    const int other = mkstemp_nid_postfix(second.data());
    Require(other >= 0 && second != pattern);
    Require(close_nid_postfix(other) == 0);
    const std::string candidates = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for (const char character : candidates) {
        std::ofstream stream(root / (std::string("occupied-") + character));
        stream << "preserved";
        Require(stream.good());
    }
    std::string occupied = "/anyps5-posix-test/occupied-X";
    Require(mkstemp_nid_postfix(occupied.data()) == -1 && *__error_nid_postfix() == 17);
    for (const char character : candidates) {
        std::ifstream stream(root / (std::string("occupied-") + character));
        std::string contents;
        stream >> contents;
        Require(contents == "preserved");
    }
    Require(std::filesystem::remove(root / "occupied-7"));
    occupied = "/anyps5-posix-test/occupied-X";
    const int collision = mkstemp_nid_postfix(occupied.data());
    Require(collision >= 0 && occupied == "/anyps5-posix-test/occupied-7");
    Require(close_nid_postfix(collision) == 0);
    std::string fixed = "/anyps5-posix-test/fixed";
    const int unchanged = mkstemp_nid_postfix(fixed.data());
    Require(unchanged >= 0 && fixed == "/anyps5-posix-test/fixed");
    Require(close_nid_postfix(unchanged) == 0);
    Require(mkstemp_nid_postfix(fixed.data()) == -1 && *__error_nid_postfix() == 17);
    std::string missing = "/anyps5-posix-test/missing/file-XXXXXX";
    Require(mkstemp_nid_postfix(missing.data()) == -1 && *__error_nid_postfix() == 2);
    std::string notDirectory = fixed + "/file-XXXXXX";
    Require(mkstemp_nid_postfix(notDirectory.data()) == -1);
    Require(*__error_nid_postfix() == 20 || *__error_nid_postfix() == 2);
    std::string empty;
    Require(mkstemp_nid_postfix(empty.data()) == -1 && *__error_nid_postfix() == 22);
    Require(mkstemp_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    std::string tooLong(1024, 'X');
    Require(mkstemp_nid_postfix(tooLong.data()) == -1 && *__error_nid_postfix() == 63);
    Require(tooLong == std::string(1024, 'X'));
    Require(isatty_nid_postfix(-1) == 0 && *__error_nid_postfix() == 9);
    Require(isatty_nid_postfix(100000) == 0 && *__error_nid_postfix() == 9);
    int pipes[2]{};
    Require(pipe_nid_postfix(pipes) == 0);
    for (const int pipe : pipes) {
        Require(isatty_nid_postfix(pipe) == 0 && *__error_nid_postfix() == 25);
        Require(close_nid_postfix(pipe) == 0);
    }
    const int socket = socket_nid_postfix(2, 2, 0);
    Require(socket >= GuestSockets::FirstDescriptor);
    Require(isatty_nid_postfix(socket) == 0 && *__error_nid_postfix() == 25);
    Require(close_nid_postfix(socket) == 0);
    Require(isatty_nid_postfix(socket) == 0 && *__error_nid_postfix() == 9);
#ifdef _WIN32
    const int nullDevice = ::_open("NUL", _O_RDWR);
    Require(nullDevice >= 0);
    Require(isatty_nid_postfix(nullDevice) == 0 && *__error_nid_postfix() == 25);
    Require(::_close(nullDevice) == 0);
    const int console = ::_open("CONIN$", _O_RDONLY);
    if (console >= 0) {
        *__error_nid_postfix() = 123;
        Require(isatty_nid_postfix(console) == 1 && *__error_nid_postfix() == 123);
        Require(::_close(console) == 0);
    }
#else
    const int terminal = ::posix_openpt(O_RDWR | O_NOCTTY);
    Require(terminal >= 0 && ::grantpt(terminal) == 0 && ::unlockpt(terminal) == 0);
    const int slave = ::open(::ptsname(terminal), O_RDWR | O_NOCTTY);
    Require(slave >= 0);
    *__error_nid_postfix() = 123;
    Require(isatty_nid_postfix(slave) == 1 && *__error_nid_postfix() == 123);
    Require(::close(slave) == 0 && ::close(terminal) == 0);
#endif
    RemovePathAlias_nid_no_patch("/anyps5-posix-test");
    Require(std::filesystem::remove_all(root) > 0);
}
