#include "libs/Require.hpp"
#include "prx/libwebrtc/include/Api.hpp"
#include "prx/libwebrtc/libwebrtc.hpp"

namespace {

void TestDestructor() {
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(nullptr);
}

}

int main() {
    TestDestructor();
    return 0;
}
