#pragma once
#include <cstdint>

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
}

extern "C" void KernelSetSocketPoller_nid_no_patch(KernelSocketPoll::Poller poller);
