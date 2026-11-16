#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace {

struct GuestQueueElement {
    GuestQueueElement* q_forw;
    GuestQueueElement* q_back;
};

struct RestoreQueueErrno {
    const int saved = errno;
    ~RestoreQueueErrno() { errno = saved; }
};

void* QueueField(void* element, std::size_t offset, const char* function) {
    const auto address = reinterpret_cast<std::uintptr_t>(element);
    if (offset > std::numeric_limits<std::uintptr_t>::max() - address) NotImplemented_nid_no_patch(function);
    return reinterpret_cast<void*>(address + offset);
}

class QueueWrite {
    void* destination;
    std::size_t bytes;
    const char* function;
    GuestArena::HostWrite guard;

    static void* CheckedRange(void* destination, std::size_t bytes, const char* function) {
        const auto address = reinterpret_cast<std::uintptr_t>(destination);
        if (address == 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - address) NotImplemented_nid_no_patch(function);
        return destination;
    }
public:
    QueueWrite(void* destination, std::size_t bytes, const char* function)
        : destination(destination), bytes(bytes), function(function), guard(CheckedRange(destination, bytes, function), bytes) {
        if (!guard.Open() || !GuestWritable_nid_no_patch(destination, bytes)) NotImplemented_nid_no_patch(function);
    }
    void Store(const void* source) const {
        if (!GuestCopyOut_nid_no_patch(destination, source, bytes)) NotImplemented_nid_no_patch(function);
    }
};

}

extern "C" void APS5_VABI insque_nid_postfix(void* element, void* pred) {
    const RestoreQueueErrno restore;
    if (element == nullptr) NotImplemented_nid_no_patch(__func__);
    GuestQueueElement links{};
    if (pred != nullptr) {
        if (!GuestCopyIn_nid_no_patch(&links.q_forw, pred, sizeof(links.q_forw))) NotImplemented_nid_no_patch(__func__);
        links.q_back = static_cast<GuestQueueElement*>(pred);
    }
    const QueueWrite elementWrite(element, sizeof(links), __func__);
    std::optional<QueueWrite> nextWrite;
    std::optional<QueueWrite> previousWrite;
    if (links.q_forw != nullptr) nextWrite.emplace(QueueField(links.q_forw, offsetof(GuestQueueElement, q_back), __func__), sizeof(void*), __func__);
    if (pred != nullptr) previousWrite.emplace(pred, sizeof(void*), __func__);
    elementWrite.Store(&links);
    auto* const elem = static_cast<GuestQueueElement*>(element);
    if (nextWrite) nextWrite->Store(&elem);
    if (previousWrite) previousWrite->Store(&elem);
}

extern "C" void APS5_VABI remque_nid_postfix(void* element) {
    const RestoreQueueErrno restore;
    GuestQueueElement links{};
    if (!GuestCopyIn_nid_no_patch(&links, element, sizeof(links))) NotImplemented_nid_no_patch(__func__);
    std::optional<QueueWrite> nextWrite;
    std::optional<QueueWrite> previousWrite;
    if (links.q_forw != nullptr) nextWrite.emplace(QueueField(links.q_forw, offsetof(GuestQueueElement, q_back), __func__), sizeof(void*), __func__);
    if (links.q_back != nullptr) previousWrite.emplace(links.q_back, sizeof(void*), __func__);
    if (nextWrite) nextWrite->Store(&links.q_back);
    if (previousWrite) previousWrite->Store(&links.q_forw);
}
