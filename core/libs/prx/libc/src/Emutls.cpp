#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_map>

namespace {

struct AlignedDelete {
    std::size_t alignment;

    void operator()(void* pointer) const noexcept {
        ::operator delete(pointer, std::align_val_t(alignment));
    }
};

using Allocation = std::unique_ptr<void, AlignedDelete>;

std::mutex controlMutex;
std::uintptr_t nextIndex = 0;
thread_local std::unordered_map<std::uintptr_t, Allocation> instances;

}

extern "C" void* APS5_VABI __emutls_get_address_nid_postfix(GuestEmutlsControl* control) {
    if (control == nullptr) throw std::invalid_argument("__emutls_get_address: null control");
    const int savedErrno = errno;
    GuestEmutlsControl guest{};
    std::uintptr_t index;
    {
        std::lock_guard lock(controlMutex);
        if (!GuestCopyIn_nid_no_patch(&guest, control, sizeof(guest)))
            throw std::invalid_argument("__emutls_get_address: unreadable control");
        const auto alignment = std::max(guest.alignment, static_cast<std::uintptr_t>(sizeof(void*)));
        if (!std::has_single_bit(alignment))
            throw std::invalid_argument("__emutls_get_address: invalid alignment");
        index = guest.index;
        if (index == 0) {
            if (nextIndex == std::numeric_limits<std::uintptr_t>::max())
                throw std::overflow_error("__emutls_get_address: index overflow");
            index = nextIndex + 1;
            if (!GuestCopyOut_nid_no_patch(&control->index, &index, sizeof(index)))
                throw std::invalid_argument("__emutls_get_address: unwritable index");
            nextIndex = index;
        } else {
            nextIndex = std::max(nextIndex, index);
        }
    }

    const auto alignment = std::max(guest.alignment, static_cast<std::uintptr_t>(sizeof(void*)));

    if (const auto found = instances.find(index); found != instances.end()) {
        errno = savedErrno;
        return found->second.get();
    }

    Allocation block(::operator new(std::max(guest.size, std::uintptr_t{1}),
        std::align_val_t(alignment)), AlignedDelete{alignment});
    if (guest.initialValue != nullptr) {
        if (!GuestCopyIn_nid_no_patch(block.get(), guest.initialValue, guest.size))
            throw std::invalid_argument("__emutls_get_address: unreadable template");
    } else {
        std::memset(block.get(), 0, guest.size);
    }
    void* result = block.get();
    instances.emplace(index, std::move(block));
    errno = savedErrno;
    return result;
}
