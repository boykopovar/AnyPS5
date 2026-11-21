#include "prx/libSceIcu/include/DataSwap.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "udataswp.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu data swap: " + message);
}

struct GuestSwapper;

using SwapFunction = std::int32_t (APS5_VABI*)(const UDataSwapper*, const void*, std::int32_t, void*, std::int32_t*);
using GuestSwap = std::int32_t (APS5_VABI*)(const GuestSwapper*, const void*, std::int32_t, void*, UErrorCode*);

struct GuestSwapper {
    UBool inIsBigEndian;
    std::uint8_t inCharset;
    UBool outIsBigEndian;
    std::uint8_t outCharset;
    std::uint16_t (APS5_VABI* readUInt16)(std::uint16_t);
    std::uint32_t (APS5_VABI* readUInt32)(std::uint32_t);
    std::int32_t (APS5_VABI* compareInvChars)(const GuestSwapper*, const char*, std::int32_t, const char16_t*, std::int32_t);
    void (APS5_VABI* writeUInt16)(std::uint16_t*, std::uint16_t);
    void (APS5_VABI* writeUInt32)(std::uint32_t*, std::uint32_t);
    GuestSwap swapArray16;
    GuestSwap swapArray32;
    GuestSwap swapArray64;
    GuestSwap swapInvChars;
    void (APS5_VABI* printError)(void*, const char*, VaList*);
    void* printErrorContext;
};

UDataSwapper* Reference = nullptr;
GuestSwapper Guest{};
int Context = 0;
int Calls = 0;
bool Mismatch = false;
std::string Messages;
GuestSwapper Inner{};
bool Nesting = false;
int Stray = 0;
const std::vector<std::uint8_t>* Nested = nullptr;
std::int32_t NestedResult = 0;

std::uint16_t APS5_VABI Read16(std::uint16_t value) {
    ++Calls;
    return Reference->readUInt16(value);
}

std::uint32_t APS5_VABI Read32(std::uint32_t value) {
    ++Calls;
    return Reference->readUInt32(value);
}

std::int32_t APS5_VABI Compare(const GuestSwapper* ds, const char* out, std::int32_t outLength, const char16_t* local, std::int32_t localLength) {
    ++Calls;
    Mismatch |= ds != &Guest;
    return Reference->compareInvChars(Reference, out, outLength, local, localLength);
}

void APS5_VABI Write16(std::uint16_t* destination, std::uint16_t value) {
    ++Calls;
    Reference->writeUInt16(destination, value);
}

void APS5_VABI Write32(std::uint32_t* destination, std::uint32_t value) {
    ++Calls;
    Reference->writeUInt32(destination, value);
}

std::uint16_t APS5_VABI InnerRead16(std::uint16_t value) {
    Stray += !Nesting;
    return Reference->readUInt16(value);
}

std::uint32_t APS5_VABI InnerRead32(std::uint32_t value) {
    Stray += !Nesting;
    return Reference->readUInt32(value);
}

template<UDataSwapFn* UDataSwapper::*Member>
std::int32_t APS5_VABI InnerSwap(const GuestSwapper* ds, const void* in, std::int32_t length, void* out, UErrorCode* status) {
    Stray += !Nesting;
    Mismatch |= ds != &Inner;
    return (Reference->*Member)(Reference, in, length, out, status);
}

void SwapNested() {
    const std::vector<std::uint8_t>* data = Nested;
    Nested = nullptr;
    std::vector<std::uint8_t> out(data->size(), 0xa5);
    std::int32_t status = U_ZERO_ERROR;
    Nesting = true;
    NestedResult = ucol_swapInverseUCA_nid_postfix(reinterpret_cast<const UDataSwapper*>(&Inner), data->data(),
        static_cast<std::int32_t>(data->size()), out.data(), &status);
    Nesting = false;
}

template<UDataSwapFn* UDataSwapper::*Member>
std::int32_t APS5_VABI Swap(const GuestSwapper* ds, const void* in, std::int32_t length, void* out, UErrorCode* status) {
    ++Calls;
    Mismatch |= ds != &Guest;
    if (Nested) SwapNested();
    return (Reference->*Member)(Reference, in, length, out, status);
}

void APS5_VABI Print(void* context, const char* format, VaList* arguments) {
    Mismatch |= context != &Context;
    char text[512];
    vsnprintf_nid_postfix(text, sizeof(text), format, arguments);
    Messages += text;
}

