#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

extern "C" {
FileStream* APS5_VABI fopen_nid_postfix(const char* filename, const char* mode);
int APS5_VABI fclose_nid_postfix(FileStream* stream);
std::size_t APS5_VABI fread_nid_postfix(void* buffer, std::size_t size, std::size_t count, FileStream* stream);
std::size_t APS5_VABI fwrite_nid_postfix(const void* buffer, std::size_t size, std::size_t count, FileStream* stream);
int APS5_VABI fseek_nid_postfix(FileStream* stream, std::int64_t offset, int origin);
std::int64_t APS5_VABI ftell_nid_postfix(FileStream* stream);
int APS5_VABI fputs_nid_postfix(const char* str, FileStream* stream);
int APS5_VABI fflush_nid_postfix(FileStream* stream);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr char payload[] = "stream round trip";

class StreamFile {
public:
    StreamFile()
        : directoryName("anyps5-file-streams-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())),
          guestPath(directoryName + "/file_streams.tmp") {
        Require(std::filesystem::create_directory(std::filesystem::current_path() / directoryName),
            "create the test directory");
    }

    ~StreamFile() {
        if (stream != nullptr) fclose_nid_postfix(stream);
        std::error_code ignored;
        std::filesystem::remove_all(std::filesystem::current_path() / directoryName, ignored);
    }

    StreamFile(const StreamFile&) = delete;
    StreamFile& operator=(const StreamFile&) = delete;

    FileStream* Open(const char* mode) {
        stream = fopen_nid_postfix(guestPath.c_str(), mode);
        Require(stream != nullptr, "open " + guestPath + " with mode " + mode);
        return stream;
    }

    int Close() {
        auto* closing = stream;
        stream = nullptr;
        return fclose_nid_postfix(closing);
    }

    const std::string directoryName;
    const std::string guestPath;

private:
    FileStream* stream = nullptr;
};

const Case standardStreams{"StandardStreamObjects_HostStandardStreams_WrapAndAcceptWrites", [] {
    Require(_Stdout_nid_postfix.GetHandle() == stdout, "stdout object wraps the host stdout");
    Require(_Stderr_nid_postfix.GetHandle() == stderr, "stderr object wraps the host stderr");
    Require(fputs_nid_postfix("stdout object works\n", &_Stdout_nid_postfix) >= 0, "write to stdout");
    Require(fputs_nid_postfix("stderr object works\n", &_Stderr_nid_postfix) >= 0, "write to stderr");
    RequireEqual(fflush_nid_postfix(nullptr), 0, "flush every stream");
}};

const Case roundTrip{"FileStream_WriteSeekRead_RoundTripsPayloadAndReachesEof", [] {
    StreamFile file;
    auto* stream = file.Open("w+b");
    Require(fputs_nid_postfix(payload, stream) >= 0, "fputs");
    RequireEqual(fwrite_nid_postfix(payload, 1, sizeof(payload), stream), sizeof(payload), "fwrite");
    RequireEqual(ftell_nid_postfix(stream), static_cast<std::int64_t>(sizeof(payload) * 2 - 1), "ftell after writing");
    RequireEqual(fflush_nid_postfix(stream), 0, "fflush");
    RequireEqual(fseek_nid_postfix(stream, 0, SEEK_SET), 0, "fseek to the beginning");
    std::array<char, sizeof(payload)> buffer{};
    RequireEqual(fread_nid_postfix(buffer.data(), 1, sizeof(payload) - 1, stream), sizeof(payload) - 1, "read the fputs text");
    RequireEqual(std::string_view(buffer.data()), std::string_view(payload), "fputs text");
    RequireEqual(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream), buffer.size(), "read the fwrite bytes");
    RequireEqual(std::string_view(buffer.data()), std::string_view(payload), "fwrite bytes");
    RequireEqual(fread_nid_postfix(buffer.data(), 1, buffer.size(), stream), std::size_t{0}, "read at end of file");
    RequireEqual(file.Close(), 0, "fclose");
}};

const Case nullBuffers{"FileStream_NullWriteBuffers_Throw", [] {
    StreamFile file;
    auto* stream = file.Open("w+b");
    RequireThrows<std::runtime_error>([&] { fputs_nid_postfix(nullptr, stream); }, "fputs of a null string");
    RequireThrows<std::runtime_error>([&] { fwrite_nid_postfix(nullptr, 1, sizeof(payload), stream); },
        "fwrite of a null buffer");
}};

const Case invalidOrigin{"Fseek_InvalidOrigin_FailsWithEinval", [] {
    StreamFile file;
    auto* stream = file.Open("w+b");
    errno = 0;
    RequireEqual(fseek_nid_postfix(stream, 0, -1), -1, "fseek result");
    RequireEqual(errno, EINVAL, "errno");
}};

const Case nullArguments{"FileStream_NullStreamOrPath_Throws", [] {
    RequireThrows<std::runtime_error>([] { fputs_nid_postfix("invalid stream", nullptr); }, "fputs to a null stream");
    RequireThrows<std::runtime_error>([] { fopen_nid_postfix(nullptr, "r"); }, "fopen of a null path");
}};

const Case closedStream{"Fflush_ClosedStream_Throws", [] {
    FileStream closed(std::tmpfile());
    closed.Close();
    RequireThrows<std::runtime_error>([&] { fflush_nid_postfix(&closed); }, "fflush of a closed stream");
}};

const Case missingFile{"Fopen_RemovedFileForReading_ReturnsNullWithEnoent", [] {
    StreamFile file;
    file.Open("w+b");
    RequireEqual(file.Close(), 0, "fclose");
    RequireEqual(std::remove(file.guestPath.c_str()), 0, "remove the file");
    errno = 0;
    Require(fopen_nid_postfix(file.guestPath.c_str(), "rb") == nullptr, "fopen of a removed file");
    RequireEqual(errno, ENOENT, "errno");
}};

} // namespace
