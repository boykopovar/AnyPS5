#include "prx/libScePosixForWebKit/include/Api.hpp"
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
    Require(NotImplemented([] { gethostbyname2_nid_postfix("h", 2); }));
    Require(NotImplemented([] { getservbyport_nid_postfix(0, "tcp"); }));
    Require(NotImplemented([] { getprotobyname_nid_postfix("tcp"); }));
    Require(NotImplemented([] { getnetbyname_nid_postfix("n"); }));
    Require(NotImplemented([] { endhostent_nid_postfix(); }));
    Require(NotImplemented([] { sethostent_nid_postfix(0); }));
    Require(NotImplemented([] { gethostent_nid_postfix(); }));
    Require(NotImplemented([] { getservent_nid_postfix(); }));
    Require(NotImplemented([] { setservent_nid_postfix(0); }));
    Require(NotImplemented([] { endservent_nid_postfix(); }));
    Require(NotImplemented([] { getprotoent_nid_postfix(); }));
    Require(NotImplemented([] { getgrouplist_nid_postfix("u", 0, nullptr, nullptr); }));
    Require(NotImplemented([] { initgroups_nid_postfix("u", 0); }));
    Require(NotImplemented([] { grantpt_nid_postfix(0); }));
    Require(NotImplemented([] { unlockpt_nid_postfix(0); }));
    Require(NotImplemented([] { ptsname_nid_postfix(0); }));
    return 0;
}
