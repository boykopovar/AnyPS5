#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using LibreSsl::Bio;
using LibreSsl::BioMethod;

constexpr int BioRetryFlags = 0xf;
constexpr int BioRetryRead = 0x9;
constexpr int BioRetryWrite = 0xa;
constexpr int NetInterrupted = 4;
constexpr int NetWouldBlock = 35;
constexpr int NetInProgress = 36;
constexpr int NetAlready = 37;
constexpr int NetNotConnected = 57;
constexpr int BioClose = 0x01;
constexpr int BioFileRead = 0x02;
constexpr int BioFileWrite = 0x04;
constexpr int BioFileAppend = 0x08;
constexpr int BioControlInfo = 3;
constexpr int BioControlSetDescriptor = 104;
constexpr int BioControlGetDescriptor = 105;
constexpr int BioControlSetFilePointer = 106;
constexpr int BioControlSetFilename = 108;
constexpr int BioControlFileSeek = 128;
constexpr int BioControlFileTell = 133;

bool CopyGuestStringPreservingErrno(const char* source, std::string& destination) {
    constexpr std::uintptr_t Chunk = 4096;
    char bytes[Chunk];
    destination.clear();
    for (auto address = reinterpret_cast<std::uintptr_t>(source);;) {
        const std::size_t count = Chunk - address % Chunk;
        if (!GuestCopyIn_nid_no_patch(bytes, reinterpret_cast<const void*>(address), count)) return false;
        const auto* end = static_cast<const char*>(std::memchr(bytes, 0, count));
        destination.append(bytes, end ? static_cast<std::size_t>(end - bytes) : count);
        if (end) return true;
        address += count;
    }
}

int FileFree(Bio* bio) {
    if (bio->shutdown) {
        if (bio->initialized && bio->data != nullptr) {
            fclose_nid_postfix(static_cast<FileStream*>(bio->data));
            bio->data = nullptr;
            bio->flags = 0;
        }
        bio->initialized = 0;
    }
    return 1;
}

int APS5_VABI FileWrite(Bio* bio, const char* data, int size) {
    if (size < 0) return -1;
    if (!bio->initialized || !bio->data || !data) return 0;
    return static_cast<int>(fwrite_nid_postfix(data, 1, static_cast<std::size_t>(size), static_cast<FileStream*>(bio->data)));
}

int APS5_VABI FileRead(Bio* bio, char* data, int size) {
    if (size < 0) return -1;
    if (!bio->initialized || !bio->data || !data) return 0;
    auto* stream = static_cast<FileStream*>(bio->data);
    const auto count = fread_nid_postfix(data, 1, static_cast<std::size_t>(size), stream);
    const int readError = errno;
    if (size > 0 && count == 0 && ferror_nid_postfix(stream) != 0) {
        LibreSslPutError_nid_no_patch(2, 0xfff, readError, __FILE__, __LINE__);
        LibreSslPutError_nid_no_patch(32, 0xfff, 2, __FILE__, __LINE__);
        return -1;
    }
    return static_cast<int>(count);
}

