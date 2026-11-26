#pragma once
#include <cstdint>

namespace Icu::FormattedValueBackend {

void Register(const void* value);
bool IsRegistered(const void* value);
const char16_t* GetString(const void* value, std::int32_t* length, std::int32_t* status);

}
