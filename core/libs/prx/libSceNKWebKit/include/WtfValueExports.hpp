#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "prx/libSceNKWebKit/src/WtfVector.hpp"

namespace NKWebKit::WtfAbi {

struct SplitResult {
    StringView string;
    char16_t separator;
    bool allowEmptyEntries;
};

static_assert(sizeof(SplitResult) == 24);
static_assert(offsetof(SplitResult, string) == 0);
static_assert(offsetof(SplitResult, separator) == 16);
static_assert(offsetof(SplitResult, allowEmptyEntries) == 18);

struct SplitResultIterator {
    const SplitResult* result;
    unsigned position;
    unsigned length;
    bool isDone;
};

static_assert(sizeof(SplitResultIterator) == 24);
static_assert(offsetof(SplitResultIterator, result) == 0);
static_assert(offsetof(SplitResultIterator, position) == 8);
static_assert(offsetof(SplitResultIterator, length) == 12);
static_assert(offsetof(SplitResultIterator, isDone) == 16);


static_assert(sizeof(Vector<String>) == 16);
static_assert(offsetof(Vector<String>, buffer) == 0);
static_assert(offsetof(Vector<String>, capacity) == 8);
static_assert(offsetof(Vector<String>, size) == 12);
static_assert(!std::is_trivially_destructible_v<Vector<String>>);
static_assert(sizeof(Vector<char16_t>) == 16);
static_assert(offsetof(Vector<char16_t>, buffer) == 0);
static_assert(offsetof(Vector<char16_t>, capacity) == 8);
static_assert(offsetof(Vector<char16_t>, size) == 12);
static_assert(!std::is_trivially_destructible_v<Vector<char16_t>>);

struct SHA1 {
    uint8_t buffer[64];
    std::size_t cursor;
    std::uint64_t totalBytes;
    std::uint32_t hash[5];
    std::uint32_t padding;
};

static_assert(sizeof(SHA1) == 104);
static_assert(offsetof(SHA1, buffer) == 0);
static_assert(offsetof(SHA1, cursor) == 64);
static_assert(offsetof(SHA1, totalBytes) == 72);
static_assert(offsetof(SHA1, hash) == 80);

struct Encoder {
    uint8_t* buffer;
    unsigned capacity;
    unsigned size;
    uint8_t inlineBuffer[4096];
    SHA1 sha1;
};

static_assert(sizeof(Encoder) == 4216);
static_assert(offsetof(Encoder, buffer) == 0);
static_assert(offsetof(Encoder, capacity) == 8);
static_assert(offsetof(Encoder, size) == 12);
static_assert(offsetof(Encoder, inlineBuffer) == 16);
static_assert(offsetof(Encoder, sha1) == 4112);

struct Decoder {
    const uint8_t* bufferData;
    std::size_t bufferSize;
    const uint8_t* bufferPosition;
    SHA1 sha1;
};

static_assert(sizeof(Decoder) == 128);
static_assert(offsetof(Decoder, bufferData) == 0);
static_assert(offsetof(Decoder, bufferSize) == 8);
static_assert(offsetof(Decoder, bufferPosition) == 16);
static_assert(offsetof(Decoder, sha1) == 24);

struct AtomString {
    String string;
};

static_assert(sizeof(AtomString) == 8);
static_assert(offsetof(AtomString, string) == 0);
static_assert(!std::is_trivially_destructible_v<AtomString>);

}  // namespace NKWebKit::WtfAbi

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl7replaceEDsDs(const NKWebKit::WtfAbi::StringImpl* self, char16_t pattern, char16_t replacement);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl7replaceEDsPKhj(const NKWebKit::WtfAbi::StringImpl* self, char16_t pattern, const unsigned char* replacement, unsigned repStrLength);
void APS5_VABI _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(NKWebKit::WtfAbi::SplitResultIterator* self);
NKWebKit::WtfAbi::SplitResultIterator& APS5_VABI _ZN3WTF10StringView11SplitResult8IteratorppEv(NKWebKit::WtfAbi::SplitResultIterator* self);

