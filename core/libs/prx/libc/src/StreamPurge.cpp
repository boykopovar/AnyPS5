#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdio>
#if defined(__linux__)
#include <stdio_ext.h>
#endif

extern "C" {
void APS5_VABI flockfile_nid_postfix(FileStream*);
void APS5_VABI funlockfile_nid_postfix(FileStream*);
}

namespace {

class LockedStream {
    FileStream* stream;

public:
    explicit LockedStream(FileStream* value) : stream(value) { flockfile_nid_postfix(stream); }
    ~LockedStream() { funlockfile_nid_postfix(stream); }
};

}

extern "C" int APS5_VABI fpurge_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetStreamHandle(stream);
    const LockedStream lock(stream);
#ifdef _WIN32
    auto* state = reinterpret_cast<UcrtStreamState*>(native);
    state->pointer = state->base;
    state->count = 0;
#elif defined(__linux__)
    ::__fpurge(native);
#else
    if (::fpurge(native) != 0) return -1;
#endif
    stream->GuestState().position = stream->GuestState().buffer;
    stream->SyncUnlockedStatus();
    return 0;
}
