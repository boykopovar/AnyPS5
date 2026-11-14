#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
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

static void CheckRejectedBufferedSeeks() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-failed-seek-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto host = root.string();
    AddPathAlias_nid_no_patch("/failed-seek", host.c_str());
    auto* writer = fopen_nid_postfix("/failed-seek/input", "wb");
    Require(writer != nullptr);
    Require(fwrite_nid_postfix("12345678", 1, 8, writer) == 8);
    Require(fclose_nid_postfix(writer) == 0);
    auto* reader = fopen_nid_postfix("/failed-seek/input", "rb");
    Require(reader != nullptr);
    char buffer[4];
    Require(fread_nid_postfix(buffer, 1, sizeof(buffer), reader) == 4);
    Require(std::string(buffer, sizeof(buffer)) == "1234");
    const int endOfFile = feof_nid_postfix(reader);
    const int error = ferror_nid_postfix(reader);
    const std::int64_t negative = -1;
    for (int attempt = 0; attempt < 5; ++attempt) {
        errno = 0;
        const int result = attempt == 0 ? fsetpos_nid_postfix(reader, &negative) :
            attempt == 1 ? fseeko_nid_postfix(reader, -1, SEEK_SET) :
            attempt == 2 ? fseek_nid_postfix(reader, -5, SEEK_CUR) :
            attempt == 3 ? fseeko_nid_postfix(reader, -9, SEEK_END) :
            fseeko_nid_postfix(reader, std::numeric_limits<std::int64_t>::max(), SEEK_CUR);
        Require(result == -1);
        Require(errno == (attempt == 4 ? 84 : 22));
        Require(ftello_nid_postfix(reader) == 4);
        Require(feof_nid_postfix(reader) == endOfFile);
        Require(ferror_nid_postfix(reader) == error);
    }
    Require(fgetc_nid_postfix(reader) == '5');
    Require(ungetc_nid_postfix('5', reader) == '5');
    errno = 0;
    Require(fseeko_nid_postfix(reader, -5, SEEK_CUR) == -1 && errno == 22);
    Require(ftello_nid_postfix(reader) == 4);
    Require(feof_nid_postfix(reader) == endOfFile);
    Require(ferror_nid_postfix(reader) == error);
    Require(fgetc_nid_postfix(reader) == '5');
    Require(ungetc_nid_postfix('X', reader) == 'X');
    errno = 0;
    Require(fseeko_nid_postfix(reader, -1, SEEK_SET) == -1 && errno == 22);
    Require(ftello_nid_postfix(reader) == 4);
    Require(feof_nid_postfix(reader) == endOfFile);
    Require(ferror_nid_postfix(reader) == error);
    Require(fgetc_nid_postfix(reader) == 'X');
    Require(fgetc_nid_postfix(reader) == '6');
    Require(fclose_nid_postfix(reader) == 0);
    RemovePathAlias_nid_no_patch("/failed-seek");
    Require(std::filesystem::remove(root / "input"));
    Require(std::filesystem::remove(root));
}

static void CheckConcurrentRelativeSeeks() {
    FileStream file(TemporaryFile::Open());
    constexpr int Seeks = 20000;
    std::atomic<int> ready{0};
    std::atomic<bool> failed{false};
    const auto seek = [&] {
        ready.fetch_add(1);
        while (ready.load() < 2) std::this_thread::yield();
        for (int index = 0; index < Seeks; ++index) {
            if (fseeko_nid_postfix(&file, 1, SEEK_CUR) != 0) failed.store(true);
        }
    };
    std::thread first(seek);
    std::thread second(seek);
    first.join();
    second.join();
    Require(!failed.load());
    Require(ftello_nid_postfix(&file) == 2 * Seeks);
    file.Close();
}

static void CheckFailedSeekKeepsPosition() {
    FileStream file(TemporaryFile::Open());
    Require(fwrite_nid_postfix("01234567", 1, 8, &file) == 8);
    rewind_nid_postfix(&file);
    for (const char expected : {'0', '1', '2', '3'}) Require(fgetc_nid_postfix(&file) == expected);
    std::int64_t position = -1;
    Require(fgetpos_nid_postfix(&file, &position) == 0 && position == 4);
    const std::int64_t negative = -1;
    errno = 0;
    Require(fsetpos_nid_postfix(&file, &negative) == -1 && errno == EINVAL);
    Require(fgetpos_nid_postfix(&file, &position) == 0 && position == 4);
    Require(fgetc_nid_postfix(&file) == '4');
    errno = 0;
    Require(fseeko_nid_postfix(&file, -6, SEEK_CUR) == -1 && errno == EINVAL);
    Require(fgetpos_nid_postfix(&file, &position) == 0 && position == 5);
    Require(fgetc_nid_postfix(&file) == '5');
    errno = 0;
    Require(fseeko_nid_postfix(&file, -9, SEEK_END) == -1 && errno == EINVAL);
    Require(fgetpos_nid_postfix(&file, &position) == 0 && position == 6);
    Require(fgetc_nid_postfix(&file) == '6');
    file.Close();
}

static void CheckSeekFromEndAfterBufferedWrite() {
    FileStream file(TemporaryFile::Open());
    Require(fwrite_nid_postfix("01234567", 1, 8, &file) == 8);
    Require(fseeko_nid_postfix(&file, -3, SEEK_END) == 0);
    Require(ftello_nid_postfix(&file) == 5);
    Require(fgetc_nid_postfix(&file) == '5');
    errno = 0;
    Require(fseeko_nid_postfix(&file, -9, SEEK_END) == -1 && errno == EINVAL);
    Require(ftello_nid_postfix(&file) == 6);
    file.Close();
}

int main() {
    CheckRejectedBufferedSeeks();
    CheckConcurrentRelativeSeeks();
    CheckFseekBounds();
    CheckFailedSeekKeepsPosition();
    CheckSeekFromEndAfterBufferedWrite();
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

    FileStream reread(TemporaryFile::Open());
    Require(std::fputs("position", reread.GetHandle()) >= 0);
    Require(fsetpos_nid_postfix(&reread, &beginning) == 0);
    for (const char expected : {'p', 'o', 's'}) Require(fgetc_nid_postfix(&reread) == expected);
    Require(fgetpos_nid_postfix(&reread, &position) == 0 && position == 3);
    const std::int64_t third = position;
    while (fgetc_nid_postfix(&reread) != EOF) {}
    Require(feof_nid_postfix(&reread) != 0);
    Require(fsetpos_nid_postfix(&reread, &third) == 0);
    Require(feof_nid_postfix(&reread) == 0);
    Require(fgetc_nid_postfix(&reread) == 'i');
    const std::int64_t negative = -1;
    errno = 0;
    Require(fsetpos_nid_postfix(&reread, &negative) != 0);
    Require(errno == EINVAL);
    reread.Close();

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

    FileStream probe(TemporaryFile::Open());
    auto* unmapped = reinterpret_cast<std::int64_t*>(std::uintptr_t{0x10});
    bool threw = false;
    try {
        fgetpos_nid_postfix(&probe, unmapped);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        fsetpos_nid_postfix(&probe, unmapped);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    probe.Close();
}
