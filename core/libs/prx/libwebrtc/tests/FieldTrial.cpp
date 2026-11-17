#include "prx/libwebrtc/include/FieldTrial.hpp"
#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void TestFieldTrialStringInit() {
    Require(webrtc::field_trial::GetFieldTrialString() == nullptr);
    const char trials[] = "WebRTC-Test/Enabled/";
    _ZN6webrtc11field_trial25InitFieldTrialsFromStringEPKc_nid_postfix(trials);
    Require(webrtc::field_trial::GetFieldTrialString() == trials);
    Require(std::strcmp(webrtc::field_trial::GetFieldTrialString(), "WebRTC-Test/Enabled/") == 0);

    _ZN6webrtc11field_trial25InitFieldTrialsFromStringEPKc_nid_postfix(nullptr);
    Require(webrtc::field_trial::GetFieldTrialString() == nullptr);
}

}

int main() {
    TestFieldTrialStringInit();
    return 0;
}
