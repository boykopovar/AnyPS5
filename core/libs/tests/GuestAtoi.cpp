#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>

int main() {
    struct Case {
        const char* text;
        int value;
        int error;
    };
    constexpr int SavedError = 1234;
    constexpr Case cases[] = {
        {"0", 0, SavedError},
        {"+42", 42, SavedError},
        {" \t\n\r\f\v-0012tail", -12, SavedError},
        {"010", 10, SavedError},
        {"0x10", 0, SavedError},
        {"0b10", 0, SavedError},
        {"", 0, SavedError},
        {"words", 0, SavedError},
        {" +", 0, SavedError},
        {"2147483647", INT32_MAX, SavedError},
        {"-2147483648", INT32_MIN, SavedError},
        {"2147483648", INT32_MIN, SavedError},
        {"-2147483649", INT32_MAX, SavedError},
        {"4294967295", -1, SavedError},
        {"-4294967295", 1, SavedError},
        {"4294967296", 0, SavedError},
        {"-4294967296", 0, SavedError},
        {" \t+4294967297tail", 1, SavedError},
        {"9223372036854775807", -1, SavedError},
        {"-9223372036854775808", 0, SavedError},
        {"9223372036854775808", 0, SavedError},
        {"-9223372036854775809", -1, SavedError},
        {"999999999999999999999999999999999999", -1, 34},
        {"-999999999999999999999999999999999999", -1, 34},
    };
    bool correct = true;
    for (const auto& test : cases) {
        *__error_nid_postfix() = SavedError;
        const int value = atoi_nid_postfix(test.text);
        const int error = *__error_nid_postfix();
        if (value != test.value || error != test.error) {
            std::fprintf(stderr, "atoi(%s): expected %d, errno %d; received %d, errno %d\n",
                test.text, test.value, test.error, value, error);
            correct = false;
        }
    }
    struct WideCase {
        const char* text;
        std::int64_t value;
        int error;
    };
    constexpr WideCase wideCases[] = {
        {"0", 0, SavedError},
        {" \t+42tail", 42, SavedError},
        {"", 0, SavedError},
        {"words", 0, SavedError},
        {"9223372036854775807", INT64_MAX, SavedError},
        {"-9223372036854775808", INT64_MIN, SavedError},
        {"9223372036854775808", INT64_MIN, SavedError},
        {"-9223372036854775809", INT64_MAX, SavedError},
        {"18446744073709551615", -1LL, SavedError},
        {"-18446744073709551615", 1LL, SavedError},
        {"18446744073709551616", -1LL, 34},
        {"999999999999999999999999999999999999", -1LL, 34},
        {"-999999999999999999999999999999999999", -1LL, 34},
    };
    for (const auto& test : wideCases) {
        *__error_nid_postfix() = SavedError;
        const std::int64_t lvalue = atol_nid_postfix(test.text);
        const int lerror = *__error_nid_postfix();
        if (lvalue != test.value || lerror != test.error) {
            std::fprintf(stderr, "atol(%s): expected %lld, errno %d; received %lld, errno %d\n",
                test.text, static_cast<long long>(test.value), test.error,
                static_cast<long long>(lvalue), lerror);
            correct = false;
        }
        *__error_nid_postfix() = SavedError;
        const long long llvalue = atoll_nid_postfix(test.text);
        const int llerror = *__error_nid_postfix();
        if (llvalue != static_cast<long long>(test.value) || llerror != test.error) {
            std::fprintf(stderr, "atoll(%s): expected %lld, errno %d; received %lld, errno %d\n",
                test.text, static_cast<long long>(test.value), test.error, llvalue, llerror);
            correct = false;
        }
    }
    return correct ? 0 : 1;
}
