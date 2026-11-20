#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu locale display: " + message);
}

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t UnknownOrientation = 4;
constexpr std::int32_t CapacityLimit = 8192;

using Display = std::int32_t (APS5_VABI*)(const char*, const char*, char16_t*, std::int32_t, std::int32_t*);

struct Entry {
    const char* name;
    Display call;
};

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());

    const Entry entries[] = {
        {"uloc_getDisplayName", uloc_getDisplayName_nid_postfix},
        {"uloc_getDisplayLanguage", uloc_getDisplayLanguage_nid_postfix},
        {"uloc_getDisplayScript", uloc_getDisplayScript_nid_postfix},
        {"uloc_getDisplayCountry", uloc_getDisplayCountry_nid_postfix},
        {"uloc_getDisplayVariant", uloc_getDisplayVariant_nid_postfix},
        {"uloc_getDisplayKeyword", uloc_getDisplayKeyword_nid_postfix},
    };

    char16_t buffer[64] = {};
    for (const Entry& entry : entries) {
        const std::string name = entry.name;
        std::int32_t status = ZeroError;
        Require(entry.call("en_US", "en_US", nullptr, 16, &status) == 0, name + " accepted a null output");
        Require(status == IllegalArgument, name + " did not report a null output");

        status = ZeroError;
        Require(entry.call("en_US", "en_US", buffer, -1, &status) == 0, name + " accepted a negative capacity");
        Require(status == IllegalArgument, name + " did not report a negative capacity");

        status = ZeroError;
        Require(entry.call("en_US", "en_US", buffer, CapacityLimit + 1, &status) == 0,
            name + " accepted a capacity above the limit");
        Require(status == IllegalArgument, name + " did not report a capacity above the limit");

        Require(entry.call("en_US", "en_US", buffer, 16, nullptr) == 0, name + " accepted a null status");
        entry.call("en_US", "en_US", buffer, 16, &status);
    }

    {
        std::int32_t status = ZeroError;
        Require(uloc_getDisplayKeywordValue_nid_postfix("en_US", "collation", "en_US", nullptr, 16, &status) == 0,
            "uloc_getDisplayKeywordValue accepted a null output");
        Require(status == IllegalArgument, "uloc_getDisplayKeywordValue did not report a null output");

        status = ZeroError;
        Require(uloc_getDisplayKeywordValue_nid_postfix("en_US", "collation", "en_US", buffer, -1, &status) == 0,
            "uloc_getDisplayKeywordValue accepted a negative capacity");
        Require(status == IllegalArgument, "uloc_getDisplayKeywordValue did not report a negative capacity");

        status = ZeroError;
        Require(uloc_getDisplayKeywordValue_nid_postfix("en_US", "collation", "en_US", buffer, CapacityLimit + 1,
            &status) == 0, "uloc_getDisplayKeywordValue accepted a capacity above the limit");
        Require(status == IllegalArgument, "uloc_getDisplayKeywordValue did not report a capacity above the limit");

        Require(uloc_getDisplayKeywordValue_nid_postfix("en_US", "collation", "en_US", buffer, 16, nullptr) == 0,
            "uloc_getDisplayKeywordValue accepted a null status");
        uloc_getDisplayKeywordValue_nid_postfix("en_US", "collation", "en_US", buffer, 16, &status);
    }

    {
        Require(uloc_getLineOrientation_nid_postfix("en_US", nullptr) == UnknownOrientation,
            "uloc_getLineOrientation dereferenced a null status");
        std::int32_t status = ZeroError;
        uloc_getLineOrientation_nid_postfix("en_US", &status);
        uloc_getLineOrientation_nid_postfix(nullptr, &status);
    }

    return 0;
}
