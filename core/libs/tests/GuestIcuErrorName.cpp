#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <string>

namespace {

constexpr const char* Bogus = "[BOGUS UErrorCode]";

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU error name: %s\n", message);
    std::abort();
}

bool Named(std::int32_t code, const char* expected) {
    const char* name = u_errorName_nid_postfix(code);
    return name && std::strcmp(name, expected) == 0;
}

void Boundaries() {
    Require(Named(-129, Bogus) && Named(-128, "U_USING_FALLBACK_WARNING") && Named(-124, "U_STRING_NOT_TERMINATED_WARNING"), "warning start");
    Require(Named(-120, "U_PLUGIN_CHANGED_LEVEL_WARNING") && Named(-119, Bogus) && Named(-1, Bogus), "warning limit");
    Require(Named(0, "U_ZERO_ERROR") && Named(1, "U_ILLEGAL_ARGUMENT_ERROR") && Named(15, "U_BUFFER_OVERFLOW_ERROR"), "standard errors");
    Require(Named(31, "U_INPUT_TOO_LONG_ERROR") && Named(32, Bogus) && Named(0xffff, Bogus), "standard limit");
    Require(Named(0x10000, "U_BAD_VARIABLE_DEFINITION") && Named(0x10022, "U_INVALID_FUNCTION") && Named(0x10023, Bogus), "transliterator errors");
    Require(Named(0x10100, "U_UNEXPECTED_TOKEN") && Named(0x1010d, "U_DUPLICATE_KEYWORD") && Named(0x10120, "U_MF_BAD_OPTION") && Named(0x10121, Bogus), "format errors");
    Require(Named(0x10200, "U_BRK_INTERNAL_ERROR") && Named(0x10206, "U_BRK_VARIABLE_REDFINITION") && Named(0x1020d, "U_BRK_MALFORMED_RULE_TAG") && Named(0x1020e, Bogus), "break iterator errors");
    Require(Named(0x10300, "U_REGEX_INTERNAL_ERROR") && Named(0x10315, "U_REGEX_INVALID_CAPTURE_GROUP_NAME") && Named(0x10316, Bogus), "regular expression errors");
    Require(Named(0x10400, "U_STRINGPREP_PROHIBITED_ERROR") && Named(0x10408, "U_IDNA_DOMAIN_NAME_TOO_LONG_ERROR") && Named(0x10409, Bogus), "IDNA errors");
    Require(Named(0x10500, "U_PLUGIN_TOO_HIGH") && Named(0x10501, "U_PLUGIN_DIDNT_SET_LEVEL") && Named(0x10502, Bogus), "plugin errors");
    Require(Named(std::numeric_limits<std::int32_t>::min(), Bogus) && Named(std::numeric_limits<std::int32_t>::max(), Bogus), "extreme codes");
}

void Coverage() {
    std::set<std::string> names;
    std::int32_t named = 0;
    for (std::int32_t code = -0x200; code < 0x10600; ++code) {
        const char* name = u_errorName_nid_postfix(code);
        Require(name != nullptr, "every code has a name");
        Require(name == u_errorName_nid_postfix(code), "names are static strings");
        if (std::strcmp(name, Bogus) == 0) continue;
        ++named;
        Require(std::strncmp(name, "U_", 2) == 0, "names start with U_");
        names.insert(name);
    }
    Require(named == 156 && names.size() == 156, "156 distinct named codes");
}

}

int main() {
    Boundaries();
    Coverage();
    return 0;
}
