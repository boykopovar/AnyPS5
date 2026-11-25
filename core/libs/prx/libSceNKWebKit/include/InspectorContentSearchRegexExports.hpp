#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/YarrRegularExpressionExports.hpp"

namespace NKWebKit::InspectorAbi {

inline constexpr int SearchStringTypeRegex = 0;
inline constexpr int SearchStringTypeExactString = 1;
inline constexpr int SearchStringTypeContainsString = 2;

}

extern "C" {

Jsc::YarrAbi::RegularExpression* APS5_VABI _ZN9Inspector22ContentSearchUtilities38createRegularExpressionForSearchStringERKN3WTF6StringEbNS0_16SearchStringTypeE(Jsc::YarrAbi::RegularExpression* result, const Jsc::YarrAbi::StringReference* searchString, bool caseSensitive, int type);
int APS5_VABI _ZN9Inspector22ContentSearchUtilities29countRegularExpressionMatchesERKN3JSC4Yarr17RegularExpressionERKN3WTF6StringE(const Jsc::YarrAbi::RegularExpression* regex, const Jsc::YarrAbi::StringReference* content);

}
