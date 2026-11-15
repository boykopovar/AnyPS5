#pragma once

#include <cstdint>

struct UText;

namespace Icu {

UText* NativeUTextSetup(UText* ut, std::int32_t extraSpace, std::int32_t& status);
UText* NativeUTextOpenUChars(UText* ut, const char16_t* text, std::int64_t length, std::int32_t& status);
UText* NativeUTextClose(UText* ut);

}
