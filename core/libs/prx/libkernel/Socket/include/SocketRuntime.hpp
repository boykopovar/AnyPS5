#pragma once
namespace GuestSockets {
constexpr int FirstDescriptor = 0x10000000;
int Duplicate(int descriptor);
int DuplicateTo(int descriptor, int target);
int Close(int descriptor);
bool IsOpen(int descriptor);
}
