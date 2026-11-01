#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

constexpr std::int32_t Sentinel = -1;
constexpr std::int32_t UnknownIndex = -2;
constexpr std::uint32_t NoState = 0xffffffff;
constexpr std::int32_t Replacement = 0xfffd;
constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t IndexOutOfBoundsError = 8;
constexpr std::int32_t UnsupportedError = 16;

constexpr std::uint8_t Lead3Trail1[16] = {0x20, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x10, 0x30, 0x30};
constexpr std::uint8_t Lead4Trail1[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0x1e, 0x0f, 0x0f, 0x0f, 0, 0, 0, 0};

std::int32_t Wrap(std::int64_t value) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

bool IsTrail(std::uint8_t byte) {
    return static_cast<std::int8_t>(byte) < -0x40;
}

bool IsLead(std::uint8_t byte) {
    return static_cast<std::uint8_t>(byte - 0xc2) <= 0x32;
}

bool ValidLead3(std::uint8_t lead, std::uint8_t trail) {
    return (Lead3Trail1[lead & 0xf] & (1 << (trail >> 5))) != 0;
}

bool ValidLead4(std::uint8_t lead, std::uint8_t trail) {
    return (Lead4Trail1[trail >> 4] & (1 << (lead & 7))) != 0;
}

std::int32_t Utf16Length(std::int32_t codePoint) {
    return static_cast<std::uint32_t>(codePoint) <= 0xffff ? 1 : 2;
}

std::int32_t LeadSurrogate(std::int32_t codePoint) {
    return static_cast<char16_t>((codePoint >> 10) + 0xd7c0);
}

std::int32_t TrailSurrogate(std::int32_t codePoint) {
    return static_cast<char16_t>((codePoint & 0x3ff) | 0xdc00);
}

std::int32_t NextCodePoint(const std::uint8_t* text, std::int32_t& index, std::int32_t length) {
    std::int32_t codePoint = text[index++];
    if (codePoint < 0x80) return codePoint;
    if (index == length) return Replacement;
    std::uint8_t trail = 0;
    bool valid;
    if (codePoint >= 0xe0) {
        if (codePoint < 0xf0) {
            codePoint &= 0xf;
            trail = text[index];
            valid = (Lead3Trail1[codePoint] & (1 << (trail >> 5))) != 0;
            trail &= 0x3f;
        } else {
            codePoint -= 0xf0;
            valid = codePoint <= 4;
            if (valid) {
                trail = text[index];
                valid = (Lead4Trail1[trail >> 4] & (1 << codePoint)) != 0;
            }
            if (valid) {
                codePoint = (codePoint << 6) | (trail & 0x3f);
                valid = ++index != length;
            }
            if (valid) {
                trail = static_cast<std::uint8_t>(text[index] - 0x80);
                valid = trail <= 0x3f;
            }
        }
        if (valid) {
            codePoint = (codePoint << 6) | trail;
            valid = ++index != length;
        }
    } else {
        valid = codePoint >= 0xc2;
        codePoint &= 0x1f;
    }
    if (!valid) return Replacement;
    trail = static_cast<std::uint8_t>(text[index] - 0x80);
    if (trail > 0x3f) return Replacement;
    ++index;
    return (codePoint << 6) | trail;
}

std::int32_t PreviousCodePoint(const std::uint8_t* text, std::int32_t start, std::int32_t& index) {
    const std::uint8_t last = text[--index];
    if (last < 0x80) return last;
    std::int32_t position = index;
    if (!IsTrail(last) || position <= start) return Replacement;
    const std::uint8_t first = text[--position];
    if (IsLead(first)) {
        if (first < 0xe0) {
            index = position;
            return ((first - 0xc0) << 6) | (last & 0x3f);
        }
        if (first < 0xf0 ? ValidLead3(first, last) : ValidLead4(first, last)) index = position;
        return Replacement;
    }
    if (!IsTrail(first) || position <= start) return Replacement;
    const std::uint8_t second = text[--position];
    if (0xe0 <= second && second <= 0xf4) {
        if (second < 0xf0) {
            if (!ValidLead3(second, first)) return Replacement;
            index = position;
            return ((second & 0xf) << 12) | ((first & 0x3f) << 6) | (last & 0x3f);
        }
        if (ValidLead4(second, first)) index = position;
        return Replacement;
    }
    if (!IsTrail(second) || position <= start) return Replacement;
    const std::uint8_t lead = text[--position];
    if (lead < 0xf0 || lead > 0xf4 || !ValidLead4(lead, second)) return Replacement;
    index = position;
    return ((lead & 7) << 18) | ((second & 0x3f) << 12) | ((first & 0x3f) << 6) | (last & 0x3f);
}

