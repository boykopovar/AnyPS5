#include "prx/libwebrtc/include/Api.hpp"
#include <charconv>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_EXPORT("kSchJ+SgltQ", libwebrtcUnknown00);
int APS5_VABI libwebrtcUnknown00(void) {
    NotImplemented_nid_no_patch("kSchJ+SgltQ");
    return 0;
}

APS5_EXPORT("Bq9Waszdevs", libwebrtcUnknown01);
int APS5_VABI libwebrtcUnknown01(void) {
    NotImplemented_nid_no_patch("Bq9Waszdevs");
    return 0;
}

int APS5_VABI _ZN6webrtc11EnableMediaERNS_33PeerConnectionFactoryDependenciesE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("tb7+sc1+V88", libwebrtcUnknown02);
int APS5_VABI libwebrtcUnknown02(void) {
    NotImplemented_nid_no_patch("tb7+sc1+V88");
    return 0;
}

int APS5_VABI _ZN6webrtc10CallConfigD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("DdAZMMuUzBg", libwebrtcUnknown03);
int APS5_VABI libwebrtcUnknown03(void) {
    NotImplemented_nid_no_patch("DdAZMMuUzBg");
    return 0;
}

char* APS5_VABI _ZN4absl16numbers_internal15FastIntToBufferEiPc_nid_postfix(int value, char* buffer) {
    if (!buffer)
        throw std::invalid_argument("FastIntToBuffer requires a buffer");
    std::uint32_t magnitude = static_cast<std::uint32_t>(value);
    if (value < 0) {
        *buffer++ = '-';
        magnitude = 0u - magnitude;
    }
    if (magnitude >= 10u && magnitude < 100000000u)
        std::memset(buffer, 0, 8);
    buffer = std::to_chars(buffer, buffer + 10, magnitude).ptr;
    *buffer = '\0';
    return buffer;
}

int APS5_VABI _ZN3rtc18webrtc_checks_impl8FatalLogEPKciS2_PKNS0_12CheckArgTypeEz_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc18callback_list_impl21CallbackListReceiversC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc19webrtc_logging_impl3LogEPKNS0_10LogArgTypeEz_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessage6IsNoopENS_15LoggingSeverityE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc13SocketAddressC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc13SocketAddressaSERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc13SocketAddress6ipaddrEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc13SocketAddressC1ERKNS_9IPAddressEi_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc13SocketAddress4portEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc9IPAddress12ipv4_addressEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc9IPAddress12ipv6_addressEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc8RmsLevel7AnalyzeEN3rtc9ArrayViewIKsLln4711EEE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc8RmsLevel7AverageEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc8RmsLevelC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc8RmsLevelD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc34CreateModularPeerConnectionFactoryENS_33PeerConnectionFactoryDependenciesE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc18EnvironmentFactory6CreateEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12RtpExtensionD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc11FileWrapper5CloseEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN7cricket18BasicPortAllocatorC1EPN3rtc14NetworkManagerEPNS1_19PacketSocketFactoryEPN6webrtc14TurnCustomizerEPNS_25RelayPortFactoryInterfaceEPKNS6_15FieldTrialsViewE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc23PeerConnectionInterface16RTCConfigurationC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc23PeerConnectionInterface16RTCConfigurationD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13CryptoOptionsC1ERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc14RTCCertificateD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc20AudioSourceInterface7optionsEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc9IPAddresseqERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc8RTCError7messageEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc21PendingTaskSafetyFlag6CreateEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN7cricket5CodecD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc21PendingTaskSafetyFlag11SetNotAliveEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc18RtpTransceiverInitC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("RVMa2ePsZR8", libwebrtcUnknown04);
int APS5_VABI libwebrtcUnknown04(void) {
    NotImplemented_nid_no_patch("RVMa2ePsZR8");
    return 0;
}

int APS5_VABI _ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("Sr1wSrhjO3E", libwebrtcUnknown05);
int APS5_VABI libwebrtcUnknown05(void) {
    NotImplemented_nid_no_patch("Sr1wSrhjO3E");
    return 0;
}

