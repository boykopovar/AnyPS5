#include "prx/libSceIcu/StringTrie.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/appendable.h"
#include "unicode/bytestream.h"
#include "unicode/bytestrie.h"
#include "unicode/ucharstrie.h"
#include <cstdint>

namespace {

template<typename TTag, typename TTag::Type Member>
struct Access {
    friend typename TTag::Type Get(TTag) {
        return Member;
    }
};

struct BytesReadValue {
    using Type = std::int32_t (*)(const std::uint8_t*, std::int32_t);
    friend Type Get(BytesReadValue);
};
struct BytesJumpByDelta {
    using Type = const std::uint8_t* (*)(const std::uint8_t*);
    friend Type Get(BytesJumpByDelta);
};
struct BytesBranchNext {
    using Type = UStringTrieResult (icu::BytesTrie::*)(const std::uint8_t*, std::int32_t, std::int32_t);
    friend Type Get(BytesBranchNext);
};
struct BytesNextImpl {
    using Type = UStringTrieResult (icu::BytesTrie::*)(const std::uint8_t*, std::int32_t);
    friend Type Get(BytesNextImpl);
};
struct BytesFindUniqueValueFromBranch {
    using Type = const std::uint8_t* (*)(const std::uint8_t*, std::int32_t, UBool, std::int32_t&);
    friend Type Get(BytesFindUniqueValueFromBranch);
};
struct BytesFindUniqueValue {
    using Type = UBool (*)(const std::uint8_t*, UBool, std::int32_t&);
    friend Type Get(BytesFindUniqueValue);
};
struct BytesGetNextBranchBytes {
    using Type = void (*)(const std::uint8_t*, std::int32_t, icu::ByteSink&);
    friend Type Get(BytesGetNextBranchBytes);
};
struct BytesAppend {
    using Type = void (*)(icu::ByteSink&, int);
    friend Type Get(BytesAppend);
};
struct UCharsBranchNext {
    using Type = UStringTrieResult (icu::UCharsTrie::*)(const char16_t*, std::int32_t, std::int32_t);
    friend Type Get(UCharsBranchNext);
};
struct UCharsNextImpl {
    using Type = UStringTrieResult (icu::UCharsTrie::*)(const char16_t*, std::int32_t);
    friend Type Get(UCharsNextImpl);
};
struct UCharsFindUniqueValueFromBranch {
    using Type = const char16_t* (*)(const char16_t*, std::int32_t, UBool, std::int32_t&);
    friend Type Get(UCharsFindUniqueValueFromBranch);
};
struct UCharsFindUniqueValue {
    using Type = UBool (*)(const char16_t*, UBool, std::int32_t&);
    friend Type Get(UCharsFindUniqueValue);
};
struct UCharsGetNextBranchUChars {
    using Type = void (*)(const char16_t*, std::int32_t, icu::Appendable&);
    friend Type Get(UCharsGetNextBranchUChars);
};

template struct Access<BytesReadValue, &icu::BytesTrie::readValue>;
template struct Access<BytesJumpByDelta, &icu::BytesTrie::jumpByDelta>;
template struct Access<BytesBranchNext, &icu::BytesTrie::branchNext>;
template struct Access<BytesNextImpl, &icu::BytesTrie::nextImpl>;
template struct Access<BytesFindUniqueValueFromBranch, &icu::BytesTrie::findUniqueValueFromBranch>;
template struct Access<BytesFindUniqueValue, &icu::BytesTrie::findUniqueValue>;
template struct Access<BytesGetNextBranchBytes, &icu::BytesTrie::getNextBranchBytes>;
template struct Access<BytesAppend, &icu::BytesTrie::append>;
template struct Access<UCharsBranchNext, &icu::UCharsTrie::branchNext>;
template struct Access<UCharsNextImpl, &icu::UCharsTrie::nextImpl>;
template struct Access<UCharsFindUniqueValueFromBranch, &icu::UCharsTrie::findUniqueValueFromBranch>;
template struct Access<UCharsFindUniqueValue, &icu::UCharsTrie::findUniqueValue>;
template struct Access<UCharsGetNextBranchUChars, &icu::UCharsTrie::getNextBranchUChars>;

class GuestByteSink final : public icu::ByteSink {
public:
    explicit GuestByteSink(Icu::ByteSink* guest) : guest_(guest) {}

