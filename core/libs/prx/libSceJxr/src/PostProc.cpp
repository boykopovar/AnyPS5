#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
std::size_t JxrPostInfoBytes_nid_no_patch();
const char* JxrSlideOneMBRow_nid_no_patch(void* table, std::size_t channels, std::size_t width, std::int32_t top,
                                          std::int32_t bottom);
const char* JxrUpdatePostProcInfo_nid_no_patch(void* table, std::int32_t* macroblock, std::size_t column,
                                               std::size_t channel);
const char* JxrPostProcMB_nid_no_patch(void* table, std::int32_t* previous, std::int32_t* current, std::size_t column,
                                       std::size_t channel, std::int32_t threshold);
const char* JxrPostProcBlock_nid_no_patch(void* table, std::int32_t* previous, std::int32_t* current, std::size_t column,
                                          std::size_t channel, std::int32_t threshold);
}

namespace {

constexpr std::size_t MaxChannels = 16;

[[noreturn]] void Fail(const char* function, const char* error) {
    throw std::invalid_argument(std::string(function) + ": " + error);
}

void Check(const char* error, const char* function) {
    if (error) Fail(function, error);
}

class Guards {
public:
    explicit Guards(const char* function) : function(function) {}

    void Add(const void* pointer, std::ptrdiff_t offset, std::size_t bytes) {
        if (!pointer) return;
        const auto address = reinterpret_cast<std::uintptr_t>(pointer) + static_cast<std::uintptr_t>(offset);
        writes.push_back(std::make_unique<GuestArena::HostWrite>(reinterpret_cast<void*>(address), bytes));
        if (!writes.back()->Open()) Fail(function, "output is not writable");
    }

private:
    const char* function;
    std::vector<std::unique_ptr<GuestArena::HostWrite>> writes;
};

void* const* Table(void* table, std::size_t channel, const char* function) {
    if (!table) Fail(function, "null post-processing table");
    if (channel >= MaxChannels) Fail(function, "channel out of range");
    return static_cast<void* const*>(table) + channel * 2;
}

std::ptrdiff_t InfoOffset(std::size_t column, std::ptrdiff_t first, const char* function) {
    const std::size_t info = JxrPostInfoBytes_nid_no_patch();
    if (column >= static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / info - 1)
        Fail(function, "column out of range");
    return (static_cast<std::ptrdiff_t>(column) + first) * static_cast<std::ptrdiff_t>(info);
}

constexpr std::ptrdiff_t Pixels(std::ptrdiff_t count) {
    return count * static_cast<std::ptrdiff_t>(sizeof(std::int32_t));
}

} // namespace

extern "C" {

void APS5_VABI slideOneMBRow_nid_postfix(void* table, std::size_t channels, std::size_t width, std::int32_t top,
                                         std::int32_t bottom) {
    if (!table) Fail(__func__, "null post-processing table");
    if (channels > MaxChannels) Fail(__func__, "more than 16 channels");
    const std::size_t info = JxrPostInfoBytes_nid_no_patch();
    if (width > std::numeric_limits<std::size_t>::max() / info) Fail(__func__, "row width out of range");
    Guards guards(__func__);
    guards.Add(table, 0, channels * 2 * sizeof(void*));
    for (std::size_t channel = 0; channel < channels; ++channel) {
        void* const* rows = Table(table, channel, __func__);
        if (top) guards.Add(rows[1], 0, width * info);
        if (bottom) guards.Add(rows[0], 0, width * info);
    }
    Check(JxrSlideOneMBRow_nid_no_patch(table, channels, width, top, bottom), __func__);
}

void APS5_VABI updatePostProcInfo_nid_postfix(void* table, std::int32_t* macroblock, std::size_t column,
                                              std::size_t channel) {
    void* const* rows = Table(table, channel, __func__);
    Guards guards(__func__);
    guards.Add(rows[1], InfoOffset(column, 0, __func__), JxrPostInfoBytes_nid_no_patch());
    Check(JxrUpdatePostProcInfo_nid_no_patch(table, macroblock, column, channel), __func__);
}

void APS5_VABI postProcMB_nid_postfix(void* table, std::int32_t* previous, std::int32_t* current, std::size_t column,
                                      std::size_t channel, std::int32_t threshold) {
    void* const* rows = Table(table, channel, __func__);
    const std::ptrdiff_t offset = InfoOffset(column, -1, __func__);
    Guards guards(__func__);
    guards.Add(rows[0], offset, 2 * JxrPostInfoBytes_nid_no_patch());
    guards.Add(rows[1], offset, 2 * JxrPostInfoBytes_nid_no_patch());
    guards.Add(previous, Pixels(-96), static_cast<std::size_t>(Pixels(209)));
    guards.Add(current, Pixels(-128), static_cast<std::size_t>(Pixels(209)));
    Check(JxrPostProcMB_nid_no_patch(table, previous, current, column, channel, threshold), __func__);
}

void APS5_VABI postProcBlock_nid_postfix(void* table, std::int32_t* previous, std::int32_t* current,
                                         std::size_t column, std::size_t channel, std::int32_t threshold) {
    Table(table, channel, __func__);
    InfoOffset(column, -1, __func__);
    Guards guards(__func__);
    guards.Add(previous, Pixels(-256), static_cast<std::size_t>(Pixels(320)));
    guards.Add(current, Pixels(-256), static_cast<std::size_t>(Pixels(208)));
    Check(JxrPostProcBlock_nid_no_patch(table, previous, current, column, channel, threshold), __func__);
}

} // extern "C"
