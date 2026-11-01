#include "SceTypes.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <string>

static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ENAMETOOLONG = 63;

static void Require(bool value) { if (!value) std::abort(); }

static std::string ReadName(Pthread thread) {
    std::array<char, 64> buffer{};
    buffer.fill(0x55);
    Require(pthread_getname_np_nid_postfix(thread, buffer.data()) == 0);
    return std::string(buffer.data());
}

static void RequireStored(Pthread thread, const std::string& name) {
    Require(ReadName(thread) == name);
}

int main() {
    const Pthread self = pthread_self_nid_postfix();
    RequireStored(self, "");

    Require(pthread_setname_np_nid_postfix(self, "webkit") == 0);
    RequireStored(self, "webkit");

    Require(pthread_setname_np_nid_postfix(self, "") == 0);
    RequireStored(self, "");

    Require(pthread_setname_np_nid_postfix(self, nullptr) == GUEST_EINVAL);
    Require(pthread_setname_np_nid_postfix(nullptr, "name") == GUEST_EINVAL);

    const std::string longest(31, 'b');
    Require(pthread_setname_np_nid_postfix(self, longest.c_str()) == 0);
    RequireStored(self, longest);

    const std::string tooLong(32, 'c');
    Require(pthread_setname_np_nid_postfix(self, tooLong.c_str()) == GUEST_ENAMETOOLONG);
    RequireStored(self, longest);
    return 0;
}
