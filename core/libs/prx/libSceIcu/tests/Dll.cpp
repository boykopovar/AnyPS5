#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu dll: " + message);
}

}

int main() {
    Require(uprv_isASCIILetter_nid_postfix('a') == 1, "a is not reported as an ASCII letter");
    Require(uprv_isASCIILetter_nid_postfix('Z') == 1, "Z is not reported as an ASCII letter");
    Require(uprv_isASCIILetter_nid_postfix('0') == 0, "0 is reported as an ASCII letter");
    Require(uprv_isASCIILetter_nid_postfix('\0') == 0, "a NUL is reported as an ASCII letter");

    std::int32_t status = 0;
    Require(uprv_dl_open_nid_postfix("no such library for anyps5", &status) == nullptr,
        "an absent library reported a handle");
    Require(status == 2, "an absent library did not report a missing resource");

    status = 0;
    Require(uprv_dl_open_nid_postfix(nullptr, &status) == nullptr, "a null library name was accepted");
    Require(status == 1, "a null library name did not report an illegal argument");

    status = 0;
    Require(uprv_dlsym_func_nid_postfix(nullptr, "x", &status) == nullptr, "a null library was accepted");
    Require(status == 1, "a null library did not report an illegal argument");

    status = 0;
    uprv_dl_close_nid_postfix(nullptr, &status);
    Require(status == 0, "closing a null library changed the status");


    {
        std::int32_t status = 0;
        void* module = uprv_dl_open_nid_postfix("kernel32.dll", &status);
#if defined(_WIN32)
        Require(module != nullptr, "kernel32.dll did not open through the guest name copy");
        const bool loaded = true;
#else
        const bool loaded = module != nullptr;
#endif
        if (loaded) {
            status = 0;
            void* symbol = uprv_dlsym_func_nid_postfix(module, "GetLastError", &status);
            Require(status == 0 && symbol != nullptr, "a known exported symbol was not found");
            status = 0;
            Require(uprv_dlsym_func_nid_postfix(module, "no such symbol in kernel32", &status) == nullptr,
                "an absent symbol reported an address");
            Require(status == 2, "an absent symbol did not report a missing resource");
            status = 0;
            uprv_dl_close_nid_postfix(module, &status);
            Require(status == 0, "closing a library changed the status");
        }
    }

    status = 0;
    Require(uprv_dl_open_nid_postfix("x", nullptr) == nullptr, "a null status was accepted");
    return 0;
}
