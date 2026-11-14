#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "unicode/uidna.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

static_assert(sizeof(UChar) == 2 && sizeof(UBool) == 1 && sizeof(UErrorCode) == 4);
static_assert(sizeof(UIDNAInfo) == 16 && alignof(UIDNAInfo) == 4);
static_assert(offsetof(UIDNAInfo, size) == 0 && offsetof(UIDNAInfo, isTransitionalDifferent) == 2);
static_assert(offsetof(UIDNAInfo, reservedB3) == 3 && offsetof(UIDNAInfo, errors) == 4);
static_assert(offsetof(UIDNAInfo, reservedI2) == 8 && offsetof(UIDNAInfo, reservedI3) == 12);

std::mutex HandlesMutex;
std::unordered_set<const UIDNA*> Handles;

std::int32_t ReadStatus(const std::int32_t* error) {
    std::int32_t status;
    if (!GuestCopyIn_nid_no_patch(&status, error, sizeof(status))) NotImplemented_nid_no_patch(__func__);
    return status;
}

void WriteStatus(std::int32_t* error, std::int32_t before, std::int32_t after) {
    if (before != after && !GuestCopyOut_nid_no_patch(error, &after, sizeof(after))) NotImplemented_nid_no_patch(__func__);
}

std::u16string ReadName(const char16_t* name, std::int32_t length) {
    if (length >= 0) {
        std::u16string input;
        std::array<char16_t, 256> chunk;
        auto address = reinterpret_cast<std::uintptr_t>(name);
        auto remaining = static_cast<std::size_t>(length);
        while (remaining != 0) {
            const auto units = std::min(remaining, chunk.size());
            const auto bytes = units * sizeof(char16_t);
            if (address > UINTPTR_MAX - bytes ||
                !GuestCopyIn_nid_no_patch(chunk.data(), reinterpret_cast<const void*>(address), bytes)) NotImplemented_nid_no_patch(__func__);
            input.append(chunk.data(), units);
            address += bytes;
            remaining -= units;
        }
        return input;
    }
    std::u16string input;
    auto address = reinterpret_cast<std::uintptr_t>(name);
    for (;;) {
        char16_t character;
        if (!GuestCopyIn_nid_no_patch(&character, reinterpret_cast<const void*>(address), sizeof(character))) NotImplemented_nid_no_patch(__func__);
        if (character == 0) return input;
        if (input.size() == static_cast<std::size_t>(INT32_MAX) || address > UINTPTR_MAX - sizeof(character)) APS5_INVALID_ARG_EX;
        input.push_back(character);
        address += sizeof(character);
    }
}

using ConvertFunction = decltype(&uidna_nameToASCII);