const UDataSwapper* Open(bool inBig, bool outBig, std::uint8_t outCharset, bool print = true) {
    if (Reference) udata_closeSwapper(Reference);
    UErrorCode status = U_ZERO_ERROR;
    Reference = udata_openSwapper(inBig, U_ASCII_FAMILY, outBig, outCharset, &status);
    Require(U_SUCCESS(status) && Reference, "udata_openSwapper failed");
    Guest = {inBig, U_ASCII_FAMILY, outBig, outCharset, Read16, Read32, Compare, Write16, Write32, Swap<&UDataSwapper::swapArray16>,
        Swap<&UDataSwapper::swapArray32>, Swap<&UDataSwapper::swapArray64>, Swap<&UDataSwapper::swapInvChars>,
        print ? Print : nullptr, &Context};
    Calls = 0;
    Mismatch = false;
    Messages.clear();
    return reinterpret_cast<const UDataSwapper*>(&Guest);
}

std::vector<std::uint8_t> Load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.good(), "cannot read " + path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::uint64_t Hash(const std::uint8_t* bytes, std::size_t size) {
    std::uint64_t hash = 1469598103934665603ull;
    for (std::size_t index = 0; index < size; ++index) hash = (hash ^ bytes[index]) * 1099511628211ull;
    return hash;
}

std::vector<std::uint8_t> InverseUca() {
    std::vector<std::uint8_t> bytes(108, 0);
    const std::uint8_t header[] = {0x20, 0, 0xda, 0x27, 20, 0, 0, 0, 0, 0, 2, 0, 'I', 'n', 'v', 'C', 2, 1, 0, 0, 6, 0, 0, 0};
    std::memcpy(bytes.data(), header, sizeof(header));
    const std::uint32_t words[] = {76, 3, 4, 32, 68};
    std::memcpy(bytes.data() + 32, words, sizeof(words));
    bytes[52] = 6;
    for (std::uint32_t index = 0; index < 9; ++index) {
        const std::uint32_t word = 0x01020304u + index;
        std::memcpy(bytes.data() + 64 + 4 * index, &word, sizeof(word));
    }
    const std::uint16_t units[] = {0x4142, 0x4344, 0x4546, 0x4748};
    std::memcpy(bytes.data() + 100, units, sizeof(units));
    return bytes;
}

void Expect(const char* name, SwapFunction swap, const std::vector<std::uint8_t>& data, bool inBig, std::uint8_t outCharset,
    std::int32_t size, std::uint64_t hash) {
    const std::string label = name;
    const UDataSwapper* ds = Open(inBig, !inBig, outCharset);
    std::int32_t status = U_ZERO_ERROR;
    Require(swap(ds, data.data(), -1, nullptr, &status) == size && status == U_ZERO_ERROR, label + " preflight");
    std::vector<std::uint8_t> out(data.size(), 0xa5);
    Require(swap(ds, data.data(), static_cast<std::int32_t>(data.size()), out.data(), &status) == size, label + " length");
    Require(status == U_ZERO_ERROR && Hash(out.data(), size) == hash, label + " bytes differ from ICU 68.2");
    Require(Calls > 0 && !Mismatch && Messages.empty(), label + " guest swapper calls");
    std::vector<std::uint8_t> inPlace(data);
    Require(swap(ds, inPlace.data(), size, inPlace.data(), &status) == size && Hash(inPlace.data(), size) == hash, label + " in place");
    if (outCharset != U_ASCII_FAMILY) return;
    ds = Open(!inBig, inBig, U_ASCII_FAMILY);
    std::vector<std::uint8_t> back(data.size(), 0xa5);
    Require(swap(ds, out.data(), size, back.data(), &status) == size && status == U_ZERO_ERROR, label + " reverse swap");
    Require(std::memcmp(back.data(), data.data(), size) == 0, label + " round trip");
}

void ExpectError(const char* name, SwapFunction swap, const UDataSwapper* ds, const std::vector<std::uint8_t>& data,
    std::int32_t length, std::int32_t expected, const std::string& message) {
    std::vector<std::uint8_t> out(data.size(), 0xa5);
    std::int32_t status = U_ZERO_ERROR;
    Messages.clear();
    Require(swap(ds, data.data(), length, length < 0 ? nullptr : out.data(), &status) == 0, std::string(name) + " length");
    Require(status == expected, std::string(name) + " status " + std::to_string(status));
    Require(Messages == message, std::string(name) + " message " + Messages);
}

}

