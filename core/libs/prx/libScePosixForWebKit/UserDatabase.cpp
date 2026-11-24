#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <atomic>

namespace {

std::atomic<bool> rootEnumerated{false};

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

PosixForWebKitPasswd* APS5_VABI getpwent_nid_postfix() {
    if (rootEnumerated.exchange(true)) return nullptr;
    return getpwuid_nid_postfix(0);
}

void APS5_VABI setpwent_nid_postfix() {
    rootEnumerated.store(false);
}

void APS5_VABI endpwent_nid_postfix() {
    rootEnumerated.store(false);
}
