#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <string>

struct Bn {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

struct BnCtx;

extern "C" {
Bn* APS5_VABI BN_new_nid_postfix(void);
void APS5_VABI BN_free_nid_postfix(Bn*);
BnCtx* APS5_VABI BN_CTX_new_nid_postfix(void);
void APS5_VABI BN_CTX_free_nid_postfix(BnCtx*);
void APS5_VABI BN_CTX_start_nid_postfix(BnCtx*);
void APS5_VABI BN_CTX_end_nid_postfix(BnCtx*);
Bn* APS5_VABI BN_bin2bn_nid_postfix(const unsigned char*, int, Bn*);
int APS5_VABI BN_bn2bin_nid_postfix(const Bn*, unsigned char*);
int APS5_VABI BN_hex2bn_nid_postfix(Bn**, const char*);
int APS5_VABI BN_num_bits_nid_postfix(const Bn*);
int APS5_VABI BN_set_word_nid_postfix(Bn*, std::uint64_t);
int APS5_VABI BN_add_word_nid_postfix(Bn*, std::uint64_t);
int APS5_VABI BN_sub_word_nid_postfix(Bn*, std::uint64_t);
int APS5_VABI BN_mul_nid_postfix(Bn*, const Bn*, const Bn*, BnCtx*);
Bn* APS5_VABI BN_mod_inverse_nid_postfix(Bn*, const Bn*, const Bn*, BnCtx*);
}

static_assert(sizeof(Bn) == 24);
static_assert(offsetof(Bn, top) == 8);
static_assert(offsetof(Bn, neg) == 16);

static void Require(bool value, int line = __builtin_LINE()) {
    if (value) return;
    std::fprintf(stderr, "check failed at line %d\n", line);
    std::abort();
}

static Bn* FromHex(const char* text) {
    Bn* value = nullptr;
    Require(BN_hex2bn_nid_postfix(&value, text) > 0 && value);
    return value;
}

static std::string Hex(const Bn* value) {
    unsigned char bytes[64];
    const int length = BN_bn2bin_nid_postfix(value, bytes);
    Require(length >= 0 && length <= 64);
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string text;
    for (int i = 0; i < length; i++) {
        text += digits[bytes[i] >> 4];
        text += digits[bytes[i] & 15];
    }
    while (text.size() > 1 && text[0] == '0') text.erase(0, 1);
    if (text.empty()) text = "0";
    return value->neg ? "-" + text : text;
}

static void InitializeHeap() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
}

