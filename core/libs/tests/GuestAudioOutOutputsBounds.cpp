#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

constexpr int portTypeMain = 0;
constexpr int portTypeBgm = 1;
constexpr int portTypeVoice = 2;
constexpr int portTypePersonal = 3;
constexpr int portTypePadspk = 4;
constexpr int portTypeAudio3d = 126;
constexpr int portTypeAux = 127;

constexpr std::uint32_t formatS16Mono = 0;
constexpr std::uint32_t formatS16Stereo = 1;
constexpr std::uint32_t formatS16_8Ch = 2;
constexpr std::uint32_t formatF32Mono = 3;
constexpr std::uint32_t formatF32Stereo = 4;
constexpr std::uint32_t formatF32_8Ch = 5;
constexpr std::uint32_t formatS16_8ChStd = 6;
constexpr std::uint32_t formatF32_8ChStd = 7;

constexpr int errorInvalidParam = -2144993276;
constexpr int errorInvalidHandle = -2144993277;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static void TestParameterValidation() {
    Require(sceAudioOutOutputs(nullptr, 0) == errorInvalidParam);
    Require(sceAudioOutOutputs(nullptr, 1) == errorInvalidParam);
    Require(sceAudioOutOutputs(nullptr, 32) == errorInvalidParam);

    AudioOutOutputParam dummy{1, nullptr};
    Require(sceAudioOutOutputs(&dummy, 0) == errorInvalidParam);
}

static void TestInvalidAndUnopenedHandles() {
    AudioOutOutputParam negHandle{-1, nullptr};
    Require(sceAudioOutOutputs(&negHandle, 1) == errorInvalidHandle);

    AudioOutOutputParam largeNegHandle{-999, nullptr};
    Require(sceAudioOutOutputs(&largeNegHandle, 1) == errorInvalidHandle);

    AudioOutOutputParam zeroHandle{0, nullptr};
    Require(sceAudioOutOutputs(&zeroHandle, 1) == errorInvalidHandle);

    AudioOutOutputParam outOfRange33{33, nullptr};
    Require(sceAudioOutOutputs(&outOfRange33, 1) == errorInvalidHandle);

    AudioOutOutputParam outOfRange100{100, nullptr};
    Require(sceAudioOutOutputs(&outOfRange100, 1) == errorInvalidHandle);

    AudioOutOutputParam outOfRangeMax{0x7FFF, nullptr};
    Require(sceAudioOutOutputs(&outOfRangeMax, 1) == errorInvalidHandle);

    for (int handle : {1, 2, 8, 16, 32}) {
        AudioOutOutputParam unopened{handle, nullptr};
        Require(sceAudioOutOutputs(&unopened, 1) == errorInvalidHandle);
    }
}

static void TestMultiPortAtomicValidation() {
    const int h1 = sceAudioOutOpen(255, portTypeMain, 0, 256, 48000, formatS16Stereo);
    const int h2 = sceAudioOutOpen(255, portTypeBgm, 0, 256, 48000, formatS16Stereo);
    Require(h1 > 0 && h2 > 0);

    std::uint64_t t1 = 999;
    std::uint64_t t2 = 999;
    Require(sceAudioOutGetLastOutputTime(h1, &t1) == 0 && t1 == 0);
    Require(sceAudioOutGetLastOutputTime(h2, &t2) == 0 && t2 == 0);

    std::vector<std::int16_t> sampleBuf(256 * 2, 0);

    AudioOutOutputParam badBatchZero[2] = {{h1, sampleBuf.data()}, {0, sampleBuf.data()}};
    Require(sceAudioOutOutputs(badBatchZero, 2) == errorInvalidHandle);
    Require(sceAudioOutGetLastOutputTime(h1, &t1) == 0 && t1 == 0);

    AudioOutOutputParam badBatchNegative[2] = {{-5, sampleBuf.data()}, {h2, sampleBuf.data()}};
    Require(sceAudioOutOutputs(badBatchNegative, 2) == errorInvalidHandle);
    Require(sceAudioOutGetLastOutputTime(h2, &t2) == 0 && t2 == 0);

    AudioOutOutputParam badBatchRange[2] = {{h1, sampleBuf.data()}, {33, sampleBuf.data()}};
    Require(sceAudioOutOutputs(badBatchRange, 2) == errorInvalidHandle);
    Require(sceAudioOutGetLastOutputTime(h1, &t1) == 0 && t1 == 0);

    AudioOutOutputParam badBatchUnopened[2] = {{h1, sampleBuf.data()}, {30, sampleBuf.data()}};
    Require(sceAudioOutOutputs(badBatchUnopened, 2) == errorInvalidHandle);
    Require(sceAudioOutGetLastOutputTime(h1, &t1) == 0 && t1 == 0);

    AudioOutOutputParam goodBatch[2] = {{h1, sampleBuf.data()}, {h2, sampleBuf.data()}};
    Require(sceAudioOutOutputs(goodBatch, 2) == 256);
    Require(sceAudioOutGetLastOutputTime(h1, &t1) == 0 && t1 > 0);
    Require(sceAudioOutGetLastOutputTime(h2, &t2) == 0 && t2 > 0);
    Require(t1 == t2);

    Require(sceAudioOutClose(h1) == 0);
    Require(sceAudioOutClose(h2) == 0);
    Require(sceAudioOutOutputs(goodBatch, 2) == errorInvalidHandle);
}

