extern "C" {
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <filesystem>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include "prx/libc/include/WindowsDescriptor.hpp"
#include "prx/libc/include/WindowsVerbatimPath.hpp"
#endif

#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/HostErrno.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/HostEntropy.hpp"

#ifdef __linux__
#include <stdio_ext.h>
#endif

struct GuestFileMode {
    std::string native;
    bool writes = false;
    bool exclusive = false;
    bool append = false;
};

void LibcRegisterDirectoryStreams_nid_no_patch(int (*open)(const wchar_t*), void (*forget)(int), void (*duplicate)(int, int)) {
    g_openDirectoryDescriptor = open;
    g_forgetDirectoryDescriptor = forget;
    g_duplicateDirectoryDescriptor = duplicate;
}

void ForgetDirectoryStream_nid_no_patch(int descriptor) {
    if (!g_forgetDirectoryDescriptor) throw std::runtime_error("ForgetDirectoryStream: libkernel has not registered its directory descriptors");
    g_forgetDirectoryDescriptor(descriptor);
}

void DuplicateDirectoryStream_nid_no_patch(int from, int to) {
    if (!g_duplicateDirectoryDescriptor) throw std::runtime_error("DuplicateDirectoryStream: libkernel has not registered its directory descriptors");
    g_duplicateDirectoryDescriptor(from, to);
}

static std::FILE* OpenDirectoryHandle(const std::filesystem::path& path, const std::string& = {}) {
    if (!g_openDirectoryDescriptor) throw std::runtime_error("OpenDirectoryHandle: libkernel has not registered its directory descriptors");
    const auto filename = HostOpenName(path);
    if (!filename) { errno = GuestErrnoFromHost_nid_no_patch(errno); return nullptr; }
    const int descriptor = g_openDirectoryDescriptor(filename->c_str());
    if (descriptor < 0) {
        const int error = GuestErrnoFromHost_nid_no_patch(errno);
        errno = error == 2 ? NotFoundError_nid_no_patch(path) : error;
        return nullptr;
    }
    auto* native = ::_fdopen(descriptor, "rb");
    if (!native) {
        const int error = GuestErrnoFromHost_nid_no_patch(errno);
        ForgetDirectoryStream_nid_no_patch(descriptor);
        ::_close(descriptor);
        errno = error;
    }
    return native;
}

static FileStream* OpenDirectoryStream(const std::filesystem::path& path) {
    const auto release = [](void* pointer) { ::operator delete(pointer); };
    std::unique_ptr<void, decltype(release)> storage(::operator new(sizeof(FileStream)), release);
    auto* native = OpenDirectoryHandle(path);
    if (!native) return nullptr;
    auto* stream = new (storage.get()) FileStream(native, true);
    storage.release();
    stream->MarkDirectory();
    return stream;
}
#endif

FileStream* APS5_VABI freopen_nid_postfix(const char* filename, const char* mode, FileStream* stream) {
    if (!stream || !mode) { errno = 22; return nullptr; }
    if (!filename && !stream->IsOpen()) { errno = 9; return nullptr; }
    if (!filename) { errno = 45; return nullptr; } // Mode-only reopening is not supported.
    const auto parsed = ParseFileMode(mode);
    if (!parsed) { errno = 22; return nullptr; }
    try {
        if (IsRandomDevicePath(filename)) {
            if (parsed->writes) throw std::runtime_error(std::string(__func__) + ": writing to " + filename + " is not implemented");
            if (!stream->Reopen(NullDevicePath, "rb", OpenNullDevice)) {
                const int error = errno;
                if (stream->IsDynamic()) delete stream;
                errno = error;
                return nullptr;
            }
            stream->MarkRandomDevice();
            return stream;
        }
        std::filesystem::path path;
        if (const int error = *filename ? ResolvePathError_nid_no_patch(filename, path) : 2) {
            if (stream->IsHostStandard()) stream->Detach();
            else if (stream->IsOpen()) stream->Close();
            if (stream->IsDynamic()) delete stream;
            errno = error;
            return nullptr;
        }
#ifdef _WIN32
        if (std::error_code status; !parsed->writes && std::filesystem::is_directory(path, status)) {
            if (!stream->Reopen(path, parsed->native.c_str(), OpenDirectoryHandle, true)) {
                const int error = errno;
                if (stream->IsDynamic()) delete stream;
                errno = error;
                return nullptr;
            }
            stream->MarkDirectory();
            return stream;
        }
#endif
        if (stream->Reopen(path, parsed->native.c_str(), OpenNative)) {
#ifdef _WIN32
            if (parsed->append && !AppendOnlyDescriptor(_fileno(stream->GetHandle()))) {
                const int error = errno;
                stream->Close();
                if (stream->IsDynamic()) delete stream;
                errno = error;
                return nullptr;
            }
#endif
            if (parsed->append) static_cast<void>(std::fseek(stream->GetHandle(), 0, SEEK_END));
            if (!path.empty() && parsed->writes) RecordWrittenPath_nid_no_patch(path);
            return stream;
        }
        int error = OpenFailure(path, parsed->writes, errno);
        if (error == 2) error = NotFoundError_nid_no_patch(path);
        if (stream->IsDynamic()) delete stream;
        errno = error;
        return nullptr;
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
      catch (const std::filesystem::filesystem_error&) { errno = 5; return nullptr; }
}

FileStream* APS5_VABI fopen_nid_postfix(const char* filename, const char* mode) {
    if (!filename || !mode) throw std::runtime_error(std::string(__func__) + ": " + FOPEN_MSG_NULL_ARG);
    const auto parsed = ParseFileMode(mode);
    if (!parsed) {
        errno = EINVAL;
        return nullptr;
    }
    if (IsRandomDevicePath(filename)) {
        if (parsed->writes) throw std::runtime_error(std::string(__func__) + ": writing to " + filename + " is not implemented");
        std::unique_ptr<std::FILE, int (*)(std::FILE*)> device(OpenNullDevice(), std::fclose);
        if (!device) return nullptr;
        auto stream = std::make_unique<FileStream>(device.get(), true);
        device.release();
        stream->MarkRandomDevice();
        return stream.release();
    }
    std::filesystem::path fpath;
    if (const int error = ResolvePathError_nid_no_patch(filename, fpath)) { errno = error; return nullptr; }
    std::unique_ptr<std::FILE, int (*)(std::FILE*)> handle(OpenNative(fpath, parsed->native), std::fclose);
    if (!handle) {
        const int hostError = errno;
#ifdef _WIN32
        std::error_code status;
        if (!parsed->writes && GuestErrnoFromHost_nid_no_patch(hostError) == 13 && std::filesystem::is_directory(fpath, status))
            return OpenDirectoryStream(fpath);
#endif
        errno = OpenFailure(fpath, parsed->writes, hostError);
        if (errno == 2) errno = NotFoundError_nid_no_patch(fpath);
        return nullptr;
    }
    // APS5_LOG_OUT("success: \"%s\"", abs_path.c_str());
    if (parsed->append) static_cast<void>(std::fseek(handle.get(), 0, SEEK_END));
    if (parsed->writes) RecordWrittenPath_nid_no_patch(fpath);
    auto stream = std::make_unique<FileStream>(handle.get(), true);
    handle.release();
    return stream.release();
}

int APS5_VABI fopen_s_nid_postfix(FileStream** result, const char* filename, const char* mode) {
    constexpr int GuestEinval = 22;
    FileStream* nullStream = nullptr;
    const int savedErrno = errno;
    const bool probed = result && GuestCopyOut_nid_no_patch(result, &nullStream, sizeof(nullStream));
    errno = savedErrno;
    if (!probed) {
        SonyInvokeConstraintHandler("fopen_s: bad FILE ** argument", nullptr, GuestEinval);
        return GuestEinval;
    }
    if (!filename || !mode) {
        SonyInvokeConstraintHandler("fopen_s: bad argument", nullptr, GuestEinval);
        return GuestEinval;
    }
    FileStream* stream = fopen_nid_postfix(filename, mode);
    if (!stream) {
        return errno;
    }
    const int openErrno = errno;
    if (!GuestCopyOut_nid_no_patch(result, &stream, sizeof(stream))) {
        fclose_nid_postfix(stream);
        errno = openErrno;
        SonyInvokeConstraintHandler("fopen_s: bad FILE ** argument", nullptr, GuestEinval);
        return GuestEinval;
    }
    errno = openErrno;
    return 0;
}

int APS5_VABI freopen_s_nid_postfix(FileStream** result, const char* filename, const char* mode, FileStream* stream) {
    constexpr int GuestEinval = 22;
    FileStream* nullStream = nullptr;
    const int savedErrno = errno;
    const bool probed = result && GuestCopyOut_nid_no_patch(result, &nullStream, sizeof(nullStream));
    errno = savedErrno;
    if (!probed) {
        SonyInvokeConstraintHandler("fopen_s: bad FILE ** argument", nullptr, GuestEinval);
        return GuestEinval;
    }
    if (!mode || !stream) {
        SonyInvokeConstraintHandler("fopen_s: bad argument", nullptr, GuestEinval);
        return GuestEinval;
    }
    FileStream* reopened = freopen_nid_postfix(filename, mode, stream);
    if (!reopened) {
        return errno;
    }
    const int reopenErrno = errno;
    if (!GuestCopyOut_nid_no_patch(result, &reopened, sizeof(reopened))) {
        errno = reopenErrno;
        SonyInvokeConstraintHandler("fopen_s: bad FILE ** argument", nullptr, GuestEinval);
        return GuestEinval;
    }
    errno = reopenErrno;
    return 0;
}

int APS5_VABI fclose_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) {
        std::unique_ptr<FileStream> owner(stream->IsDynamic() ? stream : nullptr);
        errno = 9;
        return -1;
    }
    GetStreamHandle(stream);
    std::unique_ptr<FileStream> owner(stream->IsDynamic() ? stream : nullptr);
    return stream->Close();
}

