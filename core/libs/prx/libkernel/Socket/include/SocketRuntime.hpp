#pragma once
namespace GuestSockets {
constexpr int FirstDescriptor = 0x10000000;
int Close(int descriptor);
int Duplicate(int descriptor);
bool IsOpen(int descriptor);
}