UCharIterator& Check(UCharIterator* iterator) {
    if (!iterator) APS5_INVALID_ARG_EX;
    return *iterator;
}

const std::uint8_t* Bytes(const UCharIterator& iterator) {
    return static_cast<const std::uint8_t*>(iterator.context);
}

const char16_t* Units(const UCharIterator& iterator) {
    return static_cast<const char16_t*>(iterator.context);
}

std::int32_t APS5_VABI NoopGetIndex_nid_no_patch(UCharIterator*, UCharIteratorOrigin) {
    return 0;
}

std::int32_t APS5_VABI NoopMove_nid_no_patch(UCharIterator*, std::int32_t, UCharIteratorOrigin) {
    return 0;
}

std::int8_t APS5_VABI NoopHasNext_nid_no_patch(UCharIterator*) {
    return 0;
}

std::int32_t APS5_VABI NoopCurrent_nid_no_patch(UCharIterator*) {
    return Sentinel;
}

std::uint32_t APS5_VABI NoopGetState_nid_no_patch(const UCharIterator*) {
    return NoState;
}

void APS5_VABI NoopSetState_nid_no_patch(UCharIterator*, std::uint32_t, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    *error = UnsupportedError;
}

std::int32_t APS5_VABI StringGetIndex_nid_no_patch(UCharIterator* handle, UCharIteratorOrigin origin) {
    const auto& iterator = Check(handle);
    switch (origin) {
    case UITER_ZERO: return 0;
    case UITER_START: return iterator.start;
    case UITER_CURRENT: return iterator.index;
    case UITER_LIMIT: return iterator.limit;
    case UITER_LENGTH: return iterator.length;
    default: return -1;
    }
}

std::int32_t APS5_VABI StringMove_nid_no_patch(UCharIterator* handle, std::int32_t delta, UCharIteratorOrigin origin) {
    auto& iterator = Check(handle);
    std::int32_t position;
    switch (origin) {
    case UITER_ZERO: position = delta; break;
    case UITER_START: position = Wrap(std::int64_t{iterator.start} + delta); break;
    case UITER_CURRENT: position = Wrap(std::int64_t{iterator.index} + delta); break;
    case UITER_LIMIT: position = Wrap(std::int64_t{iterator.limit} + delta); break;
    case UITER_LENGTH: position = Wrap(std::int64_t{iterator.length} + delta); break;
    default: return -1;
    }
    if (position < iterator.start) position = iterator.start;
    else if (position > iterator.limit) position = iterator.limit;
    return iterator.index = position;
}

std::int8_t APS5_VABI StringHasNext_nid_no_patch(UCharIterator* handle) {
    const auto& iterator = Check(handle);
    return iterator.index < iterator.limit;
}

std::int8_t APS5_VABI StringHasPrevious_nid_no_patch(UCharIterator* handle) {
    const auto& iterator = Check(handle);
    return iterator.index > iterator.start;
}

std::int32_t APS5_VABI StringCurrent_nid_no_patch(UCharIterator* handle) {
    const auto& iterator = Check(handle);
    return iterator.index < iterator.limit ? Units(iterator)[iterator.index] : Sentinel;
}

std::int32_t APS5_VABI StringNext_nid_no_patch(UCharIterator* handle) {
    auto& iterator = Check(handle);
    return iterator.index < iterator.limit ? Units(iterator)[iterator.index++] : Sentinel;
}

std::int32_t APS5_VABI StringPrevious_nid_no_patch(UCharIterator* handle) {
    auto& iterator = Check(handle);
    return iterator.index > iterator.start ? Units(iterator)[--iterator.index] : Sentinel;
}

std::uint32_t APS5_VABI StringGetState_nid_no_patch(const UCharIterator* handle) {
    if (!handle) APS5_INVALID_ARG_EX;
    return static_cast<std::uint32_t>(handle->index);
}

