// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace {

using namespace Icu::Abi;
constexpr char16_t Done = 0xffff;

template<typename TValue>
TValue& Require(TValue* pointer) {
    if (!pointer) throw std::invalid_argument("ICU character iterator: null pointer");
    return *pointer;
}

std::int32_t Checked(std::int64_t value) {
    if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("ICU character iterator: integer overflow");
    return static_cast<std::int32_t>(value);
}

char16_t Unit(const UCharCharacterIterator& iterator, std::int32_t index) {
    if (!iterator.text || index < 0) throw std::invalid_argument("ICU character iterator: invalid text access");
    return iterator.text[index];
}

bool Lead(char16_t unit) { return unit >= 0xd800 && unit <= 0xdbff; }
bool Trail(char16_t unit) { return unit >= 0xdc00 && unit <= 0xdfff; }
std::int32_t Pair(char16_t lead, char16_t trail) { return 0x10000 + ((lead - 0xd800) << 10) + trail - 0xdc00; }

std::int32_t Forward(const UCharCharacterIterator& iterator, std::int32_t& position) {
    const auto unit = Unit(iterator, position);
    position = Checked(static_cast<std::int64_t>(position) + 1);
    if (Lead(unit) && position != iterator.base.end) {
        const auto trail = Unit(iterator, position);
        if (Trail(trail)) {
            position = Checked(static_cast<std::int64_t>(position) + 1);
            return Pair(unit, trail);
        }
    }
    return unit;
}

std::int32_t Backward(const UCharCharacterIterator& iterator, std::int32_t& position) {
    position = Checked(static_cast<std::int64_t>(position) - 1);
    const auto unit = Unit(iterator, position);
    if (Trail(unit) && position > iterator.base.begin) {
        const auto lead = Unit(iterator, position - 1);
        if (Lead(lead)) {
            --position;
            return Pair(lead, unit);
        }
    }
    return unit;
}

void Assign(CharacterIterator& iterator, const CharacterIterator& source) {
    iterator.textLength = source.textLength;
    iterator.position = source.position;
    iterator.begin = source.begin;
    iterator.end = source.end;
}

}

extern "C" {

Icu::Abi::CharacterIterator* APS5_VABI _ZN6icu_6817CharacterIteratoraSERKS0__nid_postfix(Icu::Abi::CharacterIterator* self, const Icu::Abi::CharacterIterator* source) {
    Assign(Require(self), Require(source));
    return self;
}

char16_t APS5_VABI _ZN6icu_6817CharacterIterator12firstPostIncEv_nid_postfix(Icu::Abi::CharacterIterator* self) {
    auto& iterator = Require(self);
    auto move = Require(iterator.vtable).move;
    if (!move) throw std::invalid_argument("ICU character iterator: null move callback");
    move(self, 0, 0);
    auto next = Require(iterator.vtable).nextPostInc;
    if (!next) throw std::invalid_argument("ICU character iterator: null next callback");
    return next(reinterpret_cast<ForwardCharacterIterator*>(self));
}

std::int32_t APS5_VABI _ZN6icu_6817CharacterIterator14first32PostIncEv_nid_postfix(Icu::Abi::CharacterIterator* self) {
    auto& iterator = Require(self);
    auto move = Require(iterator.vtable).move;
    if (!move) throw std::invalid_argument("ICU character iterator: null move callback");
    move(self, 0, 0);
    auto next = Require(iterator.vtable).next32PostInc;
    if (!next) throw std::invalid_argument("ICU character iterator: null next32 callback");
    return next(reinterpret_cast<ForwardCharacterIterator*>(self));
}

UCharCharacterIterator* APS5_VABI _ZN6icu_6822UCharCharacterIteratoraSERKS0__nid_postfix(UCharCharacterIterator* self, const UCharCharacterIterator* source) {
    auto& iterator = Require(self);
    const auto& other = Require(source);
    Assign(iterator.base, other.base);
    iterator.text = other.text;
    return self;
}

char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator5firstEv_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    iterator.base.position = iterator.base.begin;
    return iterator.base.position < iterator.base.end ? Unit(iterator, iterator.base.position) : Done;
}

