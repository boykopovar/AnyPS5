#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" std::size_t APS5_VABI wcsrtombs_nid_postfix(char*, const std::uint16_t**, std::size_t, void*);
static void Require(bool value) { if (!value) std::abort(); }

constexpr auto Failed = static_cast<std::size_t>(-1);
static const std::uint16_t text[] = {'a', 0xe9, 'c', 0};
static const std::uint16_t wide[] = {'a', 0x100, 'b', 0};
static const std::uint16_t empty[] = {0};

struct Run {
    std::size_t result;
    const std::uint16_t* source;
    int error;
    char out[8];
};

static Run Convert(const std::uint16_t* input, std::size_t count, bool toBuffer = true) {
    Run run{};
    std::memset(run.out, 'x', sizeof(run.out));
    run.source = input;
    std::uint64_t state[2] = {};
    errno = 0;
    run.result = wcsrtombs_nid_postfix(toBuffer ? run.out : nullptr, &run.source, count, state);
    run.error = errno;
    return run;
}

int main() {
    auto run = Convert(text, 0, false);
    Require(run.result == 3 && run.source == text && run.out[0] == 'x');
    run = Convert(empty, 0, false);
    Require(run.result == 0 && run.source == empty);
    run = Convert(wide, 0, false);
    Require(run.result == Failed && run.error == 86 && run.source == wide);

    run = Convert(text, 8);
    Require(run.result == 3 && run.source == nullptr && std::memcmp(run.out, "a\xe9" "c\0x", 5) == 0);
    run = Convert(text, 4);
    Require(run.result == 3 && run.source == nullptr && std::memcmp(run.out, "a\xe9" "c\0x", 5) == 0);
    run = Convert(text, 3);
    Require(run.result == 3 && run.source == text + 3 && std::memcmp(run.out, "a\xe9" "cx", 4) == 0);
    run = Convert(text, 2);
    Require(run.result == 2 && run.source == text + 2 && std::memcmp(run.out, "a\xe9x", 3) == 0);
    run = Convert(text, 0);
    Require(run.result == 0 && run.source == text && run.out[0] == 'x');
    run = Convert(empty, 1);
    Require(run.result == 0 && run.source == nullptr && std::memcmp(run.out, "\0x", 2) == 0);

    run = Convert(wide, 8);
    Require(run.result == Failed && run.error == 86 && run.source == wide + 1 && std::memcmp(run.out, "axx", 3) == 0);
    run = Convert(wide, 1);
    Require(run.result == 1 && run.source == wide + 1 && std::memcmp(run.out, "ax", 2) == 0);

    std::uint64_t state[2] = {};
    char out[4];
    const std::uint16_t* source = text;
    Require(wcsrtombs_nid_postfix(out, &source, 2, state) == 2 && source == text + 2);
    Require(wcsrtombs_nid_postfix(out + 2, &source, 2, state) == 1 && source == nullptr);
    Require(std::memcmp(out, "a\xe9" "c\0", 4) == 0);
}
