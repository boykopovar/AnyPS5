#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU canonical time zone check failed at line %u\n", location.line());
    std::abort();
}

void NullStatus() {
    const char16_t id[] = u"UTC";
    char16_t result[16] = {u'x', u'y', u'z', 0};
    std::int8_t isSystemID = 42;
    Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 3, result, 16, &isSystemID, nullptr) == 0);
    Require(isSystemID == 42);
    Require(result[0] == u'x' && result[1] == u'y' && result[2] == u'z' && result[3] == 0);

    Require(ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, 0, nullptr, 0, &isSystemID, nullptr) == 0);
    Require(isSystemID == 42);

    Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 3, result, 16, nullptr, nullptr) == 0);
    Require(result[0] == u'x');
}

void PreexistingFailure() {
    const char16_t id[] = u"America/New_York";
    char16_t result[32] = {u'k', u'e', u'e', u'p', 0};
    for (std::int32_t incoming : {1, 7, 10, 15, 16, 27, 0x7fffffff}) {
        std::int32_t status = incoming;
        std::int8_t isSystemID = 99;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 16, result, 32, &isSystemID, &status) == 0);
        Require(status == incoming);
        Require(isSystemID == 99);
        Require(result[0] == u'k' && result[1] == u'e');

        isSystemID = 99;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, 0, nullptr, 0, &isSystemID, &status) == 0);
        Require(status == incoming);
        Require(isSystemID == 99);
    }
}

void PreexistingWarning() {
    constexpr std::int32_t IllegalArgument = 1;
    char16_t result[32] = {u'a', u'b', 0};
    for (std::int32_t warning : {-124, -5, -1}) {
        std::int32_t status = warning;
        std::int8_t isSystemID = 77;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, 13, result, 32, &isSystemID, &status) == 0);
        Require(status == IllegalArgument);
        Require(isSystemID == 0);
        Require(result[0] == u'a');
    }
}

void ArgumentValidation() {
    constexpr std::int32_t IllegalArgument = 1;
    const char16_t id[] = u"Asia/Tokyo";
    char16_t result[32] = {u'i', u'n', u'i', u't', 0};

    {
        std::int32_t status = 0;
        std::int8_t isSystemID = 1;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, 10, result, 32, &isSystemID, &status) == 0);
        Require(status == IllegalArgument);
        Require(isSystemID == 0);
        Require(result[0] == u'i');
    }

    {
        std::int32_t status = 0;
        std::int8_t isSystemID = 1;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 0, result, 32, &isSystemID, &status) == 0);
        Require(status == IllegalArgument);
        Require(isSystemID == 0);
        Require(result[0] == u'i');
    }

    {
        std::int32_t status = 0;
        std::int8_t isSystemID = 1;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 10, nullptr, 32, &isSystemID, &status) == 0);
        Require(status == IllegalArgument);
        Require(isSystemID == 0);
    }

    for (std::int32_t capacity : {0, -1, -100}) {
        std::int32_t status = 0;
        std::int8_t isSystemID = 1;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 10, result, capacity, &isSystemID, &status) == 0);
        Require(status == IllegalArgument);
        Require(isSystemID == 0);
        Require(result[0] == u'i');
    }

    {
        std::int32_t status = 0;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, 0, nullptr, 0, nullptr, &status) == 0);
        Require(status == IllegalArgument);
    }
}

}

int main() {
    NullStatus();
    PreexistingFailure();
    PreexistingWarning();
    ArgumentValidation();
    std::puts("ICU canonical time zone tests passed");
}