char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator12firstPostIncEv_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    iterator.base.position = iterator.base.begin;
    if (iterator.base.position >= iterator.base.end) return Done;
    const auto position = iterator.base.position;
    iterator.base.position = Checked(static_cast<std::int64_t>(position) + 1);
    return Unit(iterator, position);
}

char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator4lastEv_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    iterator.base.position = iterator.base.end;
    return iterator.base.position > iterator.base.begin ? Unit(iterator, --iterator.base.position) : Done;
}

char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator8setIndexEi_nid_postfix(UCharCharacterIterator* self, std::int32_t position) {
    auto& iterator = Require(self);
    iterator.base.position = position < iterator.base.begin ? iterator.base.begin : position > iterator.base.end ? iterator.base.end : position;
    return iterator.base.position < iterator.base.end ? Unit(iterator, iterator.base.position) : Done;
}

char16_t APS5_VABI _ZNK6icu_6822UCharCharacterIterator7currentEv_nid_postfix(const UCharCharacterIterator* self) {
    const auto& iterator = Require(self);
    return iterator.base.position >= iterator.base.begin && iterator.base.position < iterator.base.end ? Unit(iterator, iterator.base.position) : Done;
}

char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator4nextEv_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    const auto next = Checked(static_cast<std::int64_t>(iterator.base.position) + 1);
    if (next < iterator.base.end) {
        iterator.base.position = next;
        return Unit(iterator, next);
    }
    iterator.base.position = iterator.base.end;
    return Done;
}

char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator11nextPostIncEv_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    if (iterator.base.position >= iterator.base.end) return Done;
    const auto position = iterator.base.position;
    iterator.base.position = Checked(static_cast<std::int64_t>(position) + 1);
    return Unit(iterator, position);
}

char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator8previousEv_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    return iterator.base.position > iterator.base.begin ? Unit(iterator, --iterator.base.position) : Done;
}

std::int8_t APS5_VABI _ZN6icu_6822UCharCharacterIterator7hasNextEv_nid_postfix(UCharCharacterIterator* self) {
    const auto& iterator = Require(self).base;
    return iterator.position < iterator.end;
}

std::int8_t APS5_VABI _ZN6icu_6822UCharCharacterIterator11hasPreviousEv_nid_postfix(UCharCharacterIterator* self) {
    const auto& iterator = Require(self).base;
    return iterator.position > iterator.begin;
}

std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator7first32Ev_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    iterator.base.position = iterator.base.begin;
    auto position = iterator.base.position;
    return position < iterator.base.end ? Forward(iterator, position) : Done;
}

std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator14first32PostIncEv_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    iterator.base.position = iterator.base.begin;
    return iterator.base.position < iterator.base.end ? Forward(iterator, iterator.base.position) : Done;
}

std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator6last32Ev_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    iterator.base.position = iterator.base.end;
    return iterator.base.position > iterator.base.begin ? Backward(iterator, iterator.base.position) : Done;
}

std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator10setIndex32Ei_nid_postfix(UCharCharacterIterator* self, std::int32_t position) {
    auto& iterator = Require(self);
    position = position < iterator.base.begin ? iterator.base.begin : position > iterator.base.end ? iterator.base.end : position;
    if (position < iterator.base.end) {
        if (Trail(Unit(iterator, position)) && position > iterator.base.begin && Lead(Unit(iterator, position - 1))) --position;
        iterator.base.position = position;
        return Forward(iterator, position);
    }
    iterator.base.position = position;
    return Done;
}