    void Append(const char* bytes, std::int32_t length) override {
        using Function = void (APS5_VABI*)(Icu::ByteSink*, const char*, std::int32_t);
        const auto* table = *reinterpret_cast<Function* const*>(guest_);
        table[2](guest_, bytes, length);
    }

private:
    Icu::ByteSink* guest_;
};

class GuestAppendable final : public icu::Appendable {
public:
    explicit GuestAppendable(Icu::Appendable* guest) : guest_(guest) {}

    UBool appendCodeUnit(char16_t unit) override {
        using Function = std::int8_t (APS5_VABI*)(Icu::Appendable*, char16_t);
        return Table<Function>()[3](guest_, unit) != 0;
    }

    UBool appendCodePoint(UChar32 codePoint) override {
        using Function = std::int8_t (APS5_VABI*)(Icu::Appendable*, std::int32_t);
        return Table<Function>()[4](guest_, codePoint) != 0;
    }

    UBool appendString(const char16_t* text, std::int32_t length) override {
        using Function = std::int8_t (APS5_VABI*)(Icu::Appendable*, const char16_t*, std::int32_t);
        return Table<Function>()[5](guest_, text, length) != 0;
    }

    UBool reserveAppendCapacity(std::int32_t capacity) override {
        using Function = std::int8_t (APS5_VABI*)(Icu::Appendable*, std::int32_t);
        return Table<Function>()[6](guest_, capacity) != 0;
    }

private:
    template<typename TFunction>
    const TFunction* Table() const {
        return *reinterpret_cast<TFunction* const*>(guest_);
    }

    Icu::Appendable* guest_;
};

icu::BytesTrie* Native(Icu::BytesTrie* trie) {
    return reinterpret_cast<icu::BytesTrie*>(trie);
}

const icu::BytesTrie* Native(const Icu::BytesTrie* trie) {
    return reinterpret_cast<const icu::BytesTrie*>(trie);
}

icu::UCharsTrie* Native(Icu::UCharsTrie* trie) {
    return reinterpret_cast<icu::UCharsTrie*>(trie);
}

const icu::UCharsTrie* Native(const Icu::UCharsTrie* trie) {
    return reinterpret_cast<const icu::UCharsTrie*>(trie);
}

void Require(const void* pointer) {
    if (!pointer) APS5_INVALID_ARG_EX;
}

}

