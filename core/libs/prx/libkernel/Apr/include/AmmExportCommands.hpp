#ifndef PRX_LIBKERNEL_APR_AMMEXPORTCOMMANDS_HPP
#define PRX_LIBKERNEL_APR_AMMEXPORTCOMMANDS_HPP

#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>

namespace {

template<class TCommand>
int WriteAmmCommand(std::initializer_list<std::uint64_t> protections, const std::optional<TCommand>& command, void* dst, std::uint64_t* outSize) {
    for (const auto protection : protections) {
        if (protection > 0xFFFFFFFFu) return SCE_KERNEL_ERROR_EINVAL;
    }
    if (!command) return SCE_KERNEL_ERROR_EINVAL;
    if (dst) std::memcpy(dst, &*command, sizeof(TCommand));
    if (outSize) *outSize = sizeof(TCommand);
    return 0;
}

constexpr std::uint64_t GuestAddressMask = 0xFFFFFFFFFFFFull;
constexpr std::uint64_t AmmPageMask = 0x3FFFu;
constexpr std::uint64_t AmmSizeMask = 0x1FFFFFFC000ull;
constexpr std::uint64_t EqueueLimit = 1ull << 48u;

bool InsideAmmMapRange(std::uint64_t address, std::uint64_t size) {
    std::uint64_t start = 0, end = 0, multimapStart = 0, multimapEnd = 0;
    AmmVirtualAddressRanges_nid_no_patch(&start, &end, &multimapStart, &multimapEnd);
    return size != 0u && ((address | size) & AmmPageMask) == 0u && address >= start && address + size <= end;
}

template<class TCommand>
int WriteCommand(void* dst, const TCommand& command, std::uint64_t* outSize) {
    if (dst) std::memcpy(dst, &command, sizeof(command));
    if (outSize) *outSize = sizeof(command);
    return 0;
}

constexpr std::uint32_t AmmPriorities = 3;
constexpr std::uint64_t MapperGranuleMask = 0x1FFFFF;
constexpr std::uint64_t MapperParamBytes = 0x38;

std::uint32_t CallIndirectBuffer(const char* function, std::uint32_t priority, void* base, std::uint64_t bytes) {
    if (priority >= AmmPriorities || !base) NotImplemented_nid_no_patch(function);
    const std::uint64_t submitted = std::uint64_t{static_cast<std::uint32_t>(bytes >> 2u)} << 2u;
    if (submitted > 0xFFFFFFFFull) NotImplemented_nid_no_patch(function);
    return AmmSubmit_nid_no_patch(base, static_cast<std::uint32_t>(submitted));
}

}

#endif
