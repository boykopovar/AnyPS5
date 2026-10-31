#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pwd.h>
#include <string>
#include <unistd.h>

static int loginError = ENXIO;

extern "C" bool GuestCopyOut_nid_no_patch(void* destination, const void* source, std::size_t bytes) {
    if (destination == nullptr) return false;
    std::memcpy(destination, source, bytes);
    return true;
}

extern "C" int getlogin_r(char* output, std::size_t size) {
    if (loginError != 0) return loginError;
    constexpr char sessionName[] = "anyps5-other-login-session";
    if (size < sizeof(sessionName)) return ERANGE;
    std::memcpy(output, sessionName, sizeof(sessionName));
    return 0;
}

int main() {
    const passwd* account = ::getpwuid(::geteuid());
    if (account == nullptr || account->pw_name == nullptr || account->pw_name[0] == '\0')
        std::abort();
    const std::string expected(account->pw_name);
    for (int error : {ENXIO, ENOENT, 0}) {
        loginError = error;
        std::array<char, 4096> output{};
        errno = 12345;
        const int result = getlogin_r_nid_postfix(output.data(), static_cast<int>(output.size()));
        if (result != 0 || errno != 12345 || output.data() != expected) {
            std::fprintf(stderr, "getlogin_r changed with host login-session result %d: guest result %d\n",
                error, result);
            return 1;
        }
    }
}