std::int32_t Convert(ConvertFunction convert, const UIDNA* idna, const char16_t* name, std::int32_t length,
                     char16_t* destination, std::int32_t capacity, UIDNAInfo* info, std::int32_t* error) {
    const auto previous = ReadStatus(error);
    if (previous > 0) return 0;
    std::int16_t infoSize = 0;
    if (info && !GuestCopyIn_nid_no_patch(&infoSize, info, sizeof(infoSize))) NotImplemented_nid_no_patch(__func__);
    if (infoSize < 16 || (name == nullptr ? length != 0 : length < -1) ||
        (destination == nullptr ? capacity != 0 : capacity < 0) || (destination == name && name != nullptr)) {
        WriteStatus(error, previous, U_ILLEGAL_ARGUMENT_ERROR);
        return 0;
    }
    const std::lock_guard lock(HandlesMutex);
    if (!Handles.contains(idna)) NotImplemented_nid_no_patch(__func__);
    try {
        const auto input = ReadName(name, length);
        UIDNAInfo preflightInfo = UIDNA_INFO_INITIALIZER;
        auto status = static_cast<UErrorCode>(previous);
        const auto required = convert(idna, input.data(), static_cast<std::int32_t>(input.size()), nullptr, 0,
                                      &preflightInfo, &status);
        const auto bounded = static_cast<std::int32_t>(std::min<std::int64_t>(capacity,
            std::max<std::int64_t>(input.size(), static_cast<std::int64_t>(required) + 1)));
        std::vector<char16_t> output(required <= capacity ? static_cast<std::size_t>(bounded) : 0);
        std::vector<std::uint32_t> nativeInfo((static_cast<std::size_t>(infoSize) + 3) / 4);
        std::memcpy(nativeInfo.data(), &infoSize, sizeof(infoSize));
        status = static_cast<UErrorCode>(previous);
        const auto perform = [&](char16_t* buffer, std::int32_t bufferCapacity) {
            return convert(idna, input.data(), static_cast<std::int32_t>(input.size()), buffer, bufferCapacity,
                           reinterpret_cast<UIDNAInfo*>(nativeInfo.data()), &status);
        };
        std::int32_t count;
        if (required > capacity && capacity > 0) {
            const auto bytes = static_cast<std::size_t>(capacity) * sizeof(char16_t);
            const GuestArena::HostWrite access(destination, bytes);
            if (!access.Open() || !GuestWritable_nid_no_patch(destination, bytes)) NotImplemented_nid_no_patch(__func__);
            count = perform(destination, capacity);
        } else {
            count = perform(output.empty() ? nullptr : output.data(), bounded);
        }
        const auto* infoBytes = reinterpret_cast<const std::uint8_t*>(nativeInfo.data());
        const auto infoAddress = reinterpret_cast<std::uintptr_t>(info);
        if (infoAddress > UINTPTR_MAX - static_cast<std::size_t>(infoSize) ||
            !GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(infoAddress + sizeof(infoSize)), infoBytes + sizeof(infoSize),
                                      static_cast<std::size_t>(infoSize) - sizeof(infoSize))) NotImplemented_nid_no_patch(__func__);
        if (required <= capacity && U_SUCCESS(status)) {
            const auto units = static_cast<std::size_t>(count) + (count < capacity ? 1 : 0);
            if (!GuestCopyOut_nid_no_patch(destination, output.data(), units * sizeof(char16_t))) NotImplemented_nid_no_patch(__func__);
        }
        WriteStatus(error, previous, status);
        return count;
    } catch (const std::bad_alloc&) {
        WriteStatus(error, previous, U_MEMORY_ALLOCATION_ERROR);
        return 0;
    }
}

} // namespace

extern "C" {

void APS5_VABI uidna_close_nid_postfix(UIDNA* idna) {
    if (!idna) return;
    const std::lock_guard lock(HandlesMutex);
    if (Handles.erase(idna) == 0) NotImplemented_nid_no_patch(__func__);
    uidna_close(idna);
}

std::int32_t APS5_VABI uidna_nameToASCII_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length,
                                                  char16_t* destination, std::int32_t capacity, UIDNAInfo* info,
                                                  std::int32_t* error) {
    return Convert(uidna_nameToASCII, idna, name, length, destination, capacity, info, error);
}

std::int32_t APS5_VABI uidna_nameToUnicode_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length,
                                                    char16_t* destination, std::int32_t capacity, UIDNAInfo* info,
                                                    std::int32_t* error) {
    return Convert(uidna_nameToUnicode, idna, name, length, destination, capacity, info, error);
}

UIDNA* APS5_VABI uidna_openUTS46_nid_postfix(std::uint32_t options, std::int32_t* error) {
    const auto previous = ReadStatus(error);
    if (previous > 0) return nullptr;
    auto status = static_cast<UErrorCode>(previous);
    std::unique_ptr<UIDNA, decltype(&uidna_close)> idna(uidna_openUTS46(options, &status), uidna_close);
    try {
        const std::lock_guard lock(HandlesMutex);
        if (idna) Handles.insert(idna.get());
        try {
            WriteStatus(error, previous, status);
        } catch (...) {
            if (idna) Handles.erase(idna.get());
            throw;
        }
    } catch (const std::bad_alloc&) {
        WriteStatus(error, previous, U_MEMORY_ALLOCATION_ERROR);
        return nullptr;
    }
    return idna.release();
}

}
