#include "prx/libc/include/AppArchive.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include "prx/libc/include/General.hpp"
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/mman.h>
#endif
#endif

namespace {

class TemporaryBacking {
public:
    TemporaryBacking() {
#ifdef _WIN32
        static std::atomic<std::uint64_t> serial{0};
        std::error_code directoryError;
        const auto directory = std::filesystem::temp_directory_path(directoryError);
        if (directoryError) throw std::system_error(directoryError, "materialize app0.zar: temporary directory");
        for (;;) {
            path = directory / (L"AnyPS5-app0-map-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(serial.fetch_add(1)) + L".tmp");
            writer = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
            if (writer != INVALID_HANDLE_VALUE) break;
            const auto error = GetLastError();
            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) Fail(error);
        }
#elif defined(__linux__)
        writer = memfd_create("app0.zar backing", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        if (writer < 0) Fail(errno);
#else
        std::error_code directoryError;
        const auto directory = std::filesystem::temp_directory_path(directoryError);
        if (directoryError) throw std::system_error(directoryError, "materialize app0.zar: temporary directory");
        const auto name = (directory / "anyps5-app0-map-XXXXXX").string();
        std::vector<char> buffer(name.begin(), name.end());
        buffer.push_back(0);
        writer = mkstemp(buffer.data());
        if (writer < 0) Fail(errno);
        try {
            path = buffer.data();
        } catch (...) {
            close(writer);
            unlink(buffer.data());
            throw;
        }
        if (fcntl(writer, F_SETFD, FD_CLOEXEC) < 0) {
            const int error = errno;
            close(writer);
            unlink(path.c_str());
            Fail(error);
        }
#endif
    }

    ~TemporaryBacking() {
#ifdef _WIN32
        if (writer != INVALID_HANDLE_VALUE) CloseHandle(writer);
#else
        if (writer >= 0) close(writer);
        if (!path.empty()) unlink(path.c_str());
#endif
    }

    TemporaryBacking(const TemporaryBacking&) = delete;
    TemporaryBacking& operator=(const TemporaryBacking&) = delete;

    void Write(const unsigned char* data, std::size_t bytes) {
        while (bytes != 0) {
#ifdef _WIN32
            DWORD written = 0;
            if (!WriteFile(writer, data, static_cast<DWORD>(bytes), &written, nullptr)) Fail(GetLastError());
#else
            const auto written = write(writer, data, bytes);
            if (written < 0) {
                if (errno == EINTR) continue;
                Fail(errno);
            }
#endif
            if (written == 0) throw std::system_error(EIO, std::generic_category(), "materialize app0.zar: empty write");
            data += written;
            bytes -= static_cast<std::size_t>(written);
        }
    }

    int ReadOnlyDescriptor() {
#ifdef _WIN32
        const HANDLE reader = ReOpenFile(writer, GENERIC_READ | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_DELETE_ON_CLOSE);
        if (reader == INVALID_HANDLE_VALUE) Fail(GetLastError());
        const int descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(reader), _O_RDONLY | _O_BINARY | _O_NOINHERIT);
        if (descriptor < 0) {
            const int error = errno;
            CloseHandle(reader);
            throw std::system_error(error, std::generic_category(), "materialize app0.zar: adopt reader");
        }
        return descriptor;
#else
#ifdef __linux__
        if (fcntl(writer, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL) < 0) Fail(errno);
        const auto readerPath = std::filesystem::path("/proc/self/fd") / std::to_string(writer);
#else
        const auto& readerPath = path;
#endif
        const int reader = open(readerPath.c_str(), O_RDONLY | O_CLOEXEC);
        if (reader < 0) Fail(errno);
        struct stat first{};
        struct stat second{};
        const bool identified = fstat(writer, &first) == 0 && fstat(reader, &second) == 0;
        const int error = identified ? EIO : errno;
        if (!identified || first.st_dev != second.st_dev || first.st_ino != second.st_ino) {
            close(reader);
            Fail(error);
        }
#ifndef __linux__
        if (unlink(path.c_str()) != 0) {
            const int error = errno;
            close(reader);
            Fail(error);
        }
        path.clear();
#endif
        return reader;
#endif
    }

private:
    [[noreturn]] static void Fail(int error) {
#ifdef _WIN32
        error = WindowsOpenError_nid_no_patch(static_cast<DWORD>(error));
#endif
        throw std::system_error(error, std::generic_category(), "materialize app0.zar backing");
    }

    std::filesystem::path path;
#ifdef _WIN32
    HANDLE writer = INVALID_HANDLE_VALUE;
#else
    int writer = -1;
#endif
};

}

extern "C" int AppArchiveMaterialize_nid_no_patch(const AppArchiveNode& node, std::uint64_t offset, std::uint64_t length) {
    constexpr auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (node.Directory || node.Size > limit || offset > limit || length > limit - offset)
        throw std::invalid_argument("materialize app0.zar: not a representable regular file");
    TemporaryBacking backing;
    std::array<unsigned char, 65536> buffer{};
    const auto available = offset < node.Size ? std::min(length, node.Size - offset) : 0;
    for (std::uint64_t done = 0; done < available;) {
        const auto amount = std::min<std::uint64_t>(buffer.size(), available - done);
        const auto read = AppArchiveRead_nid_no_patch(node.Handle, offset + done, buffer.data(), amount);
        if (read != amount) throw std::runtime_error("materialize app0.zar: truncated archive entry");
        backing.Write(buffer.data(), static_cast<std::size_t>(amount));
        done += amount;
    }
    return backing.ReadOnlyDescriptor();
}