static void TestMultiPortReturnValues() {
    const int shortPort = sceAudioOutOpen(255, portTypeMain, 0, 256, 48000, formatS16Stereo);
    const int longPort = sceAudioOutOpen(255, portTypePersonal, 0, 512, 48000, formatS16Stereo);
    Require(shortPort > 0 && longPort > 0);

    std::vector<std::int16_t> shortBuf(256 * 2, 0);
    std::vector<std::int16_t> longBuf(512 * 2, 0);

    AudioOutOutputParam shortFirst[2] = {{shortPort, shortBuf.data()}, {longPort, longBuf.data()}};
    Require(sceAudioOutOutputs(shortFirst, 2) == 256);

    AudioOutOutputParam longFirst[2] = {{longPort, longBuf.data()}, {shortPort, shortBuf.data()}};
    Require(sceAudioOutOutputs(longFirst, 2) == 512);

    Require(sceAudioOutClose(shortPort) == 0);
    Require(sceAudioOutClose(longPort) == 0);
}

static void TestDrainMode() {
    const int pA = sceAudioOutOpen(255, portTypeMain, 0, 256, 48000, formatS16Stereo);
    const int pB = sceAudioOutOpen(255, portTypeVoice, 0, 256, 48000, formatS16Mono);
    Require(pA > 0 && pB > 0);

    std::vector<std::int16_t> bufA(256 * 2, 0);
    std::vector<std::int16_t> bufB(256 * 1, 0);

    AudioOutOutputParam batchDrain[2] = {{pA, bufA.data()}, {pB, nullptr}};
    Require(sceAudioOutOutputs(batchDrain, 2) == 256);

    std::uint64_t timeA = 0;
    std::uint64_t timeB = 999;
    Require(sceAudioOutGetLastOutputTime(pA, &timeA) == 0 && timeA > 0);
    Require(sceAudioOutGetLastOutputTime(pB, &timeB) == 0 && timeB == 0);

    AudioOutOutputParam batchDrainRev[2] = {{pA, nullptr}, {pB, bufB.data()}};
    Require(sceAudioOutOutputs(batchDrainRev, 2) == 256);

    std::uint64_t timeA2 = 0;
    std::uint64_t timeB2 = 0;
    Require(sceAudioOutGetLastOutputTime(pA, &timeA2) == 0 && timeA2 == timeA);
    Require(sceAudioOutGetLastOutputTime(pB, &timeB2) == 0 && timeB2 > 0);

    Require(sceAudioOutClose(pA) == 0);
    Require(sceAudioOutClose(pB) == 0);
}

static void TestDuplicateHandlesInBatch() {
    const int pDup = sceAudioOutOpen(255, portTypeMain, 0, 256, 48000, formatS16Stereo);
    Require(pDup > 0);

    std::vector<std::int16_t> dupBuf(256 * 2, 0);
    AudioOutOutputParam dupArray[2] = {{pDup, dupBuf.data()}, {pDup, dupBuf.data()}};
    Require(sceAudioOutOutputs(dupArray, 2) == 256);

    AudioOutOutputParam dupArray3[3] = {{pDup, dupBuf.data()}, {pDup, nullptr}, {pDup, dupBuf.data()}};
    Require(sceAudioOutOutputs(dupArray3, 3) == 256);

    Require(sceAudioOutClose(pDup) == 0);
}

