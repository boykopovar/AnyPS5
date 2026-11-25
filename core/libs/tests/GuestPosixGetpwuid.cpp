#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    errno = E2BIG;
    auto* root = getpwuid_nid_postfix(0);
    Require(root != nullptr);
    Require(root->pw_uid == 0 && root->pw_gid == 0);
    Require(std::strcmp(root->pw_name, "root") == 0);
    Require(std::strcmp(root->pw_dir, "/") == 0);
    Require(root->pw_passwd != nullptr && root->pw_class != nullptr);
    Require(root->pw_gecos != nullptr && root->pw_shell != nullptr);
    Require(root->pw_change == 0 && root->pw_expire == 0);
    Require((root->pw_fields & 0x3ff) == 0x3ff);
    Require(errno == E2BIG);

    root->pw_name[0] = 'X';
    Require(getpwuid_nid_postfix(0) == root);
    Require(std::strcmp(root->pw_name, "root") == 0);
    Require(getpwuid_nid_postfix(1) == nullptr);
    Require(getpwuid_nid_postfix(UINT32_MAX) == nullptr);
    Require(errno == E2BIG);

    Require(getpwnam_nid_postfix("root") == root);
    Require(std::strcmp(root->pw_name, "root") == 0 && root->pw_uid == 0);
    Require(getpwnam_nid_postfix("nobody") == nullptr);
    Require(getpwnam_nid_postfix("") == nullptr);
    Require(getpwnam_nid_postfix("Root") == nullptr);
    const std::string longName(300, 'r');
    Require(getpwnam_nid_postfix(longName.c_str()) == nullptr);
    Require(errno == E2BIG);
    bool threw = false;
    try {
        getpwnam_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}
