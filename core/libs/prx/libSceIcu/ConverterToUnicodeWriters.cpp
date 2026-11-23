#include "prx/libSceIcu/ConverterToUnicodeWriters.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv_cb.h"
#include "ucnv_bld.h"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

namespace {

static_assert(sizeof(UChar) == 2 && sizeof(UBool) == 1 && sizeof(UErrorCode) == 4);
static_assert(sizeof(UConverterToUnicodeArgs) == 56 && offsetof(UConverterToUnicodeArgs, size) == 0 &&
    offsetof(UConverterToUnicodeArgs, flush) == 2 && offsetof(UConverterToUnicodeArgs, converter) == 8 &&
    offsetof(UConverterToUnicodeArgs, source) == 16 && offsetof(UConverterToUnicodeArgs, sourceLimit) == 24 &&
    offsetof(UConverterToUnicodeArgs, target) == 32 && offsetof(UConverterToUnicodeArgs, targetLimit) == 40 &&
    offsetof(UConverterToUnicodeArgs, offsets) == 48);

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* caller) {
    if (bytes && !GuestCopyIn_nid_no_patch(destination, source, bytes)) NotImplemented_nid_no_patch(caller);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* caller) {
    if (bytes && !GuestCopyOut_nid_no_patch(destination, source, bytes)) NotImplemented_nid_no_patch(caller);
}

std::uintptr_t End(const void* pointer, std::size_t bytes, const char* caller) {
    const auto start = reinterpret_cast<std::uintptr_t>(pointer);
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - start) NotImplemented_nid_no_patch(caller);
    return start + bytes;
}

struct Status {
    std::int32_t* address;
    std::int32_t incoming = 0;
    std::int32_t entered = 0;
    UErrorCode value = U_ZERO_ERROR;
    const char* caller;

    Status(std::int32_t* error, const char* name) : address(error), caller(name) {
        if (!error) APS5_INVALID_ARG_EX;
        CopyIn(&incoming, error, sizeof(incoming), caller);
        if (incoming <= 0) {
            entered = incoming < U_ERROR_WARNING_START ? 0 : incoming;
            value = static_cast<UErrorCode>(entered);
        }
    }

    void Commit() const {
        const auto result = static_cast<std::int32_t>(value);
        if (result != entered) CopyOut(address, &result, sizeof(result), caller);
    }
};

struct Buffers {
    std::vector<char16_t> input;
    std::vector<char16_t> output;
    std::vector<char16_t> joint;
    std::vector<std::int32_t> offsets;
    char16_t empty = 0;
    const char16_t* source = &empty;
    char16_t* target = &empty;

    Buffers(const char16_t* guestSource, char16_t* guestTarget, std::size_t read,
        std::size_t write, bool hasOffsets, const char* caller) {
        const auto sourceStart = reinterpret_cast<std::uintptr_t>(guestSource);
        const auto targetStart = reinterpret_cast<std::uintptr_t>(guestTarget);
        const auto sourceEnd = End(guestSource, read * sizeof(char16_t), caller);
        const auto targetEnd = End(guestTarget, write * sizeof(char16_t), caller);
        if (read && write && sourceStart < targetEnd && targetStart < sourceEnd) {
            const auto start = std::min(sourceStart, targetStart);
            const auto end = std::max(sourceEnd, targetEnd);
            if ((sourceStart - start) % sizeof(char16_t) || (targetStart - start) % sizeof(char16_t))
                NotImplemented_nid_no_patch(caller);
            joint.resize((end - start) / sizeof(char16_t));
            auto* copiedSource = joint.data() + (sourceStart - start) / sizeof(char16_t);
            CopyIn(copiedSource, guestSource, read * sizeof(char16_t), caller);
            source = copiedSource;
            target = joint.data() + (targetStart - start) / sizeof(char16_t);
        } else {
            if (read) {
                input.resize(read);
                CopyIn(input.data(), guestSource, read * sizeof(char16_t), caller);
                source = input.data();
            }
            if (write) {
                output.resize(write);
                target = output.data();
            }
        }
        if (hasOffsets && write) offsets.resize(write);
    }
};

