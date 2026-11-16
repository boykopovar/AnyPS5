#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BN_div check failed at line %u\n", location.line());
    std::abort();
}

int liveBlocks = 0;
int zeroedCallsBeforeFailure = -1;

constexpr std::uint64_t MallocFailure = 0x03fff041;
constexpr std::uint64_t DivisionByZero = 0x03fff067;
constexpr std::uint64_t ExpandOnStaticData = 0x03fff069;
constexpr std::uint64_t NotInitialized = 0x03fff06b;

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
    Require(BN_hex2bn_nid_postfix(&value, hex) > 0);
    return value;
}

std::string Hex(const Bignum* value) {
    std::string text;
    char word[17];
    for (int i = value->top; i-- > 0;) {
        std::snprintf(word, sizeof(word), text.empty() ? "%llX" : "%016llX", static_cast<unsigned long long>(value->d[i]));
        text += word;
    }
    if (text.empty()) return value->neg ? "-0" : "0";
    return value->neg ? "-" + text : text;
}

struct Vector {
    const char* numerator;
    const char* divisor;
    const char* quotient;
    const char* remainder;
};

const Vector Vectors[] {
        {"100000000000000000000000000000005", "10000000000000000",
            "10000000000000000", "5"},
        {"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", "10000000000000000000000000000000000000000000000000000000000003039",
            "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFCFC7", "9156CB0"},
        {"1FD5863C3EB0469EC21A937A76F3432FFD73D97E447606B683ECF6F6E4A7AE225BFAFF1EAAF8B0A1", "-12A4E415E1E1B36FF883D1",
            "-1B51C2726C6E38058BFB8CA6E9262DB42203A7D58C566E9578E0512CA15", "5D8BE24B0061209E0F67C"},
        {"-100000000000000000000000000000000000000000000000000", "FFFFFFFFFFFFFFFF",
            "-10000000000000001000000000000000100", "-100"},
        {"50000000000000003", "5",
            "10000000000000000", "3"},
        {"-400000000000000000000000000000000", "-3FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
            "1", "-1"},
        {"1000000000000000000000000000000000000000000000000", "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
            "10000000000000000", "10000000000000000"},
        {"-B0C11FDECB91CE375BC8FBBCBDE5C0994164D8399F767C45", "-D76D4330F1446BEA",
            "D20B488EA2DC0064FD209DA8A1260544", "-1AB9DEED0472401D"},
        {"-17F83D46F17FD374C6A5387777330BDBD7210DFF076CE2EF", "46A3209CA6233255",
            "-56DEA9D37718AD2C8C48855F562A609F", "-3DF4232CD4BEC024"},
        {"41A1AFE878B33E968617959CE3F1F65A8DE5271007814E8A2", "1035B73993FD42359",
            "40C832E4FBB07A3F3C649BDA374A58B7E", "704CDB80C21D2FD4"},
        {"-1E3C180F9E115E4B9E30691C238642EA126A1E48CC11D357C30D8B7628DBD25E63B229F1C4069545DE11CC9DEA959C21", "7C758C5C0074513021DA8978206F5C66",
            "-3E30984456B04E77FD1B0FD469E53558CC4E67A26B8B900BCE371E5C572CECC4", "-613730A110A68F4D65973A8BE09CD609"},
        {"-E049548E8A0A8C9632EA6928F6236BF2504B74BA4A0FE75D2A9EBA0CDF561D802A759159FB7FF337F5CAE3BF3729C619", "-AE81D66D346C6E2BA02FDAA1AD864C44",
            "149068E367F4EDB2E9A08A6F037692F2C29DD122A2B8998BD1177E14622FBA87B", "-2704D8D2834FD4C34E6526CAE6EA816D"},
        {"443892DFC254CB864EF901B932A7C18806A3753915C76F18A0585A01C4C7D6DF0621AEF57E4CC4132F7108E96F770C226", "-19A656AAFD14125844D25DEB354F46A69",
            "-2A8E2EDB44F54BFEDAC822EDC24CB8235208287A407E7123DEFE412138D560A22", "D1FC02F59CC69AE75D2988F18E8C8634"},
        {"1E0C627FF9A01FE8419521FE0E979CF32D1634B4B465325278F845F57B3120DF2F4D4C8650D7D13FB24891917B121DC54E5A3A26D18A669A5AF84E6B4F59672710E6D8E6568068B9", "-73BFE89CD84A1D3A5B8E8FB2BFF29101F3001CEE05DA8467",
            "-42750407240BDF19906BF704012EC73C9C96F93989982E18F2E34E1519030159CF037C0148CEA376F48CF169FC245ABE", "713E610E803159ADDF09454D74ECC8789BD07FE02949EE47"},
};

}