FileStream* APS5_VABI _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(const char* filename, int mode, int protection) {
    static_cast<void>(protection);
    constexpr int In = 0x01, Out = 0x02, Ate = 0x04, App = 0x08, Trunc = 0x10, Nocreate = 0x20, Noreplace = 0x40,
        Binary = 0x80;
    constexpr std::pair<int, const char*> modes[] = {
        {In, "r"}, {Out, "w"}, {Out | Trunc, "w"}, {Out | App, "a"},
        {In | Binary, "rb"}, {Out | Binary, "wb"}, {Out | Trunc | Binary, "wb"}, {Out | App | Binary, "ab"},
        {In | Out, "r+"}, {In | Out | Trunc, "w+"}, {In | Out | App, "a+"},
        {In | Out | Binary, "r+b"}, {In | Out | Trunc | Binary, "w+b"}, {In | Out | App | Binary, "a+b"},
        {Out | Noreplace, "wx"}, {Out | Trunc | Noreplace, "wx"}, {In | Out | Trunc | Noreplace, "w+x"},
        {Out | Binary | Noreplace, "wbx"}, {Out | Trunc | Binary | Noreplace, "wbx"},
        {In | Out | Trunc | Binary | Noreplace, "w+bx"}};
    const int open = (mode & (In | Out | App | Trunc | Binary)) | ((mode & Nocreate) ? In : 0) | ((mode & App) ? Out : 0) |
        (((mode & Noreplace) && (mode & (Out | App))) ? Noreplace : 0);
    const char* openMode = nullptr;
    for (const auto& [flags, text] : modes) if (flags == open) openMode = text;
    if (!openMode) return nullptr;
    auto* stream = fopen_nid_postfix(filename, openMode);
    if (!stream || !(mode & Ate) || fseek_nid_postfix(stream, 0, SEEK_END) == 0) return stream;
    fclose_nid_postfix(stream);

    return nullptr;
}

