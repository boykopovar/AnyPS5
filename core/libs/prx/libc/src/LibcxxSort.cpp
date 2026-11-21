#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

extern "C" void APS5_VABI _ZNSt3__16__sortIRNS_6__lessIjjEEPjEEvT0_S5_T__nid_postfix(std::uint32_t* first, std::uint32_t* last, const void*) {
    const auto begin = reinterpret_cast<std::uintptr_t>(first);
    const auto end = reinterpret_cast<std::uintptr_t>(last);
    if (end < begin || (end - begin) % sizeof(std::uint32_t) != 0) throw std::invalid_argument("libc++ unsigned sort: invalid range");
    const auto count = (end - begin) / sizeof(std::uint32_t);
    if (count < 2) return;
    const auto bytes = end - begin;
    if (!GuestWritable_nid_no_patch(first, bytes)) throw std::invalid_argument("libc++ unsigned sort: unwritable range");
    std::vector<std::uint32_t> values(count);
    if (!GuestCopyIn_nid_no_patch(values.data(), first, count * sizeof(std::uint32_t))) throw std::invalid_argument("libc++ unsigned sort: unreadable range");
    std::sort(values.begin(), values.end());
    if (!GuestCopyOut_nid_no_patch(first, values.data(), count * sizeof(std::uint32_t))) throw std::invalid_argument("libc++ unsigned sort: unwritable range");
}
