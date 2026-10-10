#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int APS5_VABI wcsrtombs_s_nid_postfix(
    std::size_t*, char*, std::size_t, const std::uint16_t**, std::size_t, void*);
static void Require(bool value) { if (!value) std::abort(); }

constexpr auto Failed = static_cast<std::size_t>(-1);
constexpr auto Huge = std::size_t{1} << 63;
static const std::uint16_t text[] = {'a', 'b', 'c', 0};
static const std::uint16_t wide[] = {'a', 0x100, 'b', 0};

struct Run {
    int status;
    std::size_t result = 7;
    const std::uint16_t* source;
    char out[8];
};

static Run Convert(const std::uint16_t* input, std::size_t capacity, std::size_t limit, bool toBuffer = true) {
    Run run{};
    std::memset(run.out, 'x', sizeof(run.out));
    run.source = input;
    std::uint64_t state[2] = {};
    run.status = wcsrtombs_s_nid_postfix(&run.result, toBuffer ? run.out : nullptr, capacity, &run.source, limit, state);
    return run;
}

int main() {
    auto run = Convert(text, 0, 0, false);
    Require(run.status == 0 && run.result == 3 && run.source == text);
    run = Convert(wide, 0, Huge, false);
    Require(run.status == 86 && run.result == Failed && run.source == wide);
    run = Convert(text, 5, 1, false);
    Require(run.status == 22 && run.result == Failed && run.source == text);

    run = Convert(text, 8, 8);
    Require(run.status == 0 && run.result == 3 && run.source == nullptr && std::memcmp(run.out, "abc\0x", 5) == 0);
    run = Convert(text, 4, 4);
    Require(run.status == 0 && run.result == 3 && run.source == nullptr && std::memcmp(run.out, "abc\0x", 5) == 0);
    run = Convert(text, 8, 2);
    Require(run.status == 0 && run.result == 2 && run.source == text + 2 && std::memcmp(run.out, "ab\0x", 4) == 0);
    run = Convert(text, 8, 0);
    Require(run.status == 0 && run.result == 0 && run.source == text && std::memcmp(run.out, "\0x", 2) == 0);

    run = Convert(text, 3, 8);
    Require(run.status == 34 && run.result == Failed && run.source == text + 3 && std::memcmp(run.out, "\0bcx", 4) == 0);
    run = Convert(text, 3, 3);
    Require(run.status == 34 && run.result == Failed && run.source == text + 3 && std::memcmp(run.out, "\0bcx", 4) == 0);
    run = Convert(text, 1, 8);
    Require(run.status == 34 && run.result == Failed && run.source == text + 1 && std::memcmp(run.out, "\0x", 2) == 0);

    run = Convert(wide, 8, 8);
    Require(run.status == 86 && run.result == Failed && run.source == wide && std::memcmp(run.out, "a\0xx", 4) == 0);

    run = Convert(text, 0, 8);
    Require(run.status == 22 && run.result == Failed && run.source == text && run.out[0] == 'x');
    run = Convert(text, Huge, 8);
    Require(run.status == 22 && run.result == Failed && run.out[0] == 'x');
    run = Convert(text, 8, Huge);
    Require(run.status == 22 && run.result == Failed && run.source == text && run.out[0] == '\0');
    run = Convert(nullptr, 8, 8);
    Require(run.status == 22 && run.result == Failed && run.out[0] == '\0');

    char out[4] = {'x', 'x', 'x', 'x'};
    std::size_t result = 7;
    const std::uint16_t* source = text;
    std::uint64_t state[2] = {};
    Require(wcsrtombs_s_nid_postfix(nullptr, out, 4, &source, 4, state) == 22 && out[0] == '\0' && source == text);
    out[0] = 'x';
    Require(wcsrtombs_s_nid_postfix(&result, out, 4, nullptr, 4, state) == 22 && result == Failed && out[0] == '\0');
    out[0] = 'x';
    result = 7;
    Require(wcsrtombs_s_nid_postfix(&result, out, 4, &source, 4, nullptr) == 22 && result == Failed && out[0] == '\0');
    Require(source == text && std::memcmp(out, "\0xxx", 4) == 0);
}