size_t APS5_VABI fread_nid_postfix(void* buffer, size_t size, size_t count, FileStream* stream) {
    if (size == 0 || count == 0) return 0;
    if (count > std::numeric_limits<std::size_t>::max() / size) { errno = EINVAL; return 0; }
    if (stream && !stream->IsOpen()) { errno = 9; return 0; }
    auto* handle = GetStreamHandle(stream);
    stream->Orientation(-1);
    if (!buffer) throw std::runtime_error("fread: null buffer");
    const auto bytes = size * count;
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - reinterpret_cast<std::uintptr_t>(buffer)) throw std::overflow_error("fread: buffer address overflow");
    const GuestArena::HostWrite destination(buffer, bytes);
    if (!destination.Open()) throw std::runtime_error("fread: buffer is not writable");
    if (stream->IsRandomDevice()) {
        HostEntropy_nid_no_patch(buffer, bytes);
        stream->SyncStatus();
        return count;
    }
    const int previousError = errno;
    errno = 0;
    if (!stream->SwitchToReading()) { errno = errno != 0 ? GuestErrnoFromHost_nid_no_patch(errno) : previousError; return 0; }
    const auto result = std::fread(buffer, size, count, handle);
    const int nativeError = errno;
    stream->SyncStatus();
    if (nativeError != 0) errno = GuestErrnoFromHost_nid_no_patch(nativeError);
    else errno = previousError;
    stream->KeepDirectoryReadError(result < count);
    return result;
}

size_t APS5_VABI fwrite_nid_postfix(const void* buffer, size_t size, size_t count, FileStream* stream) {
    if (size * count == 0) return 0;
    if (stream && !stream->IsOpen()) { errno = 9; return 0; }
    auto* handle = GetNativeStream(stream);
    stream->Orientation(-1);
    if (!buffer) throw std::runtime_error("fwrite: null buffer");
    if (count > std::numeric_limits<std::size_t>::max() / size) throw std::overflow_error("fwrite: buffer size overflow");
    const auto bytes = size * count;
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - reinterpret_cast<std::uintptr_t>(buffer)) throw std::overflow_error("fwrite: buffer address overflow");
    if (!stream->SwitchToWriting()) return 0;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buffer, bytes);
    const int previousError = errno;
    errno = 0;
    const auto result = std::fwrite(buffer, size, count, handle);
    const int nativeError = errno;
    if (result < count) stream->DropFailedUnbufferedWrite();
    stream->SyncStatus();
    errno = nativeError != 0 ? GuestErrnoFromHost_nid_no_patch(nativeError) : previousError;
    return result;
}

