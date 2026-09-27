#include "SceTypes.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int APS5_VABI vsscanf_nid_postfix(const char*, const char*, VaList*);

static void Require(bool valid) { if (!valid) std::abort(); }

static int APS5_VABI ScanGuest(const char* input, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::ScanWindows(input, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

static int APS5_VABI ScanExport(const char* input, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = vsscanf_nid_postfix(input, format, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
    return result;
}

int main() {
    int decimal = 0, hex = 0, count = -1;
    double fraction = 0;
    char word[16]{};
    Require(ScanGuest("  -42 0xff 3.25 Doom", "%d %i %lf %15s%n", &decimal, &hex, &fraction, word, &count) == 4);
    Require(decimal == -42 && hex == 255 && fraction == 3.25 && std::strcmp(word, "Doom") == 0 && count == 20);
    std::int64_t guestLong = 0;
    Require(ScanGuest("4294967297", "%ld", &guestLong) == 1 && guestLong == 4294967297LL);
    char bracket[8]{};
    Require(ScanGuest("abc,tail", "%3[a-z],%*s", bracket) == 1 && std::strcmp(bracket, "abc") == 0);
    Require(ScanGuest("", "%d", &decimal) == EOF);
    Require(ScanGuest("word", "%d", &decimal) == 0);
    Require(ScanGuest("50 60", "%d %d", &decimal, &hex) == 2 && decimal == 50 && hex == 60);
    std::array<int, 8> stack{};
    Require(ScanGuest("1 2 3 4 5 6 7 8", "%d %d %d %d %d %d %d %d",
                      &stack[0], &stack[1], &stack[2], &stack[3], &stack[4], &stack[5], &stack[6], &stack[7]) == 8);
    for (int index = 0; index < 8; ++index) Require(stack[index] == index + 1);
    Require(ScanExport("18", "%d", &decimal) == 1 && decimal == 18);
}
