// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>
#include <cstring>

void Check(bool condition) { if (!condition) std::abort(); }
int main() {
    std::int32_t status = -1;
    const char keywords[] = "calendar\0collation";
    auto* enumeration = uloc_openKeywordList_nid_postfix(keywords, sizeof(keywords), &status);
    Check(enumeration && status == -1 && enumeration->count(enumeration, &status) == 2);
    std::int32_t length = -1;
    Check(std::strcmp(enumeration->next(enumeration, &length, &status), "calendar") == 0 && length == 8);
    Check(std::strcmp(enumeration->next(enumeration, &length, &status), "collation") == 0 && length == 9);
    Check(!enumeration->next(enumeration, &length, &status) && length == 0);
    enumeration->reset(enumeration, &status);
    Check(std::strcmp(enumeration->next(enumeration, nullptr, &status), "calendar") == 0);
    enumeration->close(enumeration);
}
