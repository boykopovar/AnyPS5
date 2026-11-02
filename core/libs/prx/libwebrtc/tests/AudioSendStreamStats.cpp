#include "prx/libwebrtc/include/AudioSendStreamStats.hpp"
#include "prx/libwebrtc/libwebrtc.hpp"

namespace {

void TestDestructor() {
    _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(nullptr);
}

}

int main() {
    TestDestructor();
    return 0;
}
