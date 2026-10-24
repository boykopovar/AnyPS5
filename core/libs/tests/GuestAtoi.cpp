#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>

extern "C" {
int APS5_VABI atoi_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
}

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
    return correct ? 0 : 1;
}
