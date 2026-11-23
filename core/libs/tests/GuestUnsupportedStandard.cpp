#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

template<class TCall>
bool NotImplemented(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    Require(NotImplemented([] { regcomp_nid_postfix(nullptr, "a", 0); }));
    Require(NotImplemented([] { regexec_nid_postfix(nullptr, "a", 0, nullptr, 0); }));
    Require(NotImplemented([] { regfree_nid_postfix(nullptr); }));
    Require(NotImplemented([] { regerror_nid_postfix(0, nullptr, nullptr, 0); }));
    Require(NotImplemented([] { glob_nid_postfix("*", 0, nullptr, nullptr); }));
    Require(NotImplemented([] { globfree_nid_postfix(nullptr); }));
    Require(NotImplemented([] { wordexp_nid_postfix("a", nullptr, 0); }));
    Require(NotImplemented([] { wordfree_nid_postfix(nullptr); }));
    Require(NotImplemented([] { catopen_nid_postfix("c", 0); }));
    Require(NotImplemented([] { catgets_nid_postfix(nullptr, 1, 1, "s"); }));
    Require(NotImplemented([] { catclose_nid_postfix(nullptr); }));
    Require(NotImplemented([] { nl_langinfo_nid_postfix(0); }));
    Require(NotImplemented([] { strptime_nid_postfix("", "", nullptr); }));
    Require(NotImplemented([] { fmtmsg_nid_postfix(0, nullptr, 0, nullptr, nullptr, nullptr); }));
    Require(NotImplemented([] { confstr_nid_postfix(0, nullptr, 0); }));
    Require(NotImplemented([] { uname_nid_postfix(nullptr); }));
    Require(NotImplemented([] { gethostid_nid_postfix(); }));
    return 0;
}
