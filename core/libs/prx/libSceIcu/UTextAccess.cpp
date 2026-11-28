// Derived from icu4c/source/common/utext.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): the UText
// framework, the UChar string provider and the accessors below.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utext.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html

#include "prx/libSceIcu/UTextAccess.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libSceIcu/TerminateBuffer.hpp"
#include "unicode/utf16.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

static_assert(sizeof(UText) == 144);
static_assert(offsetof(UText, magic) == 0);
static_assert(offsetof(UText, flags) == 4);
static_assert(offsetof(UText, extraSize) == 24);
static_assert(offsetof(UText, chunkNativeStart) == 32);
static_assert(offsetof(UText, chunkContents) == 48);
static_assert(offsetof(UText, pFuncs) == 56);
static_assert(offsetof(UText, pExtra) == 64);
static_assert(offsetof(UText, context) == 72);
static_assert(offsetof(UText, a) == 112);
static_assert(offsetof(UTextFuncs, clone) == 16);
static_assert(offsetof(UTextFuncs, nativeLength) == 24);
static_assert(offsetof(UTextFuncs, access) == 32);
static_assert(offsetof(UTextFuncs, extract) == 40);
static_assert(offsetof(UTextFuncs, replace) == 48);
static_assert(offsetof(UTextFuncs, copy) == 56);
static_assert(offsetof(UTextFuncs, mapOffsetToNative) == 64);
static_assert(offsetof(UTextFuncs, mapNativeIndexToUTF16) == 72);
static_assert(offsetof(UTextFuncs, close) == 80);

