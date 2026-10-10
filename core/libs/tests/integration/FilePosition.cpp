#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" {
int APS5_VABI fgetpos_nid_postfix(FileStream*, std::int64_t*);
int APS5_VABI fsetpos_nid_postfix(FileStream*, const std::int64_t*);
std::int64_t APS5_VABI ftello_nid_postfix(FileStream*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

class StreamFixture {
public:
    explicit StreamFixture(std::FILE* handle) : stream(handle) {}

    ~StreamFixture() {
        if (!closed) std::fclose(stream.GetHandle());
    }

    StreamFixture(const StreamFixture&) = delete;
    StreamFixture& operator=(const StreamFixture&) = delete;

    FileStream& Stream() {
        return stream;
    }

    int Close() {
        closed = true;
        return std::fclose(stream.GetHandle());
    }

private:
    FileStream stream;
    bool closed = false;
};

std::FILE* OpenTemporaryFile() {
    std::FILE* handle = std::tmpfile();
    if (handle == nullptr) throw std::runtime_error("cannot create a temporary file");
    return handle;
}

std::FILE* OpenUnseekableStream() {
    int descriptors[2];
#ifdef _WIN32
    Require(_pipe(descriptors, 4096, _O_BINARY) == 0, "create a pipe");
    std::FILE* handle = _fdopen(descriptors[0], "rb");
    Require(handle != nullptr, "open the pipe read end");
    Require(_close(descriptors[1]) == 0, "close the pipe write end");
    Require(CloseHandle(reinterpret_cast<HANDLE>(_get_osfhandle(descriptors[0]))) != 0, "close the read end OS handle");
#else
    Require(::pipe(descriptors) == 0, "create a pipe");
    std::FILE* handle = ::fdopen(descriptors[0], "rb");
    Require(handle != nullptr, "open the pipe read end");
    Require(::close(descriptors[1]) == 0, "close the pipe write end");
#endif
    return handle;
}

const Case afterWrite{"Fgetpos_AfterWrite_ReportsBytesWritten", [] {
    StreamFixture file(OpenTemporaryFile());
    Require(std::fputs("position", file.Stream().GetHandle()) >= 0, "write the sample");
    std::int64_t position = -1;
    RequireEqual(fgetpos_nid_postfix(&file.Stream(), &position), 0, "fgetpos result");
    RequireEqual(position, std::int64_t{8}, "position");
}};

const Case rewindToBeginning{"Fsetpos_Beginning_RewindsAndAdvancesOnRead", [] {
    StreamFixture file(OpenTemporaryFile());
    Require(std::fputs("position", file.Stream().GetHandle()) >= 0, "write the sample");
    const std::int64_t beginning = 0;
    RequireEqual(fsetpos_nid_postfix(&file.Stream(), &beginning), 0, "fsetpos result");
    RequireEqual(std::fgetc(file.Stream().GetHandle()), static_cast<int>('p'), "first byte");
    std::int64_t position = -1;
    RequireEqual(fgetpos_nid_postfix(&file.Stream(), &position), 0, "fgetpos result");
    RequireEqual(position, std::int64_t{1}, "position after one byte");
}};

const Case ftelloUnseekable{"Ftello_UnseekableStream_ReturnsMinusOneAndSetsErrno", [] {
    StreamFixture pipe(OpenUnseekableStream());
    errno = 0;
    RequireEqual(ftello_nid_postfix(&pipe.Stream()), std::int64_t{-1}, "ftello result");
    Require(errno != 0, "errno is set");
}};

const Case fgetposUnseekable{"Fgetpos_UnseekableStream_FailsWithFtelloErrnoAndKeepsPosition", [] {
    StreamFixture pipe(OpenUnseekableStream());
    RequireEqual(ftello_nid_postfix(&pipe.Stream()), std::int64_t{-1}, "ftello result");
    const int expectedError = errno;
    Require(expectedError != 0, "ftello sets errno");
    std::int64_t position = 123;
    errno = 0;
    Require(fgetpos_nid_postfix(&pipe.Stream(), &position) != 0, "fgetpos fails");
    RequireEqual(errno, expectedError, "errno");
    RequireEqual(position, std::int64_t{123}, "position untouched");
}};

#ifdef _WIN32
const Case closedHandle{"Fclose_PipeWithClosedOsHandle_ReturnsEof", [] {
    StreamFixture pipe(OpenUnseekableStream());
    RequireEqual(pipe.Close(), EOF, "fclose result");
}};
#endif

} // namespace
