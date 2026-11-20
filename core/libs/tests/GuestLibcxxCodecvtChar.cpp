#include "prx/libc/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    std::array<char, 8> input{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    std::array<char, 8> output{};
    output.fill('!');
    char facet = 0;
    char state = 0;
    const char* fromNext = nullptr;
    char* toNext = nullptr;
    errno = ESRCH;

    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE11do_encodingEv_nid_postfix(&facet) == 1);
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE13do_max_lengthEv_nid_postfix(&facet) == 1);
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE5do_inERS1_PKcS5_RS5_PcS7_RS7__nid_postfix(
        &facet, &state, input.data(), input.data() + input.size(), &fromNext,
        output.data(), output.data() + output.size(), &toNext) == 3);
    Require(fromNext == input.data() && toNext == output.data());
    Require(output[0] == '!' && output[7] == '!' && errno == ESRCH);

    fromNext = nullptr;
    toNext = nullptr;
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE6do_outERS1_PKcS5_RS5_PcS7_RS7__nid_postfix(
        &facet, &state, input.data(), input.data() + input.size(), &fromNext,
        output.data(), output.data() + output.size(), &toNext) == 3);
    Require(fromNext == input.data() && toNext == output.data());
    Require(output[0] == '!' && output[7] == '!' && errno == ESRCH);

    toNext = nullptr;
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE10do_unshiftERS1_PcS4_RS4__nid_postfix(
        &facet, &state, output.data(), output.data() + output.size(), &toNext) == 3);
    Require(toNext == output.data() && output[0] == '!' && errno == ESRCH);

    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE9do_lengthERS1_PKcS5_m_nid_postfix(
        &facet, &state, input.data(), input.data() + input.size(), 3) == 3);
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE9do_lengthERS1_PKcS5_m_nid_postfix(
        &facet, &state, input.data(), input.data() + input.size(), 99) == 8);
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE9do_lengthERS1_PKcS5_m_nid_postfix(
        &facet, &state, input.data(), input.data(), 99) == 0);

    try {
        _ZNKSt3__17codecvtIcc11__mbstate_tE10do_unshiftERS1_PcS4_RS4__nid_postfix(
            &facet, &state, output.data(), output.data() + output.size(), nullptr);
        std::abort();
    } catch (const std::invalid_argument&) { Require(errno == ESRCH); }
}
