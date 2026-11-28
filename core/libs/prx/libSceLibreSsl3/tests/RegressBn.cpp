/* Adapted for AnyPS5 from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a)
 * src/regress/lib/libcrypto/bn/bn_convert.c
 * (https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/regress/lib/libcrypto/bn/bn_convert.c).
 * Altered: only the hex round-trip table (bn_convert_tests) is kept; the decimal and ASCII cases are
 * left out because libSceLibreSsl3 does not export BN_bn2dec, BN_dec2bn or BN_asc2bn.
 */
/*	$OpenBSD: bn_convert.c,v 1.6 2024/04/17 08:51:11 jsing Exp $ */
/*
 * Copyright (c) 2023 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceLibreSsl3/tests/RegressBnVectors.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BN regress check failed at line %u\n", location.line());
    std::abort();
}

int liveBlocks = 0;
int zeroedCallsBeforeFailure = -1;

constexpr std::uint64_t MallocFailure = 0x03fff041;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (zeroedCallsBeforeFailure == 0) {
        zeroedCallsBeforeFailure = -1;
        return nullptr;
    }
    if (zeroedCallsBeforeFailure > 0) --zeroedCallsBeforeFailure;
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

Bignum* FromHex(const char* hex) {
    Bignum* value = nullptr;
    Require(BN_hex2bn_nid_postfix(&value, hex) > 0 && value);
    return value;
}

std::string Printed(const Bignum* value) {
    LibreSsl::Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio && BN_print_nid_postfix(bio, value) == 1);
    char* data = nullptr;
    const auto length = BIO_ctrl_nid_postfix(bio, 3, 0, &data);
    std::string text(data, static_cast<std::size_t>(length));
    BIO_free_nid_postfix(bio);
    return text;
}

void TestConvertCase(const LibreSslRegressBn::ConvertCase& tc) {
    Bignum* a = FromHex(tc.hex);
    Require(BN_num_bits_nid_postfix(a) == tc.numBits);
    Require(BN_is_negative_nid_postfix(a) == tc.neg);
    Require(ERR_get_error_nid_postfix() == 0);

    std::vector<unsigned char> bin(tc.binLen);
    Require(BN_bn2bin_nid_postfix(a, bin.data()) == tc.binLen);
    for (int i = 0; i < tc.binLen; ++i) {
        const char expected = tc.binHex[2 * i];
        const char expectedLow = tc.binHex[2 * i + 1];
        unsigned char hi = (expected >= '0' && expected <= '9') ? expected - '0' : (expected >= 'a' && expected <= 'f') ? expected - 'a' + 10 : expected - 'A' + 10;
        unsigned char lo = (expectedLow >= '0' && expectedLow <= '9') ? expectedLow - '0' : (expectedLow >= 'a' && expectedLow <= 'f') ? expectedLow - 'a' + 10 : expectedLow - 'A' + 10;
        Require(bin[i] == (hi << 4 | lo));
    }
    Require(ERR_get_error_nid_postfix() == 0);

    Bignum* b = BN_bin2bn_nid_postfix(bin.data(), tc.binLen, nullptr);
    Require(b != nullptr);
    Require(BN_is_negative_nid_postfix(b) == 0);
    Require(BN_num_bits_nid_postfix(b) == tc.numBits);

    Bignum* dup = BN_dup_nid_postfix(b);
    Require(dup != nullptr && dup != b);
    Require(BN_is_negative_nid_postfix(dup) == 0);
    Require(BN_num_bits_nid_postfix(dup) == tc.numBits);
    BN_free_nid_postfix(dup);
    Require(ERR_get_error_nid_postfix() == 0);

    std::string expectedPrinted;
    if (tc.binLen == 0) {
        expectedPrinted = "0";
    } else {
        bool leading = true;
        for (const char* p = tc.binHex; *p; ++p) {
            const char c = *p;
            char upper = static_cast<char>((c >= 'a' && c <= 'f') ? c - 'a' + 'A' : c);
            if (leading && upper == '0') continue;
            leading = false;
            expectedPrinted += upper;
        }
        if (expectedPrinted.empty()) expectedPrinted = "0";
    }
    std::string signedExpected = expectedPrinted;
    if (tc.neg) signedExpected = "-" + signedExpected;
    Require(Printed(a) == signedExpected);
    Require(Printed(b) == expectedPrinted);
    Require(ERR_get_error_nid_postfix() == 0);

    BN_free_nid_postfix(a);
    BN_free_nid_postfix(b);
    Require(ERR_get_error_nid_postfix() == 0);
}

}

int main() {
    InitializeHeap();

    Require(BN_hex2bn_nid_postfix(nullptr, nullptr) == 0);
    Require(BN_hex2bn_nid_postfix(nullptr, "") == 0);
    Require(BN_hex2bn_nid_postfix(nullptr, "z") == 0);
    Require(BN_hex2bn_nid_postfix(nullptr, "-") == 1);
    Require(BN_hex2bn_nid_postfix(nullptr, "-z") == 1);
    Require(BN_hex2bn_nid_postfix(nullptr, "-0") == 2);
    Require(BN_hex2bn_nid_postfix(nullptr, "12G") == 2);
    Require(BN_hex2bn_nid_postfix(nullptr, "-12G") == 3);

    Bignum* fresh = nullptr;
    Require(BN_hex2bn_nid_postfix(&fresh, "") == 0 && fresh == nullptr);
    Require(BN_hex2bn_nid_postfix(&fresh, nullptr) == 0 && fresh == nullptr);
    Require(BN_hex2bn_nid_postfix(&fresh, "z") == 0 && fresh && fresh->top == 0 && fresh->neg == 0);
    BN_free_nid_postfix(fresh);
    fresh = nullptr;
    Require(BN_hex2bn_nid_postfix(&fresh, "-") == 1 && fresh && fresh->top == 0 && fresh->neg == 1);
    BN_free_nid_postfix(fresh);
    fresh = nullptr;
    Require(BN_hex2bn_nid_postfix(&fresh, "-z") == 1 && fresh && fresh->top == 0 && fresh->neg == 1);
    BN_free_nid_postfix(fresh);
    fresh = nullptr;

    for (const auto& tc : LibreSslRegressBn::ConvertCases) {
        TestConvertCase(tc);
    }

    Bignum* value = FromHex("1");
    zeroedCallsBeforeFailure = 0;
    Require(BN_dup_nid_postfix(value) == nullptr);
    Require(ERR_get_error_nid_postfix() == MallocFailure);
    Require(ERR_get_error_nid_postfix() == 0);
    BN_free_nid_postfix(value);

    value = FromHex("1");
    zeroedCallsBeforeFailure = 1;
    Require(BN_dup_nid_postfix(value) == nullptr);
    Require(ERR_get_error_nid_postfix() == MallocFailure);
    Require(ERR_get_error_nid_postfix() == 0);
    BN_free_nid_postfix(value);

    Require(liveBlocks == 0);
    return 0;
}
