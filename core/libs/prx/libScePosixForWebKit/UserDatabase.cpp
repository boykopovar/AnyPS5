#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>
#include <string>

namespace {

struct RootAccount {
    char name[5] = "root";
    char password[2] = "*";
    char accessClass[1] = "";
    char gecos[1] = "";
    char directory[2] = "/";
    char shell[1] = "";
    PosixForWebKitPasswd entry{};
};

}

PosixForWebKitPasswd* APS5_VABI getpwuid_nid_postfix(std::uint32_t uid) {
    if (uid != 0) return nullptr;
    static thread_local RootAccount account;
    account = RootAccount{};
    account.entry = {
        account.name, account.password, 0, 0, 0, account.accessClass,
        account.gecos, account.directory, account.shell, 0, 0x3ff
    };
    return &account.entry;
}

PosixForWebKitPasswd* APS5_VABI getpwnam_nid_postfix(const char* name) {
    if (name == nullptr) throw std::invalid_argument("getpwnam: null name");
    std::string copy;
    if (CopyGuestString_nid_no_patch(name, 256, copy) != 0 || copy != "root") return nullptr;
    return getpwuid_nid_postfix(0);
}
