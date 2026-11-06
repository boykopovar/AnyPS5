#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>

namespace {

using LibreSsl::Bio;
using LibreSsl::BioMethod;

int APS5_VABI FileWrite(Bio* bio, const char* data, int size) {
    if (!bio->data || size < 0 || (size && !data)) return -1;
    return static_cast<int>(fwrite_nid_postfix(data, 1, static_cast<std::size_t>(size), static_cast<FileStream*>(bio->data)));
}

int APS5_VABI FileRead(Bio* bio, char* data, int size) {
    if (!bio->data || size < 0 || (size && !data)) return -1;
    auto* stream = static_cast<FileStream*>(bio->data);
    const auto count = fread_nid_postfix(data, 1, static_cast<std::size_t>(size), stream);
    const int readError = errno;
    if (size > 0 && count == 0 && ferror_nid_postfix(stream) != 0) {
        LibreSslPutError_nid_no_patch(2, 0, readError, __FILE__, __LINE__);
        LibreSslPutError_nid_no_patch(32, 0, 2, __FILE__, __LINE__);
        return -1;
    }
    return static_cast<int>(count);
}

int APS5_VABI FilePuts(Bio* bio, const char* data) {
    if (!data) return -1;
    const std::size_t size = std::strlen(data);
    if (size > INT_MAX) return -1;
    return FileWrite(bio, data, static_cast<int>(size));
}

int APS5_VABI FileGets(Bio* bio, char* data, int size) {
    if (!data || size <= 0) return 0;
    int count = 0;
    while (count < size - 1) {
        char character = 0;
        if (FileRead(bio, &character, 1) != 1) break;
        data[count++] = character;
        if (character == '\n') break;
    }
    data[count] = '\0';
    return count;
}

std::int64_t APS5_VABI FileControl(Bio* bio, int command, std::int64_t argument, void* pointer) {
    auto* stream = static_cast<FileStream*>(bio->data);
    switch (command) {
    case 1:
        return stream ? fseek_nid_postfix(stream, 0, SEEK_SET) : -1;
    case 2:
        return stream ? feof_nid_postfix(stream) : 1;
    case 8:
        return bio->shutdown;
    case 9:
        bio->shutdown = argument != 0;
        return 1;
    case 10:
        return 0;
    case 11:
        return stream && fflush_nid_postfix(stream) == 0;
    case 12:
        return 1;
    case 107:
        if (pointer) *static_cast<FileStream**>(pointer) = stream;
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

int APS5_VABI SocketWrite(Bio* bio, const char* data, int size) {
    if (bio->number < 0 || size < 0 || (size && !data)) return -1;
    const auto result = sceNetSend(bio->number, data, static_cast<std::size_t>(size), 0);
    return result >= 0 && result <= INT_MAX ? static_cast<int>(result) : -1;
}

int APS5_VABI SocketRead(Bio* bio, char* data, int size) {
    if (bio->number < 0 || size < 0 || (size && !data)) return -1;
    const auto result = sceNetRecv(bio->number, data, static_cast<std::size_t>(size), 0);
    return result >= 0 && result <= INT_MAX ? static_cast<int>(result) : -1;
}

int APS5_VABI SocketPuts(Bio* bio, const char* data) {
    if (!data) return -1;
    const std::size_t size = std::strlen(data);
    if (size > INT_MAX) return -1;
    return SocketWrite(bio, data, static_cast<int>(size));
}

std::int64_t APS5_VABI SocketControl(Bio* bio, int command, std::int64_t argument, void*) {
    switch (command) {
    case 1:
        return 0;
    case 2:
        return 0;
    case 8:
        return bio->shutdown;
    case 9:
        bio->shutdown = argument != 0;
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
    bio->shutdown = closeFlag != 0;
    bio->initialized = 1;
    return bio;
}

LibreSsl::Bio* APS5_VABI BIO_new_socket_nid_postfix(int socket, int closeFlag) {
    if (socket < 0) return nullptr;
    auto* bio = BIO_new_nid_postfix(&SocketMethod);
    if (!bio) return nullptr;
    bio->number = socket;
    bio->shutdown = closeFlag != 0;
    bio->initialized = 1;
    return bio;
}

}