int APS5_VABI _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17RtpPacketReceivedC1ERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc17CopyOnWriteBuffer24UnshareAndEnsureCapacityEm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc17CopyOnWriteBufferC1Emm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13TaskQueueBase7CurrentEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10PacketInfoD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread7CurrentEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10TimeMillisEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("qFYB-adjVIM", libwebrtcUnknown06);
int APS5_VABI libwebrtcUnknown06(void) {
    NotImplemented_nid_no_patch("qFYB-adjVIM");
    return 0;
}

int APS5_VABI _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10TimeMicrosEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("AEjxDisAkPE", libwebrtcUnknown07);
int APS5_VABI libwebrtcUnknown07(void) {
    NotImplemented_nid_no_patch("AEjxDisAkPE");
    return 0;
}

int APS5_VABI _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("ftP-2ZTa94E", libwebrtcUnknown08);
int APS5_VABI libwebrtcUnknown08(void) {
    NotImplemented_nid_no_patch("ftP-2ZTa94E");
    return 0;
}

int APS5_VABI _ZNK3rtc13SocketAddress5IsNilEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("uWpaQ5UuMwQ", libwebrtcUnknown09);
int APS5_VABI libwebrtcUnknown09(void) {
    NotImplemented_nid_no_patch("uWpaQ5UuMwQ");
    return 0;
}

APS5_EXPORT("BRiFWS8-Khk", libwebrtcUnknown10);
int APS5_VABI libwebrtcUnknown10(void) {
    NotImplemented_nid_no_patch("BRiFWS8-Khk");
    return 0;
}

APS5_EXPORT("s+XzpKPY7JA", libwebrtcUnknown11);
int APS5_VABI libwebrtcUnknown11(void) {
    NotImplemented_nid_no_patch("s+XzpKPY7JA");
    return 0;
}

int APS5_VABI _ZN3rtc9KeyParamsC1ENS_7KeyTypeE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc33PeerConnectionFactoryDependenciesC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("IEo8FXYcE8c", libwebrtcUnknown12);
int APS5_VABI libwebrtcUnknown12(void) {
    NotImplemented_nid_no_patch("IEo8FXYcE8c");
    return 0;
}

APS5_EXPORT("U373Sdo7lsw", libwebrtcUnknown13);
int APS5_VABI libwebrtcUnknown13(void) {
    NotImplemented_nid_no_patch("U373Sdo7lsw");
    return 0;
}

int APS5_VABI _ZN6webrtc32webrtc_sequence_checker_internal19SequenceCheckerImplC2Eb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc15CurrentThreadIdEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterC1EPS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc18EncodedImageBuffer6CreateEPKhm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10ColorSpaceC1ERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10ColorSpaceaSERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12EncodedImageC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12EncodedImageD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc19I420BufferInterface7GetI420Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc16VideoFrameBuffer12CropAndScaleEiiiiii_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("R+hhR9jq4sU", libwebrtcUnknown14);
int APS5_VABI libwebrtcUnknown14(void) {
    NotImplemented_nid_no_patch("R+hhR9jq4sU");
    return 0;
}

int APS5_VABI _ZNK6webrtc19I420BufferInterface11ChromaWidthEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc19I420BufferInterface12ChromaHeightEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc16VideoFrameBuffer7GetI420Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10VideoFrame7Builder17set_timestamp_rtpEj_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10VideoFrame7Builder15set_ntp_time_msEl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc10VideoFrame5widthEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc10VideoFrame6heightEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc10VideoFrame18video_frame_bufferEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc10VideoFrame14render_time_msEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10VideoFrameD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc14VideoSinkWantsC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc14VideoSinkWantsC1ERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc14VideoSinkWantsD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessage6streamEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessage17GetMinLogSeverityEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessage13GetLogToDebugEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessage10LogThreadsEb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessage13LogTimestampsEb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessage10LogToDebugENS_15LoggingSeverityE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessage14SetLogToStderrEb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessage14AddLogToStreamEPNS_7LogSinkENS_15LoggingSeverityE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessage17RemoveLogToStreamEPNS_7LogSinkE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc7LogSink12OnLogMessageERKNS_10LogLineRefE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("pMv17ToRoH8", libwebrtcUnknown15);
int APS5_VABI libwebrtcUnknown15(void) {
    NotImplemented_nid_no_patch("pMv17ToRoH8");
    return 0;
}

