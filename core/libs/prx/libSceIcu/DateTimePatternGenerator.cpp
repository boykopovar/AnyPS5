#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/DateTimePatternGeneratorBackend.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using Native = std::unique_ptr<void, decltype(&IcuPatternBackend::Close_nid_no_patch)>;

struct Entry {
    Native native;
    std::mutex mutex;
    explicit Entry(Native value) : native(std::move(value)) {}
};

struct Token {};
std::mutex HandlesMutex;
std::unordered_map<const UDateTimePatternGenerator*, std::shared_ptr<Entry>> Handles;
std::vector<std::unique_ptr<Token>> Tokens;

std::int32_t ReadStatus(const std::int32_t* pointer) {
    std::int32_t value;
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(value))) NotImplemented_nid_no_patch(__func__);
    return value;
}

void WriteStatus(std::int32_t* pointer, std::int32_t before, std::int32_t after) {
    if (before != after && !GuestCopyOut_nid_no_patch(pointer, &after, sizeof(after))) NotImplemented_nid_no_patch(__func__);
}

std::shared_ptr<Entry> Find(const UDateTimePatternGenerator* handle) {
    const std::lock_guard lock(HandlesMutex);
    const auto found = Handles.find(handle);
    if (found == Handles.end()) NotImplemented_nid_no_patch(__func__);
    return found->second;
}

std::string ReadLocale(const char* locale) {
    std::string result;
    auto address = reinterpret_cast<std::uintptr_t>(locale);
    for (;;) {
        char value;
        if (!GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address), sizeof(value))) NotImplemented_nid_no_patch(__func__);
        if (!value) return result;
        if (address == UINTPTR_MAX) APS5_INVALID_ARG_EX;
        result.push_back(value);
        ++address;
    }
}

std::u16string ReadText(const char16_t* text, std::int32_t length) {
    if (length < -1) APS5_INVALID_ARG_EX;
    std::u16string result;
    auto address = reinterpret_cast<std::uintptr_t>(text);
    if (length >= 0) {
        std::array<char16_t, 256> chunk;
        auto remaining = static_cast<std::size_t>(length);
        while (remaining) {
            const auto count = std::min(remaining, chunk.size());
            const auto bytes = count * sizeof(char16_t);
            if (address > UINTPTR_MAX - bytes ||
                !GuestCopyIn_nid_no_patch(chunk.data(), reinterpret_cast<const void*>(address), bytes)) NotImplemented_nid_no_patch(__func__);
            result.append(chunk.data(), count);
            address += bytes;
            remaining -= count;
        }
        return result;
    }
    for (;;) {
        char16_t value;
        if (!GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address), sizeof(value))) NotImplemented_nid_no_patch(__func__);
        if (!value) return result;
        if (result.size() == static_cast<std::size_t>(INT32_MAX) || address > UINTPTR_MAX - sizeof(value)) APS5_INVALID_ARG_EX;
        result.push_back(value);
        address += sizeof(value);
    }
}

template<class Function>
std::int32_t Extract(Function&& function, char16_t* destination, std::int32_t capacity, std::int32_t* error, std::int32_t before) {
    auto status = before;
    const auto required = function(nullptr, 0, &status);
    if ((status > 0 && status != 15) || (!destination && capacity == 0)) {
        WriteStatus(error, before, status);
        return required;
    }
    char16_t dummy = 0;
    if (capacity < 0 || (!destination && capacity != 0)) {
        status = before;
        const auto count = function(destination ? &dummy : nullptr, capacity, &status);
        WriteStatus(error, before, status);
        return count;
    }
    const auto bounded = static_cast<std::int32_t>(std::min<std::int64_t>(capacity, static_cast<std::int64_t>(required) + 1));
    if (bounded < 0) NotImplemented_nid_no_patch(__func__);
    std::vector<char16_t> output(static_cast<std::size_t>(bounded));
    status = before;
    const auto count = function(output.empty() ? &dummy : output.data(), bounded, &status);
    if (status <= 0 && count >= 0 && count <= capacity) {
        const auto units = static_cast<std::size_t>(count) + (count < capacity ? 1 : 0);
        if (units > output.size()) NotImplemented_nid_no_patch(__func__);
        if (units && !GuestCopyOut_nid_no_patch(destination, output.data(), units * sizeof(char16_t))) NotImplemented_nid_no_patch(__func__);
    }
    WriteStatus(error, before, status);
    return count;
}

