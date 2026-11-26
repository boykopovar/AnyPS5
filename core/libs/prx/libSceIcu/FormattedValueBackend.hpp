#pragma once
#include <cstdint>

namespace Icu::FormattedValueBackend {

void Register(const void* value, const void* owner);
void UnregisterOwner(const void* owner);
bool IsRegistered(const void* value);
const char16_t* GetString(const void* value, std::int32_t* length, std::int32_t* status);

}
