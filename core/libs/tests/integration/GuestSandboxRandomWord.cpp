#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cctype>
#include <cstring>
#include <string>
#include <thread>

extern "C" {
const char* APS5_VABI sceKernelGetFsSandboxRandomWord();
int APS5_VABI open_nid_postfix(const char*, int, int);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int enoent = 2;

const Case alphanumeric{"FsSandboxRandomWord_Get_ReturnsNonEmptyAlphanumericWord", [] {
    const char* word = sceKernelGetFsSandboxRandomWord();
    Require(word != nullptr, "word is not null");
    const std::size_t length = std::strlen(word);
    Require(length >= 1, "word is not empty");
    for (std::size_t index = 0; index < length; ++index) {
        Require(std::isalnum(static_cast<unsigned char>(word[index])) != 0,
                "character " + std::to_string(index) + " of " + word + " is alphanumeric");
    }
}};

const Case stable{"FsSandboxRandomWord_RepeatedCalls_ReturnSamePointer", [] {
    const char* word = sceKernelGetFsSandboxRandomWord();
    Require(sceKernelGetFsSandboxRandomWord() == word, "second call returns the same word");
}};

const Case otherThread{"FsSandboxRandomWord_OtherThread_ReturnsSamePointer", [] {
    const char* word = sceKernelGetFsSandboxRandomWord();
    const char* fromThread = nullptr;
    std::thread([&fromThread] { fromThread = sceKernelGetFsSandboxRandomWord(); }).join();
    Require(fromThread == word, "worker thread sees the same word");
}};

const Case sandboxPath{"Open_SandboxSystemLibraryPath_FailsWithEnoent", [] {
    const std::string path = std::string("/") + sceKernelGetFsSandboxRandomWord() + "/common/lib/libc.sprx";
    *__error_nid_postfix() = 0;
    RequireEqual(open_nid_postfix(path.c_str(), 0, 0), -1, "open " + path);
    RequireEqual(*__error_nid_postfix(), enoent, "errno");
}};

} // namespace