void Write(Icu::ConverterToUnicodeArgs* guest, const char16_t* source, std::int32_t length,
    std::int32_t offsetIndex, std::int32_t* error, bool substitute, const char* caller) {
    const ErrnoGuard preserveErrno;
    Status status(error, caller);
    if (status.incoming > 0) return;
    UConverterToUnicodeArgs original{};
    CopyIn(&original, guest, sizeof(original), caller);
    if (!substitute && length <= 0) return;
    if (substitute && !original.converter) APS5_INVALID_ARG_EX;
    const auto target = reinterpret_cast<std::uintptr_t>(original.target);
    const auto limit = reinterpret_cast<std::uintptr_t>(original.targetLimit);
    if (limit < target || (limit - target) % sizeof(char16_t)) APS5_INVALID_ARG_EX;
    const auto count = static_cast<std::size_t>(length);
    const auto available = (limit - target) / sizeof(char16_t);
    const auto produced = std::min(count, available);
    const auto remaining = count - produced;
    if (original.converter && remaining > UCNV_ERROR_BUFFER_LENGTH) NotImplemented_nid_no_patch(caller);
    const auto read = substitute ? 0 : original.converter ? count : produced;
    if (original.offsets) End(original.offsets, produced * sizeof(std::int32_t), caller);
    auto* fields = reinterpret_cast<std::uint8_t*>(guest);
    if (produced && (!GuestWritable_nid_no_patch(original.target, produced * sizeof(char16_t)) ||
        !GuestWritable_nid_no_patch(fields + offsetof(UConverterToUnicodeArgs, target), sizeof(original.target)))) {
        NotImplemented_nid_no_patch(caller);
    }
    if (original.offsets && produced && (!GuestWritable_nid_no_patch(original.offsets, produced * sizeof(std::int32_t)) ||
        !GuestWritable_nid_no_patch(fields + offsetof(UConverterToUnicodeArgs, offsets), sizeof(original.offsets)))) {
        NotImplemented_nid_no_patch(caller);
    }
    if (remaining && !GuestWritable_nid_no_patch(error, sizeof(*error))) NotImplemented_nid_no_patch(caller);
    try {
        Buffers buffers(source, original.target, read, produced, original.offsets != nullptr, caller);
        auto native = original;
        native.target = buffers.target;
        native.targetLimit = buffers.target + produced;
        native.offsets = buffers.offsets.empty() ? nullptr : buffers.offsets.data();
        if (substitute)
            ucnv_cbToUWriteSub(&native, offsetIndex, &status.value);
        else
            ucnv_cbToUWriteUChars(&native, buffers.source, length, offsetIndex, &status.value);
        const auto written = native.target - buffers.target;
        if (written < 0 || static_cast<std::size_t>(written) != produced) NotImplemented_nid_no_patch(caller);
        CopyOut(original.target, buffers.target, produced * sizeof(char16_t), caller);
        if (original.offsets) CopyOut(original.offsets, buffers.offsets.data(), produced * sizeof(std::int32_t), caller);
        auto* newTarget = reinterpret_cast<char16_t*>(End(original.target, produced * sizeof(char16_t), caller));
        auto* newOffsets = original.offsets ?
            reinterpret_cast<std::int32_t*>(End(original.offsets, produced * sizeof(std::int32_t), caller)) : nullptr;
        if (newOffsets != original.offsets) CopyOut(fields + offsetof(UConverterToUnicodeArgs, offsets), &newOffsets, sizeof(newOffsets), caller);
        if (newTarget != original.target) CopyOut(fields + offsetof(UConverterToUnicodeArgs, target), &newTarget, sizeof(newTarget), caller);
        status.Commit();
    } catch (const std::bad_alloc&) {
        status.value = U_MEMORY_ALLOCATION_ERROR;
        status.Commit();
    }
}

}

extern "C" {

void APS5_VABI ucnv_cbToUWriteUChars_nid_postfix(Icu::ConverterToUnicodeArgs* arguments,
    const char16_t* source, std::int32_t length, std::int32_t offsetIndex, std::int32_t* error) {
    Write(arguments, source, length, offsetIndex, error, false, __func__);
}

void APS5_VABI ucnv_cbToUWriteSub_nid_postfix(Icu::ConverterToUnicodeArgs* arguments,
    std::int32_t offsetIndex, std::int32_t* error) {
    Write(arguments, nullptr, 1, offsetIndex, error, true, __func__);
}

}
