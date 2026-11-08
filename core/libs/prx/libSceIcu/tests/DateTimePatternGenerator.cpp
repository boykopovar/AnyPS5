#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU pattern generator check failed at line %u\n", location.line());
    std::abort();
}

void PreExistingFailure() {
    constexpr std::int32_t IllegalArgument = 1;
    constexpr std::int32_t BufferOverflow = 15;
    char16_t buffer[8] = {u'x', u'y', u'z', u'w', 0, 0, 0, 0};

    std::int32_t status = IllegalArgument;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 5, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z' && buffer[3] == u'w');

    status = BufferOverflow;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, -1, buffer, 8, &status) == 0);
    Require(status == BufferOverflow);
    Require(buffer[0] == u'x' && buffer[1] == u'y');

    status = 100;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 0, buffer, 8, &status) == 0);
    Require(status == 100);
    Require(buffer[0] == u'x');
}

void InvalidArguments() {
    constexpr std::int32_t IllegalArgument = 1;
    constexpr std::int32_t NotTerminated = -124;
    char16_t buffer[8] = {u'k', u'e', u'e', u'p', 0, 0, 0, 0};

    std::int32_t status = 0;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 4, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k' && buffer[1] == u'e' && buffer[2] == u'e' && buffer[3] == u'p');

    status = 0;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 1, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k');

    status = 0;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, -1, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k');

    status = 0;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, -10, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k');

    status = NotTerminated;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 2, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k');

    status = -1;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, -1, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k');
}

}

int main() {
    PreExistingFailure();
    InvalidArguments();
    std::puts("ICU date time pattern generator tests passed");
}