APS5_EXPORT("Z9NAPHRBMEI", libwebrtcUnknown16);
int APS5_VABI libwebrtcUnknown16(void) {
    NotImplemented_nid_no_patch("Z9NAPHRBMEI");
    return 0;
}

APS5_EXPORT("SsXGM-KrjHo", libwebrtcUnknown17);
int APS5_VABI libwebrtcUnknown17(void) {
    NotImplemented_nid_no_patch("SsXGM-KrjHo");
    return 0;
}

APS5_EXPORT("DqwPN50HkI4", libwebrtcUnknown18);
int APS5_VABI libwebrtcUnknown18(void) {
    NotImplemented_nid_no_patch("DqwPN50HkI4");
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessageC1EPKciNS_15LoggingSeverityE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10LogMessageD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc13SocketAddress10IsCompleteEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc9IPAddress27v4AddressAsHostOrderIntegerEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("z-b1v5cik7s", libwebrtcUnknown19);
int APS5_VABI libwebrtcUnknown19(void) {
    NotImplemented_nid_no_patch("z-b1v5cik7s");
    return 0;
}

int APS5_VABI _ZNK3rtc13SocketAddress7IsAnyIPEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10TruncateIPERKNS_9IPAddressEi_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc15CountIPMaskBitsERKNS_9IPAddressE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc17AsyncPacketSocketD2Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc17AsyncPacketSocket20NotifyPacketReceivedERKNS_14ReceivedPacketE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("t2C2h5xmnms", libwebrtcUnknown20);
int APS5_VABI libwebrtcUnknown20(void) {
    NotImplemented_nid_no_patch("t2C2h5xmnms");
    return 0;
}

APS5_EXPORT("GGzOuMbXsGw", libwebrtcUnknown21);
int APS5_VABI libwebrtcUnknown21(void) {
    NotImplemented_nid_no_patch("GGzOuMbXsGw");
    return 0;
}

int APS5_VABI _ZN3rtc10SentPacketC1Ell_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc10SentPacketC1EllRKNS_10PacketInfoE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc17CopyOnWriteBufferC1Em_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17CodecSpecificInfoD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17CodecSpecificInfoC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("0Uh0ycY9nog", libwebrtcUnknown22);
int APS5_VABI libwebrtcUnknown22(void) {
    NotImplemented_nid_no_patch("0Uh0ycY9nog");
    return 0;
}

int APS5_VABI _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("EhpYIe91V44", libwebrtcUnknown23);
int APS5_VABI libwebrtcUnknown23(void) {
    NotImplemented_nid_no_patch("EhpYIe91V44");
    return 0;
}

int APS5_VABI _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc12VideoDecoder14GetDecoderInfoEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc19VideoDecoderFactory17QueryCodecSupportERKNS_14SdpVideoFormatEb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12VideoEncoder24SetFecControllerOverrideEPNS_21FecControllerOverrideE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12VideoEncoder10InitEncodeEPKNS_10VideoCodecEim_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12VideoEncoder11EncoderInfoC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12VideoEncoder21RateControlParametersD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("P+3Zbh6lqCo", libwebrtcUnknown24);
int APS5_VABI libwebrtcUnknown24(void) {
    NotImplemented_nid_no_patch("P+3Zbh6lqCo");
    return 0;
}

int APS5_VABI _ZN3rtc11RaceCheckerC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("oPJkGGMqNaw", libwebrtcUnknown25);
int APS5_VABI libwebrtcUnknown25(void) {
    NotImplemented_nid_no_patch("oPJkGGMqNaw");
    return 0;
}

int APS5_VABI _ZN3rtc14PlatformThreadaSEOS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc14PlatformThreadD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("SGDTMkzQm78", libwebrtcUnknown26);
int APS5_VABI libwebrtcUnknown26(void) {
    NotImplemented_nid_no_patch("SGDTMkzQm78");
    return 0;
}

int APS5_VABI _ZN6webrtc11field_trial25InitFieldTrialsFromStringEPKc_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("pH5oyjlfrL0", libwebrtcUnknown27);
int APS5_VABI libwebrtcUnknown27(void) {
    NotImplemented_nid_no_patch("pH5oyjlfrL0");
    return 0;
}

int APS5_VABI _ZN3rtc10SSLAdapter6CreateEPNS_6SocketE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("QI9yvWEMEbs", libwebrtcUnknown28);
int APS5_VABI libwebrtcUnknown28(void) {
    NotImplemented_nid_no_patch("QI9yvWEMEbs");
    return 0;
}

int APS5_VABI _ZN6webrtc16AsyncDnsResolverC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc9CritScopeC1EPKNS_24RecursiveCriticalSectionE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc9CritScopeD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc24RecursiveCriticalSectionC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc24RecursiveCriticalSectionD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc13ThreadManager8InstanceEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread15ProcessMessagesEi_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc13ThreadManager13CurrentThreadEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("uBpjHIuzumQ", libwebrtcUnknown29);
int APS5_VABI libwebrtcUnknown29(void) {
    NotImplemented_nid_no_patch("uBpjHIuzumQ");
    return 0;
}

APS5_EXPORT("TmP5kmKawgA", libwebrtcUnknown30);
int APS5_VABI libwebrtcUnknown30(void) {
    NotImplemented_nid_no_patch("TmP5kmKawgA");
    return 0;
}

APS5_EXPORT("ogpyBO3hYSY", libwebrtcUnknown31);
int APS5_VABI libwebrtcUnknown31(void) {
    NotImplemented_nid_no_patch("ogpyBO3hYSY");
    return 0;
}

int APS5_VABI _ZN3rtc6Thread6DoInitEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6ThreadD2Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread9DoDestroyEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread12socketserverEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread4QuitEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread10IsQuittingEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread7RestartEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread12PostTaskImplEN4absl12AnyInvocableIFvvOEEERKN6webrtc13TaskQueueBase14PostTaskTraitsERKNS5_8LocationE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread19PostDelayedTaskImplEN4absl12AnyInvocableIFvvOEEEN6webrtc9TimeDeltaERKNS5_13TaskQueueBase21PostDelayedTaskTraitsERKNS5_8LocationE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread8GetDelayEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc6Thread9IsCurrentEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("Y18cti5zQig", libwebrtcUnknown32);
int APS5_VABI libwebrtcUnknown32(void) {
    NotImplemented_nid_no_patch("Y18cti5zQig");
    return 0;
}

int APS5_VABI _ZN3rtc16NullSocketServerC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread5StartEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("IBOheohLw3g", libwebrtcUnknown33);
int APS5_VABI libwebrtcUnknown33(void) {
    NotImplemented_nid_no_patch("IBOheohLw3g");
    return 0;
}

int APS5_VABI _ZN3rtc6Thread3RunEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread4StopEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread16BlockingCallImplENS_12FunctionViewIFvvEEERKN6webrtc8LocationE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread6DeleteEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6Thread30IsProcessingMessagesForTestingEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc6ThreadC1EPNS_12SocketServerE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc24RecursiveCriticalSection5EnterEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc24RecursiveCriticalSection5LeaveEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp3App5ParseERKNS0_12CommonHeaderE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp3AppC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp3AppD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("PEV9t77ljzc", libwebrtcUnknown34);
int APS5_VABI libwebrtcUnknown34(void) {
    NotImplemented_nid_no_patch("PEV9t77ljzc");
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp4Nack5ParseERKNS0_12CommonHeaderE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp4NackC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp4NackD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp3PliD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp4Remb5ParseERKNS0_12CommonHeaderE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp17TransportFeedback5ParseERKNS0_12CommonHeaderE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp17TransportFeedbackC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc4rtcp17TransportFeedbackD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc9RtpPacket5ParseEPKhm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioDecoder15DecodeRedundantEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc12AudioDecoder12HasDecodePlcEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioDecoder9DecodePlcEmPs_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioDecoder11GeneratePlcEmPN3rtc7BufferTIsLb0EEE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioDecoder9ErrorCodeEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioDecoder11ParseResultC1EOS1__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("vzyzE1cN1b8", libwebrtcUnknown35);
int APS5_VABI libwebrtcUnknown35(void) {
    NotImplemented_nid_no_patch("vzyzE1cN1b8");
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoaSERKS1__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder6EncodeEjN3rtc9ArrayViewIKsLln4711EEEPNS1_7BufferTIhLb0EEE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc12AudioEncoder6GetDtxEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder16SetTargetBitrateEi_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("qli1Cjz9UIo", libwebrtcUnknown36);
int APS5_VABI libwebrtcUnknown36(void) {
    NotImplemented_nid_no_patch("qli1Cjz9UIo");
    return 0;
}

APS5_EXPORT("Vf6lJz4-bEU", libwebrtcUnknown37);
int APS5_VABI libwebrtcUnknown37(void) {
    NotImplemented_nid_no_patch("Vf6lJz4-bEU");
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder26DisableAudioNetworkAdaptorEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder45OnReceivedUplinkRecoverablePacketLossFractionEf_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder28OnReceivedTargetAudioBitrateEi_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder26OnReceivedUplinkAllocationENS_23BitrateAllocationUpdateE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder13OnReceivedRttEi_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder18OnReceivedOverheadEm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder27SetReceiverFrameLengthRangeEii_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc12AudioEncoder11GetANAStatsEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc8ANAStatsC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoC1ERKS1__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc14SdpAudioFormataSERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc14SdpAudioFormatC1EOS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("qGXgWFIHHgk", libwebrtcUnknown38);
int APS5_VABI libwebrtcUnknown38(void) {
    NotImplemented_nid_no_patch("qGXgWFIHHgk");
    return 0;
}

APS5_EXPORT("VfqMb0eRCr0", libwebrtcUnknown39);
int APS5_VABI libwebrtcUnknown39(void) {
    NotImplemented_nid_no_patch("VfqMb0eRCr0");
    return 0;
}

APS5_EXPORT("OnkJ4kSZYkk", libwebrtcUnknown40);
int APS5_VABI libwebrtcUnknown40(void) {
    NotImplemented_nid_no_patch("OnkJ4kSZYkk");
    return 0;
}

int APS5_VABI _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc14AudioCodecInfoC1Eimiii_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13PacketOptionsC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13PacketOptionsC1ERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13PacketOptionsD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc27AudioReceiveStreamInterface5StatsD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc27AudioReceiveStreamInterface6ConfigC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10AudioState6ConfigD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc15AudioSendStream6ConfigC1EPNS_9TransportE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc15AudioSendStream6Config13SendCodecSpecD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10AudioFrame11UpdateFrameEjPKsmiNS0_10SpeechTypeENS0_11VADActivityEm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc10AudioFrame5mutedEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc10AudioFrame9data_viewEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc10AudioFrame4dataEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10AudioFrame12mutable_dataEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10AudioFrame4MuteEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10AudioFrameC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc20EchoCanceller3ConfigaSERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc20EchoCanceller3ConfigC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc20EchoCanceller3ConfigC1ERKS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("3hzqH2yWvjU", libwebrtcUnknown41);
int APS5_VABI libwebrtcUnknown41(void) {
    NotImplemented_nid_no_patch("3hzqH2yWvjU");
    return 0;
}

int APS5_VABI _ZN6webrtc11FileWrapper5FlushEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc11FileWrapper5WriteEPKvm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc11FileWrapperC1EOS0__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceModule6CreateENS0_10AudioLayerEPNS_16TaskQueueFactoryE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceBufferC1EPNS_16TaskQueueFactoryEb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceBuffer20SetRecordingChannelsEm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceBuffer18SetPlayoutChannelsEm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceBuffer12StartPlayoutEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceBuffer11StopPlayoutEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceBuffer14StartRecordingEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceBuffer13StopRecordingEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceBuffer21RegisterAudioCallbackEPNS_14AudioTransportE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceBuffer20SetPlayoutSampleRateEj_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17AudioDeviceBuffer22SetRecordingSampleRateEj_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI WebRtcVad_Create_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI WebRtcVad_Free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI WebRtcVad_Init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI WebRtcVad_set_mode_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI WebRtcVad_Process_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc18AudioDeviceGeneric21BuiltInAECIsAvailableEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc18AudioDeviceGeneric21BuiltInAGCIsAvailableEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc18AudioDeviceGeneric20BuiltInNSIsAvailableEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc18AudioDeviceGeneric16EnableBuiltInAECEb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc18AudioDeviceGeneric16EnableBuiltInAGCEb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc18AudioDeviceGeneric15EnableBuiltInNSEb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc18AudioDeviceGeneric23GetPlayoutUnderrunCountEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13PushResamplerIsEC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13PushResamplerIsEC1Emmm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13PushResamplerIsED1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13PushResamplerIsE8ResampleENS_15InterleavedViewIKsEENS2_IsEE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13PushResamplerIsE8ResampleEN3rtc9ArrayViewIKsLln4711EEENS3_IsLln4711EEE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc19SmoothingFilterImplD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc9WavReader5ResetEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc9WavReader5CloseEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("QbtLnp-i47c", libwebrtcUnknown42);
int APS5_VABI libwebrtcUnknown42(void) {
    NotImplemented_nid_no_patch("QbtLnp-i47c");
    return 0;
}

int APS5_VABI _ZN6webrtc11AudioBufferC1Emmmmmm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc11AudioBuffer8CopyFromEPKsRKNS_12StreamConfigE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc11AudioBuffer6CopyToERKNS_12StreamConfigEPs_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc23GetAvailableCpuFeaturesEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc7rnn_vad17FeaturesExtractorC1ERKNS_20AvailableCpuFeaturesE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc7rnn_vad6RnnVadC1ERKNS_20AvailableCpuFeaturesE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc7rnn_vad6RnnVadD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc7rnn_vad17FeaturesExtractorD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("lQyI5UWffS8", libwebrtcUnknown43);
int APS5_VABI libwebrtcUnknown43(void) {
    NotImplemented_nid_no_patch("lQyI5UWffS8");
    return 0;
}

int APS5_VABI _ZN6webrtc7rnn_vad6RnnVad21ComputeVadProbabilityEN3rtc9ArrayViewIKfLl42EEEb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc20AudioFrameOperations4MuteEPNS_10AudioFrameE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("GMnT0UEJDZY", libwebrtcUnknown44);
int APS5_VABI libwebrtcUnknown44(void) {
    NotImplemented_nid_no_patch("GMnT0UEJDZY");
    return 0;
}

APS5_EXPORT("sGIEeW5HLOg", libwebrtcUnknown45);
int APS5_VABI libwebrtcUnknown45(void) {
    NotImplemented_nid_no_patch("sGIEeW5HLOg");
    return 0;
}

int APS5_VABI _ZN3rtc11RateTrackerC1Elm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("OGFO5SAL9Sg", libwebrtcUnknown46);
int APS5_VABI libwebrtcUnknown46(void) {
    NotImplemented_nid_no_patch("OGFO5SAL9Sg");
    return 0;
}

int APS5_VABI _ZN3rtc11RateTrackerD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc11RateTracker10AddSamplesEl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc11RateTracker22ComputeRateForIntervalEl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("0FWPqLUBGmw", libwebrtcUnknown47);
int APS5_VABI libwebrtcUnknown47(void) {
    NotImplemented_nid_no_patch("0FWPqLUBGmw");
    return 0;
}

int APS5_VABI _ZNK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZThn8_NK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("01K7H6PnH5g", libwebrtcUnknown48);
int APS5_VABI libwebrtcUnknown48(void) {
    NotImplemented_nid_no_patch("01K7H6PnH5g");
    return 0;
}

int APS5_VABI _ZNK3rtc18NetworkManagerBase22enumeration_permissionEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("CwWvzQchcdk", libwebrtcUnknown49);
int APS5_VABI libwebrtcUnknown49(void) {
    NotImplemented_nid_no_patch("CwWvzQchcdk");
    return 0;
}

APS5_EXPORT("GUP93yoyxE0", libwebrtcUnknown50);
int APS5_VABI libwebrtcUnknown50(void) {
    NotImplemented_nid_no_patch("GUP93yoyxE0");
    return 0;
}

APS5_EXPORT("57abazv-Yts", libwebrtcUnknown51);
int APS5_VABI libwebrtcUnknown51(void) {
    NotImplemented_nid_no_patch("57abazv-Yts");
    return 0;
}

int APS5_VABI _ZN3rtc18NetworkManagerBase27set_default_local_addressesERKNS_9IPAddressES3__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3rtc18NetworkManagerBase21GetNetworkFromAddressERKNS_9IPAddressE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("QPwI2zGqqxI", libwebrtcUnknown52);
int APS5_VABI libwebrtcUnknown52(void) {
    NotImplemented_nid_no_patch("QPwI2zGqqxI");
    return 0;
}

int APS5_VABI _ZN3rtc7NetworkD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN7cricket18AsyncStunTCPSocketC1EPN3rtc6SocketE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3rtc14AsyncTCPSocketC1EPNS_6SocketE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("X8ypYWKrUXE", libwebrtcUnknown53);
int APS5_VABI libwebrtcUnknown53(void) {
    NotImplemented_nid_no_patch("X8ypYWKrUXE");
    return 0;
}

int APS5_VABI _ZN3rtc14AsyncSSLSocketC1EPNS_6SocketE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("EoYFZeImOJo", libwebrtcUnknown54);
int APS5_VABI libwebrtcUnknown54(void) {
    NotImplemented_nid_no_patch("EoYFZeImOJo");
    return 0;
}

int APS5_VABI _ZN6webrtc22AudioReceiveStreamImpl22SetMinimumPlayoutDelayEi_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc10AudioState6CreateERKNS0_6ConfigE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc3voe16RemixAndResampleENS_15InterleavedViewIKsEEiPNS_13PushResamplerIsEEPNS_10AudioFrameE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc21EchoCanceller3FactoryC1ERKNS_20EchoCanceller3ConfigE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc25AudioEncoderRuntimeConfigD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc14AudioMixerImpl6CreateEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13FrameCombinerC1Eb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13FrameCombinerD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc13FrameCombiner7CombineEN3rtc9ArrayViewIKPNS_10AudioFrameELln4711EEEmimS4__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc29BuiltinAudioProcessingBuilder5BuildERKNS_11EnvironmentE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc32CreateBuiltinAudioDecoderFactoryEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("7OP9jmeBYaM", libwebrtcUnknown55);
int APS5_VABI libwebrtcUnknown55(void) {
    NotImplemented_nid_no_patch("7OP9jmeBYaM");
    return 0;
}

APS5_EXPORT("ratmuLethas", libwebrtcUnknown56);
int APS5_VABI libwebrtcUnknown56(void) {
    NotImplemented_nid_no_patch("ratmuLethas");
    return 0;
}

int APS5_VABI _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc20AudioDecoderOpusImplD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc23AudioNetworkAdaptorImpl6ConfigC1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("Jq-CaqgT024", libwebrtcUnknown57);
int APS5_VABI libwebrtcUnknown57(void) {
    NotImplemented_nid_no_patch("Jq-CaqgT024");
    return 0;
}

APS5_EXPORT("HLZgnt-VWOg", libwebrtcUnknown58);
int APS5_VABI libwebrtcUnknown58(void) {
    NotImplemented_nid_no_patch("HLZgnt-VWOg");
    return 0;
}

int APS5_VABI _ZN6webrtc23AudioNetworkAdaptorImpl6ConfigD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN6webrtc32CreateBuiltinAudioEncoderFactoryEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("e9ZbbX3j1iM", libwebrtcUnknown59);
int APS5_VABI libwebrtcUnknown59(void) {
    NotImplemented_nid_no_patch("e9ZbbX3j1iM");
    return 0;
}

int APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
