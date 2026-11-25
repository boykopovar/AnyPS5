#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t StateSize = 16;
constexpr auto Invalid = static_cast<std::size_t>(-1);
constexpr auto Incomplete = static_cast<std::size_t>(-2);

void CopyInChecked(void* destination, const void* source, std::size_t size, const char* function) {
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, size);
    if (!copied) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void CopyOutChecked(void* destination, const void* source, std::size_t size, const char* function) {
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, size);
    if (!copied) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

} // namespace

extern "C" {

int APS5_VABI _Mbtowc_nid_postfix(std::uint16_t* destination, const char* source, std::size_t count, void* state) {
    thread_local std::array<unsigned char, StateSize> fallbackState{};
    if (source == nullptr) {
        std::array<unsigned char, StateSize> zero{};
        if (state != nullptr) {
            CopyOutChecked(state, zero.data(), zero.size(), __func__);
        } else {
            fallbackState = zero;
        }
        return 0;
    }

    std::array<unsigned char, StateSize> localState{};
    if (state != nullptr) {
        CopyInChecked(localState.data(), state, localState.size(), __func__);
    } else {
        localState = fallbackState;
    }

    if (count == 0) {
        return -2;
    }

    for (std::size_t consumed = 0; consumed < count;) {
        char byte = 0;
        CopyInChecked(&byte, source + consumed, 1, __func__);
        ++consumed;
        std::uint16_t decoded = 0;
        const std::size_t step = mbrtowc_nid_postfix(&decoded, &byte, 1, localState.data());
        if (step == Invalid) {
            return -1;
        }
        if (step != Incomplete) {
            if (destination != nullptr) {
                CopyOutChecked(destination, &decoded, sizeof(decoded), __func__);
            }
            if (state != nullptr) {
                CopyOutChecked(state, localState.data(), localState.size(), __func__);
            } else {
                fallbackState = localState;
            }
            return decoded == 0 ? 0 : static_cast<int>(consumed);
        }
    }

    if (state != nullptr) {
        CopyOutChecked(state, localState.data(), localState.size(), __func__);
    } else {
        fallbackState = localState;
    }
    return -2;
}

int APS5_VABI _Wctomb_nid_postfix(char* destination, std::uint16_t value, void* state) {
    std::array<unsigned char, StateSize> localState{};
    if (state != nullptr) {
        CopyInChecked(localState.data(), state, localState.size(), __func__);
    }
    if (destination == nullptr) {
        if (state != nullptr) {
            std::array<unsigned char, StateSize> zero{};
            CopyOutChecked(state, zero.data(), zero.size(), __func__);
        }
        return 0;
    }
    char encoded[4]{};
    const std::size_t length = wcrtomb_nid_postfix(encoded, value, nullptr);
    CopyOutChecked(destination, encoded, length, __func__);
    return static_cast<int>(length);
}

}