extern "C" {

void APS5_VABI _ZN6icu_689BytesTrieD1Ev_nid_postfix(Icu::BytesTrie* self) {
    Require(self);
    Native(self)->~BytesTrie();
}

void APS5_VABI _ZN6icu_689BytesTrieD2Ev_nid_postfix(Icu::BytesTrie* self) {
    Require(self);
    Native(self)->~BytesTrie();
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(const std::uint8_t* pos, std::int32_t leadByte) {
    Require(pos);
    return Get(BytesReadValue{})(pos, leadByte);
}

const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(const std::uint8_t* pos) {
    Require(pos);
    return Get(BytesJumpByDelta{})(pos);
}

std::int32_t APS5_VABI _ZNK6icu_689BytesTrie7currentEv_nid_postfix(const Icu::BytesTrie* self) {
    Require(self);
    return Native(self)->current();
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie10branchNextEPKhii_nid_postfix(Icu::BytesTrie* self, const std::uint8_t* pos,
                                                                         std::int32_t length, std::int32_t inByte) {
    Require(self);
    Require(pos);
    return (Native(self)->*Get(BytesBranchNext{}))(pos, length, inByte);
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie8nextImplEPKhi_nid_postfix(Icu::BytesTrie* self, const std::uint8_t* pos,
                                                                     std::int32_t inByte) {
    Require(self);
    Require(pos);
    return (Native(self)->*Get(BytesNextImpl{}))(pos, inByte);
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEi_nid_postfix(Icu::BytesTrie* self, std::int32_t inByte) {
    Require(self);
    return Native(self)->next(inByte);
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(Icu::BytesTrie* self, const char* text, std::int32_t length) {
    Require(self);
    if (length != 0 && !text) APS5_INVALID_ARG_EX;
    return Native(self)->next(text, length);
}

const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie25findUniqueValueFromBranchEPKhiaRi_nid_postfix(const std::uint8_t* pos,
                                                                                                std::int32_t length,
                                                                                                std::int8_t haveUniqueValue,
                                                                                                std::int32_t* uniqueValue) {
    Require(pos);
    Require(uniqueValue);
    return Get(BytesFindUniqueValueFromBranch{})(pos, length, haveUniqueValue != 0, *uniqueValue);
}

std::int8_t APS5_VABI _ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(const std::uint8_t* pos, std::int8_t haveUniqueValue,
                                                                              std::int32_t* uniqueValue) {
    Require(pos);
    Require(uniqueValue);
    return Get(BytesFindUniqueValue{})(pos, haveUniqueValue != 0, *uniqueValue);
}

std::int32_t APS5_VABI _ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(const Icu::BytesTrie* self, Icu::ByteSink* out) {
    Require(self);
    Require(out);
    GuestByteSink sink(out);
    return Native(self)->getNextBytes(sink);
}

void APS5_VABI _ZN6icu_689BytesTrie18getNextBranchBytesEPKhiRNS_8ByteSinkE_nid_postfix(const std::uint8_t* pos, std::int32_t length,
                                                                                      Icu::ByteSink* out) {
    Require(pos);
    Require(out);
    GuestByteSink sink(out);
    Get(BytesGetNextBranchBytes{})(pos, length, sink);
}

void APS5_VABI _ZN6icu_689BytesTrie6appendERNS_8ByteSinkEi_nid_postfix(Icu::ByteSink* out, std::int32_t byte) {
    Require(out);
    GuestByteSink sink(out);
    Get(BytesAppend{})(sink, byte);
}

void APS5_VABI _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(Icu::UCharsTrie* self) {
    Require(self);
    Native(self)->~UCharsTrie();
}

void APS5_VABI _ZN6icu_6810UCharsTrieD2Ev_nid_postfix(Icu::UCharsTrie* self) {
    Require(self);
    Native(self)->~UCharsTrie();
}

std::int32_t APS5_VABI _ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(const Icu::UCharsTrie* self) {
    Require(self);
    return Native(self)->current();
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(Icu::UCharsTrie* self, std::int32_t codePoint) {
    Require(self);
    return Native(self)->firstForCodePoint(codePoint);
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(Icu::UCharsTrie* self, std::int32_t codePoint) {
    Require(self);
    return Native(self)->nextForCodePoint(codePoint);
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie10branchNextEPKDsii_nid_postfix(Icu::UCharsTrie* self, const char16_t* pos,
                                                                            std::int32_t length, std::int32_t unit) {
    Require(self);
    Require(pos);
    return (Native(self)->*Get(UCharsBranchNext{}))(pos, length, unit);
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie8nextImplEPKDsi_nid_postfix(Icu::UCharsTrie* self, const char16_t* pos, std::int32_t unit) {
    Require(self);
    Require(pos);
    return (Native(self)->*Get(UCharsNextImpl{}))(pos, unit);
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextEi_nid_postfix(Icu::UCharsTrie* self, std::int32_t unit) {
    Require(self);
    return Native(self)->next(unit);
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(Icu::UCharsTrie* self, const char16_t* const* text,
                                                                                    std::int32_t length) {
    Require(self);
    Require(text);
    if (length != 0 && !*text) APS5_INVALID_ARG_EX;
    return Native(self)->next(icu::ConstChar16Ptr(*text), length);
}

const char16_t* APS5_VABI _ZN6icu_6810UCharsTrie25findUniqueValueFromBranchEPKDsiaRi_nid_postfix(const char16_t* pos,
                                                                                               std::int32_t length,
                                                                                               std::int8_t haveUniqueValue,
                                                                                               std::int32_t* uniqueValue) {
    Require(pos);
    Require(uniqueValue);
    return Get(UCharsFindUniqueValueFromBranch{})(pos, length, haveUniqueValue != 0, *uniqueValue);
}

std::int8_t APS5_VABI _ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(const char16_t* pos, std::int8_t haveUniqueValue,
                                                                                 std::int32_t* uniqueValue) {
    Require(pos);
    Require(uniqueValue);
    return Get(UCharsFindUniqueValue{})(pos, haveUniqueValue != 0, *uniqueValue);
}

std::int32_t APS5_VABI _ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(const Icu::UCharsTrie* self,
                                                                                           Icu::Appendable* out) {
    Require(self);
    Require(out);
    GuestAppendable appendable(out);
    return Native(self)->getNextUChars(appendable);
}

void APS5_VABI _ZN6icu_6810UCharsTrie19getNextBranchUCharsEPKDsiRNS_10AppendableE_nid_postfix(const char16_t* pos, std::int32_t length,
                                                                                             Icu::Appendable* out) {
    Require(pos);
    Require(out);
    GuestAppendable appendable(out);
    Get(UCharsGetNextBranchUChars{})(pos, length, appendable);
}

}
