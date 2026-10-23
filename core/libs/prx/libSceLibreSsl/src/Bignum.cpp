#include <bit>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "prx/libc/include/General.hpp"

extern "C" {
void* APS5_VABI malloc_nid_postfix(std::size_t size);
void* APS5_VABI calloc_nid_postfix(std::size_t count, std::size_t size);
void APS5_VABI free_nid_postfix(void* pointer);
}

namespace {
struct Bignum {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

struct BignumContext {
    std::vector<std::size_t> marks;
};

static_assert(sizeof(Bignum) == 24);
static_assert(offsetof(Bignum, d) == 0);
static_assert(offsetof(Bignum, top) == 8);
static_assert(offsetof(Bignum, dmax) == 12);
static_assert(offsetof(Bignum, neg) == 16);
static_assert(offsetof(Bignum, flags) == 20);

constexpr int FlagMalloced = 1;
constexpr int FlagStaticData = 2;

using Mag = std::vector<std::uint64_t>;

struct Int {
    Mag mag;
    bool neg = false;
};

void Trim(Mag& m) {
    while (!m.empty() && m.back() == 0) m.pop_back();
}

int Compare(const Mag& a, const Mag& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (std::size_t i = a.size(); i-- > 0;) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

Mag AddMag(const Mag& a, const Mag& b) {
    const std::size_t count = a.size() > b.size() ? a.size() : b.size();
    Mag r;
    r.reserve(count + 1);
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < count; i++) {
        const unsigned __int128 sum = static_cast<unsigned __int128>(i < a.size() ? a[i] : 0) + (i < b.size() ? b[i] : 0) + carry;
        r.push_back(static_cast<std::uint64_t>(sum));
        carry = static_cast<std::uint64_t>(sum >> 64);
    }
    if (carry) r.push_back(carry);
    return r;
}

Mag SubMag(const Mag& a, const Mag& b) {
    Mag r(a.size());
    std::uint64_t borrow = 0;
    for (std::size_t i = 0; i < a.size(); i++) {
        const std::uint64_t x = a[i];
        const std::uint64_t y = i < b.size() ? b[i] : 0;
        const std::uint64_t diff = x - y;
        const std::uint64_t first = x < y;
        r[i] = diff - borrow;
        borrow = first | (diff < borrow);
    }
    Trim(r);
    return r;
}

Mag MulMag(const Mag& a, const Mag& b) {
    if (a.empty() || b.empty()) return {};
    Mag r(a.size() + b.size(), 0);
    for (std::size_t i = 0; i < a.size(); i++) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < b.size(); j++) {
            const unsigned __int128 product = static_cast<unsigned __int128>(a[i]) * b[j] + r[i + j] + carry;
            r[i + j] = static_cast<std::uint64_t>(product);
            carry = static_cast<std::uint64_t>(product >> 64);
        }
        r[i + b.size()] = carry;
    }
    Trim(r);
    return r;
}

std::size_t BitLength(const Mag& m) {
    if (m.empty()) return 0;
    return (m.size() - 1) * 64 + std::bit_width(m.back());
}

Mag ShiftLeft(const Mag& m, std::size_t bits) {
    if (m.empty()) return {};
    const std::size_t words = bits / 64;
    const unsigned rest = static_cast<unsigned>(bits % 64);
    Mag r(m.size() + words + 1, 0);
    for (std::size_t i = 0; i < m.size(); i++) {
        r[i + words] |= m[i] << rest;
        if (rest) r[i + words + 1] |= m[i] >> (64 - rest);
    }
    Trim(r);
    return r;
}

void ShiftRightOne(Mag& m) {
    for (std::size_t i = 0; i < m.size(); i++) {
        m[i] >>= 1;
        if (i + 1 < m.size()) m[i] |= m[i + 1] << 63;
    }
    Trim(m);
}

void DivMod(const Mag& a, const Mag& b, Mag& quotient, Mag& remainder) {
    remainder = a;
    quotient.clear();
    const std::size_t aBits = BitLength(a);
    const std::size_t bBits = BitLength(b);
    if (aBits < bBits) return;
    const std::size_t shift = aBits - bBits;
    Mag divisor = ShiftLeft(b, shift);
    quotient.assign(shift / 64 + 1, 0);
    for (std::size_t s = shift + 1; s-- > 0;) {
        if (Compare(remainder, divisor) >= 0) {
            remainder = SubMag(remainder, divisor);
            quotient[s / 64] |= std::uint64_t(1) << (s % 64);
        }
        ShiftRightOne(divisor);
    }
    Trim(quotient);
}