int main() {
    InitializeHeap();
    BignumContext* context = BN_CTX_new_nid_postfix();
    Require(context != nullptr);
    for (const auto& vector : Vectors) {
        Bignum* numerator = FromHex(vector.numerator);
        Bignum* divisor = FromHex(vector.divisor);
        Bignum* quotient = BN_new_nid_postfix();
        Bignum* remainder = BN_new_nid_postfix();
        Require(BN_div_nid_postfix(quotient, remainder, numerator, divisor, context) == 1);
        Require(Hex(quotient) == vector.quotient && Hex(remainder) == vector.remainder);
        Require(BN_div_nid_postfix(nullptr, remainder, numerator, divisor, context) == 1 && Hex(remainder) == vector.remainder);
        Require(BN_div_nid_postfix(quotient, nullptr, numerator, divisor, context) == 1 && Hex(quotient) == vector.quotient);
        Require(BN_div_nid_postfix(nullptr, nullptr, numerator, divisor, context) == 1);
        numerator->flags |= 0x04;
        Require(BN_div_nid_postfix(quotient, remainder, numerator, divisor, context) == 1);
        Require(Hex(quotient) == vector.quotient && Hex(remainder) == vector.remainder);
        numerator->flags &= ~0x04;
        Require(BN_div_nid_postfix(numerator, divisor, numerator, divisor, context) == 1);
        Require(Hex(numerator) == vector.quotient && Hex(divisor) == vector.remainder);
        for (auto* value : {numerator, divisor, quotient, remainder}) BN_free_nid_postfix(value);
        Require(ERR_get_error_nid_postfix() == 0);
    }
    {
        Bignum* small = FromHex("-5");
        Bignum* large = FromHex("7");
        Bignum* quotient = FromHex("123");
        Bignum* remainder = BN_new_nid_postfix();
        Require(BN_div_nid_postfix(quotient, remainder, small, large, context) == 1);
        Require(Hex(quotient) == "0" && Hex(remainder) == "-5");
        Require(BN_div_nid_postfix(nullptr, small, small, large, context) == 1 && Hex(small) == "-5");
        Require(BN_div_nid_postfix(remainder, remainder, small, large, context) == 1 && Hex(remainder) == "0");
        large->flags |= 0x04;
        Require(BN_div_nid_postfix(remainder, remainder, small, large, context) == 1 && Hex(remainder) == "-5");
        large->flags &= ~0x04;
        Bignum* numerator = FromHex("-64");
        Require(BN_div_nid_postfix(remainder, remainder, numerator, large, context) == 1 && Hex(remainder) == "-2");
        Require(BN_div_nid_postfix(quotient, remainder, numerator, numerator, context) == 1);
        Require(Hex(quotient) == "1" && Hex(remainder) == "0");
        Bignum* zero = BN_new_nid_postfix();
        Require(BN_div_nid_postfix(quotient, remainder, zero, large, context) == 1);
        Require(Hex(quotient) == "0" && Hex(remainder) == "0");
        Require(BN_div_nid_postfix(quotient, remainder, numerator, zero, context) == 0);
        Require(ERR_get_error_nid_postfix() == DivisionByZero && ERR_get_error_nid_postfix() == 0);
        Require(Hex(quotient) == "0" && Hex(remainder) == "0");
        numerator->d[numerator->top - 1] = 0;
        Require(BN_div_nid_postfix(quotient, remainder, numerator, large, context) == 0);
        Require(ERR_get_error_nid_postfix() == NotInitialized && ERR_get_error_nid_postfix() == 0);
        for (auto* value : {small, large, quotient, remainder, numerator, zero}) BN_free_nid_postfix(value);
    }
    {
        Bignum* numerator = FromHex("123456789ABCDEF0123456789ABCDEF0123456789");
        Bignum* divisor = FromHex("FEDCBA987");
        Bignum* quotient = BN_new_nid_postfix();
        Bignum* remainder = BN_new_nid_postfix();
        zeroedCallsBeforeFailure = 0;
        Require(BN_div_nid_postfix(quotient, remainder, numerator, divisor, context) == 0);
        Require(ERR_get_error_nid_postfix() == MallocFailure && ERR_get_error_nid_postfix() == 0);
        Require(quotient->top == 0 && remainder->top == 0);
        remainder->neg = 1;
        zeroedCallsBeforeFailure = 1;
        Require(BN_div_nid_postfix(quotient, remainder, numerator, divisor, context) == 1);
        Require(ERR_get_error_nid_postfix() == MallocFailure && ERR_get_error_nid_postfix() == 0);
        Require(Hex(quotient) == "124924924998D0E9A1F8700B8417F46E" && remainder->top == 0 && remainder->neg == 0);
        std::uint64_t words[2] {};
        Bignum fixed {words, 0, 2, 0, 0x02};
        Require(BN_div_nid_postfix(nullptr, &fixed, numerator, divisor, context) == 1 && fixed.d == words);
        Require(Hex(&fixed) == "10CE387");
        Require(BN_div_nid_postfix(&fixed, nullptr, numerator, divisor, context) == 0 && fixed.d == words);
        Require(ERR_get_error_nid_postfix() == ExpandOnStaticData && ERR_get_error_nid_postfix() == 0);
        for (auto* value : {numerator, divisor, quotient, remainder}) BN_free_nid_postfix(value);
    }
    BN_CTX_free_nid_postfix(context);
    Require(liveBlocks == 0);
}