namespace {

constexpr std::uint32_t Magic = 0x345ad82c;
constexpr std::int32_t HeapAllocated = 1;
constexpr std::int32_t ExtraHeapAllocated = 2;
constexpr std::int32_t Open = 4;
constexpr std::int32_t ProviderLengthIsExpensive = 1 << 1;
constexpr std::int32_t ProviderStableChunks = 1 << 2;
constexpr std::int32_t ProviderWritable = 1 << 3;
constexpr std::int32_t ProviderHasMetaData = 1 << 4;
constexpr std::int32_t ProviderOwnsText = 1 << 5;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t NoWritePermission = 30;
constexpr std::int32_t Sentinel = -1;
constexpr char16_t EmptyText[] = {0};

struct ExtendedText {
    UText text;
    std::max_align_t extension;
};

bool Failed(const std::int32_t* status) {
    return status == nullptr || *status > 0;
}

bool Valid(const UText* text) {
    return text != nullptr && text->magic == Magic;
}

UText* AllocateText(std::size_t bytes) {
    return static_cast<UText*>(uprv_malloc_nid_postfix(bytes));
}

void ReleaseText(void* memory) {
    IcuReleaseMemory_nid_no_patch(memory);
}

std::int32_t Pin(std::int64_t& index, std::int64_t limit) {
    if (index < 0) {
        index = 0;
    } else if (index > limit) {
        index = limit;
    }
    return static_cast<std::int32_t>(index);
}

void InvalidateChunk(UText* text) {
    text->chunkLength = 0;
    text->chunkNativeLimit = 0;
    text->chunkNativeStart = 0;
    text->chunkOffset = 0;
    text->nativeIndexingLimit = 0;
}

void AdjustPointer(UText* destination, const void** pointer, const UText* source) {
    auto* target = reinterpret_cast<char*>(const_cast<void*>(*pointer));
    auto* destinationBase = reinterpret_cast<char*>(destination);
    auto* sourceBase = reinterpret_cast<const char*>(source);
    if (target >= reinterpret_cast<const char*>(source->pExtra) &&
        target < reinterpret_cast<const char*>(source->pExtra) + source->extraSize) {
        *pointer = reinterpret_cast<char*>(destination->pExtra) +
            (target - reinterpret_cast<const char*>(source->pExtra));
    } else if (target >= sourceBase && target < sourceBase + source->sizeOfStruct) {
        *pointer = destinationBase + (target - sourceBase);
    }
}

UText* ShallowClone(UText* destination, const UText* source, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    const std::int32_t extraSize = source->extraSize;
    destination = utext_setup_nid_postfix(destination, extraSize, status);
    if (Failed(status)) return destination;
    void* destinationExtra = destination->pExtra;
    const std::int32_t flags = destination->flags;
    std::int32_t copySize = source->sizeOfStruct;
    if (copySize > destination->sizeOfStruct) copySize = destination->sizeOfStruct;
    std::memcpy(destination, source, static_cast<std::size_t>(copySize));
    destination->pExtra = static_cast<UText*>(destinationExtra);
    destination->flags = flags;
    if (extraSize > 0 && source->pExtra != nullptr) {
        std::memcpy(destination->pExtra, source->pExtra, static_cast<std::size_t>(extraSize));
    }
    AdjustPointer(destination, &destination->context, source);
    AdjustPointer(destination, &destination->p, source);
    AdjustPointer(destination, &destination->q, source);
    AdjustPointer(destination, &destination->r, source);
    AdjustPointer(destination, reinterpret_cast<const void**>(&destination->chunkContents), source);
    destination->providerProperties &= ~ProviderOwnsText;
    return destination;
}

void APS5_VABI StringClose(UText* text) {
    if ((text->providerProperties & ProviderOwnsText) != 0) {
        ReleaseText(const_cast<char16_t*>(static_cast<const char16_t*>(text->context)));
        text->context = nullptr;
    }
}

// Number of UTF-16 units readable from text->context. A known length is used as is;
// a NUL-terminated input is walked to its terminator once, and the result is cached
// in text->a, so no access can look at a unit past the terminator.
std::int64_t StringAvailable(UText* text) {
    if (text->a < 0) {
        const auto* string = static_cast<const char16_t*>(text->context);
        std::int64_t length = text->chunkNativeLimit;
        while (string[length] != 0) ++length;
        text->a = length;
        text->chunkNativeLimit = length;
        text->chunkLength = static_cast<std::int32_t>(length);
        text->nativeIndexingLimit = static_cast<std::int32_t>(length);
        text->providerProperties &= ~ProviderLengthIsExpensive;
    }
    return text->a;
}

std::int64_t APS5_VABI StringLength(UText* text) {
    return StringAvailable(text);
}

std::int8_t APS5_VABI StringAccess(UText* text, std::int64_t index, std::int8_t forward) {
    const auto* string = static_cast<const char16_t*>(text->context);
    const std::int64_t available = StringAvailable(text);
    // This provider's chunk is always the whole string, so the fields that describe it
    // are correct before any index is looked at.
    text->chunkNativeStart = 0;
    text->chunkNativeLimit = available;
    text->chunkLength = static_cast<std::int32_t>(available);
    text->nativeIndexingLimit = static_cast<std::int32_t>(available);
    if (index < 0) {
        index = 0;
    } else if (index >= available) {
        // The requested index is at or past the end of the input: a forward read has
        // no unit there. A backward read at exactly the end still has the unit before
        // it, which is the chunk contract in unicode/utext.h; a backward read past the
        // end is out of bounds.
        const bool atEnd = index == available;
        text->chunkOffset = static_cast<std::int32_t>(available);
        return static_cast<std::int8_t>(forward == 0 && atEnd && available > 0);
    }
    U16_SET_CP_START(string, 0, index);
    text->chunkOffset = static_cast<std::int32_t>(index);
    return static_cast<std::int8_t>((forward != 0 && index < available) || (forward == 0 && index > 0));
}

std::int32_t APS5_VABI StringExtract(UText* text, std::int64_t start, std::int64_t limit, char16_t* destination,
    std::int32_t destinationCapacity, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (destinationCapacity < 0 || (destination == nullptr && destinationCapacity > 0) || start > limit) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    if (!StringAccess(text, start, 1)) {
        // Nothing at or after start. StringAccess has measured the string but there is
        // no unit to copy, so return an empty extraction without reading chunkContents
        // and without writing to the caller's buffer.
        return 0;
    }
    const auto* string = text->chunkContents;
    const std::int32_t start32 = text->chunkOffset;
    const std::int32_t stringLength = static_cast<std::int32_t>(text->a);
    const std::int32_t limit32 = Pin(limit, stringLength);
    std::int32_t written = 0;
    std::int32_t index = start32;
    for (; index < limit32; ++index) {
        if (written < destinationCapacity) {
            destination[written] = string[index];
        } else {
            written = limit32 - start32;
            index = limit32;
            break;
        }
        ++written;
    }
    // If the limit index points to a lead surrogate of a pair, add the trail surrogate
    // to the destination. index < stringLength keeps this read inside the string.
    if (index > 0 && index < stringLength && U16_IS_LEAD(string[index - 1]) && U16_IS_TRAIL(string[index])) {
        if (written < destinationCapacity) destination[written++] = string[index];
        ++index;
    }
    text->chunkOffset = index;
    u_terminateUChars_nid_postfix(destination, destinationCapacity, written, status);
    return written;
}

UText* APS5_VABI StringClone(UText* destination, const UText* source, std::int8_t deep, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (deep != 0) {
        const auto* string = static_cast<const char16_t*>(source->context);
        const std::int64_t length = source->a;
        if (length < 0) {
            destination = ShallowClone(destination, source, status);
            return destination;
        }
        auto* copy = static_cast<char16_t*>(uprv_malloc_nid_postfix(
            static_cast<std::size_t>(length + 1) * sizeof(char16_t)));
        if (copy == nullptr) {
            *status = MemoryAllocationError;
            return nullptr;
        }
        std::memcpy(copy, string, static_cast<std::size_t>(length) * sizeof(char16_t));
        copy[length] = 0;
        destination = utext_openUChars_nid_postfix(destination, copy, length, status);
        if (Failed(status)) {
            ReleaseText(copy);
            return destination;
        }
        destination->providerProperties |= ProviderOwnsText;
        utext_setNativeIndex_nid_postfix(destination, utext_getNativeIndex_nid_postfix(source));
        return destination;
    }
    destination = ShallowClone(destination, source, status);
    return destination;
}

const UTextFuncs StringFunctions = {
    sizeof(UTextFuncs),
    0,
    0,
    0,
    StringClone,
    StringLength,
    StringAccess,
    StringExtract,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    StringClose,
    nullptr,
    nullptr,
    nullptr,
};

void Access(UText* text, std::int64_t index, std::int8_t forward, std::int8_t* result) {
    *result = text->pFuncs->access(text, index, forward);
}

}

