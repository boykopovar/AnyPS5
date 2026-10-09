#include "prx/libc/include/general/VabiMacros.hpp"
#include "zlib.h"
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace {

static_assert(sizeof(std::uint32_t) == 4 && sizeof(std::uint64_t) == 8 && sizeof(std::int32_t) == 4);

constexpr std::int32_t NOT_INITIALIZED = static_cast<std::int32_t>(0x81120032);
constexpr std::int32_t ALREADY_INITIALIZED = static_cast<std::int32_t>(0x81120033);
constexpr std::int32_t NO_SPACE = static_cast<std::int32_t>(0x8112001C);
constexpr std::int32_t FATAL = static_cast<std::int32_t>(0x811200FF);

struct Result {
    std::uint64_t id;
    std::uint32_t produced;
    std::int32_t status;
    bool retrieved = false;
};

std::mutex serviceMutex;
bool initialized = false;
std::uint64_t nextId = 1;
std::optional<Result> result;

} // namespace

extern "C" {

std::int32_t APS5_VABI sceZlibInitialize(const void* buffer, std::uint64_t length) {
    std::lock_guard lock(serviceMutex);
    if (initialized) return ALREADY_INITIALIZED;
    if ((buffer == nullptr) != (length == 0)) throw std::runtime_error("sceZlibInitialize: invalid work buffer");
    initialized = true;
    return 0;
}

std::int32_t APS5_VABI sceZlibFinalize() {
    std::lock_guard lock(serviceMutex);
    if (!initialized) return NOT_INITIALIZED;
    if (result && !result->retrieved) throw std::runtime_error("sceZlibFinalize: outstanding result semantics unknown");
    result.reset();
    initialized = false;
    return 0;
}

std::int32_t APS5_VABI sceZlibInflate(const void* source, std::uint32_t sourceLength,
                                     void* destination, std::uint32_t destinationLength,
                                     std::uint64_t* requestId) {
    std::lock_guard lock(serviceMutex);
    if (!initialized) return NOT_INITIALIZED;
    if (!source || !sourceLength || !destination || !requestId || !destinationLength ||
        destinationLength > 65536 || reinterpret_cast<std::uintptr_t>(destination) % 2048 != 0)
        throw std::runtime_error("sceZlibInflate: invalid buffer or size");
    if (result && !result->retrieved) throw std::runtime_error("sceZlibInflate: multiple outstanding requests unsupported");
    if (nextId == std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("sceZlibInflate: request IDs exhausted");
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(static_cast<const Bytef*>(source));
    stream.avail_in = sourceLength;
    stream.next_out = static_cast<Bytef*>(destination);
    stream.avail_out = destinationLength;
    if (inflateInit(&stream) != Z_OK) throw std::runtime_error("sceZlibInflate: decoder initialization failed");
    const int decoded = inflate(&stream, Z_FINISH);
    const auto produced = static_cast<std::uint32_t>(stream.total_out);
    const std::int32_t status = decoded == Z_STREAM_END ? 0 :
        (decoded == Z_BUF_ERROR && stream.avail_out == 0 ? NO_SPACE : FATAL);
    const int ended = inflateEnd(&stream);
    if (ended != Z_OK) throw std::runtime_error("sceZlibInflate: decoder cleanup failed");
    result = Result{nextId++, produced, status};
    *requestId = result->id;
    return 0;
}

std::int32_t APS5_VABI sceZlibWaitForDone(std::uint64_t* requestId, std::uint32_t* timeout) {
    std::lock_guard lock(serviceMutex);
    if (!initialized) return NOT_INITIALIZED;
    if (timeout) throw std::runtime_error("sceZlibWaitForDone: timeout semantics unknown");
    if (!requestId || !result || result->retrieved) throw std::runtime_error("sceZlibWaitForDone: no outstanding request");
    *requestId = result->id;
    return 0;
}

std::int32_t APS5_VABI sceZlibGetResult(std::uint64_t requestId, std::uint32_t* destinationLength,
                                       std::int32_t* status) {
    std::lock_guard lock(serviceMutex);
    if (!initialized) return NOT_INITIALIZED;
    if (!destinationLength || !status || !result || result->id != requestId)
        throw std::runtime_error("sceZlibGetResult: invalid request or output");
    if (result->retrieved) throw std::runtime_error("sceZlibGetResult: repeated retrieval semantics unknown");
    *destinationLength = result->produced;
    *status = result->status;
    result->retrieved = true;
    return 0;
}

}