void APS5_VABI _ZN3WTF11Persistence7EncoderC1Ev(NKWebKit::WtfAbi::Encoder* self);
void APS5_VABI _ZN3WTF11Persistence7EncoderC2Ev(NKWebKit::WtfAbi::Encoder* self);
void APS5_VABI _ZN3WTF11Persistence7EncoderD1Ev(NKWebKit::WtfAbi::Encoder* self);
void APS5_VABI _ZN3WTF11Persistence7EncoderD2Ev(NKWebKit::WtfAbi::Encoder* self);
void APS5_VABI _ZN3WTF11Persistence7Encoder14encodeChecksumEv(NKWebKit::WtfAbi::Encoder* self);
NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEb(NKWebKit::WtfAbi::Encoder* self, bool value);
NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEh(NKWebKit::WtfAbi::Encoder* self, std::uint8_t value);
NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEs(NKWebKit::WtfAbi::Encoder* self, std::int16_t value);
NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEt(NKWebKit::WtfAbi::Encoder* self, std::uint16_t value);
NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEi(NKWebKit::WtfAbi::Encoder* self, std::int32_t value);
NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEj(NKWebKit::WtfAbi::Encoder* self, std::uint32_t value);
NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEl(NKWebKit::WtfAbi::Encoder* self, std::int64_t value);
NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEm(NKWebKit::WtfAbi::Encoder* self, std::uint64_t value);
NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEf(NKWebKit::WtfAbi::Encoder* self, float value);
NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEd(NKWebKit::WtfAbi::Encoder* self, double value);
bool APS5_VABI _ZN3WTF11Persistence7Decoder14verifyChecksumEv(NKWebKit::WtfAbi::Decoder* self);
bool APS5_VABI _ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(const NKWebKit::WtfAbi::Decoder* self, unsigned long size);
void APS5_VABI _ZN3WTF11Persistence5CoderINS_10AtomStringEE6encodeERNS0_7EncoderERKS2_(NKWebKit::WtfAbi::Encoder& encoder, const NKWebKit::WtfAbi::AtomString& string);
void APS5_VABI _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(NKWebKit::WtfAbi::Encoder& encoder, const NKWebKit::WtfAbi::String& string);
void APS5_VABI _ZN3WTF11Persistence5CoderINS_7CStringEE6encodeERNS0_7EncoderERKS2_(NKWebKit::WtfAbi::Encoder& encoder, const NKWebKit::WtfAbi::CString& string);

bool APS5_VABI _ZNK3WTF3URL10protocolIsEPKc(const NKWebKit::WtfAbi::URL* self, const char* protocol);
bool APS5_VABI _ZN3WTF3URL15hostIsIPAddressENS_10StringViewE(NKWebKit::WtfAbi::StringView host);
void APS5_VABI _ZN3WTF3URL32removeQueryAndFragmentIdentifierEv(NKWebKit::WtfAbi::URL* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF3URL11hostAndPortEv(const NKWebKit::WtfAbi::URL* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF3URL14fileSystemPathEv(const NKWebKit::WtfAbi::URL* self);
bool APS5_VABI _ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(const NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView domain);
NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF3URL19protocolHostAndPortEv(const NKWebKit::WtfAbi::URL* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF3URL4userEv(const NKWebKit::WtfAbi::URL* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF3URL8passwordEv(const NKWebKit::WtfAbi::URL* self);

NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> APS5_VABI _ZNK3WTF6String25splitAllowingEmptyEntriesEDs(const NKWebKit::WtfAbi::String* self, char16_t separator);
NKWebKit::WtfAbi::Vector<char16_t> APS5_VABI _ZNK3WTF6String29charactersWithNullTerminationEv(const NKWebKit::WtfAbi::String* self);
NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> APS5_VABI _ZNK3WTF6String5splitEDs(const NKWebKit::WtfAbi::String* self, char16_t separator);
float APS5_VABI _ZNK3WTF6String7toFloatEPb(const NKWebKit::WtfAbi::String* self, bool* ok);
double APS5_VABI _ZNK3WTF6String8toDoubleEPb(const NKWebKit::WtfAbi::String* self, bool* ok);
NKWebKit::WtfAbi::String APS5_VABI _ZNKR3WTF6String12isolatedCopyEv(const NKWebKit::WtfAbi::String* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNO3WTF6String12isolatedCopyEv(NKWebKit::WtfAbi::String* self);

}
