#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>

extern "C" {
char* APS5_VABI strerror_nid_postfix(int);
int APS5_VABI strerror_r_nid_postfix(int, char*, std::size_t);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int eacces = 13;
constexpr int einval = 22;
constexpr int erange = 34;

const Case eagainMessage{"Strerror_Eagain_ReturnsMessageAndKeepsErrno", [] {
    *__error_nid_postfix() = eacces;
    RequireEqual(std::string_view(strerror_nid_postfix(35)), std::string_view("Resource temporarily unavailable"), "strerror(35)");
    RequireEqual(*__error_nid_postfix(), eacces, "errno");
}};

const Case enosysMessage{"Strerror_Enosys_ReturnsFunctionNotImplemented", [] {
    RequireEqual(std::string_view(strerror_nid_postfix(78)), std::string_view("Function not implemented"), "strerror(78)");
}};

const Case einvalKeepsErrno{"Strerror_Einval_KeepsErrno", [] {
    *__error_nid_postfix() = eacces;
    strerror_nid_postfix(einval);
    RequireEqual(*__error_nid_postfix(), eacces, "errno");
}};

const Case perThreadBuffer{"Strerror_CallOnOtherThread_DoesNotOverwriteCallerMessage", [] {
    const char* parent = strerror_nid_postfix(einval);
    std::string workerMessage;
    std::thread worker([&] { workerMessage = strerror_nid_postfix(45); });
    worker.join();
    RequireEqual(workerMessage, std::string("Operation not supported"), "worker strerror(45)");
    RequireEqual(std::string_view(parent), std::string_view("Invalid argument"), "parent strerror(22)");
}};

const Case knownCodes{"StrerrorR_EveryKnownCode_ReturnsDescriptiveMessageAndKeepsErrno", [] {
    char buffer[128];
    *__error_nid_postfix() = eacces;
    for (int error = 0; error <= 96; ++error) {
        const std::string code = "error " + std::to_string(error);
        RequireEqual(strerror_r_nid_postfix(error, buffer, sizeof(buffer)), 0, code + " result");
        Require(buffer[0] != '\0', code + " message is not empty");
        Require(std::strstr(buffer, "Unknown") == nullptr, code + " message is not unknown: " + buffer);
    }
    RequireEqual(*__error_nid_postfix(), eacces, "errno");
}};

const Case negativeCode{"StrerrorR_NegativeCode_ReturnsEinvalWithCodeInMessage", [] {
    char buffer[128];
    RequireEqual(strerror_r_nid_postfix(-1, buffer, sizeof(buffer)), einval, "result");
    Require(std::strstr(buffer, "-1") != nullptr, std::string("message mentions -1: ") + buffer);
}};

const Case negativeCodeErrno{"StrerrorR_NegativeCode_KeepsErrno", [] {
    char buffer[128];
    *__error_nid_postfix() = eacces;
    strerror_r_nid_postfix(-1, buffer, sizeof(buffer));
    RequireEqual(*__error_nid_postfix(), eacces, "errno");
}};

const Case oneByte{"StrerrorR_OneByteBuffer_ReturnsErangeAndTerminates", [] {
    char sentinel[] = "xyz";
    RequireEqual(strerror_r_nid_postfix(einval, sentinel, 1), erange, "result");
    RequireEqual(sentinel[0], '\0', "first byte");
    RequireEqual(sentinel[1], 'y', "second byte untouched");
}};

const Case zeroLength{"StrerrorR_ZeroLengthBuffer_ReturnsErangeWithoutWriting", [] {
    char sentinel[] = "xyz";
    RequireEqual(strerror_r_nid_postfix(einval, sentinel, 0), erange, "result");
    RequireEqual(sentinel[0], 'x', "first byte untouched");
}};

const Case nullBuffer{"StrerrorR_NullZeroLengthBuffer_ReturnsErange", [] {
    RequireEqual(strerror_r_nid_postfix(einval, nullptr, 0), erange, "result");
}};

} // namespace
