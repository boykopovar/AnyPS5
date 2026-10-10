#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "File position check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    FileStream file(std::tmpfile());
    Require(std::fputs("position", file.GetHandle()) >= 0);
    std::int64_t position = -1;
    Require(fgetpos_nid_postfix(&file, &position) == 0);
    Require(position == 8);
    const std::int64_t beginning = 0;
    Require(fsetpos_nid_postfix(&file, &beginning) == 0);
    Require(std::fgetc(file.GetHandle()) == 'p');
    Require(fgetpos_nid_postfix(&file, &position) == 0 && position == 1);
    file.Close();

    int descriptors[2];
#ifdef _WIN32
    Require(_pipe(descriptors, 4096, _O_BINARY) == 0);
    FileStream pipe(_fdopen(descriptors[0], "rb"));
    Require(_close(descriptors[1]) == 0);
    Require(CloseHandle(reinterpret_cast<HANDLE>(_get_osfhandle(descriptors[0]))) != 0);
#else
    Require(::pipe(descriptors) == 0);
    FileStream pipe(::fdopen(descriptors[0], "rb"));
    Require(::close(descriptors[1]) == 0);
#endif
    Require(ftello_nid_postfix(&pipe) == -1);
    const int expectedError = errno;
    Require(expectedError != 0);
    position = 123;
    errno = 0;
    Require(fgetpos_nid_postfix(&pipe, &position) != 0);
    Require(errno == expectedError);
    Require(position == 123);
#ifdef _WIN32
    Require(std::fclose(pipe.GetHandle()) == EOF);
#else
    pipe.Close();
#endif
}