int main() {
    const std::string c = ANYPS5_ICU_SOURCE_DIR;
    const std::string j = ANYPS5_ICU4J_DIR;
    const auto uca = Load(c + "/data/in/coll/ucadata-unihan.icu");
    const auto names = Load(c + "/data/in/unames.icu");
    const auto nfkc = Load(c + "/data/in/nfkc.nrm");
    const std::string data = j + "/core/src/main/resources/com/ibm/icu/impl/data/icudata/";
    const auto aliases = Load(j + "/charset/src/main/resources/com/ibm/icu/impl/data/icudata/cnvalias.icu");
    const auto sentences = Load(data + "brkitr/sent.brk");
    const auto thai = Load(data + "brkitr/thaidict.dict");

    Expect("ucol_swap", ucol_swap_nid_postfix, uca, false, U_ASCII_FAMILY, 597156, 0xeb252b3ed693aab3ull);
    Expect("uchar_swapNames", uchar_swapNames_nid_postfix, names, false, U_ASCII_FAMILY, 341800, 0xf232ca23487db496ull);
    Expect("ucnv_swapAliases", ucnv_swapAliases_nid_postfix, aliases, true, U_ASCII_FAMILY, 64002, 0x85027cc94f720382ull);
    Expect("ucnv_swapAliases ebcdic", ucnv_swapAliases_nid_postfix, aliases, true, U_EBCDIC_FAMILY, 64002, 0x32b4da2b6980799full);
    Expect("ubrk_swap", ubrk_swap_nid_postfix, sentences, true, U_ASCII_FAMILY, 19856, 0x2aa99e351b3ec8dbull);
    Expect("udict_swap", udict_swap_nid_postfix, thai, true, U_ASCII_FAMILY, 126136, 0xed9762d97726425full);
    Expect("ucol_swapInverseUCA", ucol_swapInverseUCA_nid_postfix, InverseUca(), false, U_ASCII_FAMILY, 108, 0x6596453c50400663ull);
    auto version4 = nfkc;
    version4[16] = 4;
    Expect("unorm2_swap", unorm2_swap_nid_postfix, version4, false, U_ASCII_FAMILY, 56124, 0xd10e2e66a389b595ull);

    const UDataSwapper* ds = Open(false, true, U_ASCII_FAMILY);
    const std::string version5 = "unorm2_swap(): data format 4e.72.6d.32 (format version 05) is not recognized as Normalizer2 data\n";
    ExpectError("unorm2_swap version 5", unorm2_swap_nid_postfix, ds, nfkc, static_cast<std::int32_t>(nfkc.size()), U_UNSUPPORTED_ERROR, version5);
    ExpectError("unorm2_swap version 5 preflight", unorm2_swap_nid_postfix, ds, nfkc, -1, U_UNSUPPORTED_ERROR, version5);
    std::vector<std::uint8_t> header(nfkc.size(), 0xa5);
    std::int32_t status = U_ZERO_ERROR;
    unorm2_swap_nid_postfix(ds, nfkc.data(), static_cast<std::int32_t>(nfkc.size()), header.data(), &status);
    Require(Hash(header.data(), 64) == 0xdac5848313196a28ull, "unorm2_swap version 5 header differs from ICU 68.2");
    ExpectError("unorm2_swap version 5 truncated header", unorm2_swap_nid_postfix, ds, nfkc, 28, U_INDEX_OUTOFBOUNDS_ERROR,
        "udata_swapDataHeader(): header size mismatch - headerSize 32 infoSize 20 length 28\n");
    ExpectError("uchar_swapNames short", uchar_swapNames_nid_postfix, ds, names, 40, U_INDEX_OUTOFBOUNDS_ERROR,
        "uchar_swapNames(): too few bytes (8 after header) for unames.icu\n");
    ExpectError("uchar_swapNames truncated", uchar_swapNames_nid_postfix, ds, names, 100, U_INDEX_OUTOFBOUNDS_ERROR,
        "uchar_swapNames(): too few bytes (68 after header) for unames.icu\n");
    ExpectError("ucol_swap on Nrm2", ucol_swap_nid_postfix, ds, nfkc, static_cast<std::int32_t>(nfkc.size()), U_UNSUPPORTED_ERROR,
        "ucol_swap(): data format 4e.72.6d.32 (format version 05.00) is not recognized as collation data\n");
    ExpectError("ucol_swap null swapper", ucol_swap_nid_postfix, nullptr, uca, static_cast<std::int32_t>(uca.size()), U_ILLEGAL_ARGUMENT_ERROR, "");
    Require(!Mismatch, "a guest callback received the host swapper or context");

    std::vector<std::uint8_t> untouched(aliases.size(), 0xa5);
    status = U_BUFFER_OVERFLOW_ERROR;
    Require(ucnv_swapAliases_nid_postfix(ds, aliases.data(), static_cast<std::int32_t>(aliases.size()), untouched.data(), &status) == 0,
        "ucnv_swapAliases ran after a failure");
    Require(status == U_BUFFER_OVERFLOW_ERROR && untouched[0] == 0xa5, "ucnv_swapAliases changed state after a failure");

    Require(ucol_looksLikeCollationBinary_nid_postfix(ds, uca.data(), static_cast<std::int32_t>(uca.size())) == 1, "ucadata not recognized");
    Require(ucol_looksLikeCollationBinary_nid_postfix(ds, nfkc.data(), static_cast<std::int32_t>(nfkc.size())) == 0, "Nrm2 recognized");
    Require(ucol_looksLikeCollationBinary_nid_postfix(nullptr, uca.data(), -1) == 0, "null swapper recognized");
    Require(ucol_looksLikeCollationBinary_nid_postfix(ds, uca.data(), 10) == 1, "short ucadata not recognized as ICU 68.2 does");

    std::vector<std::uint8_t> version3(168, 0);
    const std::uint32_t tableSize = 168;
    const std::uint32_t magic = 0x20030618u;
    std::memcpy(version3.data(), &tableSize, sizeof(tableSize));
    std::memcpy(version3.data() + 16, &magic, sizeof(magic));
    version3[80] = 3;
    Require(ucol_looksLikeCollationBinary_nid_postfix(ds, version3.data(), 168) == 1, "format version 3 collation not recognized");
    Require(ucol_looksLikeCollationBinary_nid_postfix(Open(true, false, U_ASCII_FAMILY), version3.data(), 168) == 0,
        "format version 3 collation recognized with the wrong input byte order");
    version3[65] = 1;
    ds = Open(false, true, U_ASCII_FAMILY);
    Require(ucol_looksLikeCollationBinary_nid_postfix(ds, version3.data(), 168) == 0, "format version 3 byte order flag ignored");
    Require(ucol_looksLikeCollationBinary_nid_postfix(ds, version3.data(), -1) == 0, "format version 3 byte order flag ignored in preflight");

    const auto inverse = InverseUca();
    ds = Open(false, true, U_ASCII_FAMILY);
    Inner = Guest;
    Inner.readUInt16 = InnerRead16;
    Inner.readUInt32 = InnerRead32;
    Inner.swapArray16 = InnerSwap<&UDataSwapper::swapArray16>;
    Inner.swapArray32 = InnerSwap<&UDataSwapper::swapArray32>;
    Inner.swapArray64 = InnerSwap<&UDataSwapper::swapArray64>;
    Inner.swapInvChars = InnerSwap<&UDataSwapper::swapInvChars>;
    Inner.printError = nullptr;
    Nested = &inverse;
    std::vector<std::uint8_t> outer(names.size(), 0xa5);
    status = U_ZERO_ERROR;
    Require(uchar_swapNames_nid_postfix(ds, names.data(), static_cast<std::int32_t>(names.size()), outer.data(), &status) == 341800 &&
        status == U_ZERO_ERROR && Hash(outer.data(), 341800) == 0xf232ca23487db496ull, "swap around a nested swap differs from ICU 68.2");
    Require(!Nested && NestedResult == 108 && Stray == 0 && !Mismatch, "a nested swap left its swapper in place");

    ds = Open(false, true, U_ASCII_FAMILY, false);
    status = U_ZERO_ERROR;
    Require(unorm2_swap_nid_postfix(ds, nfkc.data(), static_cast<std::int32_t>(nfkc.size()), header.data(), &status) == 0 &&
        status == U_UNSUPPORTED_ERROR, "unorm2_swap version 5 without printError");
    udata_closeSwapper(Reference);
    return 0;
}
