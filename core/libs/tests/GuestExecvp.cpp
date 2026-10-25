#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>

template <typename TCall> static bool Throws(TCall call, const std::string& message) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return error.what() == message;
    }
    return false;
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    char name[] = "sh";
    char* arguments[] = {name, nullptr};
    char variable[] = "HOME=/";
    char* environment[] = {variable, nullptr};
    Require(Throws([&] { execvp_nid_postfix("sh", arguments); }, "execvp: executable replacement not implemented"));
    Require(Throws([&] { execvp_nid_postfix("/bin/sh", arguments); }, "execvp: executable replacement not implemented"));
    Require(Throws([&] { execvp_nid_postfix(nullptr, nullptr); }, "execvp: executable replacement not implemented"));
    Require(Throws([&] { execv_nid_postfix("/bin/sh", arguments); }, "execv: executable replacement not implemented"));
    Require(Throws([&] { execve_nid_postfix("/bin/sh", arguments, environment); }, "execve: executable replacement not implemented"));
}