void APS5_VABI StringSetState_nid_no_patch(UCharIterator* iterator, std::uint32_t state, std::int32_t* error) {
    if (!error || *error > 0) return;
    if (!iterator) {
        *error = IllegalArgumentError;
        return;
    }
    const auto position = static_cast<std::int32_t>(state);
    if (position < iterator->start || iterator->limit < position) *error = IndexOutOfBoundsError;
    else iterator->index = position;
}

std::int32_t Utf16Units(const std::uint8_t* text, std::int32_t& byte, std::int32_t limit) {
    std::int32_t units = 0;
    while (byte < limit) units += Utf16Length(NextCodePoint(text, byte, limit));
    return units;
}

std::int32_t APS5_VABI Utf8GetIndex_nid_no_patch(UCharIterator* handle, UCharIteratorOrigin origin) {
    auto& iterator = Check(handle);
    switch (origin) {
    case UITER_ZERO:
    case UITER_START:
        return 0;
    case UITER_CURRENT:
        if (iterator.index < 0) {
            std::int32_t byte = 0;
            std::int32_t index = Utf16Units(Bytes(iterator), byte, iterator.start);
            iterator.start = byte;
            if (byte == iterator.limit) iterator.length = index;
            if (iterator.reservedField != 0) --index;
            iterator.index = index;
        }
        return iterator.index;
    case UITER_LIMIT:
    case UITER_LENGTH:
        if (iterator.length < 0) {
            std::int32_t byte;
            std::int32_t length;
            if (iterator.index < 0) {
                byte = 0;
                length = Utf16Units(Bytes(iterator), byte, iterator.start);
                iterator.start = byte;
                iterator.index = iterator.reservedField != 0 ? length - 1 : length;
            } else {
                byte = iterator.start;
                length = iterator.index;
                if (iterator.reservedField != 0) ++length;
            }
            iterator.length = length + Utf16Units(Bytes(iterator), byte, iterator.limit);
        }
        return iterator.length;
    default:
        return -1;
    }
}

std::int32_t APS5_VABI Utf8Move_nid_no_patch(UCharIterator* handle, std::int32_t delta, UCharIteratorOrigin origin) {
    auto& iterator = Check(handle);
    std::int32_t position;
    bool havePosition;
    switch (origin) {
    case UITER_ZERO:
    case UITER_START:
        position = delta;
        havePosition = true;
        break;
    case UITER_CURRENT:
        havePosition = iterator.index >= 0;
        position = havePosition ? Wrap(std::int64_t{iterator.index} + delta) : 0;
        break;
    case UITER_LIMIT:
    case UITER_LENGTH:
        if (iterator.length >= 0) {
            position = Wrap(std::int64_t{iterator.length} + delta);
            havePosition = true;
        } else {
            iterator.index = -1;
            iterator.start = iterator.limit;
            iterator.reservedField = 0;
            if (delta >= 0) return UnknownIndex;
            position = 0;
            havePosition = false;
        }
        break;
    default:
        return -1;
    }
    if (havePosition) {
        if (position <= 0) {
            iterator.index = iterator.start = iterator.reservedField = 0;
            return 0;
        }
        if (iterator.length >= 0 && position >= iterator.length) {
            iterator.index = iterator.length;
            iterator.start = iterator.limit;
            iterator.reservedField = 0;
            return iterator.index;
        }
        if (iterator.index < 0 || position < iterator.index / 2) {
            iterator.index = iterator.start = iterator.reservedField = 0;
        } else if (iterator.length >= 0 && iterator.length - position < position - iterator.index) {
            iterator.index = iterator.length;
            iterator.start = iterator.limit;
            iterator.reservedField = 0;
        }
        delta = position - iterator.index;
        if (delta == 0) return iterator.index;
    } else {
        if (delta == 0) return UnknownIndex;
        if (Wrap(-std::int64_t{delta}) >= iterator.start) {
            iterator.index = iterator.start = iterator.reservedField = 0;
            return 0;
        }
        if (delta >= iterator.limit - iterator.start) {
            iterator.index = iterator.length;
            iterator.start = iterator.limit;
            iterator.reservedField = 0;
            return iterator.index >= 0 ? iterator.index : UnknownIndex;
        }
    }
    const std::uint8_t* text = Bytes(iterator);
    position = iterator.index;
    std::int32_t byte = iterator.start;
    if (delta > 0) {
        const std::int32_t limit = iterator.limit;
        if (iterator.reservedField != 0) {
            iterator.reservedField = 0;
            ++position;
            --delta;
        }
        while (delta > 0 && byte < limit) {
            const std::int32_t codePoint = NextCodePoint(text, byte, limit);
            if (codePoint <= 0xffff) {
                ++position;
                --delta;
            } else if (delta >= 2) {
                position += 2;
                delta -= 2;
            } else {
                iterator.reservedField = codePoint;
                ++position;
                break;
            }
        }
        if (byte == limit) {
            if (iterator.length < 0 && iterator.index >= 0) {
                iterator.length = iterator.reservedField == 0 ? position : position + 1;
            } else if (iterator.index < 0 && iterator.length >= 0) {
                iterator.index = iterator.reservedField == 0 ? iterator.length : iterator.length - 1;
            }
        }
    } else {
        if (iterator.reservedField != 0) {
            iterator.reservedField = 0;
            byte -= 4;
            --position;
            ++delta;
        }
        while (delta < 0 && byte > 0) {
            const std::int32_t codePoint = PreviousCodePoint(text, 0, byte);
            if (codePoint <= 0xffff) {
                --position;
                ++delta;
            } else if (delta <= -2) {
                position -= 2;
                delta += 2;
            } else {
                byte += 4;
                iterator.reservedField = codePoint;
                --position;
                break;
            }
        }
    }
    iterator.start = byte;
    if (iterator.index >= 0) return iterator.index = position;
    if (byte <= 1) return iterator.index = byte;
    return UnknownIndex;
}

