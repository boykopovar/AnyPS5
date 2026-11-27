#include "prx/libc/include/Api.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) {
        return;
    }
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

}

int main() {
    std::array<unsigned char, 24> shortString{10, 'l', 'o', 'g', 'i', 'c', 0};
    LibcException::ExceptionObject logicComplete{};
    _ZNSt11logic_errorC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &logicComplete, shortString.data());
    shortString.fill(0);
    Require(std::strcmp(_ZNKSt11logic_error4whatEv_nid_postfix(&logicComplete), "logic") == 0,
            "logic_error C1 must copy short string content");
    _ZNSt11logic_errorD1Ev_nid_postfix(&logicComplete);

    constexpr char LongDiag[] = "A diagnostic text stored inside a long heap-allocated guest string";
    std::array<char, 80> longBuffer{};
    std::memcpy(longBuffer.data(), LongDiag, sizeof(LongDiag));
    const std::array<std::uint64_t, 3> longDesc{
        longBuffer.size() | 1u, sizeof(LongDiag) - 1, reinterpret_cast<std::uintptr_t>(longBuffer.data()),
    };

    LibcException::ExceptionObject logicBase{};
    _ZNSt11logic_errorC2ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &logicBase, longDesc.data());
    Require(std::strcmp(_ZNKSt11logic_error4whatEv_nid_postfix(&logicBase), LongDiag) == 0,
            "logic_error C2 must copy long string content");
    _ZNSt11logic_errorD2Ev_nid_postfix(&logicBase);

    LibcException::ExceptionObject invComplete{};
    _ZNSt16invalid_argumentC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &invComplete, longDesc.data());
    Require(std::strcmp(_ZNKSt16invalid_argument4whatEv_nid_postfix(&invComplete), LongDiag) == 0,
            "invalid_argument C1 must copy string content");
    _ZNSt16invalid_argumentD1Ev_nid_postfix(&invComplete);

    LibcException::ExceptionObject invBase{};
    _ZNSt16invalid_argumentC2ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &invBase, longDesc.data());
    Require(std::strcmp(_ZNKSt16invalid_argument4whatEv_nid_postfix(&invBase), LongDiag) == 0,
            "invalid_argument C2 must copy string content");
    _ZNSt16invalid_argumentD2Ev_nid_postfix(&invBase);

    LibcException::ExceptionObject rangeComplete{};
    _ZNSt12out_of_rangeC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &rangeComplete, longDesc.data());
    Require(std::strcmp(_ZNKSt12out_of_range4whatEv_nid_postfix(&rangeComplete), LongDiag) == 0,
            "out_of_range C1 must copy string content");
    _ZNSt12out_of_rangeD1Ev_nid_postfix(&rangeComplete);

    LibcException::ExceptionObject rangeBase{};
    _ZNSt12out_of_rangeC2ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &rangeBase, longDesc.data());
    Require(std::strcmp(_ZNKSt12out_of_range4whatEv_nid_postfix(&rangeBase), LongDiag) == 0,
            "out_of_range C2 must copy string content");
    _ZNSt12out_of_rangeD2Ev_nid_postfix(&rangeBase);

    LibcException::ExceptionObject lenComplete{};
    _ZNSt12length_errorC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &lenComplete, longDesc.data());
    Require(std::strcmp(_ZNKSt12length_error4whatEv_nid_postfix(&lenComplete), LongDiag) == 0,
            "length_error C1 must copy string content");
    _ZNSt12length_errorD1Ev_nid_postfix(&lenComplete);

    LibcException::ExceptionObject lenBase{};
    _ZNSt12length_errorC2ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &lenBase, longDesc.data());
    Require(std::strcmp(_ZNKSt12length_error4whatEv_nid_postfix(&lenBase), LongDiag) == 0,
            "length_error C2 must copy string content");
    _ZNSt12length_errorD2Ev_nid_postfix(&lenBase);

    LibcException::ExceptionObject domComplete{};
    _ZNSt12domain_errorC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &domComplete, longDesc.data());
    Require(std::strcmp(_ZNKSt12domain_error4whatEv_nid_postfix(&domComplete), LongDiag) == 0,
            "domain_error C1 must copy string content");
    _ZNSt12domain_errorD1Ev_nid_postfix(&domComplete);

    LibcException::ExceptionObject domBase{};
    _ZNSt12domain_errorC2ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &domBase, longDesc.data());
    Require(std::strcmp(_ZNKSt12domain_error4whatEv_nid_postfix(&domBase), LongDiag) == 0,
            "domain_error C2 must copy string content");
    _ZNSt12domain_errorD2Ev_nid_postfix(&domBase);

    LibcException::ExceptionObject invalid{};
    const std::array<unsigned char, 24> invalidShort{46};
    bool rejected = false;
    try {
        _ZNSt11logic_errorC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
            &invalid, invalidShort.data());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "logic_error constructor must reject invalid short string");

    std::puts("libc++ logic_error family string constructor ABI checks passed");
    return 0;
}
