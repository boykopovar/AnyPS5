#include <array>
#include <cerrno>
#include <cstdint>
#include <type_traits>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr int QueueSize = 16;

struct ErrorQueue {
    std::array<std::uint64_t, QueueSize> codes;
    std::array<const char*, QueueSize> files;
    std::array<int, QueueSize> lines;
    int top;
    int bottom;
};

static_assert(std::is_trivially_destructible_v<ErrorQueue>);

thread_local ErrorQueue queue{};

std::uint64_t Oldest(bool remove) {
    if (queue.bottom == queue.top) return 0;
    const int index = (queue.bottom + 1) % QueueSize;
    const std::uint64_t code = queue.codes[index];
    if (remove) {
        queue.bottom = index;
        queue.codes[index] = 0;
        queue.files[index] = nullptr;
        queue.lines[index] = 0;
    }
    return code;
}

void Clear() {
    queue = {};
}
}

extern "C" {

void LibreSslPutError_nid_no_patch(int library, int function, int reason, const char* file, int line) {
    const int savedError = errno;
    queue.top = (queue.top + 1) % QueueSize;
    if (queue.top == queue.bottom) queue.bottom = (queue.bottom + 1) % QueueSize;
    queue.codes[queue.top] = (static_cast<std::uint64_t>(library) & 0xff) << 24 |
                             (static_cast<std::uint64_t>(function) & 0xfff) << 12 |
                             (static_cast<std::uint64_t>(reason) & 0xfff);
    queue.files[queue.top] = file;
    queue.lines[queue.top] = line;
    errno = savedError;
}

std::uint64_t LibreSslPeekLastError_nid_no_patch() {
    return queue.top == queue.bottom ? 0 : queue.codes[queue.top];
}

std::uint64_t APS5_VABI ERR_get_error_nid_postfix() {
    return Oldest(true);
}

std::uint64_t APS5_VABI ERR_peek_error_nid_postfix() {
    return Oldest(false);
}

void APS5_VABI ERR_clear_error_nid_postfix() {
    Clear();
}

void APS5_VABI ERR_remove_thread_state_nid_postfix(const void* threadId) {
    if (threadId != nullptr) NotImplemented_nid_no_patch(__func__);
    Clear();
}

}