std::int8_t APS5_VABI Utf8HasNext_nid_no_patch(UCharIterator* handle) {
    const auto& iterator = Check(handle);
    return iterator.start < iterator.limit || iterator.reservedField != 0;
}

std::int8_t APS5_VABI Utf8HasPrevious_nid_no_patch(UCharIterator* handle) {
    return Check(handle).start > 0;
}

std::int32_t APS5_VABI Utf8Current_nid_no_patch(UCharIterator* handle) {
    const auto& iterator = Check(handle);
    if (iterator.reservedField != 0) return TrailSurrogate(iterator.reservedField);
    if (iterator.start >= iterator.limit) return Sentinel;
    std::int32_t byte = iterator.start;
    const std::int32_t codePoint = NextCodePoint(Bytes(iterator), byte, iterator.limit);
    return codePoint <= 0xffff ? codePoint : LeadSurrogate(codePoint);
}

std::int32_t APS5_VABI Utf8Next_nid_no_patch(UCharIterator* handle) {
    auto& iterator = Check(handle);
    if (iterator.reservedField != 0) {
        const std::int32_t trail = TrailSurrogate(iterator.reservedField);
        iterator.reservedField = 0;
        if (iterator.index >= 0) ++iterator.index;
        return trail;
    }
    if (iterator.start >= iterator.limit) return Sentinel;
    const std::int32_t codePoint = NextCodePoint(Bytes(iterator), iterator.start, iterator.limit);
    if (iterator.index >= 0) {
        const std::int32_t index = ++iterator.index;
        if (iterator.length < 0 && iterator.start == iterator.limit) iterator.length = codePoint <= 0xffff ? index : index + 1;
    } else if (iterator.start == iterator.limit && iterator.length >= 0) {
        iterator.index = codePoint <= 0xffff ? iterator.length : iterator.length - 1;
    }
    if (codePoint <= 0xffff) return codePoint;
    iterator.reservedField = codePoint;
    return LeadSurrogate(codePoint);
}

