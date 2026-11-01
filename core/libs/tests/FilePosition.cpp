#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"
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

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "File position check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void CheckFseekBounds() {
    FileStream file(TemporaryFile::Open());
    Require(fwrite_nid_postfix("0123456789", 1, 10, &file) == 10);
    rewind_nid_postfix(&file);
    errno = 0;
    Require(fseek_nid_postfix(&file, -1, SEEK_SET) == -1 && errno == EINVAL);
    Require(ftello_nid_postfix(&file) == 0);
    Require(fseek_nid_postfix(&file, -3, SEEK_END) == 0 && ftello_nid_postfix(&file) == 7);
    Require(fgetc_nid_postfix(&file) == '7');
    Require(fseek_nid_postfix(&file, 100, SEEK_SET) == 0 && ftello_nid_postfix(&file) == 100);
    Require(fgetc_nid_postfix(&file) == EOF && feof_nid_postfix(&file) != 0);
    file.Close();
}

int main() {
    CheckFseekBounds();
    FileStream file(TemporaryFile::Open());
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