std::int32_t Best(UDateTimePatternGenerator* handle, const char16_t* skeleton, std::int32_t length, std::int32_t options,
                  char16_t* output, std::int32_t capacity, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return 0;
    if (!skeleton && length != 0) {
        WriteStatus(error, before, 1);
        return 0;
    }
    try {
        const auto input = ReadText(skeleton, length);
        const auto owner = Find(handle);
        const std::lock_guard lock(owner->mutex);
        return Extract([&](char16_t* destination, std::int32_t size, std::int32_t* status) {
            return IcuPatternBackend::Best_nid_no_patch(owner->native.get(), input.data(), input.size(), options, destination, size, status);
        }, output, capacity, error, before);
    } catch (const std::bad_alloc&) {
        WriteStatus(error, before, 7);
        return 0;
    }
}

} // namespace

extern "C" {

void APS5_VABI udatpg_close_nid_postfix(UDateTimePatternGenerator* handle) {
    if (!handle) return;
    std::shared_ptr<Entry> closing;
    {
        const std::lock_guard lock(HandlesMutex);
        const auto found = Handles.find(handle);
        if (found == Handles.end()) NotImplemented_nid_no_patch(__func__);
        closing = std::move(found->second);
        Handles.erase(found);
    }
}

UDateTimePatternGenerator* APS5_VABI udatpg_open_nid_postfix(const char* locale, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return nullptr;
    try {
        const auto input = locale ? ReadLocale(locale) : std::string();
        auto status = before;
        Native native(IcuPatternBackend::Open_nid_no_patch(locale ? input.c_str() : nullptr, &status), IcuPatternBackend::Close_nid_no_patch);
        if (!native) {
            WriteStatus(error, before, status);
            return nullptr;
        }
        auto owner = std::make_shared<Entry>(std::move(native));
        auto token = std::make_unique<Token>();
        auto* handle = reinterpret_cast<UDateTimePatternGenerator*>(token.get());
        WriteStatus(error, before, status);
        const std::lock_guard lock(HandlesMutex);
        Tokens.push_back(std::move(token));
        Handles.emplace(handle, std::move(owner));
        return handle;
    } catch (const std::bad_alloc&) {
        WriteStatus(error, before, 7);
        return nullptr;
    }
}

std::int32_t APS5_VABI udatpg_getBestPattern_nid_postfix(UDateTimePatternGenerator* handle, const char16_t* skeleton,
                                                       std::int32_t length, char16_t* output, std::int32_t capacity, std::int32_t* status) {
    return Best(handle, skeleton, length, 0, output, capacity, status);
}

std::int32_t APS5_VABI udatpg_getBestPatternWithOptions_nid_postfix(UDateTimePatternGenerator* handle, const char16_t* skeleton,
    std::int32_t length, UDateTimePatternMatchOptions options, char16_t* output, std::int32_t capacity, std::int32_t* status) {
    return Best(handle, skeleton, length, options, output, capacity, status);
}

std::int32_t APS5_VABI udatpg_getSkeleton_nid_postfix(UDateTimePatternGenerator*, const char16_t* pattern, std::int32_t length,
                                                  char16_t* output, std::int32_t capacity, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return 0;
    if (!pattern && length != 0) {
        WriteStatus(error, before, 1);
        return 0;
    }
    try {
        const auto input = ReadText(pattern, length);
        return Extract([&](char16_t* destination, std::int32_t size, std::int32_t* status) {
            return IcuPatternBackend::Skeleton_nid_no_patch(input.data(), input.size(), destination, size, status);
        }, output, capacity, error, before);
    } catch (const std::bad_alloc&) {
        WriteStatus(error, before, 7);
        return 0;
    }
}

std::int32_t APS5_VABI udatpg_getFieldDisplayName_nid_postfix(const UDateTimePatternGenerator* handle, std::int32_t field,
    std::int32_t width, char16_t* output, std::int32_t capacity, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return -1;
    if (output == nullptr ? capacity != 0 : capacity < 0) {
        WriteStatus(error, before, 1);
        return -1;
    }
    if (field < 0 || field >= 16 || width < 0 || width >= 3) APS5_INVALID_ARG_EX;
    try {
        const auto owner = Find(handle);
        const std::lock_guard lock(owner->mutex);
        return Extract([&](char16_t* destination, std::int32_t size, std::int32_t* status) {
            return IcuPatternBackend::Field_nid_no_patch(owner->native.get(), field, width, destination, size, status);
        }, output, capacity, error, before);
    } catch (const std::bad_alloc&) {
        WriteStatus(error, before, 7);
        return 0;
    }
}

}
