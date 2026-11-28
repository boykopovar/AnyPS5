#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdio>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#if defined(__linux__)
#include <stdio_ext.h>
#endif

namespace {

class LockedStream {
    FileStream* stream;

public:
    explicit LockedStream(FileStream* value) : stream(value) { flockfile_nid_postfix(stream); }
    ~LockedStream() { funlockfile_nid_postfix(stream); }
};

std::int64_t DescriptorPosition(std::FILE* native) {
#ifdef _WIN32
    return ::_telli64(::_fileno(native));
#else
    return ::lseek(::fileno(native), 0, SEEK_CUR);
#endif
}

void SeekDescriptor(std::FILE* native, std::int64_t position) {
#ifdef _WIN32
    ::_lseeki64(::_fileno(native), position, SEEK_SET);
#else
    ::fseeko(native, static_cast<off_t>(position), SEEK_SET);
#endif
}

}

extern "C" int APS5_VABI fpurge_nid_postfix(FileStream* stream) {
    const FileStreamOperation_nid_no_patch operation(stream);
    if (stream && !operation.Open()) { errno = 9; return -1; }
    auto* native = GetStreamHandle(stream);
    const LockedStream lock(stream);
    std::string kept;
    std::int64_t resume = -1;
    const std::int64_t logical = stream->TellSeekable();
    if (logical >= 0) {
        if (stream->PushbackLive() && !stream->PushedBackBytes().empty()) {
            kept = stream->PushedBackBytes();
            resume = logical + static_cast<std::int64_t>(kept.size());
        } else {
            const std::int64_t descriptor = DescriptorPosition(native);
            if (descriptor >= 0 && logical < descriptor) resume = logical;
        }
    }
    stream->ClearPushback();
#ifdef _WIN32
    auto* state = reinterpret_cast<UcrtStreamState*>(native);
    state->pointer = state->base;
    state->count = 0;
#elif defined(__linux__)
    ::__fpurge(native);
#else
    if (::fpurge(native) != 0) return -1;
#endif
    if (resume >= 0) {
        SeekDescriptor(native, resume);
        for (const char byte : kept) stream->PushBack(static_cast<unsigned char>(byte));
    }
    stream->GuestState().position = stream->GuestState().buffer;
    stream->SyncUnlockedStatus();
    return 0;
}
