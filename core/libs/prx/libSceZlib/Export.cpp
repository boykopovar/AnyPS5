#include "prx/libc/include/general/VabiMacros.hpp"
#include "zlib.h"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace {

static_assert(sizeof(std::uint32_t) == 4 && sizeof(std::uint64_t) == 8 && sizeof(std::int32_t) == 4);

constexpr std::int32_t NOT_FOUND = static_cast<std::int32_t>(0x81120002);
constexpr std::int32_t NOT_INITIALIZED = static_cast<std::int32_t>(0x81120032);
constexpr std::int32_t ALREADY_INITIALIZED = static_cast<std::int32_t>(0x81120033);
constexpr std::int32_t TIMED_OUT = static_cast<std::int32_t>(0x81120027);
constexpr std::int32_t NO_SPACE = static_cast<std::int32_t>(0x8112001C);
constexpr std::int32_t INVALID_ARGUMENT = static_cast<std::int32_t>(0x81120016);
constexpr std::int32_t FATAL = static_cast<std::int32_t>(0x811200FF);

struct Result {
    std::uint32_t produced;
    std::int32_t status;
};

std::mutex serviceMutex;
bool initialized = false;
std::uint64_t nextId = 1;
std::uint64_t session = 0;
std::unordered_map<std::uint64_t, Result> results;
std::deque<std::uint64_t> completed;
std::condition_variable completion;

} // namespace

extern "C" {

std::int32_t APS5_VABI sceZlibInitialize(const void* buffer, std::uint64_t length) {
    std::lock_guard lock(serviceMutex);
    if (initialized) return ALREADY_INITIALIZED;
    if ((buffer == nullptr) != (length == 0)) return INVALID_ARGUMENT;
    if (buffer) throw std::runtime_error("sceZlibInitialize: work memory registration is not implemented");
    initialized = true;
    return 0;
}

std::int32_t APS5_VABI sceZlibFinalize() {
    std::lock_guard lock(serviceMutex);
    if (!initialized) return NOT_INITIALIZED;
    results.clear();
    completed.clear();
    initialized = false;
    ++session;
    completion.notify_all();
    return 0;
}

std::int32_t APS5_VABI sceZlibInflate(const void* source, std::uint32_t sourceLength, void* destination, std::uint32_t destinationLength, std::uint64_t* requestId) {
    std::lock_guard lock(serviceMutex);
    if (!initialized) return NOT_INITIALIZED;
    if (!source || !sourceLength || !destination || !requestId || !destinationLength ||
        destinationLength > 65536 || destinationLength % 2048 != 0) return INVALID_ARGUMENT;
    if (reinterpret_cast<std::uintptr_t>(destination) % 2048 != 0)
        throw std::runtime_error("sceZlibInflate: unaligned destination driver behavior is not implemented");
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
        (decoded == Z_BUF_ERROR && stream.avail_out == 0 && stream.avail_in != 0 ? NO_SPACE : FATAL);
    const int ended = inflateEnd(&stream);
    if (ended != Z_OK) throw std::runtime_error("sceZlibInflate: decoder cleanup failed");
    const std::uint64_t id = nextId++;
    results[id] = Result{produced, status};
    completed.push_back(id);
    completion.notify_one();
    *requestId = id;
    return 0;
}

std::int32_t APS5_VABI sceZlibWaitForDone(std::uint64_t* requestId, std::uint32_t* timeout) {
    std::unique_lock lock(serviceMutex);
    if (!initialized) return NOT_INITIALIZED;
    if (!requestId) return INVALID_ARGUMENT;
    const std::uint64_t waitSession = session;
    const auto finished = [waitSession] { return !completed.empty() || session != waitSession; };
    if (!timeout) completion.wait(lock, finished);
    else if (!completion.wait_for(lock, std::chrono::microseconds(*timeout), finished)) return TIMED_OUT;
    if (session != waitSession) throw std::runtime_error("sceZlibWaitForDone: finalization during a wait semantics unknown");
    *requestId = completed.front();
    completed.pop_front();
    return 0;
}

std::int32_t APS5_VABI sceZlibGetResult(std::uint64_t requestId, std::uint32_t* destinationLength, std::int32_t* status) {
    std::lock_guard lock(serviceMutex);
    if (!initialized) return NOT_INITIALIZED;
    if (!destinationLength || !status) return INVALID_ARGUMENT;
    const auto found = results.find(requestId);
    if (found == results.end()) return NOT_FOUND;
    *destinationLength = found->second.produced;
    *status = found->second.status;
    return 0;
}

}