static bool WritingStream(std::FILE* native) {
#ifdef _WIN32
    constexpr long UcrtWriting = 0x0002;
    std::atomic_ref<long> state(reinterpret_cast<UcrtStreamState*>(native)->flags);
    return (state.load() & UcrtWriting) != 0;
#elif defined(__GLIBC__)
    return ::__fwriting(native) != 0;
#else
    return true;
#endif
}

static std::optional<std::int64_t> RegularFileSize(std::FILE* native) {
#ifdef _WIN32
    struct _stat64 status;
    if (_fstat64(_fileno(native), &status) != 0 || (status.st_mode & _S_IFMT) != _S_IFREG) return std::nullopt;
#else
    struct stat status;
    if (::fstat(::fileno(native), &status) != 0 || !S_ISREG(status.st_mode)) return std::nullopt;
#endif
    if (status.st_size < 0) return std::nullopt;
    return status.st_size;
}

namespace {

class SeekLock {
    FileStream* stream;

public:
    explicit SeekLock(FileStream* value) : stream(value) { if (stream) flockfile_nid_postfix(stream); }
    ~SeekLock() { if (stream) funlockfile_nid_postfix(stream); }
    SeekLock(const SeekLock&) = delete;
    SeekLock& operator=(const SeekLock&) = delete;
};

}

static bool ResolveSeekTarget(FileStream* stream, std::int64_t& offset, int& origin) {
    std::int64_t base = 0;
    if (origin == SEEK_CUR) {
        base = ftello_nid_postfix(stream);
        if (base == -1) return false;
        if (base < 0) { errno = 29; return false; }
    } else if (origin == SEEK_END) {
        auto* native = GetNativeStream(stream);
        if (WritingStream(native)) return true;
        const auto size = RegularFileSize(native);
        if (!size) return true;
        base = *size;
    }
    if (offset > 0 && base > std::numeric_limits<std::int64_t>::max() - offset) { errno = 84; return false; }
    offset += base;
    if (offset < 0) { errno = 22; return false; }
    origin = SEEK_SET;
    return true;
}

int APS5_VABI fseeko_nid_postfix(FileStream* stream, std::int64_t offset, int origin) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) { errno = 22; return -1; }
#ifdef _WIN32
    auto* native = GetNativeStream(stream);
    if (IsPipeStream(native)) { errno = 29; return -1; }
#endif
    const SeekLock lock(stream);
    if (!ResolveSeekTarget(stream, offset, origin)) return -1;
#ifdef _WIN32
    const int result = _fseeki64(native, offset, origin);
#else
    static_assert(sizeof(off_t) == 8);
    const int result = ::fseeko(GetNativeStream(stream), offset, origin);
#endif
    const int nativeError = errno;
    stream->SyncStatus();
    if (result) errno = nativeError == EOVERFLOW ? 84 : nativeError;
    else stream->SetLowSurrogate(0);
    return result;
}

std::int64_t APS5_VABI ftello_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
#ifdef _WIN32
    auto* native = GetNativeStream(stream);
    if (IsPipeStream(native)) { errno = 29; return -1; }
    const auto result = _ftelli64(native);
#else
    static_assert(sizeof(off_t) == 8);
    const auto result = ::ftello(GetNativeStream(stream));
#endif
    if (result == -1 && errno == EOVERFLOW) errno = 84;
    return result;
}

int APS5_VABI fseek_nid_postfix(FileStream* stream, std::int64_t offset, int origin) {
    return fseeko_nid_postfix(stream, offset, origin);
}

std::int64_t APS5_VABI ftell_nid_postfix(FileStream* stream) { return ftello_nid_postfix(stream); }

int APS5_VABI fgetpos_nid_postfix(FileStream* stream, std::int64_t* position) {
    if (!position) throw std::invalid_argument("fgetpos: null position");
    const int savedErrno = errno;
    std::int64_t current = 0;
    if (!GuestCopyIn_nid_no_patch(&current, position, sizeof(current)) ||
        !GuestCopyOut_nid_no_patch(position, &current, sizeof(current))) {
        errno = savedErrno;
        throw std::invalid_argument("fgetpos: unwritable position");
    }
    errno = savedErrno;
    const auto result = ftello_nid_postfix(stream);
    if (result == -1) return -1;
    const int ftellErrno = errno;
    if (!GuestCopyOut_nid_no_patch(position, &result, sizeof(result))) {
        errno = ftellErrno;
        throw std::invalid_argument("fgetpos: unwritable position");
    }
    errno = ftellErrno;
    return 0;
}

}
