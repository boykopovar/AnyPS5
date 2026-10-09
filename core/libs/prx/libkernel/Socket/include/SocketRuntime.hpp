#pragma once
#include <cstdint>
#include <vector>
namespace GuestSockets {
constexpr int FirstDescriptor = 0x10000000;
int Close(int descriptor);
bool IsOpen(int descriptor);
bool Ready(int descriptor, bool write, std::int64_t* data, bool* eof);
struct Interest {
    int descriptor;
    bool write;
};
std::uintptr_t CurrentWaker();
void Wake(std::uintptr_t waker);
bool WaitAny(const std::vector<Interest>& interests, std::uint64_t deadlineNanos);
}
