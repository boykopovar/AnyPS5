/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb) src/lib/libcrypto/asn1/a_object.c
 * (a_object.c,v 1.29) and src/lib/libcrypto/objects/obj_dat.c (obj_dat.c,v 1.39) */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include <bit>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int ObjectLibrary = 8;
constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int UnknownNid = 101;
constexpr int FirstNumberTooLarge = 122;
constexpr int InvalidDigit = 130;
constexpr int InvalidSeparator = 131;
constexpr int MissingSecondNumber = 138;
constexpr int SecondNumberTooLarge = 147;
constexpr std::uint64_t BigNumberThreshold = (UINT64_MAX - 80) / 10;

struct AddedObject {
    LibreSslAsn1Object object{};
    std::vector<unsigned char> data;
    std::optional<std::string> shortName;
    std::optional<std::string> longName;
};

std::mutex mutex;
bool tableExists = false;
int nextNid = LibreSslObjectCount;
std::vector<std::unique_ptr<AddedObject>> objects;
std::map<std::vector<unsigned char>, const AddedObject*> byData;
std::map<int, const AddedObject*> byNid;

void Asn1Error(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

class Arc {
public:
    explicit Arc(std::uint64_t value) : limbs{static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32)} {}

    void MultiplyAdd(std::uint32_t factor, std::uint32_t addend) {
        std::uint64_t carry = addend;
        for (auto& limb : limbs) {
            const std::uint64_t product = static_cast<std::uint64_t>(limb) * factor + carry;
            limb = static_cast<std::uint32_t>(product);
            carry = product >> 32;
        }
        if (carry) limbs.push_back(static_cast<std::uint32_t>(carry));
    }

    int Bits() const {
        for (std::size_t index = limbs.size(); index-- > 0;) {
            if (limbs[index]) return static_cast<int>(index * 32) + 32 - std::countl_zero(limbs[index]);
        }
        return 0;
    }

    unsigned char DivideBy128() {
        std::uint64_t remainder = 0;
        for (std::size_t index = limbs.size(); index-- > 0;) {
            const std::uint64_t current = (remainder << 32) | limbs[index];
            limbs[index] = static_cast<std::uint32_t>(current / 128);
            remainder = current % 128;
        }
        return static_cast<unsigned char>(remainder);
    }

private:
    std::vector<std::uint32_t> limbs;
};

std::vector<unsigned char> TextToContent(const char* text) {
    std::vector<unsigned char> out;
    std::size_t remaining = std::strlen(text);
    const char* next = text;
    int character = static_cast<signed char>(*next++);
    if (character < '0' || character > '2') {
        Asn1Error(FirstNumberTooLarge);
        return {};
    }
    --remaining;
    const int first = character - '0';
    if (!remaining) {
        Asn1Error(MissingSecondNumber);
        return {};
    }
    character = static_cast<signed char>(*next++);
    --remaining;
    while (remaining) {
        if (character != '.' && character != ' ') {
            Asn1Error(InvalidSeparator);
            return {};
        }
        std::uint64_t value = 0;
        std::optional<Arc> big;
        while (remaining) {
            --remaining;
            character = static_cast<signed char>(*next++);
            if (character == ' ' || character == '.') break;
            if (character < '0' || character > '9') {
                Asn1Error(InvalidDigit);
                return {};
            }
            if (!big && value >= BigNumberThreshold) big.emplace(value);
            if (big) big->MultiplyAdd(10, static_cast<std::uint32_t>(character - '0'));
            else value = value * 10 + static_cast<std::uint64_t>(character - '0');
        }
        if (out.empty()) {
            if (first < 2 && value >= 40) {
                Asn1Error(SecondNumberTooLarge);
                return {};
            }
            if (big) big->MultiplyAdd(1, static_cast<std::uint32_t>(first * 40));
            else value += static_cast<std::uint64_t>(first) * 40;
        }
        std::vector<unsigned char> groups;
        if (big) {
            for (int count = (big->Bits() + 6) / 7; count > 0; --count) groups.push_back(big->DivideBy128());
        } else {
            do {
                groups.push_back(static_cast<unsigned char>(value & 0x7f));
                value >>= 7;
            } while (value);
        }
        for (std::size_t index = groups.size(); index-- > 1;) out.push_back(groups[index] | 0x80);
        out.push_back(groups[0]);
    }
    return out;
}

} // namespace

extern "C" {

int LibreSslAddedObjectNid_nid_no_patch(const LibreSslAsn1Object* object) {
    std::lock_guard lock(mutex);
    if (!tableExists || !object->data || object->length <= 0) return 0;
    const auto found = byData.find(std::vector<unsigned char>(object->data, object->data + object->length));
    return found == byData.end() ? 0 : found->second->object.nid;
}

const char* LibreSslAddedObjectName_nid_no_patch(int nid, bool longName) {
    std::lock_guard lock(mutex);
    if (!tableExists) return nullptr;
    const auto found = byNid.find(nid);
    if (found == byNid.end()) {
        LibreSslPutError_nid_no_patch(ObjectLibrary, AnyFunction, UnknownNid);
        return nullptr;
    }
    return longName ? found->second->object.longName : found->second->object.shortName;
}

int APS5_VABI OBJ_create_nid_postfix(const char* oid, const char* shortName, const char* longName) {
    if (!oid) throw std::invalid_argument("OBJ_create: null OID text");
    std::vector<unsigned char> content = TextToContent(oid);
    if (content.empty()) return 0;
    std::lock_guard lock(mutex);
    tableExists = true;
    const int nid = nextNid++;
    auto added = std::make_unique<AddedObject>();
    added->data = std::move(content);
    if (shortName) added->shortName = shortName;
    if (longName) added->longName = longName;
    added->object = {added->shortName ? added->shortName->c_str() : nullptr, added->longName ? added->longName->c_str() : nullptr, nid,
        static_cast<std::int32_t>(added->data.size()), added->data.data(), 0};
    objects.reserve(objects.size() + 1);
    const auto indexed = byNid.try_emplace(nid, added.get()).first;
    std::map<std::vector<unsigned char>, const AddedObject*>::iterator encoding;
    try {
        encoding = byData.try_emplace(added->data, nullptr).first;
    } catch (...) {
        byNid.erase(indexed);
        throw;
    }
    encoding->second = added.get();
    objects.push_back(std::move(added));
    return nid;
}

void APS5_VABI OBJ_cleanup_nid_postfix(void) {
    std::lock_guard lock(mutex);
    byData.clear();
    byNid.clear();
    objects.clear();
    tableExists = false;
}

}