Int AddSigned(const Int& x, const Int& y) {
    Int r;
    if (x.neg == y.neg) {
        r.mag = AddMag(x.mag, y.mag);
        r.neg = x.neg;
    } else {
        const int order = Compare(x.mag, y.mag);
        if (order > 0) {
            r.mag = SubMag(x.mag, y.mag);
            r.neg = x.neg;
        } else if (order < 0) {
            r.mag = SubMag(y.mag, x.mag);
            r.neg = y.neg;
        }
    }
    if (r.mag.empty()) r.neg = false;
    return r;
}

bool InverseMod(const Int& a, const Mag& n, Mag& out) {
    if (n.size() == 1 && n[0] == 1) {
        out.clear();
        return true;
    }
    Mag quotient;
    Mag r1;
    DivMod(a.mag, n, quotient, r1);
    if (a.neg && !r1.empty()) r1 = SubMag(n, r1);
    Mag r0 = n;
    Int t0;
    Int t1;
    t1.mag = {1};
    while (!r1.empty()) {
        Mag remainder;
        DivMod(r0, r1, quotient, remainder);
        Int scaled;
        scaled.mag = MulMag(quotient, t1.mag);
        scaled.neg = !t1.neg && !scaled.mag.empty();
        Int t2 = AddSigned(t0, scaled);
        r0 = std::move(r1);
        r1 = std::move(remainder);
        t0 = std::move(t1);
        t1 = std::move(t2);
    }
    if (r0.size() != 1 || r0[0] != 1) return false;
    out = t0.neg ? SubMag(n, t0.mag) : t0.mag;
    return true;
}

Bignum* Checked(Bignum* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + ": null BIGNUM");
    return value;
}

const Bignum* Checked(const Bignum* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + ": null BIGNUM");
    return value;
}

Int Load(const Bignum* bn) {
    Int r;
    if (bn->top > 0) r.mag.assign(bn->d, bn->d + bn->top);
    Trim(r.mag);
    r.neg = bn->neg && !r.mag.empty();
    return r;
}

bool Store(Bignum* bn, const Mag& m, bool neg) {
    if (m.empty()) {
        bn->top = 0;
        bn->neg = 0;
        return true;
    }
    const std::size_t count = m.size();
    if (!bn->d || static_cast<std::size_t>(bn->dmax) < count) {
        if (bn->flags & FlagStaticData) return false;
        if (count > 0x7FFFFFFF) return false;
        auto* grown = static_cast<std::uint64_t*>(calloc_nid_postfix(count, sizeof(std::uint64_t)));
        if (!grown) return false;
        if (bn->d) free_nid_postfix(bn->d);
        bn->d = grown;
        bn->dmax = static_cast<int>(count);
    }
    for (std::size_t i = 0; i < count; i++) bn->d[i] = m[i];
    for (std::size_t i = count; i < static_cast<std::size_t>(bn->dmax); i++) bn->d[i] = 0;
    bn->top = static_cast<int>(count);
    bn->neg = neg ? 1 : 0;
    return true;
}

Bignum* NewBignum() {
    auto* bn = static_cast<Bignum*>(calloc_nid_postfix(1, sizeof(Bignum)));
    if (bn) bn->flags = FlagMalloced;
    return bn;
}

void FreeBignum(Bignum* bn) {
    if (!bn) return;
    if (bn->d && !(bn->flags & FlagStaticData)) free_nid_postfix(bn->d);
    if (bn->flags & FlagMalloced) {
        free_nid_postfix(bn);
    } else {
        bn->d = nullptr;
    }
}

int HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool ApplyWord(Bignum* a, std::uint64_t w, bool subtract) {
    Int word;
    if (w) word.mag = {w};
    word.neg = subtract && w;
    const Int sum = AddSigned(Load(a), word);
    return Store(a, sum.mag, sum.neg);
}
}

extern "C" {
Bignum* APS5_VABI BN_new_nid_postfix(void) {
    return NewBignum();
}

void APS5_VABI BN_free_nid_postfix(Bignum* a) {
    FreeBignum(a);
}

BignumContext* APS5_VABI BN_CTX_new_nid_postfix(void) {
    return new (std::nothrow) BignumContext();
}

void APS5_VABI BN_CTX_free_nid_postfix(BignumContext* ctx) {
    delete ctx;
}

void APS5_VABI BN_CTX_start_nid_postfix(BignumContext* ctx) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    ctx->marks.push_back(0);
}