std::int32_t APS5_VABI _ZNK6icu_6822UCharCharacterIterator9current32Ev_nid_postfix(const UCharCharacterIterator* self) {
    const auto& iterator = Require(self);
    auto position = iterator.base.position;
    if (position < iterator.base.begin || position >= iterator.base.end) return Done;
    const auto unit = Unit(iterator, position);
    if (Trail(unit) && position > iterator.base.begin) {
        const auto lead = Unit(iterator, position - 1);
        if (Lead(lead)) return Pair(lead, unit);
    }
    return Forward(iterator, position);
}

std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator6next32Ev_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    if (iterator.base.position < iterator.base.end) {
        Forward(iterator, iterator.base.position);
        auto position = iterator.base.position;
        if (position < iterator.base.end) return Forward(iterator, position);
    }
    iterator.base.position = iterator.base.end;
    return Done;
}

std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator13next32PostIncEv_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    return iterator.base.position < iterator.base.end ? Forward(iterator, iterator.base.position) : Done;
}

std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator10previous32Ev_nid_postfix(UCharCharacterIterator* self) {
    auto& iterator = Require(self);
    return iterator.base.position > iterator.base.begin ? Backward(iterator, iterator.base.position) : Done;
}

std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix(UCharCharacterIterator* self, std::int32_t delta, std::int32_t origin) {
    auto& iterator = Require(self).base;
    if (origin >= 0 && origin <= 2) {
        const auto base = origin == 0 ? iterator.begin : origin == 1 ? iterator.position : iterator.end;
        iterator.position = Checked(static_cast<std::int64_t>(base) + delta);
    }
    if (iterator.position < iterator.begin) iterator.position = iterator.begin;
    else if (iterator.position > iterator.end) iterator.position = iterator.end;
    return iterator.position;
}

std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix(UCharCharacterIterator* self, std::int32_t delta, std::int32_t origin) {
    auto& iterator = Require(self);
    auto& base = iterator.base;
    if (origin == 0) base.position = base.begin;
    else if (origin == 2) base.position = base.end;
    else if (origin != 1) return base.position;
    if (delta > 0 && origin != 2) {
        while (delta > 0 && (base.position < base.end || (base.end < 0 && Unit(iterator, base.position) != 0))) {
            Forward(iterator, base.position);
            --delta;
        }
    } else if (delta <= 0 && origin != 0) {
        auto count = Checked(-static_cast<std::int64_t>(delta));
        while (count > 0 && base.position > base.begin) {
            Backward(iterator, base.position);
            --count;
        }
    }
    return base.position;
}

void APS5_VABI _ZN6icu_6822UCharCharacterIterator7setTextENS_14ConstChar16PtrEi_nid_postfix(UCharCharacterIterator* self, const ConstChar16Ptr* pointer, std::int32_t length) {
    auto& iterator = Require(self);
    iterator.text = Require(pointer).pointer;
    if (!iterator.text || length < 0) length = 0;
    iterator.base.end = iterator.base.textLength = length;
    iterator.base.position = iterator.base.begin = 0;
}

std::int32_t APS5_VABI _ZNK6icu_6822UCharCharacterIterator8hashCodeEv_nid_postfix(const UCharCharacterIterator* self) {
    const auto& iterator = Require(self);
    std::uint32_t hash = 0;
    if (iterator.text) {
        const auto length = iterator.base.textLength;
        const auto step = Checked(static_cast<std::int64_t>(length) - 32) / 32 + 1;
        for (std::int64_t index = 0; index < length; index += step) hash = hash * 37 + iterator.text[index];
    }
    return std::bit_cast<std::int32_t>(hash) ^ iterator.base.position ^ iterator.base.begin ^ iterator.base.end;
}

void* APS5_VABI _ZN6icu_6822UCharCharacterIterator16getStaticClassIDEv_nid_postfix() {
    static char identifier;
    return &identifier;
}

void* APS5_VABI _ZNK6icu_6822UCharCharacterIterator17getDynamicClassIDEv_nid_postfix(const UCharCharacterIterator*) {
    return _ZN6icu_6822UCharCharacterIterator16getStaticClassIDEv_nid_postfix();
}

}
