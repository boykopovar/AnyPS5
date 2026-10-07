#pragma once
#include <cstdint>
namespace GuestSockets {
constexpr int FirstDescriptor = 0x10000000;
int Close(int descriptor);
bool IsOpen(int descriptor);
bool Ready(int descriptor, bool write, std::int64_t* data, bool* eof);
}