void APS5_VABI BN_CTX_end_nid_postfix(BignumContext* ctx) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    if (!ctx->marks.empty()) ctx->marks.pop_back();
}

Bignum* APS5_VABI BN_bin2bn_nid_postfix(const unsigned char* s, int len, Bignum* ret) {
    if (len < 0) return nullptr;
    if (len > 0 && !s) throw std::invalid_argument(std::string(__func__) + ": null input");
    Bignum* target = ret ? ret : NewBignum();
    if (!target) return nullptr;
    Mag m((static_cast<std::size_t>(len) + 7) / 8, 0);
    for (int i = 0; i < len; i++) {
        const std::size_t bit = static_cast<std::size_t>(len - 1 - i);
        m[bit / 8] |= static_cast<std::uint64_t>(s[i]) << (bit % 8 * 8);
    }
    Trim(m);
    if (!Store(target, m, false)) {
        if (!ret) FreeBignum(target);
        return nullptr;
    }
    return target;
}

int APS5_VABI BN_num_bits_nid_postfix(const Bignum* a) {
    return static_cast<int>(BitLength(Load(Checked(a, __func__)).mag));
}

int APS5_VABI BN_bn2bin_nid_postfix(const Bignum* a, unsigned char* to) {
    const Mag m = Load(Checked(a, __func__)).mag;
    const std::size_t length = (BitLength(m) + 7) / 8;
    if (length && !to) throw std::invalid_argument(std::string(__func__) + ": null output");
    for (std::size_t i = 0; i < length; i++) {
        const std::size_t bit = length - 1 - i;
        to[i] = static_cast<unsigned char>(m[bit / 8] >> (bit % 8 * 8));
    }
    return static_cast<int>(length);
}

int APS5_VABI BN_hex2bn_nid_postfix(Bignum** a, const char* str) {
    if (!str || !*str) return 0;
    const bool negative = *str == '-';
    const char* digits = str + negative;
    std::size_t count = 0;
    while (HexValue(digits[count]) >= 0) count++;
    if (count == 0 || count > 0x7FFFFFFF / 4) return 0;
    const int consumed = static_cast<int>(count + negative);
    if (!a) return consumed;
    Mag m((count + 15) / 16, 0);
    for (std::size_t i = 0; i < count; i++) {
        const std::size_t nibble = count - 1 - i;
        m[nibble / 16] |= static_cast<std::uint64_t>(HexValue(digits[i])) << (nibble % 16 * 4);
    }
    Trim(m);
    Bignum* target = *a ? *a : NewBignum();
    if (!target) return 0;
    if (!Store(target, m, negative)) {
        if (!*a) FreeBignum(target);
        return 0;
    }
    *a = target;
    return consumed;
}

int APS5_VABI BN_set_word_nid_postfix(Bignum* a, std::uint64_t w) {
    Checked(a, __func__);
    Mag m;
    if (w) m = {w};
    return Store(a, m, false);
}

int APS5_VABI BN_add_word_nid_postfix(Bignum* a, std::uint64_t w) {
    return ApplyWord(Checked(a, __func__), w, false);
}

int APS5_VABI BN_sub_word_nid_postfix(Bignum* a, std::uint64_t w) {
    return ApplyWord(Checked(a, __func__), w, true);
}

int APS5_VABI BN_mul_nid_postfix(Bignum* r, const Bignum* a, const Bignum* b, BignumContext*) {
    Checked(r, __func__);
    const Int x = Load(Checked(a, __func__));
    const Int y = Load(Checked(b, __func__));
    const Mag product = MulMag(x.mag, y.mag);
    return Store(r, product, x.neg != y.neg);
}

Bignum* APS5_VABI BN_mod_inverse_nid_postfix(Bignum* r, const Bignum* a, const Bignum* n, BignumContext*) {
    const Int value = Load(Checked(a, __func__));
    const Int modulus = Load(Checked(n, __func__));
    if (modulus.mag.empty()) return nullptr;
    Mag inverse;
    if (!InverseMod(value, modulus.mag, inverse)) return nullptr;
    Bignum* target = r ? r : NewBignum();
    if (!target) return nullptr;
    if (!Store(target, inverse, false)) {
        if (!r) FreeBignum(target);
        return nullptr;
    }
    return target;
}
}