extern "C" {

UText* APS5_VABI utext_setup_nid_postfix(UText* text, std::int32_t extraSpace, std::int32_t* status) {
    if (Failed(status)) return text;
    if (text == nullptr) {
        std::int32_t spaceRequired = static_cast<std::int32_t>(sizeof(UText));
        if (extraSpace > 0) {
            spaceRequired = static_cast<std::int32_t>(sizeof(ExtendedText)) + extraSpace -
                static_cast<std::int32_t>(sizeof(std::max_align_t));
        }
        text = AllocateText(static_cast<std::size_t>(spaceRequired));
        if (text == nullptr) {
            *status = MemoryAllocationError;
            return nullptr;
        }
        std::memset(text, 0, static_cast<std::size_t>(spaceRequired));
        text->magic = Magic;
        text->sizeOfStruct = static_cast<std::int32_t>(sizeof(UText));
        text->flags |= HeapAllocated;
        if (spaceRequired > 0) {
            text->extraSize = extraSpace;
            text->pExtra = reinterpret_cast<UText*>(&reinterpret_cast<ExtendedText*>(text)->extension);
        }
    } else {
        if (text->magic != Magic) {
            *status = Icu::IllegalArgument;
            return text;
        }
        if ((text->flags & Open) != 0 && text->pFuncs != nullptr && text->pFuncs->close != nullptr) {
            text->pFuncs->close(text);
        }
        text->flags &= ~Open;
        if (extraSpace > text->extraSize) {
            if ((text->flags & ExtraHeapAllocated) != 0) {
                ReleaseText(text->pExtra);
                text->extraSize = 0;
            }
            text->pExtra = static_cast<UText*>(uprv_malloc_nid_postfix(static_cast<std::size_t>(extraSpace)));
            if (text->pExtra == nullptr) {
                *status = MemoryAllocationError;
            } else {
                text->extraSize = extraSpace;
                text->flags |= ExtraHeapAllocated;
            }
        }
    }
    if (!Failed(status)) {
        text->flags |= Open;
        text->context = nullptr;
        text->chunkContents = nullptr;
        text->p = nullptr;
        text->q = nullptr;
        text->r = nullptr;
        text->a = 0;
        text->b = 0;
        text->c = 0;
        text->chunkOffset = 0;
        text->chunkLength = 0;
        text->chunkNativeStart = 0;
        text->chunkNativeLimit = 0;
        text->nativeIndexingLimit = 0;
        text->providerProperties = 0;
        text->privA = 0;
        text->privB = 0;
        text->privC = 0;
        text->privP = nullptr;
        if (text->pExtra != nullptr && text->extraSize > 0) {
            std::memset(text->pExtra, 0, static_cast<std::size_t>(text->extraSize));
        }
    }
    return text;
}

UText* APS5_VABI utext_openUChars_nid_postfix(UText* text, const char16_t* source, std::int64_t length,
    std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (source == nullptr && length == 0) source = EmptyText;
    if (source == nullptr || length < -1 || length > INT32_MAX) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    text = utext_setup_nid_postfix(text, 0, status);
    if (!Failed(status)) {
        text->pFuncs = &StringFunctions;
        text->context = source;
        text->providerProperties = ProviderStableChunks;
        if (length == -1) text->providerProperties |= ProviderLengthIsExpensive;
        text->a = length;
        text->chunkContents = source;
        text->chunkNativeStart = 0;
        text->chunkNativeLimit = length >= 0 ? length : 0;
        text->chunkLength = static_cast<std::int32_t>(text->chunkNativeLimit);
        text->chunkOffset = 0;
        text->nativeIndexingLimit = text->chunkLength;
    }
    return text;
}

UText* APS5_VABI utext_clone_nid_postfix(UText* destination, const UText* source, std::int8_t deep,
    std::int8_t readOnly, std::int32_t* status) {
    if (Failed(status) || !Valid(source) || source->pFuncs == nullptr) return destination;
    UText* result = source->pFuncs->clone(destination, source, deep, status);
    if (Failed(status)) return result;
    if (result == nullptr) {
        *status = MemoryAllocationError;
        return result;
    }
    if (readOnly != 0) result->providerProperties &= ~ProviderWritable;
    return result;
}

std::int8_t APS5_VABI utext_equals_nid_postfix(const UText* first, const UText* second) {
    if (!Valid(first) || !Valid(second)) return 0;
    if (first->pFuncs != second->pFuncs) return 0;
    if (first->context != second->context) return 0;
    if (utext_getNativeIndex_nid_postfix(first) != utext_getNativeIndex_nid_postfix(second)) return 0;
    return 1;
}

std::int64_t APS5_VABI utext_nativeLength_nid_postfix(UText* text) {
    if (!Valid(text) || text->pFuncs == nullptr) return 0;
    return text->pFuncs->nativeLength(text);
}

std::int8_t APS5_VABI utext_isLengthExpensive_nid_postfix(const UText* text) {
    if (!Valid(text)) return 0;
    return static_cast<std::int8_t>((text->providerProperties & ProviderLengthIsExpensive) != 0);
}

std::int8_t APS5_VABI utext_isWritable_nid_postfix(const UText* text) {
    if (!Valid(text)) return 0;
    return static_cast<std::int8_t>((text->providerProperties & ProviderWritable) != 0);
}

std::int8_t APS5_VABI utext_hasMetaData_nid_postfix(const UText* text) {
    if (!Valid(text)) return 0;
    return static_cast<std::int8_t>((text->providerProperties & ProviderHasMetaData) != 0);
}

void APS5_VABI utext_freeze_nid_postfix(UText* text) {
    if (!Valid(text)) return;
    text->providerProperties &= ~ProviderWritable;
}

std::int64_t APS5_VABI utext_getNativeIndex_nid_postfix(const UText* text) {
    if (!Valid(text) || text->pFuncs == nullptr) return 0;
    if (text->chunkOffset <= text->nativeIndexingLimit) return text->chunkNativeStart + text->chunkOffset;
    return text->pFuncs->mapOffsetToNative(text);
}

void APS5_VABI utext_setNativeIndex_nid_postfix(UText* text, std::int64_t index) {
    if (!Valid(text) || text->pFuncs == nullptr) return;
    if (index < text->chunkNativeStart || index >= text->chunkNativeLimit) {
        std::int8_t result = 0;
        Access(text, index, 1, &result);
    } else if (static_cast<std::int32_t>(index - text->chunkNativeStart) <= text->nativeIndexingLimit) {
        text->chunkOffset = static_cast<std::int32_t>(index - text->chunkNativeStart);
    } else {
        text->chunkOffset = text->pFuncs->mapNativeIndexToUTF16(text, index);
    }
    if (text->chunkOffset < text->chunkLength) {
        const char16_t current = text->chunkContents[text->chunkOffset];
        if (U16_IS_TRAIL(current)) {
            if (text->chunkOffset == 0) {
                std::int8_t result = 0;
                Access(text, text->chunkNativeStart, 0, &result);
            }
            if (text->chunkOffset > 0) {
                const char16_t lead = text->chunkContents[text->chunkOffset - 1];
                if (U16_IS_LEAD(lead)) --text->chunkOffset;
            }
        }
    }
}

std::int64_t APS5_VABI utext_getPreviousNativeIndex_nid_postfix(UText* text) {
    if (!Valid(text) || text->pFuncs == nullptr) return 0;
    const std::int32_t index = text->chunkOffset - 1;
    if (index >= 0) {
        const char16_t current = text->chunkContents[index];
        if (!U16_IS_TRAIL(current)) {
            if (index <= text->nativeIndexingLimit) return text->chunkNativeStart + index;
            text->chunkOffset = index;
            const std::int64_t result = text->pFuncs->mapOffsetToNative(text);
            ++text->chunkOffset;
            return result;
        }
    }
    if (text->chunkOffset == 0 && text->chunkNativeStart == 0) return 0;
    utext_previous32_nid_postfix(text);
    const std::int64_t result = utext_getNativeIndex_nid_postfix(text);
    utext_next32_nid_postfix(text);
    return result;
}

std::int8_t APS5_VABI utext_moveIndex32_nid_postfix(UText* text, std::int32_t delta) {
    if (!Valid(text) || text->pFuncs == nullptr) return 0;
    if (delta > 0) {
        do {
            if (text->chunkOffset >= text->chunkLength) {
                std::int8_t available = 0;
                Access(text, text->chunkNativeLimit, 1, &available);
                if (available == 0) return 0;
            }
            std::int32_t current = text->chunkContents[text->chunkOffset];
            if (U16_IS_SURROGATE(current)) {
                current = utext_next32_nid_postfix(text);
                if (current == Sentinel) return 0;
            } else {
                ++text->chunkOffset;
            }
        } while (--delta > 0);
    } else if (delta < 0) {
        do {
            if (text->chunkOffset <= 0) {
                std::int8_t available = 0;
                Access(text, text->chunkNativeStart, 0, &available);
                if (available == 0) return 0;
            }
            std::int32_t current = text->chunkContents[text->chunkOffset - 1];
            if (U16_IS_SURROGATE(current)) {
                current = utext_previous32_nid_postfix(text);
                if (current == Sentinel) return 0;
            } else {
                --text->chunkOffset;
            }
        } while (++delta < 0);
    }
    return 1;
}

std::int32_t APS5_VABI utext_current32_nid_postfix(UText* text) {
    if (!Valid(text) || text->pFuncs == nullptr) return Sentinel;
    if (text->chunkOffset == text->chunkLength) {
        std::int8_t available = 0;
        Access(text, text->chunkNativeLimit, 1, &available);
        if (available == 0) return Sentinel;
    }
    std::int32_t current = text->chunkContents[text->chunkOffset];
    if (!U16_IS_LEAD(current)) return current;
    std::int32_t trail = 0;
    if ((text->chunkOffset + 1) < text->chunkLength) {
        trail = text->chunkContents[text->chunkOffset + 1];
    } else {
        const std::int64_t nativePosition = text->chunkNativeLimit;
        std::int8_t available = 0;
        Access(text, nativePosition, 1, &available);
        if (available != 0) trail = text->chunkContents[text->chunkOffset];
        Access(text, nativePosition, 0, &available);
        text->chunkOffset = text->chunkLength - 1;
        if (available == 0) return Sentinel;
    }
    if (U16_IS_TRAIL(trail)) return U16_GET_SUPPLEMENTARY(current, trail);
    return current;
}

std::int32_t APS5_VABI utext_next32_nid_postfix(UText* text) {
    if (!Valid(text) || text->pFuncs == nullptr) return Sentinel;
    if (text->chunkOffset >= text->chunkLength) {
        std::int8_t available = 0;
        Access(text, text->chunkNativeLimit, 1, &available);
        if (available == 0) return Sentinel;
    }
    std::int32_t current = text->chunkContents[text->chunkOffset++];
    if (!U16_IS_LEAD(current)) return current;
    if (text->chunkOffset >= text->chunkLength) {
        std::int8_t available = 0;
        Access(text, text->chunkNativeLimit, 1, &available);
        if (available == 0) return current;
    }
    const std::int32_t trail = text->chunkContents[text->chunkOffset];
    if (!U16_IS_TRAIL(trail)) return current;
    ++text->chunkOffset;
    return U16_GET_SUPPLEMENTARY(current, trail);
}

std::int32_t APS5_VABI utext_previous32_nid_postfix(UText* text) {
    if (!Valid(text) || text->pFuncs == nullptr) return Sentinel;
    if (text->chunkOffset <= 0) {
        std::int8_t available = 0;
        Access(text, text->chunkNativeStart, 0, &available);
        if (available == 0) return Sentinel;
    }
    --text->chunkOffset;
    std::int32_t current = text->chunkContents[text->chunkOffset];
    if (!U16_IS_TRAIL(current)) return current;
    if (text->chunkOffset <= 0) {
        std::int8_t available = 0;
        Access(text, text->chunkNativeStart, 0, &available);
        if (available == 0) return current;
    }
    const std::int32_t lead = text->chunkContents[text->chunkOffset - 1];
    if (!U16_IS_LEAD(lead)) return current;
    --text->chunkOffset;
    return U16_GET_SUPPLEMENTARY(lead, current);
}

std::int32_t APS5_VABI utext_next32From_nid_postfix(UText* text, std::int64_t index) {
    if (!Valid(text) || text->pFuncs == nullptr) return Sentinel;
    if (index < text->chunkNativeStart || index >= text->chunkNativeLimit) {
        std::int8_t available = 0;
        Access(text, index, 1, &available);
        if (available == 0) return Sentinel;
    } else if (index - text->chunkNativeStart <= text->nativeIndexingLimit) {
        text->chunkOffset = static_cast<std::int32_t>(index - text->chunkNativeStart);
    } else {
        text->chunkOffset = text->pFuncs->mapNativeIndexToUTF16(text, index);
    }
    std::int32_t current = text->chunkContents[text->chunkOffset++];
    if (U16_IS_SURROGATE(current)) {
        utext_setNativeIndex_nid_postfix(text, index);
        current = utext_next32_nid_postfix(text);
    }
    return current;
}

std::int32_t APS5_VABI utext_previous32From_nid_postfix(UText* text, std::int64_t index) {
    if (!Valid(text) || text->pFuncs == nullptr) return Sentinel;
    if (index <= text->chunkNativeStart || index > text->chunkNativeLimit) {
        std::int8_t available = 0;
        Access(text, index, 0, &available);
        if (available == 0) return Sentinel;
    } else if (index - text->chunkNativeStart <= text->nativeIndexingLimit) {
        text->chunkOffset = static_cast<std::int32_t>(index - text->chunkNativeStart);
    } else {
        text->chunkOffset = text->pFuncs->mapNativeIndexToUTF16(text, index);
        if (text->chunkOffset == 0) {
            std::int8_t available = 0;
            Access(text, index, 0, &available);
            if (available == 0) return Sentinel;
        }
    }
    --text->chunkOffset;
    std::int32_t current = text->chunkContents[text->chunkOffset];
    if (U16_IS_SURROGATE(current)) {
        utext_setNativeIndex_nid_postfix(text, index);
        current = utext_previous32_nid_postfix(text);
    }
    return current;
}

std::int32_t APS5_VABI utext_char32At_nid_postfix(UText* text, std::int64_t nativeIndex) {
    if (!Valid(text) || text->pFuncs == nullptr) return Sentinel;
    std::int32_t current = Sentinel;
    if (nativeIndex >= text->chunkNativeStart && nativeIndex < text->chunkNativeStart + text->nativeIndexingLimit) {
        text->chunkOffset = static_cast<std::int32_t>(nativeIndex - text->chunkNativeStart);
        current = text->chunkContents[text->chunkOffset];
        if (!U16_IS_SURROGATE(current)) return current;
    }
    utext_setNativeIndex_nid_postfix(text, nativeIndex);
    if (nativeIndex >= text->chunkNativeStart && text->chunkOffset < text->chunkLength) {
        current = text->chunkContents[text->chunkOffset];
        if (U16_IS_SURROGATE(current)) current = utext_current32_nid_postfix(text);
    }
    return current;
}

std::int32_t APS5_VABI utext_extract_nid_postfix(UText* text, std::int64_t start, std::int64_t limit,
    char16_t* destination, std::int32_t destinationCapacity, std::int32_t* status) {
    if (Failed(status) || !Valid(text) || text->pFuncs == nullptr || text->pFuncs->extract == nullptr) {
        return 0;
    }
    return text->pFuncs->extract(text, start, limit, destination, destinationCapacity, status);
}

std::int32_t APS5_VABI utext_replace_nid_postfix(UText* text, std::int64_t nativeStart, std::int64_t nativeLimit,
    const char16_t* replacement, std::int32_t replacementLength, std::int32_t* status) {
    if (Failed(status) || !Valid(text) || text->pFuncs == nullptr) return 0;
    if ((text->providerProperties & ProviderWritable) == 0) {
        *status = NoWritePermission;
        return 0;
    }
    if (text->pFuncs->replace == nullptr) {
        *status = NoWritePermission;
        return 0;
    }
    return text->pFuncs->replace(text, nativeStart, nativeLimit, replacement, replacementLength, status);
}

void APS5_VABI utext_copy_nid_postfix(UText* text, std::int64_t nativeStart, std::int64_t nativeLimit,
    std::int64_t destinationIndex, std::int8_t move, std::int32_t* status) {
    if (Failed(status) || !Valid(text) || text->pFuncs == nullptr) return;
    if ((text->providerProperties & ProviderWritable) == 0) {
        *status = NoWritePermission;
        return;
    }
    if (text->pFuncs->copy == nullptr) {
        *status = NoWritePermission;
        return;
    }
    text->pFuncs->copy(text, nativeStart, nativeLimit, destinationIndex, move, status);
}

}
