#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>


namespace {

using LibreSsl::Bio;
using LibreSsl::BioBuffer;
using LibreSsl::BioMethod;

constexpr int ReadOnly = 0x200;
constexpr int RetryFlags = 0xf;
constexpr int RetryRead = 0x9;
constexpr std::size_t MaximumGrowth = 0x5ffffffc;
constexpr int BioLibrary = 32;
constexpr int BufferLibrary = 7;
constexpr int AllocationFailure = 65;
constexpr int NullParameter = 115;
constexpr int Uninitialized = 120;
constexpr int UnsupportedMethod = 121;
constexpr int ReadOnlyWrite = 126;
constexpr int SslLibrary = 20;
constexpr int SslBioType = 0x207;
constexpr int SslBioSetSsl = 109;
constexpr int SslBioGetSsl = 110;
constexpr int SslBioGetRenegotiates = 126;
constexpr int SslBioGetClose = 8;
constexpr int SslBioSetClose = 9;
constexpr int SslBioInfo = 3;
constexpr int SslBioSetCallback = 14;

struct BioSslData {
    void* ssl;
    int renegotiates;
    std::uint64_t renegotiateCount;
    std::uint64_t byteCount;
    std::uint64_t renegotiateTimeout;
    std::int64_t lastTime;
};

static_assert(sizeof(BioSslData) == 48);
static_assert(offsetof(BioSslData, ssl) == 0);
static_assert(offsetof(BioSslData, renegotiates) == 8);
static_assert(offsetof(BioSslData, renegotiateCount) == 16);
static_assert(offsetof(BioSslData, byteCount) == 24);
static_assert(offsetof(BioSslData, renegotiateTimeout) == 32);
static_assert(offsetof(BioSslData, lastTime) == 40);

int queueError(int library, int reason, int result) {
    LibreSslPutError_nid_no_patch(library, 0xfff, reason, __FILE__, __LINE__);
    return result;
}

template <typename T>
void CopyGuestIn(T& destination, const T* source, const char* caller) {
    if (!GuestCopyIn_nid_no_patch(&destination, source, sizeof(T))) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

template <typename T>
void CopyGuestOut(T* destination, const T& source, const char* caller) {
    if (!GuestCopyOut_nid_no_patch(destination, &source, sizeof(T))) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void CopyGuestBytesIn(void* destination, const void* source, std::size_t bytes, const char* caller) {
    if (bytes != 0 && !GuestCopyIn_nid_no_patch(destination, source, bytes)) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void CopyGuestBytesOut(void* destination, const void* source, std::size_t bytes, const char* caller) {
    if (bytes != 0 && !GuestCopyOut_nid_no_patch(destination, source, bytes)) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void RequireGuestWritableBytes(void* guest, std::size_t bytes, const char* caller) {
    if (bytes != 0 && !GuestWritable_nid_no_patch(guest, bytes)) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

template <typename T>
void RequireGuestWritable(T* guest, const char* caller) {
    RequireGuestWritableBytes(guest, sizeof(T), caller);
}

template <typename T>
void ReadGuestField(const void* base, std::size_t offset, T& value, const char* caller) {
    CopyGuestBytesIn(&value, static_cast<const std::byte*>(base) + offset, sizeof(value), caller);
}

template <typename T>
void WriteGuestField(void* base, std::size_t offset, const T& value, const char* caller) {
    CopyGuestBytesOut(static_cast<std::byte*>(base) + offset, &value, sizeof(value), caller);
}

Bio ReadWritableBio(Bio* bio, const char* caller) {
    Bio guestBio{};
    CopyGuestIn(guestBio, bio, caller);
    RequireGuestWritable(bio, caller);
    return guestBio;
}

BioMethod ReadBioMethod(const BioMethod* method, const char* caller) {
    BioMethod guestMethod{};
    CopyGuestIn(guestMethod, method, caller);
    return guestMethod;
}

std::mutex referenceMutex;

int DecrementReferences(Bio* bio, const char* caller) {
    std::lock_guard<std::mutex> lock(referenceMutex);
    int references = 0;
    ReadGuestField(bio, offsetof(Bio, references), references, caller);
    const int updated = static_cast<int>(static_cast<unsigned>(references) - 1u);
    WriteGuestField(bio, offsetof(Bio, references), updated, caller);
    return references;
}

void AddBytesWritten(Bio* bio, std::uint64_t delta, const char* caller) {
    std::uint64_t bytesWritten = 0;
    ReadGuestField(bio, offsetof(Bio, bytesWritten), bytesWritten, caller);
    bytesWritten += delta;
    WriteGuestField(bio, offsetof(Bio, bytesWritten), bytesWritten, caller);
}

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

void ZeroGuestBytes(void* destination, std::size_t bytes, const char* caller) {
    constexpr std::array<unsigned char, 4096> Zeros{};
    auto* cursor = static_cast<unsigned char*>(destination);
    std::size_t remaining = bytes;
    while (remaining != 0) {
        const std::size_t chunk = std::min(remaining, Zeros.size());
        CopyGuestBytesOut(cursor, Zeros.data(), chunk, caller);
        cursor += chunk;
        remaining -= chunk;
    }
}

void clearBuffer(const BioBuffer& buffer, const char* caller) {
    if (!buffer.data || buffer.capacity == 0) return;
    RequireGuestWritableBytes(buffer.data, buffer.capacity, caller);
    ZeroGuestBytes(buffer.data, buffer.capacity, caller);
}

template <typename TData>
struct GuestRecord {
    TData* address = nullptr;
    TData value{};
};

template <typename TData>
GuestRecord<TData> ReadWritableRecord(Bio* bio, Bio& guestBio, const char* missingMessage, const char* caller) {
    if (!bio) throw std::invalid_argument(missingMessage);
    guestBio = ReadWritableBio(bio, caller);
    if (!guestBio.data) throw std::invalid_argument(missingMessage);
    auto* const address = static_cast<TData*>(guestBio.data);
    TData record{};
    CopyGuestIn(record, address, caller);
    RequireGuestWritable(address, caller);
    return {address, record};
}

GuestRecord<BioBuffer> ReadMemoryBuffer(Bio* bio, Bio& guestBio, const char* caller) {
    return ReadWritableRecord<BioBuffer>(bio, guestBio, "BIO: missing memory buffer", caller);
}

GuestRecord<BioSslData> ReadSslData(Bio* bio, Bio& guestBio, const char* caller) {
    return ReadWritableRecord<BioSslData>(bio, guestBio, "SSL BIO: missing state", caller);
}

int APS5_VABI createMemory(Bio* bio) {
    if (!bio) throw std::invalid_argument("BIO: missing memory buffer");
    Bio guestBio = ReadWritableBio(bio, __func__);
    auto* buffer = static_cast<BioBuffer*>(calloc_nid_postfix(1, sizeof(BioBuffer)));
    if (!buffer) return queueError(BufferLibrary, AllocationFailure, 0);
    guestBio.data = buffer;
    guestBio.initialized = 1;
    guestBio.shutdown = 1;
    guestBio.number = -1;
    if (!GuestCopyOut_nid_no_patch(bio, &guestBio, sizeof(guestBio))) {
        free_nid_postfix(buffer);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return 1;
}

int APS5_VABI destroyMemory(Bio* bio) {
    if (!bio) return 0;
    const Bio guestBio = ReadWritableBio(bio, __func__);
    if (guestBio.shutdown && guestBio.initialized && guestBio.data) {
        Bio currentBio{};
        const auto state = ReadMemoryBuffer(bio, currentBio, __func__);
        if (!(currentBio.flags & ReadOnly)) {
            clearBuffer(state.value, __func__);
            free_nid_postfix(state.value.data);
        }
        WriteGuestField(bio, offsetof(Bio, data), static_cast<void*>(nullptr), __func__);
        free_nid_postfix(state.address);
    }
    return 1;
}

int APS5_VABI readSsl(Bio*, char* destination, int) {
    if (!destination) return 0;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI writeSsl(Bio*, const char* source, int) {
    if (!source) return 0;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI putsSsl(Bio* bio, const char* source) {
    if (!source) throw std::invalid_argument("SSL BIO puts: null string");
    std::string text;
    if (!CopyGuestStringPreservingErrno(source, text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return writeSsl(bio, text.c_str(), static_cast<int>(text.size()));
}

std::int64_t APS5_VABI controlSsl(Bio* bio, int command, std::int64_t argument, void* pointer) {
    Bio guestBio{};
    const auto state = ReadSslData(bio, guestBio, __func__);
    if (!state.value.ssl && command != SslBioSetSsl) return 0;
    switch (command) {
    case SslBioSetSsl:
        if (state.value.ssl || pointer) NotImplemented_nid_no_patch(__func__);
        WriteGuestField(bio, offsetof(Bio, shutdown), static_cast<int>(argument), __func__);
        WriteGuestField(state.address, offsetof(BioSslData, ssl), static_cast<void*>(nullptr), __func__);
        throw std::invalid_argument("SSL BIO control: null SSL");
    case SslBioInfo:
        return 0;
    case SslBioGetSsl:
        if (!pointer) return 0;
        CopyGuestBytesOut(pointer, &state.value.ssl, sizeof(state.value.ssl), __func__);
        return 1;
    case SslBioGetClose:
        return guestBio.shutdown;
    case SslBioSetClose:
        WriteGuestField(bio, offsetof(Bio, shutdown), static_cast<int>(argument), __func__);
        return 1;
    case SslBioGetRenegotiates:
        return state.value.renegotiates;
    default:
        NotImplemented_nid_no_patch(__func__);
    }
}

int APS5_VABI createSsl(Bio* bio) {
    if (!bio) throw std::invalid_argument("SSL BIO: missing state");
    Bio guestBio = ReadWritableBio(bio, __func__);
    auto* state = static_cast<BioSslData*>(calloc_nid_postfix(1, sizeof(BioSslData)));
    if (!state) return queueError(SslLibrary, AllocationFailure, 0);
    guestBio.initialized = 0;
    guestBio.data = state;
    guestBio.flags = 0;
    if (!GuestCopyOut_nid_no_patch(bio, &guestBio, sizeof(guestBio))) {
        free_nid_postfix(state);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return 1;
}

int APS5_VABI destroySsl(Bio* bio) {
    if (!bio) return 0;
    Bio guestBio{};
    const auto state = ReadSslData(bio, guestBio, __func__);
    if (state.value.ssl) NotImplemented_nid_no_patch(__func__);
    if (guestBio.shutdown) {
        guestBio.initialized = 0;
        guestBio.flags = 0;
        CopyGuestOut(bio, guestBio, __func__);
    }
    free_nid_postfix(state.address);
    return 1;
}

template <typename TMethod, typename TInvoker>
std::int64_t DispatchControl(Bio* bio, int command, std::int64_t argument, const char* callbackPointer,
                             TMethod BioMethod::* methodField, TInvoker invoke, const char* caller) {
    if (!bio) return 0;
    const Bio guestBio = ReadWritableBio(bio, caller);
    if (!guestBio.method) return queueError(BioLibrary, UnsupportedMethod, -2);
    const BioMethod guestMethod = ReadBioMethod(guestBio.method, caller);
    const auto methodFunction = guestMethod.*methodField;
    if (!methodFunction) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto callback = guestBio.callback;
    if (callback) {
        const auto result = callback(bio, 6, callbackPointer, command, argument, 1);
        if (result <= 0) return result;
    }
    const auto result = invoke(methodFunction, bio);
    return callback ? callback(bio, 0x86, callbackPointer, command, argument, result) : result;
}

template <typename TMethod, typename TInvoker>
int DispatchOutput(Bio* bio, int operation, const char* source, int size,
                   TMethod BioMethod::* methodField, TInvoker invoke, const char* caller) {
    Bio guestBio = ReadWritableBio(bio, caller);
    if (!guestBio.method) return queueError(BioLibrary, UnsupportedMethod, -2);
    BioMethod guestMethod = ReadBioMethod(guestBio.method, caller);
    auto methodFunction = guestMethod.*methodField;
    if (!methodFunction) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto callback = guestBio.callback;
    if (callback) {
        const int result = static_cast<int>(callback(bio, operation, source, size, 0, 1));
        if (result <= 0) return result;
        guestBio = ReadWritableBio(bio, caller);
        if (!guestBio.method) return queueError(BioLibrary, UnsupportedMethod, -2);
        guestMethod = ReadBioMethod(guestBio.method, caller);
        methodFunction = guestMethod.*methodField;
        if (!methodFunction) return queueError(BioLibrary, UnsupportedMethod, -2);
    }
    if (!guestBio.initialized) return queueError(BioLibrary, Uninitialized, -2);
    const int result = invoke(methodFunction, bio);
    if (result > 0) AddBytesWritten(bio, static_cast<std::uint64_t>(result), caller);
    return callback ? static_cast<int>(callback(bio, operation | 0x80, source, size, 0, result)) : result;
}

std::int64_t forwardCallbackControl(Bio* bio, int command, LibreSsl::BioInfoCallback callback) {
    return DispatchControl(bio, command, 0, reinterpret_cast<const char*>(&callback),
        &BioMethod::callbackControl,
        [command, callback](auto fn, Bio* target) { return fn(target, command, callback); },
        __func__);
}

std::int64_t APS5_VABI callbackControlSsl(Bio* bio, int command, LibreSsl::BioInfoCallback callback) {
    Bio guestBio{};
    const auto state = ReadSslData(bio, guestBio, __func__);
    if (!state.value.ssl) throw std::invalid_argument("SSL BIO callback control: null SSL");
    LibreSslSsl ssl{};
    CopyGuestIn(ssl, static_cast<const LibreSslSsl*>(state.value.ssl), __func__);
    if (command != SslBioSetCallback) return forwardCallbackControl(static_cast<Bio*>(ssl.rbio), command, callback);
    if (!ssl.internal) throw std::invalid_argument("SSL BIO callback control: null internal state");
    const auto value = reinterpret_cast<decltype(ssl.internal->infoCallback)>(callback);
    WriteGuestField(ssl.internal, offsetof(LibreSslSslInternal, infoCallback), value, __func__);
    return 1;
}

void consumeMemory(const Bio& guestBio, GuestRecord<BioBuffer>& state, std::size_t count, const char* caller) {
    state.value.length -= count;
    if (guestBio.flags & ReadOnly) {
        state.value.data += count;
    } else if (state.value.length != 0) {
        std::vector<char> remaining(state.value.length);
        CopyGuestBytesIn(remaining.data(), state.value.data + count, remaining.size(), caller);
        CopyGuestBytesOut(state.value.data, remaining.data(), remaining.size(), caller);
    }
    CopyGuestOut(state.address, state.value, caller);
}

int APS5_VABI readMemory(Bio* bio, char* destination, int size) {
    Bio guestBio{};
    auto state = ReadMemoryBuffer(bio, guestBio, __func__);
    guestBio.flags &= ~RetryFlags;
    WriteGuestField(bio, offsetof(Bio, flags), guestBio.flags, __func__);
    const int count = size >= 0 ? static_cast<int>(std::min<std::size_t>(size, state.value.length)) : size;
    if (destination && count > 0) {
        std::vector<char> bytes(static_cast<std::size_t>(count));
        CopyGuestBytesIn(bytes.data(), state.value.data, bytes.size(), __func__);
        CopyGuestBytesOut(destination, bytes.data(), bytes.size(), __func__);
        consumeMemory(guestBio, state, static_cast<std::size_t>(count), __func__);
        return count;
    }
    if (state.value.length != 0) return count;
    if (guestBio.number != 0) {
        guestBio.flags |= RetryRead;
        WriteGuestField(bio, offsetof(Bio, flags), guestBio.flags, __func__);
    }
    return guestBio.number;
}

int APS5_VABI writeMemory(Bio* bio, const char* source, int size) {
    Bio guestBio{};
    auto state = ReadMemoryBuffer(bio, guestBio, __func__);
    if (!source) return queueError(BioLibrary, NullParameter, -1);
    if (guestBio.flags & ReadOnly) return queueError(BioLibrary, ReadOnlyWrite, -1);
    if (size < 0) throw std::invalid_argument("BIO: negative memory write size");
    guestBio.flags &= ~RetryFlags;
    WriteGuestField(bio, offsetof(Bio, flags), guestBio.flags, __func__);
    const auto count = static_cast<std::size_t>(size);
    if (state.value.length > std::numeric_limits<std::size_t>::max() - count) NotImplemented_nid_no_patch(__func__);
    const std::size_t length = state.value.length + count;
    std::vector<char> incoming(count);
    if (length > state.value.capacity) {
        if (length > MaximumGrowth) return queueError(BufferLibrary, AllocationFailure, -1);
        const std::size_t capacity = (length + 3) / 3 * 4;
        auto* data = static_cast<char*>(calloc_nid_postfix(capacity, 1));
        if (!data) return queueError(BufferLibrary, AllocationFailure, -1);
        const BioBuffer previous = state.value;
        std::vector<char> prefix(previous.length);
        if ((count != 0 && !GuestCopyIn_nid_no_patch(incoming.data(), source, count)) ||
            (previous.length != 0 && !GuestCopyIn_nid_no_patch(prefix.data(), previous.data, previous.length)) ||
            (previous.data && previous.capacity != 0 && !GuestWritable_nid_no_patch(previous.data, previous.capacity))) {
            free_nid_postfix(data);
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
        if ((prefix.size() != 0 && !GuestCopyOut_nid_no_patch(data, prefix.data(), prefix.size())) ||
            (incoming.size() != 0 && !GuestCopyOut_nid_no_patch(data + previous.length, incoming.data(), incoming.size()))) {
            ZeroGuestBytes(data, capacity, __func__);
            free_nid_postfix(data);
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
        try {
            clearBuffer(previous, __func__);
        } catch (...) {
            ZeroGuestBytes(data, capacity, __func__);
            free_nid_postfix(data);
            throw;
        }
        state.value.data = data;
        state.value.capacity = capacity;
        state.value.length = length;
        try {
            CopyGuestOut(state.address, state.value, __func__);
        } catch (...) {
            ZeroGuestBytes(data, capacity, __func__);
            free_nid_postfix(data);
            throw;
        }
        free_nid_postfix(previous.data);
        return size;
    } else if (count != 0) {
        CopyGuestBytesIn(incoming.data(), source, count, __func__);
        CopyGuestBytesOut(state.value.data + state.value.length, incoming.data(), count, __func__);
    }
    state.value.length = length;
    CopyGuestOut(state.address, state.value, __func__);
    return size;
}

int APS5_VABI putsMemory(Bio* bio, const char* source) {
    if (!source) throw std::invalid_argument("BIO_puts: null string");
    std::string text;
    if (!CopyGuestStringPreservingErrno(source, text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return writeMemory(bio, text.data(), static_cast<int>(text.size()));
}

int APS5_VABI getsMemory(Bio* bio, char* destination, int size) {
    Bio guestBio{};
    auto state = ReadMemoryBuffer(bio, guestBio, __func__);
    if (!destination || size == std::numeric_limits<int>::min()) throw std::invalid_argument("BIO: invalid line buffer");
    guestBio.flags &= ~RetryFlags;
    WriteGuestField(bio, offsetof(Bio, flags), guestBio.flags, __func__);
    constexpr char Terminator = '\0';
    if (size <= 1 || state.value.length == 0) {
        CopyGuestBytesOut(destination, &Terminator, 1, __func__);
        return 0;
    }
    const auto limit = std::min<std::size_t>(size - 1, state.value.length);
    std::vector<char> line(limit + 1);
    CopyGuestBytesIn(line.data(), state.value.data, limit, __func__);
    const void* newline = std::memchr(line.data(), '\n', limit);
    const std::size_t count = newline ? static_cast<const char*>(newline) - line.data() + 1 : limit;
    line[count] = Terminator;
    CopyGuestBytesOut(destination, line.data(), count + 1, __func__);
    consumeMemory(guestBio, state, count, __func__);
    return static_cast<int>(count);
}

std::int64_t APS5_VABI controlMemory(Bio* bio, int command, std::int64_t argument, void* pointer) {
    if (!bio) throw std::invalid_argument("BIO: null memory BIO");
    const Bio guestBio = ReadWritableBio(bio, __func__);
    switch (command) {
    case 1: {
        Bio currentBio{};
        auto state = ReadMemoryBuffer(bio, currentBio, __func__);
        if (state.value.data) {
            if (currentBio.flags & ReadOnly) {
                if (state.value.capacity < state.value.length) throw std::invalid_argument("BIO: invalid read-only buffer length");
                state.value.data -= state.value.capacity - state.value.length;
                state.value.length = state.value.capacity;
            } else {
                clearBuffer(state.value, __func__);
                state.value.length = 0;
            }
            CopyGuestOut(state.address, state.value, __func__);
        }
        return 1;
    }
    case 2: {
        Bio currentBio{};
        return ReadMemoryBuffer(bio, currentBio, __func__).value.length == 0;
    }
    case 3: {
        Bio currentBio{};
        const auto state = ReadMemoryBuffer(bio, currentBio, __func__);
        if (pointer) CopyGuestBytesOut(pointer, &state.value.data, sizeof(char*), __func__);
        return static_cast<std::int64_t>(state.value.length);
    }
    case 8:
        return guestBio.shutdown;
    case 9:
        WriteGuestField(bio, offsetof(Bio, shutdown), static_cast<int>(argument), __func__);
        return 1;
    case 10: {
        Bio currentBio{};
        return static_cast<std::int64_t>(ReadMemoryBuffer(bio, currentBio, __func__).value.length);
    }
    case 11:
    case 12:
        return 1;
    case 13:
        return 0;
    case 114:
        destroyMemory(bio);
        WriteGuestField(bio, offsetof(Bio, data), pointer, __func__);
        WriteGuestField(bio, offsetof(Bio, shutdown), static_cast<int>(argument), __func__);
        return 1;
    case 115:
        if (pointer) CopyGuestBytesOut(pointer, &guestBio.data, sizeof(guestBio.data), __func__);
        return 1;
    case 130:
        WriteGuestField(bio, offsetof(Bio, number), static_cast<int>(argument), __func__);
        return 1;
    default:
        return 0;
    }
}

const BioMethod MemoryMethod{0x401, "memory buffer", writeMemory, readMemory, putsMemory, getsMemory, controlMemory, createMemory, destroyMemory, nullptr};
const BioMethod SslMethod{SslBioType, "ssl", writeSsl, readSsl, putsSsl, nullptr, controlSsl, createSsl, destroySsl, callbackControlSsl};

Bio* NewBio(const BioMethod* method, const char* caller) {
    if (!method) throw std::invalid_argument("BIO_new: null method");
    const BioMethod guestMethod = ReadBioMethod(method, caller);
    auto* bio = static_cast<Bio*>(calloc_nid_postfix(1, sizeof(Bio)));
    if (!bio) {
        queueError(BioLibrary, AllocationFailure, 0);
        return nullptr;
    }
    Bio initial{};
    initial.method = method;
    initial.shutdown = 1;
    initial.references = 1;
    if (!GuestCopyOut_nid_no_patch(bio, &initial, sizeof(initial))) {
        free_nid_postfix(bio);
        NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
    }
    try {
        if (!guestMethod.create || guestMethod.create(bio)) return bio;
    } catch (...) {
        free_nid_postfix(bio);
        throw;
    }
    free_nid_postfix(bio);
    return nullptr;
}

int FreeBio(Bio* bio, const char* caller) {
    if (!bio) return 0;
    Bio guestBio = ReadWritableBio(bio, caller);
    if (guestBio.extraData) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
    if (guestBio.references <= 1 && !guestBio.callback && guestBio.method)
        ReadBioMethod(guestBio.method, caller);
    if (DecrementReferences(bio, caller) > 1) return 1;
    if (guestBio.callback) {
        const int result = static_cast<int>(guestBio.callback(bio, 1, nullptr, 0, 0, 1));
        if (result <= 0) return result;
        guestBio = ReadWritableBio(bio, caller);
        if (guestBio.extraData) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
    }
    if (guestBio.method) {
        const BioMethod guestMethod = ReadBioMethod(guestBio.method, caller);
        if (guestMethod.destroy) guestMethod.destroy(bio);
    }
    free_nid_postfix(bio);
    return 1;
}

std::int64_t ControlBio(Bio* bio, int command, std::int64_t argument, void* pointer, const char* caller) {
    return DispatchControl(bio, command, argument, static_cast<const char*>(pointer),
        &BioMethod::control,
        [command, argument, pointer](auto fn, Bio* target) { return fn(target, command, argument, pointer); },
        caller);
}

int PutsBio(Bio* bio, const char* source, const char* caller) {
    if (!bio) return queueError(BioLibrary, UnsupportedMethod, -2);
    return DispatchOutput(bio, 4, source, 0, &BioMethod::puts,
        [source](auto fn, Bio* target) { return fn(target, source); },
        caller);
}

}  // namespace

extern "C" {

const LibreSsl::BioMethod* APS5_VABI BIO_s_mem_nid_postfix() {
    return &MemoryMethod;
}

const LibreSsl::BioMethod* APS5_VABI BIO_f_ssl_nid_postfix() {
    return &SslMethod;
}

LibreSsl::Bio* APS5_VABI BIO_new_nid_postfix(const LibreSsl::BioMethod* method) {
    return NewBio(method, __func__);
}

LibreSsl::Bio* APS5_VABI BIO_new_mem_buf_nid_postfix(const void* source, int size) {
    if (!source) {
        queueError(BioLibrary, NullParameter, 0);
        return nullptr;
    }
    auto length = static_cast<std::size_t>(size);
    if (size < 0) {
        std::string text;
        if (!CopyGuestStringPreservingErrno(static_cast<const char*>(source), text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        length = text.size();
    }
    auto* bio = NewBio(&MemoryMethod, __func__);
    if (!bio) return nullptr;
    Bio guestBio{};
    auto state = ReadMemoryBuffer(bio, guestBio, __func__);
    state.value.data = const_cast<char*>(static_cast<const char*>(source));
    state.value.length = length;
    state.value.capacity = state.value.length;
    CopyGuestOut(state.address, state.value, __func__);
    guestBio.flags |= ReadOnly;
    guestBio.number = 0;
    CopyGuestOut(bio, guestBio, __func__);
    return bio;
}

int APS5_VABI BIO_free_nid_postfix(LibreSsl::Bio* bio) {
    return FreeBio(bio, __func__);
}

std::int64_t APS5_VABI BIO_ctrl_nid_postfix(LibreSsl::Bio* bio, int command, std::int64_t argument, void* pointer) {
    return ControlBio(bio, command, argument, pointer, __func__);
}

int LibreSslBioWrite_nid_no_patch(LibreSsl::Bio* bio, const void* source, int size) {
    if (!bio || size < 0) return 0;
    return DispatchOutput(bio, 3, static_cast<const char*>(source), size, &BioMethod::write,
        [source, size](auto fn, Bio* target) { return fn(target, static_cast<const char*>(source), size); },
        __func__);
}

int APS5_VABI BIO_puts_nid_postfix(LibreSsl::Bio* bio, const char* source) {
    return PutsBio(bio, source, __func__);
}

}
