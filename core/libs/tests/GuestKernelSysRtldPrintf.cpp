#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <source_location>
#include <string>

extern "C" {
int APS5_VABI rtld_printf_nid_postfix(const char* format, ...);
int APS5_VABI open_nid_postfix(const char* path, int flags, int mode);
int APS5_VABI close_nid_postfix(int descriptor);
int APS5_VABI dup_nid_postfix(int descriptor);
int APS5_VABI dup2_nid_postfix(int from, int to);
}

namespace {

constexpr int ORdwr = 2;
constexpr int OCreat = 0x200;
constexpr int OTrunc = 0x400;

std::string g_path;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

template <typename Print>
std::string Capture(Print print) {
    std::fflush(stdout);
    const int saved = dup_nid_postfix(1);
    const int file = open_nid_postfix(g_path.c_str(), ORdwr | OCreat | OTrunc, 0600);
    Require(saved >= 0 && file >= 0 && dup2_nid_postfix(file, 1) == 1);
    print();
    Require(dup2_nid_postfix(saved, 1) == 1 && close_nid_postfix(saved) == 0 && close_nid_postfix(file) == 0);
    std::ifstream stream(g_path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

#define PRINTS(expected, ...) \
    Require(Capture([&] { Require(rtld_printf_nid_postfix(__VA_ARGS__) == 0); }) == std::string(expected, sizeof(expected) - 1))

void Conversions() {
    PRINTS("plain -42|7|4000000000\n", "plain %d|%i|%u\n", -42, 7, 4000000000u);
    PRINTS("ff FF 10 0xff 010 0 5", "%x %X %o %#x %#o %#x %+d", 255, 255, 8, 255, 8, 0, 5);
    PRINTS("[   42][42   ][00042][007][3    ]-0042", "[%5d][%-5d][%05d][%.3d][%-05d]%05d", 42, 42, 42, 7, 3, -42);
    PRINTS("abc|ab|   ab|ab   |000ab|(null)", "%s|%.2s|%5s|%-5s|%05s|%s", "abc", "abc", "ab", "ab", "ab", static_cast<const char*>(nullptr));
    PRINTS("ok%", "%c%c%%", 'o', 'k');
    PRINTS("-1 -2 -3 -4 -5 -6", "%ld %lld %qd %jd %zd %td", std::int64_t{-1}, std::int64_t{-2}, std::int64_t{-3}, std::int64_t{-4}, std::int64_t{-5}, std::int64_t{-6});
    PRINTS("-32767 -1 32769 255 511", "%hd %hhd %hu %hhu %hhhd", 0x18001, 0x1ff, 0x18001, 0x1ff, 0x1ff);
    PRINTS("0x1234 0     1234", "%p %p %8p", reinterpret_cast<void*>(0x1234), static_cast<void*>(nullptr), reinterpret_cast<void*>(0x1234));
    PRINTS("   1|   2|005", "%*d|%-*d|%.*d", 4, 1, -4, 2, 3, 5);
    PRINTS("5<ONE,TWO> 0", "%b %b", 5, "\10\3ONE\1TWO", 0, "\20\1A");
    const unsigned char bytes[] = {0xde, 0xad, 0x01};
    PRINTS("de:ad:01", "%3D", bytes, ":");
    PRINTS("10 -10 ff -ff", "%r %+r %y %y", 10, -10, 255, -255);
}

void Quirks() {
    int count = 99;
    std::int64_t wide = 99;
    PRINTS("abcd", "ab%ncd%ln", &count, &wide);
    Require(count == 0 && wide == 0);
    PRINTS("x%wy%d%%z", "x%wy%d%%z", 5);
    PRINTS("(fmt null)\n", static_cast<const char*>(nullptr));
    PRINTS("end%\0", "end%");
    const std::string long_text(600, 'a');
    const auto printed = Capture([&] { Require(rtld_printf_nid_postfix(long_text.c_str()) == 0); });
    Require(printed == std::string(599, 'a'));
}

}

int main() {
    g_path = "anyps5-rtld-printf-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt";
    Conversions();
    Quirks();
    Require(std::filesystem::remove(g_path));
    std::puts("guest_kernel_sys_rtld_printf passed");
    return 0;
}