int APS5_VABI FilePuts(Bio* bio, const char* data) {
    if (!data) return -1;
    std::string text;
    if (!CopyGuestStringPreservingErrno(data, text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (text.size() > INT_MAX) return -1;
    return FileWrite(bio, text.data(), static_cast<int>(text.size()));
}

int APS5_VABI FileGets(Bio* bio, char* data, int size) {
    if (!data || !bio->data) throw std::invalid_argument("BIO_gets: null line buffer or file stream");
    std::vector<char> buffer(size > 1 ? static_cast<std::size_t>(size) : 1);
    if (!GuestCopyIn_nid_no_patch(buffer.data(), data, buffer.size()) || !GuestCopyOut_nid_no_patch(data, buffer.data(), buffer.size()))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    buffer[0] = 0;
    const auto line = fgets_nid_postfix(buffer.data(), size, static_cast<FileStream*>(bio->data));
    if (!GuestCopyOut_nid_no_patch(data, buffer.data(), buffer.size())) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return line ? static_cast<int>(std::strlen(buffer.data())) : 0;
}

std::int64_t APS5_VABI FileControl(Bio* bio, int command, std::int64_t argument, void* pointer) {
    auto* stream = static_cast<FileStream*>(bio->data);
    switch (command) {
    case 1:
    case BioControlFileSeek:
        return stream ? fseek_nid_postfix(stream, argument, SEEK_SET) : -1;
    case BioControlInfo:
    case BioControlFileTell:
        return stream ? ftell_nid_postfix(stream) : -1;
    case 2:
        return stream ? feof_nid_postfix(stream) : 1;
    case 8:
        return bio->shutdown;
    case 9:
        bio->shutdown = static_cast<int>(argument);
        return 1;
    case 10:
        return 0;
    case 11:
        fflush_nid_postfix(stream);
        return 1;
    case 12:
        return 1;
    case BioControlSetFilename: {
        FileFree(bio);
        bio->shutdown = static_cast<int>(argument) & BioClose;

        const char* mode = nullptr;
        if ((argument & BioFileAppend) != 0) {
            mode = (argument & BioFileRead) != 0 ? "a+" : "a";
        } else if ((argument & (BioFileRead | BioFileWrite)) == (BioFileRead | BioFileWrite)) {
            mode = "r+";
        } else if ((argument & BioFileWrite) != 0) {
            mode = "w";
        } else if ((argument & BioFileRead) != 0) {
            mode = "r";
        }

        if (mode == nullptr) {
            LibreSslPutError_nid_no_patch(32, 0xfff, 101, __FILE__, __LINE__);
            return 0;
        }
        auto* opened = fopen_nid_postfix(static_cast<const char*>(pointer), mode);
        if (opened == nullptr) {
            const int openError = errno;
            LibreSslPutError_nid_no_patch(2, 0xfff, openError, __FILE__, __LINE__);
            LibreSslPutError_nid_no_patch(32, 0xfff, 2, __FILE__, __LINE__);
            return 0;
        }
        bio->data = opened;
        bio->initialized = 1;
        return 1;
    }
    case BioControlSetFilePointer:
        FileFree(bio);
        bio->shutdown = static_cast<int>(argument) & BioClose;
        bio->data = pointer;
        bio->initialized = 1;
        return 1;
    case 107:
        if (pointer && !GuestCopyOut_nid_no_patch(pointer, &stream, sizeof(stream))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return 1;
    default:
        return 0;
    }
}

int APS5_VABI FileDestroy(Bio* bio) {
    if (bio->shutdown && bio->initialized && bio->data)
        fclose_nid_postfix(static_cast<FileStream*>(bio->data));
    bio->data = nullptr;
    bio->initialized = 0;
    return 1;
}

bool SocketShouldRetry(std::int64_t result, int error) {
    if (result > 0) return false;
    switch (error) {
    case NetInterrupted:
    case NetWouldBlock:
    case NetInProgress:
    case NetAlready:
    case NetNotConnected:
        return true;
    default:
        return false;
    }
}

int APS5_VABI SocketWrite(Bio* bio, const char* data, int size) {
    if (bio->number < 0 || size < 0 || (size && !data)) return -1;
    int* error = sceNetErrnoLoc();
    *error = 0;
    const auto result = sceNetSend(bio->number, data, static_cast<std::size_t>(size), 0);
    bio->flags &= ~BioRetryFlags;
    if (SocketShouldRetry(result, *error)) bio->flags |= BioRetryWrite;
    return result >= 0 && result <= INT_MAX ? static_cast<int>(result) : -1;
}

int APS5_VABI SocketRead(Bio* bio, char* data, int size) {
    if (bio->number < 0 || size < 0 || (size && !data)) return -1;
    int* error = sceNetErrnoLoc();
    *error = 0;
    const auto result = sceNetRecv(bio->number, data, static_cast<std::size_t>(size), 0);
    bio->flags &= ~BioRetryFlags;
    if (SocketShouldRetry(result, *error)) bio->flags |= BioRetryRead;
    return result >= 0 && result <= INT_MAX ? static_cast<int>(result) : -1;
}

int APS5_VABI SocketPuts(Bio* bio, const char* data) {
    if (!data) return -1;
    std::string text;
    if (!CopyGuestStringPreservingErrno(data, text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (text.size() > INT_MAX) return -1;
    return SocketWrite(bio, text.data(), static_cast<int>(text.size()));
}

std::int64_t APS5_VABI SocketControl(Bio* bio, int command, std::int64_t argument, void* pointer) {
    switch (command) {
    case BioControlSetDescriptor: {
        int descriptor = 0;
        if (!pointer || !GuestCopyIn_nid_no_patch(&descriptor, pointer, sizeof(descriptor)))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        if (bio->shutdown) {
            if (bio->initialized && bio->number >= 0) sceNetSocketClose(bio->number);
            bio->initialized = 0;
            bio->flags = 0;
        }
        bio->number = descriptor;
        bio->shutdown = static_cast<int>(argument);
        bio->initialized = 1;
        return 1;
    }
    case BioControlGetDescriptor:
        if (!bio->initialized) return -1;
        if (pointer && !GuestCopyOut_nid_no_patch(pointer, &bio->number, sizeof(bio->number)))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return bio->number;
    case 1:
        return 0;
    case 2:
        return 0;
    case 8:
        return bio->shutdown;
    case 9:
        bio->shutdown = static_cast<int>(argument);
        return 1;
    case 10:
        return 0;
    case 11:
    case 12:
        return 1;
    default:
        return 0;
    }
}

int APS5_VABI SocketDestroy(Bio* bio) {
    if (bio->shutdown && bio->initialized && bio->number >= 0)
        sceNetSocketClose(bio->number);
    bio->number = -1;
    bio->initialized = 0;
    return 1;
}

const BioMethod FileMethod{0x402, "FILE pointer", FileWrite, FileRead, FilePuts, FileGets, FileControl, nullptr, FileDestroy, nullptr};
const BioMethod SocketMethod{0x405, "socket", SocketWrite, SocketRead, SocketPuts, nullptr, SocketControl, nullptr, SocketDestroy, nullptr};

}  // namespace

extern "C" {

const LibreSsl::BioMethod* APS5_VABI BIO_s_file_nid_postfix() {
    return &FileMethod;
}

LibreSsl::Bio* APS5_VABI BIO_new_fp_nid_postfix(void* value, int closeFlag) {
    if (!value) return nullptr;
    auto* bio = BIO_new_nid_postfix(&FileMethod);
    if (!bio) return nullptr;
    bio->data = value;
    bio->shutdown = closeFlag & BioClose;
    bio->initialized = 1;
    return bio;
}

LibreSsl::Bio* APS5_VABI BIO_new_socket_nid_postfix(int socket, int closeFlag) {
    auto* bio = BIO_new_nid_postfix(&SocketMethod);
    if (!bio) return nullptr;
    bio->number = socket;
    bio->shutdown = closeFlag;
    bio->initialized = 1;
    return bio;
}

}
