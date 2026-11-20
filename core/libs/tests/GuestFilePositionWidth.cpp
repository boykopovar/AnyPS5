#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "tests/TemporaryFile.hpp"
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <optional>

static bool Check(bool value, std::int64_t offset, const char* operation) {
    if (!value) std::fprintf(stderr, "File position offset %" PRId64 ": %s failed\n", offset, operation);
    return value;
}

static bool CheckPosition(std::int64_t offset) {
    std::FILE* handle = nullptr;
    std::optional<FileStream> file;
    bool passed = true;
    try {
        handle = TemporaryFile::Open();
        if (!Check(handle != nullptr, offset, "open temporary file")) return false;
        file.emplace(handle);
        do {
            if (!Check(fseeko_nid_postfix(&*file, offset, SEEK_SET) == 0, offset, "seek to offset")) {
                passed = false;
                break;
            }
            std::int64_t saved = -1;
            const int saveResult = fgetpos_nid_postfix(&*file, &saved);
            const bool savedSuccessfully = Check(saveResult == 0, offset, "save position");
            const bool savedExactly = Check(saved == offset, offset, "saved position width");
            if (!savedSuccessfully || !savedExactly) {
                passed = false;
                break;
            }
            if (!Check(fseeko_nid_postfix(&*file, 0, SEEK_SET) == 0, offset, "seek to beginning")) {
                passed = false;
                break;
            }
            const int character = fgetc_nid_postfix(&*file);
            const int endOfFile = feof_nid_postfix(&*file);
            const bool readEnd = Check(character == EOF, offset, "read empty file");
            const bool markedEnd = Check(endOfFile != 0, offset, "set EOF indicator");
            if (!readEnd || !markedEnd) {
                passed = false;
                break;
            }
            const int restoreResult = fsetpos_nid_postfix(&*file, &saved);
            const std::int64_t restored = ftello_nid_postfix(&*file);
            const int restoredEndOfFile = feof_nid_postfix(&*file);
            if (!Check(restoreResult == 0, offset, "restore position")) passed = false;
            if (!Check(restored == offset, offset, "restored position width")) passed = false;
            if (!Check(restoredEndOfFile == 0, offset, "clear EOF indicator")) passed = false;
        } while (false);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "File position offset %" PRId64 ": exception: %s\n", offset, error.what());
        passed = false;
    } catch (...) {
        std::fprintf(stderr, "File position offset %" PRId64 ": unknown exception\n", offset);
        passed = false;
    }
    try {
        if (file) {
            if (!Check(file->Close() == 0, offset, "close stream")) passed = false;
        } else if (handle) {
            if (!Check(std::fclose(handle) == 0, offset, "close temporary file")) passed = false;
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "File position offset %" PRId64 ": close exception: %s\n", offset, error.what());
        passed = false;
    } catch (...) {
        std::fprintf(stderr, "File position offset %" PRId64 ": unknown close exception\n", offset);
        passed = false;
    }
    return passed;
}

int main() {
    const bool first = CheckPosition(INT64_C(2147483665));
    const bool second = CheckPosition(INT64_C(4294967313));
    return first && second ? 0 : 1;
}
