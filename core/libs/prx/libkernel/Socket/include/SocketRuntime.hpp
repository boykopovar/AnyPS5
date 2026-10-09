#pragma once
namespace GuestSockets {
constexpr int FirstDescriptor = 0x10000000;
int Close(int descriptor);
bool IsOpen(int descriptor);
int Family(int descriptor);
}

extern "C" bool GuestSocketIsOpen_nid_no_patch(int descriptor);
