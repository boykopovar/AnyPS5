#include "prx/libc/include/Api.hpp"
#include "LibcxxStringArgument.hpp"

#include <cerrno>
#include <limits>

namespace {

struct RestoreErrno {
    int& value;
    int saved;

    ~RestoreErrno() { value = saved; }
};

template <typename Value>
struct ParsedNumber {
    Value value;
    std::uint64_t consumed;
    bool outOfRange;
};

template <typename Parse>
auto Convert(const void* value, std::uint64_t* index, const char* invalid, const char* range, Parse parse) {
    const auto parsed = [&] {
        auto& error = *__error_nid_postfix();
        const RestoreErrno restore{error, error};
        const auto input = CopyLibcxxStringArgument(value);
        const char* begin = input.c_str();
        char* end = nullptr;
        error = 0;
        const auto number = parse(begin, &end);
        return ParsedNumber{number, end ? static_cast<std::uint64_t>(end - begin) : 0, error == ERANGE};
    }();
    if (parsed.outOfRange) {
        _ZSt14_Xout_of_rangePKc_nid_postfix(range);
    }
    if (parsed.consumed == 0) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix(invalid);
    }
    if (index && !GuestCopyOut_nid_no_patch(index, &parsed.consumed, sizeof(parsed.consumed))) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("libc++ conversion: unwritable index");
    }
    return parsed.value;
}

template <typename Parse>
auto ConvertInteger(const void* value, std::uint64_t* index, int base, const char* invalid, const char* range, Parse parse) {
    if (base != 0 && (base < 2 || base > 36)) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix(invalid);
    }
    return Convert(value, index, invalid, range, [base, parse](const char* input, char** end) {
        return parse(input, end, base);
    });
}

}

extern "C" int APS5_VABI _ZNSt3__14stoiERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi_nid_postfix(
    const void* value, std::uint64_t* index, int base) {
    constexpr const char* RangeError = "stoi: out of range";
    const auto result = ConvertInteger(value, index, base, "stoi: no conversion", RangeError, strtoll_nid_postfix);
    if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max()) {
        _ZSt14_Xout_of_rangePKc_nid_postfix(RangeError);
    }
    return static_cast<int>(result);
}

extern "C" std::uint64_t APS5_VABI _ZNSt3__15stoulERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi_nid_postfix(
    const void* value, std::uint64_t* index, int base) {
    return ConvertInteger(value, index, base, "stoul: no conversion", "stoul: out of range", strtoul_nid_postfix);
}

extern "C" std::uint64_t APS5_VABI _ZNSt3__16stoullERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi_nid_postfix(
    const void* value, std::uint64_t* index, int base) {
    return ConvertInteger(value, index, base, "stoull: no conversion", "stoull: out of range", strtoull_nid_postfix);
}

extern "C" float APS5_VABI _ZNSt3__14stofERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPm_nid_postfix(
    const void* value, std::uint64_t* index) {
    return Convert(value, index, "stof: no conversion", "stof: out of range", strtof_nid_postfix);
}
