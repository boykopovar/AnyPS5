#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace KernelSocketPoll {
constexpr short Readable = 0x1;
constexpr short Urgent = 0x2;
constexpr short Writable = 0x4;
constexpr short Error = 0x8;
constexpr short HangUp = 0x10;
constexpr short Unknown = 0x20;

struct Entry {
    int descriptor;
    short events;
    short revents;
};

using Poller = int (*)(Entry* entries, int count, int timeoutMilliseconds);
using Closer = bool (*)(int descriptor, int* error);
using OptionSetter = bool (*)(int descriptor, int level, int option, const void* value, std::uint32_t length, int* error);
using StatusControl = bool (*)(int descriptor, int command, int flags, int* result, int* error);
struct NativeOwner {
    int (*transfer)(NativeOwner* owner, bool send, void* buffer, int length) = nullptr;
};

using NativeLookup = bool (*)(int descriptor, std::shared_ptr<void>* owner, std::uintptr_t* native);

struct Description {
    std::shared_ptr<void> owner;
    std::uintptr_t native;
    int family;
    int type;
    bool nonblocking;
};
}

extern "C" void KernelSetSocketPoller_nid_no_patch(KernelSocketPoll::Poller poller);
extern "C" void KernelSetSocketCloser_nid_no_patch(KernelSocketPoll::Closer closer);
extern "C" bool KernelCloseSocket_nid_no_patch(int descriptor, int* error);
extern "C" void KernelSetSocketOptionSetter_nid_no_patch(KernelSocketPoll::OptionSetter setter);
extern "C" bool KernelSetSocketOption_nid_no_patch(int descriptor, int level, int option, const void* value, std::uint32_t length, int* error);
extern "C" void KernelSetSocketStatusControl_nid_no_patch(KernelSocketPoll::StatusControl control);
extern "C" bool KernelControlSocketStatus_nid_no_patch(int descriptor, int command, int flags, int* result, int* error);
extern "C" void KernelSetSocketNativeLookup_nid_no_patch(KernelSocketPoll::NativeLookup lookup);
extern "C" bool KernelFindSocketNative_nid_no_patch(int descriptor, std::shared_ptr<void>* owner, std::uintptr_t* native);
extern "C" bool KernelLookupSocketNative_nid_no_patch(int descriptor, std::shared_ptr<void>* owner, std::uintptr_t* native);
extern "C" int KernelReserveSocketNumber_nid_no_patch(std::uintptr_t native);
extern "C" void KernelReleaseSocketNumber_nid_no_patch(int number);
extern "C" bool KernelDescribeSocket_nid_no_patch(int descriptor, KernelSocketPoll::Description* description);
extern "C" void KernelListSockets_nid_no_patch(std::vector<int>* descriptors);