int main() {
    InitializeHeap();
    Bn* fresh = BN_new_nid_postfix();
    Require(fresh && fresh->top == 0 && fresh->neg == 0 && BN_num_bits_nid_postfix(fresh) == 0);
    BN_free_nid_postfix(fresh);
    BN_free_nid_postfix(nullptr);

    BnCtx* ctx = BN_CTX_new_nid_postfix();
    Require(ctx);
    BN_CTX_start_nid_postfix(ctx);
    BN_CTX_end_nid_postfix(ctx);
    BN_CTX_free_nid_postfix(ctx);
    BN_CTX_free_nid_postfix(nullptr);
    ctx = BN_CTX_new_nid_postfix();
    Require(ctx);

    {
        Bn* a = FromHex("FFFFFFFFFFFFFFFF");
        Bn* b = FromHex("FFFFFFFFFFFFFFFF");
        Bn* r = BN_new_nid_postfix();
        Require(BN_mul_nid_postfix(r, a, b, ctx) == 1);
        Require(Hex(r) == "FFFFFFFFFFFFFFFE0000000000000001");
        Require(r->top == 2 && r->neg == 0);
        BN_free_nid_postfix(a);
        BN_free_nid_postfix(b);
        BN_free_nid_postfix(r);
    }
    {
        Bn* a = FromHex("0123456789ABCDEFFEDCBA98765432100F1E2D3C4B5A6978");
        Bn* b = FromHex("00FF00FF00FF00FF00FF00FF00FF00FF00FF");
        Bn* r = BN_new_nid_postfix();
        Require(BN_mul_nid_postfix(r, a, b, ctx) == 1);
        Require(Hex(r) == "12223444566678876665444322210000F0F1CFC09E8F6D55CB34DD56FF79219B43BB52CA61D970E88");
        Require(BN_mul_nid_postfix(a, a, a, ctx) == 1);
        Require(Hex(a) == "14B66DC33F6ACDEE9D609EBE253A6424AD884E9A04269A9706601BB096443363E2CE44634913DF9C3B9FAA3D3A840");
        Bn* c = FromHex("-3");
        Bn* d = FromHex("5");
        Require(BN_mul_nid_postfix(d, c, d, ctx) == 1 && Hex(d) == "-F" && d->neg == 1);
        Require(BN_mul_nid_postfix(c, c, c, ctx) == 1 && Hex(c) == "9" && c->neg == 0);
        Bn* zero = FromHex("0");
        Require(BN_mul_nid_postfix(d, d, zero, ctx) == 1 && d->top == 0 && d->neg == 0);
        BN_free_nid_postfix(a);
        BN_free_nid_postfix(b);
        BN_free_nid_postfix(c);
        BN_free_nid_postfix(d);
        BN_free_nid_postfix(r);
        BN_free_nid_postfix(zero);
    }
    {
        const unsigned char bytes[] = {0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
        Bn* value = BN_bin2bn_nid_postfix(bytes, sizeof(bytes), nullptr);
        Require(value && value->neg == 0 && value->top == 2);
        Require(Hex(value) == "10203040506070809");
        unsigned char out[16];
        std::memset(out, 0xEE, sizeof(out));
        Require(BN_bn2bin_nid_postfix(value, out) == 9);
        Require(std::memcmp(out, bytes + 2, 9) == 0 && out[9] == 0xEE);
        Require(BN_bin2bn_nid_postfix(bytes, 4, value) == value && Hex(value) == "102" && value->top == 1);
        const unsigned char zeros[] = {0, 0, 0};
        Require(BN_bin2bn_nid_postfix(zeros, 3, value) == value && value->top == 0);
        std::memset(out, 0xEE, sizeof(out));
        Require(BN_bn2bin_nid_postfix(value, out) == 0 && out[0] == 0xEE);
        Require(BN_bin2bn_nid_postfix(bytes, 0, value) == value && value->top == 0);
        Require(BN_bin2bn_nid_postfix(bytes, -1, value) == nullptr);
        BN_free_nid_postfix(value);
    }
    {
        Bn* value = BN_new_nid_postfix();
        Require(BN_num_bits_nid_postfix(value) == 0);
        Require(BN_set_word_nid_postfix(value, 1) == 1 && BN_num_bits_nid_postfix(value) == 1);
        Require(BN_set_word_nid_postfix(value, 0x100) == 1 && BN_num_bits_nid_postfix(value) == 9);
        Bn* big = FromHex("10000000000000000");
        Require(BN_num_bits_nid_postfix(big) == 65 && big->top == 2);
        Require(BN_set_word_nid_postfix(big, 0) == 1 && BN_num_bits_nid_postfix(big) == 0 && big->top == 0 && big->neg == 0);
        Require(BN_set_word_nid_postfix(value, 0xFFFFFFFFFFFFFFFFull) == 1 && BN_num_bits_nid_postfix(value) == 64);
        BN_free_nid_postfix(value);
        BN_free_nid_postfix(big);
    }
    {
        Bn* value = nullptr;
        Require(BN_hex2bn_nid_postfix(&value, "-1F") == 3 && value);
        Require(value->neg == 1 && value->top == 1 && value->d[0] == 0x1F);
        Bn* same = value;
        Require(BN_hex2bn_nid_postfix(&value, "ABC") == 3 && value == same);
        Require(value->neg == 0 && value->d[0] == 0xABC);
        Require(BN_hex2bn_nid_postfix(&value, "FFzz") == 2 && value->d[0] == 0xFF);
        Require(BN_hex2bn_nid_postfix(&value, "") == 0 && value->d[0] == 0xFF);
        Require(BN_hex2bn_nid_postfix(&value, "zz") == 0 && BN_hex2bn_nid_postfix(&value, "-") == 0);
        Require(BN_hex2bn_nid_postfix(&value, "-0") == 2 && value->top == 0 && value->neg == 0);
        Require(BN_hex2bn_nid_postfix(&value, "deadBEEF0123456789abcdef") == 24);
        Require(Hex(value) == "DEADBEEF0123456789ABCDEF" && value->top == 2);
        Require(BN_hex2bn_nid_postfix(nullptr, "12G") == 2);
        BN_free_nid_postfix(value);
    }
    {
        Bn* value = FromHex("FFFFFFFFFFFFFFFF");
        Require(BN_add_word_nid_postfix(value, 1) == 1);
        Require(value->top == 2 && value->d[0] == 0 && value->d[1] == 1 && Hex(value) == "10000000000000000");
        Require(BN_sub_word_nid_postfix(value, 1) == 1);
        Require(value->top == 1 && value->d[0] == 0xFFFFFFFFFFFFFFFFull && value->neg == 0);
        Require(BN_set_word_nid_postfix(value, 3) == 1);
        Require(BN_sub_word_nid_postfix(value, 5) == 1 && value->neg == 1 && value->top == 1 && value->d[0] == 2);
        Require(BN_add_word_nid_postfix(value, 2) == 1 && value->top == 0 && value->neg == 0);
        Require(BN_sub_word_nid_postfix(value, 7) == 1 && value->neg == 1 && Hex(value) == "-7");
        Require(BN_add_word_nid_postfix(value, 10) == 1 && value->neg == 0 && Hex(value) == "3");
        Require(BN_sub_word_nid_postfix(value, 0) == 1 && Hex(value) == "3");
        Bn* wide = FromHex("-10000000000000000");
        Require(BN_sub_word_nid_postfix(wide, 1) == 1 && wide->neg == 1 && Hex(wide) == "-10000000000000001");
        Require(BN_add_word_nid_postfix(wide, 2) == 1 && wide->neg == 1 && Hex(wide) == "-FFFFFFFFFFFFFFFF" && wide->top == 1);
        Bn* chain = FromHex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        Require(BN_add_word_nid_postfix(chain, 1) == 1 && chain->top == 3 && Hex(chain) == "100000000000000000000000000000000");
        Require(BN_sub_word_nid_postfix(chain, 1) == 1 && chain->top == 2 && Hex(chain) == "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        BN_free_nid_postfix(value);
        BN_free_nid_postfix(wide);
        BN_free_nid_postfix(chain);
    }
    {
        Bn* r = BN_new_nid_postfix();
        Bn* three = FromHex("3");
        Bn* eleven = FromHex("B");
        Require(BN_mod_inverse_nid_postfix(r, three, eleven, ctx) == r && Hex(r) == "4");
        Bn* seven = FromHex("7");
        Bn* forty = FromHex("28");
        Require(BN_mod_inverse_nid_postfix(r, seven, forty, ctx) == r && Hex(r) == "17");
        Bn* four = FromHex("4");
        Bn* eight = FromHex("8");
        Require(BN_mod_inverse_nid_postfix(r, four, eight, ctx) == nullptr);
        Bn* allocated = BN_mod_inverse_nid_postfix(nullptr, three, eleven, ctx);
        Require(allocated && Hex(allocated) == "4");
        Bn* minusThree = FromHex("-3");
        Require(BN_mod_inverse_nid_postfix(r, minusThree, eleven, ctx) == r && Hex(r) == "7");
        Bn* fifteen = FromHex("F");
        Require(BN_mod_inverse_nid_postfix(r, fifteen, eleven, ctx) == r && Hex(r) == "3");
        Bn* inPlace = FromHex("3");
        Require(BN_mod_inverse_nid_postfix(inPlace, inPlace, eleven, ctx) == inPlace && Hex(inPlace) == "4");

        Bn* prime = FromHex("7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        Bn* a = FromHex("123456789ABCDEF0FEDCBA9876543210");
        Bn* inverse = BN_mod_inverse_nid_postfix(nullptr, a, prime, ctx);
        Require(inverse && Hex(inverse) == "62CFEFE6EB41053969967BD711C5169F");
        Bn* product = BN_new_nid_postfix();
        Require(BN_mul_nid_postfix(product, a, inverse, ctx) == 1);
        Bn* quotient = FromHex("E0DA4F6BE78BFD74B8C9F95AF178811");
        Bn* check = BN_new_nid_postfix();
        Require(BN_mul_nid_postfix(check, quotient, prime, ctx) == 1);
        Require(BN_add_word_nid_postfix(check, 1) == 1);
        Require(Hex(check) == Hex(product));

        Bn* all[] = {r, three, eleven, seven, forty, four, eight, allocated, minusThree, fifteen, inPlace, prime, a, inverse, product, quotient, check};
        for (Bn* value : all) BN_free_nid_postfix(value);
    }
    BN_CTX_free_nid_postfix(ctx);
    return 0;
}
