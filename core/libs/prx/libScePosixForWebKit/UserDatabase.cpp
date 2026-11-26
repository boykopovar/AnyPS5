#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
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

namespace {

constexpr int PosixEinval = 22;
constexpr int PosixErange = 34;
constexpr int PosixEfault = 14;
constexpr std::size_t RootStringBytes = 12;
constexpr std::size_t MaximumName = 4096;
constexpr char RootStrings[RootStringBytes] = {'r', 'o', 'o', 't', 0, '*', 0, 0, 0, '/', 0, 0};

int CopyRootAccount(PosixForWebKitPasswd* pwd, char* buffer, std::size_t bufsize) {
    if (bufsize < RootStringBytes) return PosixErange;
    if (!GuestCopyOut_nid_no_patch(buffer, RootStrings, RootStringBytes)) return PosixEfault;
    const PosixForWebKitPasswd entry = {
        buffer, buffer + 5, 0, 0, 0, buffer + 7, buffer + 8, buffer + 9, buffer + 11, 0, 0x3ff
    };
    if (!GuestCopyOut_nid_no_patch(pwd, &entry, sizeof(entry))) return PosixEfault;
    return 0;
}

int StoreResult(PosixForWebKitPasswd** result, PosixForWebKitPasswd* value) {
    if (!GuestCopyOut_nid_no_patch(result, &value, sizeof(value))) return PosixEfault;
    return 0;
}

int Report(PosixForWebKitPasswd** result, int error) {
    if (result != nullptr) StoreResult(result, nullptr);
    return error;
}

int LookupRoot(PosixForWebKitPasswd* pwd, char* buffer, std::size_t bufsize, PosixForWebKitPasswd** result) {
    const int error = CopyRootAccount(pwd, buffer, bufsize);
    if (error != 0) return Report(result, error);
    return StoreResult(result, pwd);
}

}

int APS5_VABI getpwuid_r_nid_postfix(std::uint32_t uid, PosixForWebKitPasswd* pwd, char* buffer, std::size_t bufsize, PosixForWebKitPasswd** result) {
    if (result == nullptr) return PosixEinval;
    if (uid != 0) return StoreResult(result, nullptr);
    return LookupRoot(pwd, buffer, bufsize, result);
}

int APS5_VABI getpwnam_r_nid_postfix(const char* name, PosixForWebKitPasswd* pwd, char* buffer, std::size_t bufsize, PosixForWebKitPasswd** result) {
    if (result == nullptr) return PosixEinval;
    if (name == nullptr) return Report(result, PosixEinval);
    std::string copy;
    const int copyError = CopyGuestString_nid_no_patch(name, MaximumName, copy);
    if (copyError == 63) return StoreResult(result, nullptr);
    if (copyError == 12) return Report(result, copyError);
    if (copyError != 0) return Report(result, PosixEfault);
    if (copy != "root") return StoreResult(result, nullptr);
    return LookupRoot(pwd, buffer, bufsize, result);
}

PosixForWebKitPasswd* APS5_VABI getpwnam_nid_postfix(const char* name) {
    if (name == nullptr) throw std::invalid_argument("getpwnam: null name");
    std::string copy;
    if (CopyGuestString_nid_no_patch(name, 256, copy) != 0 || copy != "root") return nullptr;
    return getpwuid_nid_postfix(0);
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
