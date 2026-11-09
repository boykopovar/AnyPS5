#include "prx/libc/include/CreationMask.hpp"

#include <atomic>

namespace {

std::atomic<std::uint16_t> creationMask{022};

}

namespace File {

std::uint16_t ExchangeCreationMask_nid_no_patch(std::uint16_t mask) {
    return creationMask.exchange(static_cast<std::uint16_t>(mask & 07777), std::memory_order_relaxed);
}

std::uint16_t GetCreationMask_nid_no_patch() {
    return creationMask.load(std::memory_order_relaxed);
}

}
