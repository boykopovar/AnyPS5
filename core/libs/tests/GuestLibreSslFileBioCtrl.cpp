#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

using LibreSsl::Bio;

namespace {

constexpr int BioClose = 0x01;
constexpr int BioFileRead = 0x02;
constexpr int BioFileWrite = 0x04;
constexpr int BioFileAppend = 0x08;
constexpr int BioControlReset = 1;
constexpr int BioControlEof = 2;
constexpr int BioControlPush = 6;
constexpr int BioControlPop = 7;
constexpr int BioControlPending = 10;
constexpr int BioControlFlush = 11;
constexpr int BioControlWpending = 13;
constexpr int BioControlSetFilename = 108;
constexpr int BioControlFileSeek = 128;
constexpr int BioControlUnknown = 999;

constexpr std::uint64_t Error(int library, int reason) {
    return (static_cast<std::uint64_t>(library) << 24) | (static_cast<std::uint64_t>(0xfff) << 12) |
           static_cast<std::uint64_t>(reason);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected realignment"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }

int ProcessId() {
#ifdef _WIN32
    return static_cast<int>(_getpid());
#else
    return static_cast<int>(::getpid());
#endif
}

const std::string& GuestPrefix() {
    static const std::string prefix = "/anyps5-file-bio-ctrl-" + std::to_string(ProcessId());
    return prefix;
}

class TempFile {
public:
    explicit TempFile(const std::string& suffix) {
        static std::atomic<unsigned> counter{0};
        const auto name = "anyps5-file-bio-ctrl-" + std::to_string(ProcessId()) + "-" +
                          std::to_string(counter.fetch_add(1)) + suffix;
        host = std::filesystem::temp_directory_path() / name;
        guest = GuestPrefix() + "/" + name;
        AddPathAlias_nid_no_patch(guest.c_str(), host.string().c_str());
    }

    ~TempFile() {
        RemovePathAlias_nid_no_patch(guest.c_str());
        std::error_code ignored;
        std::filesystem::remove(host, ignored);
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    std::string Host() const { return host.string(); }
    const std::string& Guest() const { return guest; }

    void WriteBytes(const std::string& content) const {
        std::ofstream output(host, std::ios::binary | std::ios::trunc);
        Require(output.is_open(), "create named file BIO fixture");
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    std::string ReadBytes() const {
        std::ifstream input(host, std::ios::binary);
        Require(input.is_open(), "open named file BIO fixture");
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

private:
    std::filesystem::path host;
    std::string guest;
};

Bio* NewFile(const std::string& guest, int flags) {
    auto* bio = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    if (!bio) return nullptr;
    if (BIO_ctrl_nid_postfix(bio, BioControlSetFilename, flags | BioClose, const_cast<char*>(guest.c_str())) == 1) return bio;
    BIO_free_nid_postfix(bio);
    return nullptr;
}

void CheckNewFile() {
    TempFile file("new");
    file.WriteBytes("seed");
    {
        auto* bio = NewFile(file.Guest(), BioFileWrite);
        Require(bio != nullptr, "BIO_s_file BIO_C_SET_FILENAME write mode constructs a file BIO");
        Require(bio->method == BIO_s_file_nid_postfix(), "BIO_s_file BIO_C_SET_FILENAME installs the file method");
        Require(bio->initialized == 1 && bio->shutdown == 1, "BIO_s_file BIO_C_SET_FILENAME owns the opened stream");
        Require(BIO_write_nid_postfix(bio, "roundtrip", 9) == 9, "BIO_s_file BIO_C_SET_FILENAME write returns the byte count");
        Require(BIO_ctrl_nid_postfix(bio, BioControlFlush, 0, nullptr) == 1, "BIO_s_file BIO_C_SET_FILENAME flush succeeds");
        Require(BIO_free_nid_postfix(bio) == 1, "free BIO_s_file BIO_C_SET_FILENAME writer");
    }
    Require(file.ReadBytes() == "roundtrip", "BIO_s_file BIO_C_SET_FILENAME write mode truncates and writes");
    {
        auto* bio = NewFile(file.Guest(), BioFileRead);
        Require(bio != nullptr, "BIO_s_file BIO_C_SET_FILENAME read mode constructs a file BIO");
        char buffer[16]{};
        Require(bio->method->read(bio, buffer, 9) == 9 && std::memcmp(buffer, "roundtrip", 9) == 0,
                "BIO_s_file BIO_C_SET_FILENAME read returns the written bytes");
        Require(bio->method->read(bio, buffer, 1) == 0, "BIO_s_file BIO_C_SET_FILENAME read reaches EOF cleanly");
        Require(BIO_free_nid_postfix(bio) == 1, "free BIO_s_file BIO_C_SET_FILENAME reader");
    }
    TempFile missing("missing");
    ERR_clear_error_nid_postfix();
    errno = 0;
    Require(NewFile(missing.Guest(), BioFileRead) == nullptr, "BIO_s_file BIO_C_SET_FILENAME on a missing path returns null");
    const auto systemError = ERR_get_error_nid_postfix();
    const auto bioError = ERR_get_error_nid_postfix();
    Require((systemError >> 24) == 2 && (systemError & 0xfff) != 0, "missing file queues an ERR_LIB_SYS fopen error");
    Require((bioError >> 24) == 32 && (bioError & 0xfff) == 2, "missing file queues the ERR_LIB_BIO system error");
    Require(ERR_get_error_nid_postfix() == 0, "missing file queues exactly two errors");
    Require(*__error_nid_postfix() == 2, "BIO_s_file BIO_C_SET_FILENAME leaves ENOENT in guest errno");
}

Bio* OpenByControl(const std::string& guest, int flags) {
    auto* bio = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    Require(bio != nullptr, "allocate a file BIO for a filename mode");
    Require(BIO_ctrl_nid_postfix(bio, BioControlSetFilename, flags | BioClose, const_cast<char*>(guest.c_str())) == 1,
            "open the file BIO in the requested filename mode");
    return bio;
}

void CheckSetFilenameModes() {
    {
        TempFile file("modew");
        file.WriteBytes("original");
        auto* bio = OpenByControl(file.Guest(), BioFileWrite);
        Require(BIO_write_nid_postfix(bio, "W", 1) == 1, "write mode writes one byte");
        Require(BIO_free_nid_postfix(bio) == 1, "close write mode BIO");
        Require(file.ReadBytes() == "W", "BIO_FP_WRITE maps to truncation");
    }
    {
        TempFile file("moder");
        file.WriteBytes("readable");
        auto* bio = OpenByControl(file.Guest(), BioFileRead);
        char buffer[16]{};
        Require(bio->method->read(bio, buffer, 8) == 8 && std::memcmp(buffer, "readable", 8) == 0,
                "BIO_FP_READ maps to r");
        Require(BIO_free_nid_postfix(bio) == 1, "close read mode BIO");
    }
    {
        TempFile file("moderw");
        file.WriteBytes("abcdef");
        auto* bio = OpenByControl(file.Guest(), BioFileRead | BioFileWrite);
        char buffer[16]{};
        Require(bio->method->read(bio, buffer, 2) == 2 && std::memcmp(buffer, "ab", 2) == 0,
                "read/write mode reads existing bytes");
        Require(BIO_ctrl_nid_postfix(bio, BioControlFileSeek, 2, nullptr) == 0, "seek before the overwrite");
        Require(BIO_write_nid_postfix(bio, "XY", 2) == 2, "read/write mode overwrites bytes");
        Require(BIO_ctrl_nid_postfix(bio, BioControlFlush, 0, nullptr) == 1, "flush read/write mode");
        Require(BIO_free_nid_postfix(bio) == 1, "close read/write mode BIO");
        Require(file.ReadBytes() == "abXYef", "BIO_FP_READ | BIO_FP_WRITE maps to r+");
    }
    {
        TempFile file("modea");
        file.WriteBytes("base");
        auto* bio = OpenByControl(file.Guest(), BioFileAppend);
        Require(BIO_write_nid_postfix(bio, "A", 1) == 1, "append mode writes one byte");
        Require(BIO_free_nid_postfix(bio) == 1, "close append mode BIO");
        Require(file.ReadBytes() == "baseA", "BIO_FP_APPEND maps to a");
    }
    {
        TempFile file("modear");
        file.WriteBytes("base");
        auto* bio = OpenByControl(file.Guest(), BioFileAppend | BioFileRead);
        Require(BIO_ctrl_nid_postfix(bio, BioControlFileSeek, 0, nullptr) == 0, "seek to the start in append/read mode");
        char buffer[16]{};
        Require(bio->method->read(bio, buffer, 4) == 4 && std::memcmp(buffer, "base", 4) == 0,
                "append/read mode can read existing bytes");
        Require(BIO_write_nid_postfix(bio, "A", 1) == 1, "append/read mode writes one byte");
        Require(BIO_free_nid_postfix(bio) == 1, "close append/read mode BIO");
        Require(file.ReadBytes() == "baseA", "BIO_FP_APPEND | BIO_FP_READ maps to a+");
    }
    {
        TempFile file("modebad");
        auto* bio = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
        Require(bio != nullptr, "allocate a file BIO for an invalid mode");
        ERR_clear_error_nid_postfix();
        Require(BIO_ctrl_nid_postfix(bio, BioControlSetFilename, BioClose, const_cast<char*>(file.Guest().c_str())) == 0,
                "invalid open-mode flags return zero");
        Require(ERR_get_error_nid_postfix() == Error(32, 101), "invalid mode queues BIO_R_BAD_FOPEN_MODE");
        Require(ERR_get_error_nid_postfix() == 0, "invalid mode queues exactly one error");
        Require(BIO_free_nid_postfix(bio) == 1, "free BIO after an invalid mode");
    }
}

void CheckPutsAndWrite() {
    TempFile file("puts");
    std::FILE* native = std::fopen(file.Host().c_str(), "wb+");
    Require(native != nullptr, "open host file for puts and writes");
    FileStream stream(native);
    auto* bio = BIO_new_fp_nid_postfix(&stream, 0);
    Require(bio != nullptr, "create file BIO for puts and writes");
    const char text[] = "hello";
    Require(BIO_puts_nid_postfix(bio, text) == 5, "file_puts returns the exact string length");
    const unsigned char binary[] = {0x41, 0x00, 0x42, 0xff, 0x00, 0x43};
    Require(BIO_write_nid_postfix(bio, binary, 6) == 6, "BIO_write uses the explicit binary length");
    Require(BIO_write_nid_postfix(bio, "tail", 4) == 4, "BIO_write accepts a partial chunk");
    const char withNul[] = {'a', 0x00, 'b', 0x00};
    Require(BIO_puts_nid_postfix(bio, withNul) == 1, "file_puts stops at the first embedded NUL");
    Require(BIO_write_nid_postfix(bio, "ignored", 0) == 0, "zero-length BIO_write writes nothing");
    Require(BIO_write_nid_postfix(bio, "x", -1) == 0, "negative BIO_write size writes nothing");
    Require(bio->bytesWritten == 16, "puts and writes update byte accounting");
    Require(BIO_ctrl_nid_postfix(bio, BioControlFlush, 0, nullptr) == 1, "flush the written file BIO");
    std::rewind(native);
    std::array<unsigned char, 32> actual{};
    Require(std::fread(actual.data(), 1, 16, native) == 16, "the file contains exactly the written bytes");
    Require(std::memcmp(actual.data(), "hello", 5) == 0, "puts bytes are written verbatim");
    Require(std::memcmp(actual.data() + 5, binary, 6) == 0, "binary writes preserve embedded NULs");
    Require(std::memcmp(actual.data() + 11, "tail", 4) == 0, "the partial chunk follows the binary bytes");
    Require(actual[15] == 'a', "the NUL-terminated puts writes only its first byte");
    Require(BIO_free_nid_postfix(bio) == 1, "free the puts file BIO");
    Require(fclose_nid_postfix(&stream) == 0, "close the puts fixture");
}

void CheckReadPartialAndEof() {
    TempFile file("read");
    file.WriteBytes("0123456789");
    auto* bio = NewFile(file.Guest(), BioFileRead);
    Require(bio != nullptr, "open a file BIO for chunked reads");
    char buffer[16];
    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(bio->method->read(bio, buffer, 4) == 4 && std::memcmp(buffer, "0123", 4) == 0, "first partial read");
    Require(bio->method->read(bio, buffer, 3) == 3 && std::memcmp(buffer, "456", 3) == 0, "second partial read");
    Require(bio->method->read(bio, buffer, 3) == 3 && std::memcmp(buffer, "789", 3) == 0, "final exact read");
    Require(bio->method->read(bio, buffer, 1) == 0, "reading at EOF returns zero");
    Require(BIO_ctrl_nid_postfix(bio, BioControlEof, 0, nullptr) != 0, "EOF is reported after reading past the end");
    Require(bio->method->read(bio, nullptr, 4) == 0, "a null read destination returns zero");
    Require(bio->method->read(bio, buffer, -1) == -1, "a negative read size is rejected");
    Require(ERR_get_error_nid_postfix() == 0, "chunked clean reads queue no errors");
    Require(BIO_free_nid_postfix(bio) == 1, "free the chunked read BIO");
}

void CheckEofControl() {
    TempFile file("eof");
    file.WriteBytes("xy");
    auto* bio = NewFile(file.Guest(), BioFileRead);
    Require(bio != nullptr, "open a file BIO for EOF checks");
    char buffer[8]{};
    Require(BIO_ctrl_nid_postfix(bio, BioControlEof, 0, nullptr) == 0, "BIO_CTRL_EOF is zero before any read");
    Require(bio->method->read(bio, buffer, 2) == 2, "read all available bytes");
    Require(bio->method->read(bio, buffer, 1) == 0, "read past the end returns zero");
    Require(BIO_ctrl_nid_postfix(bio, BioControlEof, 0, nullptr) != 0, "BIO_CTRL_EOF is non-zero after EOF");
    Require(BIO_ctrl_nid_postfix(bio, BioControlFileSeek, 0, nullptr) == 0, "seek after reaching EOF");
    Require(BIO_ctrl_nid_postfix(bio, BioControlEof, 0, nullptr) == 0, "seek clears the EOF state");
    Require(bio->method->read(bio, buffer, 8) == 2, "read the file again after the seek");
    Require(bio->method->read(bio, buffer, 1) == 0, "reach EOF a second time");
    Require(BIO_ctrl_nid_postfix(bio, BioControlEof, 0, nullptr) != 0, "BIO_CTRL_EOF reports the second EOF");
    Require(BIO_ctrl_nid_postfix(bio, BioControlReset, 0, nullptr) == 0, "reset after reaching EOF");
    Require(BIO_ctrl_nid_postfix(bio, BioControlEof, 0, nullptr) == 0, "reset clears the EOF state");
    Require(BIO_free_nid_postfix(bio) == 1, "free the EOF BIO");
}

void CheckUnsupportedControls() {
    TempFile file("ctrl");
    file.WriteBytes("data");
    auto* bio = NewFile(file.Guest(), BioFileRead);
    Require(bio != nullptr, "open a file BIO for unsupported controls");
    Require(BIO_ctrl_nid_postfix(bio, BioControlPending, 0, nullptr) == 0, "BIO_CTRL_PENDING is unsupported");
    Require(BIO_ctrl_nid_postfix(bio, BioControlWpending, 0, nullptr) == 0, "BIO_CTRL_WPENDING is unsupported");
    Require(BIO_ctrl_nid_postfix(bio, BioControlPush, 0, nullptr) == 0, "BIO_CTRL_PUSH is unsupported");
    Require(BIO_ctrl_nid_postfix(bio, BioControlPop, 0, nullptr) == 0, "BIO_CTRL_POP is unsupported");
    Require(BIO_ctrl_nid_postfix(bio, BioControlUnknown, 0, nullptr) == 0, "an unknown control is unsupported");
    Require(ERR_get_error_nid_postfix() == 0, "unsupported controls queue no errors");
    Require(BIO_free_nid_postfix(bio) == 1, "free the BIO for unsupported controls");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckNewFile();
    CheckSetFilenameModes();
    CheckPutsAndWrite();
    CheckReadPartialAndEof();
    CheckEofControl();
    CheckUnsupportedControls();
    return 0;
}