static void TestChannelMappingAcrossPorts() {
    const int p0 = sceAudioOutOpen(255, portTypeMain, 0, 256, 48000, formatS16Mono);
    const int p1 = sceAudioOutOpen(255, portTypeVoice, 0, 256, 48000, formatS16Stereo);
    const int p2 = sceAudioOutOpen(255, portTypeBgm, 0, 256, 48000, formatS16_8Ch);
    const int p3 = sceAudioOutOpen(255, portTypeAux, 0, 256, 48000, formatF32Mono);
    const int p4 = sceAudioOutOpen(255, portTypePersonal, 0, 256, 48000, formatF32Stereo);
    const int p5 = sceAudioOutOpen(255, portTypeMain, 0, 256, 48000, formatF32_8Ch);
    const int p6 = sceAudioOutOpen(255, portTypeAudio3d, 0, 256, 48000, formatS16_8ChStd);
    const int p7 = sceAudioOutOpen(255, portTypePadspk, 0, 256, 48000, formatF32_8ChStd);

    Require(p0 > 0 && p1 > 0 && p2 > 0 && p3 > 0);
    Require(p4 > 0 && p5 > 0 && p6 > 0 && p7 > 0);

    AudioOutPortState s0{};
    AudioOutPortState s1{};
    AudioOutPortState s2{};
    AudioOutPortState s3{};
    AudioOutPortState s4{};
    AudioOutPortState s5{};
    AudioOutPortState s6{};
    AudioOutPortState s7{};

    Require(sceAudioOutGetPortState(p0, &s0) == 0 && s0.channel == 1);
    Require(sceAudioOutGetPortState(p1, &s1) == 0 && s1.channel == 1);
    Require(sceAudioOutGetPortState(p2, &s2) == 0 && s2.channel <= 2);
    Require(sceAudioOutGetPortState(p3, &s3) == 0 && s3.channel == 0);
    Require(sceAudioOutGetPortState(p4, &s4) == 0 && s4.channel == 1);
    Require(sceAudioOutGetPortState(p5, &s5) == 0 && s5.channel <= 2);
    Require(sceAudioOutGetPortState(p6, &s6) == 0 && s6.channel <= 2);
    Require(sceAudioOutGetPortState(p7, &s7) == 0 && s7.channel == 1);

    std::vector<std::int16_t> b0(256 * 1, 0);
    std::vector<std::int16_t> b1(256 * 2, 0);
    std::vector<std::int16_t> b2(256 * 8, 0);
    std::vector<float> b3(256 * 1, 0.0f);
    std::vector<float> b4(256 * 2, 0.0f);
    std::vector<float> b5(256 * 8, 0.0f);
    std::vector<std::int16_t> b6(256 * 8, 0);
    std::vector<float> b7(256 * 8, 0.0f);

    AudioOutOutputParam multi[8] = {
        {p0, b0.data()},
        {p1, b1.data()},
        {p2, b2.data()},
        {p3, b3.data()},
        {p4, b4.data()},
        {p5, b5.data()},
        {p6, b6.data()},
        {p7, b7.data()}
    };

    Require(sceAudioOutOutputs(multi, 8) == 256);

    for (int handle : {p0, p1, p2, p3, p4, p5, p6, p7}) {
        std::uint64_t outTime = 0;
        Require(sceAudioOutGetLastOutputTime(handle, &outTime) == 0 && outTime > 0);
        Require(sceAudioOutClose(handle) == 0);
    }
}

static void TestBufferSizeAlignmentAndOverflow() {
    const int ovf = sceAudioOutOpen(255, portTypeMain, 0, 0x40000000u, 48000, formatS16Stereo);
    Require(ovf > 0);

    std::int16_t dummySample = 0;
    AudioOutOutputParam ovfParam{ovf, &dummySample};
    bool threw = false;
    try {
        static_cast<void>(sceAudioOutOutputs(&ovfParam, 1));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    AudioOutOutputParam ovfDrain{ovf, nullptr};
    Require(sceAudioOutOutputs(&ovfDrain, 1) == static_cast<int>(0x40000000u));

    Require(sceAudioOutClose(ovf) == 0);
}

static void TestPostCloseAndDoubleClose() {
    const int p = sceAudioOutOpen(255, portTypeMain, 0, 256, 48000, formatS16Stereo);
    Require(p > 0);
    Require(sceAudioOutClose(p) == 0);

    AudioOutOutputParam param{p, nullptr};
    Require(sceAudioOutOutputs(&param, 1) == errorInvalidHandle);
    Require(sceAudioOutClose(p) == errorInvalidHandle);
}

} // namespace

int main() {
    Require(sceAudioOutInit() == 0);

    TestParameterValidation();
    TestInvalidAndUnopenedHandles();
    TestMultiPortAtomicValidation();
    TestMultiPortReturnValues();
    TestDrainMode();
    TestDuplicateHandlesInBatch();
    TestChannelMappingAcrossPorts();
    TestBufferSizeAlignmentAndOverflow();
    TestPostCloseAndDoubleClose();

    LibcRunShutdown_nid_postfix();
    return 0;
}
