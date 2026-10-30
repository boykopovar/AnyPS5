#ifndef CORE_LIBS_PRX_LIBSCEOPUSDEC_INCLUDE_OPUSDECODEVARIANTS_HPP
#define CORE_LIBS_PRX_LIBSCEOPUSDEC_INCLUDE_OPUSDECODEVARIANTS_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include "prx/libc/include/General.hpp"

namespace {

constexpr int OpusDefaultDecodePriority = 40;
constexpr std::uint32_t OpusInfiniteTimeout = 0xFFFFFFFFu;

inline int OpusCheckVariant(std::uint32_t errorBase, const void* state, const std::uint8_t* data, int bytes, const void* pcm, int capacity, int priority) {
    const auto error = [errorBase](std::uint32_t offset) { return static_cast<int>(errorBase + offset); };
    if (!state) return error(0);
    if (bytes != 0 && !data) return error(6);
    if (bytes < 0) return error(7);
    if (!pcm) return error(8);
    if (capacity < 0) return error(9);
    if (priority < 20 || priority > 100) return error(10);
    return 0;
}

template <typename TLibrary>
int OpusDecodeVariant(TLibrary& library, std::uint32_t errorBase, void* state, const std::uint8_t* data, int bytes, void* pcm,
                      int capacity, int priority, bool asFloat) {
    if (const int check = OpusCheckVariant(errorBase, state, data, bytes, pcm, capacity, priority)) return check;
    auto* samples = static_cast<std::int16_t*>(pcm);
    if (!asFloat) return library.Decode(state, data, bytes, samples, capacity);
    const int result = library.Decode(state, data, bytes, samples, capacity / 4 * 2);
    if (result < 0) return result;
    const auto count = static_cast<std::size_t>(result / 2);
    std::vector<std::int16_t> decoded(count);
    std::memcpy(decoded.data(), pcm, count * 2);
    auto* output = static_cast<std::uint8_t*>(pcm);
    for (std::size_t index = 0; index < count; ++index) {
        const float value = decoded[index] > 0 ? decoded[index] / 32767.0f : decoded[index] / 32768.0f;
        std::memcpy(output + index * 4, &value, 4);
    }
    return result * 2;
}

template <typename TLibrary>
int OpusDecodeVariantWithTimeout(TLibrary& library, std::uint32_t errorBase, void* state, const std::uint8_t* data, int bytes, void* pcm,
                                 int capacity, int priority, bool asFloat, std::uint32_t timeout, const char* name) {
    if (timeout != OpusInfiniteTimeout) {
        if (const int check = OpusCheckVariant(errorBase, state, data, bytes, pcm, capacity, priority)) return check;
        NotImplemented_nid_no_patch(name);
        return 0;
    }
    return OpusDecodeVariant(library, errorBase, state, data, bytes, pcm, capacity, priority, asFloat);
}

} // namespace

#endif
