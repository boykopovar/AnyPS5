#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <cstdlib>
#include <cstring>
#include <string>

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    PosixForWebKitPasswd pwd{};
    char buffer[64];
    PosixForWebKitPasswd* result = nullptr;

    Require(getpwuid_r_nid_postfix(0, &pwd, buffer, sizeof(buffer), &result) == 0);
    Require(result == &pwd);
    Require(pwd.pw_uid == 0 && pwd.pw_gid == 0);
    Require(std::strcmp(pwd.pw_name, "root") == 0 && pwd.pw_name == buffer);
    Require(std::strcmp(pwd.pw_passwd, "*") == 0);
    Require(std::strcmp(pwd.pw_dir, "/") == 0);
    Require(std::strcmp(pwd.pw_class, "") == 0 && std::strcmp(pwd.pw_gecos, "") == 0 && std::strcmp(pwd.pw_shell, "") == 0);
    Require(pwd.pw_change == 0 && pwd.pw_expire == 0 && (pwd.pw_fields & 0x3ff) == 0x3ff);

    result = &pwd;
    Require(getpwuid_r_nid_postfix(1, &pwd, buffer, sizeof(buffer), &result) == 0);
    Require(result == nullptr);

    result = nullptr;
    Require(getpwuid_r_nid_postfix(0, &pwd, buffer, 11, &result) == 34);

    result = nullptr;
    Require(getpwnam_r_nid_postfix("root", &pwd, buffer, sizeof(buffer), &result) == 0);
    Require(result == &pwd && pwd.pw_uid == 0);

    result = &pwd;
    Require(getpwnam_r_nid_postfix("nobody", &pwd, buffer, sizeof(buffer), &result) == 0);
    Require(result == nullptr);

    const std::string overlong(300, 'x');
    result = &pwd;
    Require(getpwnam_r_nid_postfix(overlong.c_str(), &pwd, buffer, sizeof(buffer), &result) == 0);
    Require(result == nullptr);
    const std::string beyondBound(5000, 'x');
    result = &pwd;
    Require(getpwnam_r_nid_postfix(beyondBound.c_str(), &pwd, buffer, sizeof(buffer), &result) == 0);
    Require(result == nullptr);

    Require(getpwnam_r_nid_postfix("root", &pwd, buffer, 11, &result) == 34);
    return 0;
}
