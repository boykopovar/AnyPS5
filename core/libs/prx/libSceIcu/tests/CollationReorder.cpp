#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU collation reorder check failed at line %u\n", location.line());
    std::abort();
}

void TestFailureStatus() {
    for (std::int32_t failureCode : {1, 2, 7, 10, 15, 16, 27}) {
        std::int32_t status = failureCode;
        Require(ucol_getReorderCodes_nid_postfix(nullptr, nullptr, 0, &status) == 0);
        Require(status == failureCode);

        std::array<std::int32_t, 4> buffer = {0x1234, 0x5678, 0x9abc, 0xdef0};
        status = failureCode;
        Require(ucol_getReorderCodes_nid_postfix(nullptr, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status) == 0);
        Require(status == failureCode);
        Require(buffer[0] == 0x1234);
        Require(buffer[1] == 0x5678);
        Require(buffer[2] == 0x9abc);
        Require(buffer[3] == 0xdef0);

        status = failureCode;
        Require(ucol_getReorderCodes_nid_postfix(nullptr, buffer.data(), -1, &status) == 0);
        Require(status == failureCode);
        Require(buffer[0] == 0x1234);

        const auto dummy = reinterpret_cast<const UCollator*>(&buffer);
        status = failureCode;
        Require(ucol_getReorderCodes_nid_postfix(dummy, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status) == 0);
        Require(status == failureCode);
        Require(buffer[0] == 0x1234);
    }
}

}

int main() {
    TestFailureStatus();
    return 0;
}
