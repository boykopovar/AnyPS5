#pragma once
#include <cstddef>
#include <cstdint>
namespace GuestSockets {
constexpr int FirstDescriptor = 0x10000000;
int Duplicate(int descriptor);
int DuplicateTo(int descriptor, int target);
int Close(int descriptor);
bool IsOpen(int descriptor);
int Family(int descriptor);
std::int64_t Read(int descriptor, void* buffer, std::size_t length);
std::int64_t Write(int descriptor, const void* buffer, std::size_t length);
}

extern "C" bool GuestSocketIsOpen_nid_no_patch(int descriptor);
