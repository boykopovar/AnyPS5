#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "prx/libkernel/Socket/include/SocketPoll.hpp"

struct FileStat;
namespace GuestSockets {
struct Iovec {
    void* base;
    std::size_t length;
};
int Duplicate(int descriptor);
int DuplicateTo(int descriptor, int target);
int Close(int descriptor);
void Forget(int descriptor);
bool IsOpen(int descriptor);
int Family(int descriptor);
int Type(int descriptor);
bool Listening(int descriptor);
std::intptr_t NativeDescriptor(int descriptor);
std::int64_t Read(int descriptor, void* buffer, std::size_t length);
std::int64_t Write(int descriptor, const void* buffer, std::size_t length);
int Stat(int descriptor, FileStat* status);
bool Ready(int descriptor, bool write, std::int64_t* data, bool* eof);
int Poll(KernelSocketPoll::Entry* entries, int count, int timeoutMilliseconds);
#ifdef _WIN32
bool PipeReady(int descriptor, short events, short& revents);
#endif
struct Interest {
    int descriptor;
    bool write;
};
std::uintptr_t CurrentWaker();
void Wake(std::uintptr_t waker);
bool WaitAny(const std::vector<Interest>& interests, std::uint64_t deadlineNanos);
bool PipeReadState(int descriptor, std::int64_t* data, bool* eof);
bool PipeWriteState(int descriptor, std::int64_t* data, bool* eof, bool* writable);
std::int64_t Readv(int descriptor, const Iovec* iov, int iovcnt);
std::int64_t Writev(int descriptor, const Iovec* iov, int iovcnt);
bool ReadGuest(void* destination, const void* source, std::size_t bytes);
}

extern "C" bool GuestSocketIsOpen_nid_no_patch(int descriptor);
