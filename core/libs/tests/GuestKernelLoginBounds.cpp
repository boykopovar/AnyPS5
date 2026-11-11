#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr int GuestErange = 34;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestGetloginrNullAndSizeBounds() {
    *__error_nid_postfix() = 123;
    Require(getlogin_r_nid_postfix(nullptr, 64) == GuestEfault);
    Require(getlogin_r_nid_postfix(nullptr, 1) == GuestEfault);
    Require(getlogin_r_nid_postfix(nullptr, 0) == GuestErange);
    Require(getlogin_r_nid_postfix(nullptr, -1) == GuestErange);
    Require(*__error_nid_postfix() == 123);

    std::array<char, 64> buffer{};
    buffer.fill('\x5a');
    const auto expectedUntouched = buffer;

    for (int size : {0, -1, -100}) {
        *__error_nid_postfix() = 99;
        Require(getlogin_r_nid_postfix(buffer.data(), size) == GuestErange);
        Require(buffer == expectedUntouched);
        Require(*__error_nid_postfix() == 99);
    }
}

void TestSetloginErrorBounds() {
    *__error_nid_postfix() = 0;
    Require(setlogin_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == GuestEfault);
    Require(SetLogin_nid_no_patch(nullptr) == GuestEfault);

    const std::string validLongest(32, 'u');
    Require(setlogin_nid_postfix(validLongest.c_str()) == 0);
    Require(SetLogin_nid_no_patch(validLongest.c_str()) == 0);

    const std::string name33(33, 'v');
    *__error_nid_postfix() = 0;
    Require(setlogin_nid_postfix(name33.c_str()) == -1);
    Require(*__error_nid_postfix() == GuestEinval);
    Require(SetLogin_nid_no_patch(name33.c_str()) == GuestEinval);

    const std::string name34(34, 'w');
    *__error_nid_postfix() = 0;
    Require(setlogin_nid_postfix(name34.c_str()) == -1);
    Require(*__error_nid_postfix() == GuestEinval);
    Require(SetLogin_nid_no_patch(name34.c_str()) == GuestEinval);

    const std::string name64(64, 'x');
    *__error_nid_postfix() = 0;
    Require(setlogin_nid_postfix(name64.c_str()) == -1);
    Require(*__error_nid_postfix() == GuestEinval);
    Require(SetLogin_nid_no_patch(name64.c_str()) == GuestEinval);

    std::array<char, 33> unterminated{};
    unterminated.fill('z');
    *__error_nid_postfix() = 0;
    Require(setlogin_nid_postfix(unterminated.data()) == -1);
    Require(*__error_nid_postfix() == GuestEinval);
    Require(SetLogin_nid_no_patch(unterminated.data()) == GuestEinval);
}

void TestGetloginrShortBufferAndSuccess() {
    const std::string loginName = "testloginuser";
    Require(setlogin_nid_postfix(loginName.c_str()) == 0);

    for (int s = 1; s < 14; ++s) {
        std::array<char, 64> output{};
        output.fill('\x7e');
        const auto untouched = output;
        *__error_nid_postfix() = 88;
        Require(getlogin_r_nid_postfix(output.data(), s) == GuestErange);
        Require(output == untouched);
        Require(*__error_nid_postfix() == 88);
    }

    std::array<char, 64> exactOutput{};
    exactOutput.fill('\x7e');
    *__error_nid_postfix() = 88;
    Require(getlogin_r_nid_postfix(exactOutput.data(), 14) == 0);
    Require(std::strcmp(exactOutput.data(), loginName.c_str()) == 0);
    Require(exactOutput[14] == '\x7e');
    Require(*__error_nid_postfix() == 88);

    std::array<char, 64> largeOutput{};
    largeOutput.fill('\x7e');
    Require(getlogin_r_nid_postfix(largeOutput.data(), static_cast<int>(largeOutput.size())) == 0);
    Require(std::strcmp(largeOutput.data(), loginName.c_str()) == 0);
    Require(largeOutput[loginName.size() + 1] == '\x7e');

    const std::string maxName(32, 'm');
    Require(setlogin_nid_postfix(maxName.c_str()) == 0);
    std::array<char, 64> maxOutput{};
    maxOutput.fill('\x33');
    Require(getlogin_r_nid_postfix(maxOutput.data(), 32) == GuestErange);
    Require(maxOutput[0] == '\x33');
    Require(getlogin_r_nid_postfix(maxOutput.data(), 33) == 0);
    Require(std::strcmp(maxOutput.data(), maxName.c_str()) == 0);
}

void TestGetloginFunction() {
    const std::string sessionName = "posixuser";
    Require(setlogin_nid_postfix(sessionName.c_str()) == 0);
    char* loginPtr = getlogin_nid_postfix();
    Require(loginPtr != nullptr);
    Require(std::strcmp(loginPtr, sessionName.c_str()) == 0);
    Require(getlogin_nid_postfix() == loginPtr);

    const std::string maxUser(32, 'k');
    Require(setlogin_nid_postfix(maxUser.c_str()) == 0);
    char* maxPtr = getlogin_nid_postfix();
    Require(maxPtr != nullptr);
    Require(std::strcmp(maxPtr, maxUser.c_str()) == 0);

    Require(setlogin_nid_postfix("") == 0);
    char* emptyPtr = getlogin_nid_postfix();
    Require(emptyPtr == nullptr);
}

void TestSetloginDirectSeam() {
    Require(SetLogin_nid_no_patch("direct_login") == 0);
    char* directPtr = getlogin_nid_postfix();
    Require(directPtr != nullptr);
    Require(std::strcmp(directPtr, "direct_login") == 0);
    Require(SetLogin_nid_no_patch(nullptr) == GuestEfault);
    const std::string overLimit(33, 'q');
    Require(SetLogin_nid_no_patch(overLimit.c_str()) == GuestEinval);
}

} // namespace

int main() {
    TestGetloginrNullAndSizeBounds();
    TestSetloginErrorBounds();
    TestGetloginrShortBufferAndSuccess();
    TestGetloginFunction();
    TestSetloginDirectSeam();
    return 0;
}
