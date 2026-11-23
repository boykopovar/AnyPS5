#include "prx/libSceIcu/ConverterToUnicodeWriters.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "unicode/ucnv_cb.h"
#include "ucnv_bld.h"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
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

struct WritableRange {
    std::optional<GuestArena::HostWrite> access;

    WritableRange(void* address, std::size_t bytes, const char* caller) {
        if (!bytes) return;
        if (!access.emplace(address, bytes).Open() || !GuestWritable_nid_no_patch(address, bytes)) NotImplemented_nid_no_patch(caller);
    }
};

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
    std::vector<char16_t> originalOverlap;
    void* guestOverlap = nullptr;
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
            const auto overlapStart = std::max(sourceStart, targetStart);
            const auto overlapEnd = std::min(sourceEnd, targetEnd);
            guestOverlap = reinterpret_cast<void*>(overlapStart);
            const auto* shared = copiedSource + (overlapStart - sourceStart) / sizeof(char16_t);
            originalOverlap.assign(shared, shared + (overlapEnd - overlapStart) / sizeof(char16_t));
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
    const auto targetBytes = produced * sizeof(char16_t);
    const auto offsetBytes = original.offsets ? produced * sizeof(std::int32_t) : 0;
    if (offsetBytes) End(original.offsets, offsetBytes, caller);
    auto* fields = reinterpret_cast<std::uint8_t*>(guest);
    auto* offsetsField = fields + offsetof(UConverterToUnicodeArgs, offsets);
    auto* targetField = fields + offsetof(UConverterToUnicodeArgs, target);
    try {
        const WritableRange targetAccess(original.target, targetBytes, caller);
        const WritableRange targetCursorAccess(targetField, produced ? sizeof(original.target) : 0, caller);
        const WritableRange offsetsAccess(original.offsets, offsetBytes, caller);
        const WritableRange offsetsCursorAccess(offsetsField, offsetBytes ? sizeof(original.offsets) : 0, caller);
        const WritableRange statusAccess(error, remaining ? sizeof(*error) : 0, caller);
        Buffers buffers(source, original.target, read, produced, original.offsets != nullptr, caller);
        auto native = original;
        native.target = buffers.target;
        native.targetLimit = buffers.target + produced;
        native.offsets = buffers.offsets.empty() ? nullptr : buffers.offsets.data();
        char16_t replacement = 0;
        if (substitute) {
            auto selection = original;
            selection.target = &replacement;
            selection.targetLimit = &replacement + 1;
            selection.offsets = nullptr;
            ucnv_cbToUWriteSub(&selection, offsetIndex, &status.value);
            buffers.source = &replacement;
        }
        native.converter = nullptr;
        ucnv_cbToUWriteUChars(&native, buffers.source, length, offsetIndex, &status.value);
        const auto written = native.target - buffers.target;
        if (written < 0 || static_cast<std::size_t>(written) != produced) NotImplemented_nid_no_patch(caller);
        auto* newTarget = reinterpret_cast<char16_t*>(End(original.target, targetBytes, caller));
        auto* newOffsets = original.offsets ?
            reinterpret_cast<std::int32_t*>(End(original.offsets, offsetBytes, caller)) : nullptr;
        bool offsetsCommitted = false;
        bool targetCommitted = false;
        try {
            CopyOut(original.target, buffers.target, targetBytes, caller);
            CopyOut(original.offsets, buffers.offsets.data(), offsetBytes, caller);
            if (newOffsets != original.offsets) {
                CopyOut(offsetsField, &newOffsets, sizeof(newOffsets), caller);
                offsetsCommitted = true;
            }
            if (newTarget != original.target) {
                CopyOut(targetField, &newTarget, sizeof(newTarget), caller);
                targetCommitted = true;
            }
            status.Commit();
        } catch (...) {
            if (buffers.guestOverlap) GuestCopyOut_nid_no_patch(buffers.guestOverlap, buffers.originalOverlap.data(),
                buffers.originalOverlap.size() * sizeof(char16_t));
            if (targetCommitted) GuestCopyOut_nid_no_patch(targetField, &original.target, sizeof(original.target));
            if (offsetsCommitted) GuestCopyOut_nid_no_patch(offsetsField, &original.offsets, sizeof(original.offsets));
            throw;
        }
        if (original.converter && remaining) {
            native.converter = original.converter;
            native.targetLimit = native.target;
            native.offsets = nullptr;
            UErrorCode suffixStatus = U_ZERO_ERROR;
            ucnv_cbToUWriteUChars(&native, buffers.source + produced, static_cast<std::int32_t>(remaining),
                offsetIndex, &suffixStatus);
        }
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
