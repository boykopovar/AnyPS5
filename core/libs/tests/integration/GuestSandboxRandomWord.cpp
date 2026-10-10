#include "prx/libc/include/general/VabiMacros.hpp"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

extern "C" {
const char* APS5_VABI sceKernelGetFsSandboxRandomWord();
int APS5_VABI open_nid_postfix(const char*, int, int);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    const char* word = sceKernelGetFsSandboxRandomWord();
    Require(word != nullptr);
    const std::size_t length = std::strlen(word);
    Require(length >= 1);
    for (std::size_t index = 0; index < length; ++index)
        Require(std::isalnum(static_cast<unsigned char>(word[index])));

    Require(sceKernelGetFsSandboxRandomWord() == word);
    const char* fromThread = nullptr;
    std::thread([&] { fromThread = sceKernelGetFsSandboxRandomWord(); }).join();
    Require(fromThread == word);

    const std::string path = std::string("/") + word + "/common/lib/libc.sprx";
    *__error_nid_postfix() = 0;
    Require(open_nid_postfix(path.c_str(), 0, 0) == -1);
    Require(*__error_nid_postfix() == 2);
    return 0;
}