std::int32_t APS5_VABI Utf8Previous_nid_no_patch(UCharIterator* handle) {
    auto& iterator = Check(handle);
    if (iterator.reservedField != 0) {
        const std::int32_t lead = LeadSurrogate(iterator.reservedField);
        iterator.reservedField = 0;
        iterator.start -= 4;
        if (iterator.index > 0) --iterator.index;
        return lead;
    }
    if (iterator.start <= 0) return Sentinel;
    const std::int32_t codePoint = PreviousCodePoint(Bytes(iterator), 0, iterator.start);
    if (iterator.index > 0) --iterator.index;
    else if (iterator.start <= 1) iterator.index = codePoint <= 0xffff ? iterator.start : iterator.start + 1;
    if (codePoint <= 0xffff) return codePoint;
    iterator.start += 4;
    iterator.reservedField = codePoint;
    return TrailSurrogate(codePoint);
}

std::uint32_t APS5_VABI Utf8GetState_nid_no_patch(const UCharIterator* handle) {
    if (!handle) APS5_INVALID_ARG_EX;
    std::uint32_t state = static_cast<std::uint32_t>(handle->start) << 1;
    if (handle->reservedField != 0) state |= 1;
    return state;
}

void APS5_VABI Utf8SetState_nid_no_patch(UCharIterator* iterator, std::uint32_t state, std::int32_t* error) {
    if (!error || *error > 0) return;
    if (!iterator) {
        *error = IllegalArgumentError;
        return;
    }
    if (state == Utf8GetState_nid_no_patch(iterator)) return;
    const auto index = static_cast<std::int32_t>(state >> 1);
    const bool inside = (state & 1) != 0;
    if ((inside ? index < 4 : index < 0) || iterator->limit < index) {
        *error = IndexOutOfBoundsError;
        return;
    }
    iterator->start = index;
    iterator->index = index <= 1 ? index : -1;
    if (!inside) {
        iterator->reservedField = 0;
        return;
    }
    std::int32_t byte = index;
    const std::int32_t codePoint = PreviousCodePoint(Bytes(*iterator), 0, byte);
    if (codePoint <= 0xffff) *error = IndexOutOfBoundsError;
    else iterator->reservedField = codePoint;
}

constexpr UCharIterator NoopIterator{nullptr, 0, 0, 0, 0, 0, NoopGetIndex_nid_no_patch, NoopMove_nid_no_patch, NoopHasNext_nid_no_patch, NoopHasNext_nid_no_patch,
                                     NoopCurrent_nid_no_patch, NoopCurrent_nid_no_patch, NoopCurrent_nid_no_patch, nullptr, NoopGetState_nid_no_patch, NoopSetState_nid_no_patch};
constexpr UCharIterator StringIterator{nullptr, 0, 0, 0, 0, 0, StringGetIndex_nid_no_patch, StringMove_nid_no_patch, StringHasNext_nid_no_patch, StringHasPrevious_nid_no_patch,
                                       StringCurrent_nid_no_patch, StringNext_nid_no_patch, StringPrevious_nid_no_patch, nullptr, StringGetState_nid_no_patch, StringSetState_nid_no_patch};
constexpr UCharIterator Utf8Iterator{nullptr, 0, 0, 0, 0, 0, Utf8GetIndex_nid_no_patch, Utf8Move_nid_no_patch, Utf8HasNext_nid_no_patch, Utf8HasPrevious_nid_no_patch,
                                     Utf8Current_nid_no_patch, Utf8Next_nid_no_patch, Utf8Previous_nid_no_patch, nullptr, Utf8GetState_nid_no_patch, Utf8SetState_nid_no_patch};

}

extern "C" {

void APS5_VABI uiter_setString_nid_postfix(UCharIterator* iterator, const char16_t* text, std::int32_t length) {
    if (!iterator) return;
    if (!text || length < -1) {
        *iterator = NoopIterator;
        return;
    }
    *iterator = StringIterator;
    iterator->context = text;
    iterator->length = length >= 0 ? length : static_cast<std::int32_t>(std::char_traits<char16_t>::length(text));
    iterator->limit = iterator->length;
}

void APS5_VABI uiter_setUTF8_nid_postfix(UCharIterator* iterator, const char* text, std::int32_t length) {
    if (!iterator) return;
    if (!text || length < -1) {
        *iterator = NoopIterator;
        return;
    }
    *iterator = Utf8Iterator;
    iterator->context = text;
    iterator->limit = length >= 0 ? length : static_cast<std::int32_t>(std::strlen(text));
    iterator->length = iterator->limit <= 1 ? iterator->limit : -1;
}

}
