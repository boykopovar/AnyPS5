#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace GuestSockets {
constexpr int FirstDescriptor = 0x10000000;
int Duplicate(int descriptor);
int DuplicateTo(int descriptor, int target);
int Close(int descriptor);
bool IsOpen(int descriptor);
int Family(int descriptor);
std::int64_t Read(int descriptor, void* buffer, std::size_t length);
std::int64_t Write(int descriptor, const void* buffer, std::size_t length);
bool Ready(int descriptor, bool write, std::int64_t* data, bool* eof);
struct Interest {
    int descriptor;
    bool write;
};
std::uintptr_t CurrentWaker();
void Wake(std::uintptr_t waker);
bool WaitAny(const std::vector<Interest>& interests, std::uint64_t deadlineNanos);
}

extern "C" bool GuestSocketIsOpen_nid_no_patch(int descriptor);
