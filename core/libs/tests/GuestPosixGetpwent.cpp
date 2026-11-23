#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <cstdlib>
#include <cstring>

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    auto* first = getpwent_nid_postfix();
    Require(first != nullptr);
    Require(first->pw_uid == 0 && std::strcmp(first->pw_name, "root") == 0);
    Require(getpwent_nid_postfix() == nullptr);
    Require(getpwent_nid_postfix() == nullptr);

    setpwent_nid_postfix();
    auto* again = getpwent_nid_postfix();
    Require(again != nullptr && std::strcmp(again->pw_name, "root") == 0);
    Require(getpwent_nid_postfix() == nullptr);

    endpwent_nid_postfix();
    auto* reopened = getpwent_nid_postfix();
    Require(reopened != nullptr && std::strcmp(reopened->pw_name, "root") == 0);
    endpwent_nid_postfix();
    return 0;
}
