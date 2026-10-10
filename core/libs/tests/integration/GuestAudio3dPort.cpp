#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
void APS5_VABI sceAudio3dGetDefaultOpenParameters(Audio3dOpenParameters* parameters);
int APS5_VABI sceAudio3dInitialize(std::int64_t reserved);
int APS5_VABI sceAudio3dTerminate();
int APS5_VABI sceAudio3dPortOpen(int user_id, const Audio3dOpenParameters* parameters, std::uint32_t* id);
int APS5_VABI sceAudio3dPortClose(std::uint32_t port_id);
int APS5_VABI sceAudio3dPortSetAttribute(std::uint32_t port_id, std::uint32_t attribute_id, const void* attribute, std::size_t attribute_size);
int APS5_VABI sceAudio3dPortGetQueueLevel(std::uint32_t port_id, std::uint32_t* queue_level, std::uint32_t* queue_available);
int APS5_VABI sceAudio3dPortAdvance(std::uint32_t port_id);
int APS5_VABI sceAudio3dPortPush(std::uint32_t port_id, std::uint32_t blocking);
int APS5_VABI sceAudio3dPortFlush(std::uint32_t port_id);
int APS5_VABI sceAudio3dObjectReserve(std::uint32_t port_id, std::uint32_t* object_id);
int APS5_VABI sceAudio3dObjectUnreserve(std::uint32_t port_id, std::uint32_t object_id);
int APS5_VABI sceAudio3dObjectSetAttributes(std::uint32_t port_id, std::uint32_t object_id, std::uint64_t num_attributes, const Audio3dAttribute* attribute_array);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int invalidPort = static_cast<int>(0x80EA0002);
constexpr int invalidObject = static_cast<int>(0x80EA0003);
constexpr int invalidParameter = static_cast<int>(0x80EA0004);
constexpr int outOfResources = static_cast<int>(0x80EA0006);
constexpr int notReady = static_cast<int>(0x80EA0007);
constexpr int systemUser = 0xFF;
constexpr std::uint32_t objectInvalid = 0xFFFFFFFF;
constexpr std::uint32_t lateReverbLevel = 0x10001;
constexpr std::uint32_t queueGranularity = 0x1800;
constexpr auto queueFrame = std::chrono::microseconds(1000000ull * queueGranularity / 48000);

class Audio3dCleanup {
public:
    Audio3dCleanup() = default;

    ~Audio3dCleanup() {
        sceAudio3dPortClose(0);
        sceAudio3dTerminate();
    }

    Audio3dCleanup(const Audio3dCleanup&) = delete;
    Audio3dCleanup& operator=(const Audio3dCleanup&) = delete;
};

class Audio3dLibrary : public Audio3dCleanup {
public:
    Audio3dLibrary() {
        RequireEqual(sceAudio3dInitialize(0), 0, "initialize the library");
    }
};

Audio3dOpenParameters Defaults() {
    Audio3dOpenParameters parameters{};
    sceAudio3dGetDefaultOpenParameters(&parameters);
    return parameters;
}

Audio3dOpenParameters TwoBedParameters() {
    Audio3dOpenParameters parameters = Defaults();
    parameters.size_this = 0x28;
    parameters.num_beds = 2;
    return parameters;
}

Audio3dOpenParameters TwoObjectParameters() {
    Audio3dOpenParameters parameters = Defaults();
    parameters.max_objects = 2;
    return parameters;
}

Audio3dOpenParameters QueueParameters() {
    Audio3dOpenParameters parameters = Defaults();
    parameters.granularity = queueGranularity;
    return parameters;
}

int Open(const Audio3dOpenParameters& parameters, std::uint32_t* id) {
    return sceAudio3dPortOpen(systemUser, &parameters, id);
}

void OpenPort(const Audio3dOpenParameters& parameters) {
    std::uint32_t id = 7;
    RequireEqual(Open(parameters, &id), 0, "open the port");
    RequireEqual(id, 0u, "opened port id");
}

void RequireLevel(std::uint32_t level, std::uint32_t available, const std::string& message) {
    std::uint32_t gotLevel = 99;
    std::uint32_t gotAvailable = 99;
    RequireEqual(sceAudio3dPortGetQueueLevel(0, &gotLevel, &gotAvailable), 0, "get the queue level, " + message);
    RequireEqual(gotLevel, level, "queue level, " + message);
    RequireEqual(gotAvailable, available, "queue available, " + message);
}

void RequireReserved(std::uint32_t expected, const std::string& message) {
    std::uint32_t object = 7;
    RequireEqual(sceAudio3dObjectReserve(0, &object), 0, "reserve, " + message);
    RequireEqual(object, expected, "reserved object id, " + message);
}

void FillQueue() {
    RequireEqual(sceAudio3dPortAdvance(0), 0, "first advance");
    RequireEqual(sceAudio3dPortAdvance(0), 0, "second advance");
}

void QueueFrameBehindBlockingPush() {
    FillQueue();
    RequireEqual(sceAudio3dPortPush(0, 1), 0, "blocking push");
    RequireEqual(sceAudio3dPortAdvance(0), 0, "third advance");
}

using ParameterChange = void (*)(Audio3dOpenParameters&);

struct ParameterRow {
    const char* name;
    ParameterChange change;
};

const Case terminateBeforeInitialize{"Audio3d_TerminateBeforeInitialize_ReturnsNotReady", [] {
    const Audio3dCleanup cleanup;
    RequireEqual(sceAudio3dTerminate(), notReady, "terminate before initialize");
}};

const Case initializeReserved{"Audio3d_InitializeNonZeroReserved_ReturnsInvalidParameter", [] {
    const Audio3dCleanup cleanup;
    RequireEqual(sceAudio3dInitialize(1), invalidParameter, "initialize with reserved 1");
}};

const Case openBeforeInitialize{"Audio3d_PortOpenBeforeInitialize_ReturnsNotReadyAndKeepsId", [] {
    const Audio3dCleanup cleanup;
    std::uint32_t id = 7;
    RequireEqual(Open(Defaults(), &id), notReady, "open before initialize");
    RequireEqual(id, 7u, "a failed open must not write the port id");
}};

const Case openBeforeInitializeNull{"Audio3d_PortOpenBeforeInitializeWithInvalidArguments_ReturnsNotReady", [] {
    const Audio3dCleanup cleanup;
    RequireEqual(sceAudio3dPortOpen(1, nullptr, nullptr), notReady, "NOT_READY is checked before the arguments");
}};

const Case portCallsBeforeInitialize{"Audio3d_PortCallsBeforeInitialize_ReturnInvalidPort", [] {
    const Audio3dCleanup cleanup;
    std::uint32_t level = 0;
    float value = 0.0f;
    RequireEqual(sceAudio3dPortGetQueueLevel(0, &level, nullptr), invalidPort, "queue level needs an open port");
    RequireEqual(sceAudio3dPortClose(0), invalidPort, "close needs an open port");
    RequireEqual(sceAudio3dPortAdvance(0), invalidPort, "advance needs an open port");
    RequireEqual(sceAudio3dPortPush(0, 0), invalidPort, "push needs an open port");
    RequireEqual(sceAudio3dPortSetAttribute(0, lateReverbLevel, &value, sizeof(value)), invalidPort, "set attribute needs an open port");
}};

const Case initialize{"Audio3d_Initialize_Succeeds", [] {
    const Audio3dCleanup cleanup;
    RequireEqual(sceAudio3dInitialize(0), 0, "initialize");
}};

const Case repeatedInitialize{"Audio3d_RepeatedInitialize_Throws", [] {
    const Audio3dLibrary library;
    RequireThrows<std::runtime_error>([] { sceAudio3dInitialize(0); }, "repeated initialize must throw");
}};

const Case openNonSystemUser{"Audio3dPortOpen_NonSystemUser_ReturnsInvalidParameter", [] {
    const Audio3dLibrary library;
    const Audio3dOpenParameters defaults = Defaults();
    std::uint32_t id = 7;
    RequireEqual(sceAudio3dPortOpen(1, &defaults, &id), invalidParameter, "user id must be the system user");
}};

const Case openNullParameters{"Audio3dPortOpen_NullParameters_ReturnsInvalidParameter", [] {
    const Audio3dLibrary library;
    std::uint32_t id = 7;
    RequireEqual(sceAudio3dPortOpen(systemUser, nullptr, &id), invalidParameter, "null parameters");
}};

const Case openNullId{"Audio3dPortOpen_NullPortId_ReturnsInvalidParameter", [] {
    const Audio3dLibrary library;
    const Audio3dOpenParameters defaults = Defaults();
    RequireEqual(sceAudio3dPortOpen(systemUser, &defaults, nullptr), invalidParameter, "null port id");
}};

const Case openInvalidParameters{"Audio3dPortOpen_InvalidParameters_ReturnInvalidParameterAndKeepId", [] {
    constexpr ParameterRow rows[] = {
        {"unknown parameter size", [](Audio3dOpenParameters& p) { p.size_this = 0x30; }},
        {"parameter size is not masked", [](Audio3dOpenParameters& p) { p.size_this = 0x21; }},
        {"only 48 kHz", [](Audio3dOpenParameters& p) { p.rate = 1; }},
        {"granularity below 256", [](Audio3dOpenParameters& p) { p.granularity = 0x80; }},
        {"granularity not a multiple of 256", [](Audio3dOpenParameters& p) { p.granularity = 0x180; }},
        {"zero objects", [](Audio3dOpenParameters& p) { p.max_objects = 0; }},
        {"zero queue depth", [](Audio3dOpenParameters& p) { p.queue_depth = 0; }},
        {"buffer mode above 2", [](Audio3dOpenParameters& p) { p.buffer_mode = 3; }},
        {"4 beds", [](Audio3dOpenParameters& p) { p.size_this = 0x28; p.num_beds = 4; }},
        {"1 bed", [](Audio3dOpenParameters& p) { p.size_this = 0x28; p.num_beds = 1; }},
    };
    const Audio3dLibrary library;
    for (const ParameterRow& row : rows) {
        Audio3dOpenParameters parameters = Defaults();
        row.change(parameters);
        std::uint32_t id = 7;
        RequireEqual(Open(parameters, &id), invalidParameter, row.name);
        RequireEqual(id, 7u, std::string("a failed open must not write the port id, ") + row.name);
    }
}};

const Case openUnsupportedParameters{"Audio3dPortOpen_UnsupportedParameters_Throw", [] {
    constexpr ParameterRow rows[] = {
        {"0x10 parameters select buffer mode 0", [](Audio3dOpenParameters& p) { p.size_this = 0x10; }},
        {"0x18 parameters select buffer mode 1", [](Audio3dOpenParameters& p) { p.size_this = 0x18; }},
        {"buffer mode 1", [](Audio3dOpenParameters& p) { p.buffer_mode = 1; }},
        {"3 beds", [](Audio3dOpenParameters& p) { p.size_this = 0x28; p.num_beds = 3; }},
    };
    const Audio3dLibrary library;
    for (const ParameterRow& row : rows) {
        Audio3dOpenParameters parameters = Defaults();
        row.change(parameters);
        std::uint32_t id = 7;
        RequireThrows<std::runtime_error>([&] { Open(parameters, &id); }, row.name);
    }
}};

const Case openTwoBeds{"Audio3dPortOpen_TwoBeds_HandsOutPortZero", [] {
    const Audio3dLibrary library;
    std::uint32_t id = 7;
    RequireEqual(Open(TwoBedParameters(), &id), 0, "open with 2 beds");
    RequireEqual(id, 0u, "open hands out port 0");
}};

const Case openSecondPort{"Audio3dPortOpen_SecondPort_ReturnsOutOfResourcesAndKeepsId", [] {
    const Audio3dLibrary library;
    OpenPort(TwoBedParameters());
    std::uint32_t second = 7;
    RequireEqual(Open(TwoBedParameters(), &second), outOfResources, "only one port");
    RequireEqual(second, 7u, "a failed open must not write the port id");
}};

const Case portOneCalls{"Audio3dPort_PortOneQueueLevelAndClose_ReturnInvalidPort", [] {
    const Audio3dLibrary library;
    OpenPort(TwoBedParameters());
    std::uint32_t level = 0;
    RequireEqual(sceAudio3dPortGetQueueLevel(1, &level, nullptr), invalidPort, "port 1 does not exist");
    RequireEqual(sceAudio3dPortClose(1), invalidPort, "close of port 1");
}};

const Case closeOpenPort{"Audio3dPortClose_OpenPort_Succeeds", [] {
    const Audio3dLibrary library;
    OpenPort(TwoBedParameters());
    RequireEqual(sceAudio3dPortClose(0), 0, "close");
}};

const Case closeTwice{"Audio3dPortClose_ClosedPort_ReturnsInvalidPort", [] {
    const Audio3dLibrary library;
    OpenPort(TwoBedParameters());
    RequireEqual(sceAudio3dPortClose(0), 0, "close");
    RequireEqual(sceAudio3dPortClose(0), invalidPort, "double close");
}};

const Case levelClosedPort{"Audio3dPortGetQueueLevel_ClosedPort_ReturnsInvalidPort", [] {
    const Audio3dLibrary library;
    OpenPort(TwoBedParameters());
    RequireEqual(sceAudio3dPortClose(0), 0, "close");
    std::uint32_t level = 0;
    RequireEqual(sceAudio3dPortGetQueueLevel(0, &level, nullptr), invalidPort, "closed port");
}};

const Case reserveNullId{"Audio3dObjectReserve_NullId_ReturnsInvalidParameter", [] {
    const Audio3dLibrary library;
    RequireEqual(sceAudio3dObjectReserve(0, nullptr), invalidParameter, "null object id");
}};

const Case reserveNoPort{"Audio3dObjectReserve_NoOpenPort_ReturnsInvalidPortAndInvalidId", [] {
    const Audio3dLibrary library;
    std::uint32_t object = 7;
    RequireEqual(sceAudio3dObjectReserve(0, &object), invalidPort, "reserve needs an open port");
    RequireEqual(object, objectInvalid, "object id after a failed reserve");
}};

const Case unreserveNoPort{"Audio3dObjectUnreserve_NoOpenPort_ReturnsInvalidPort", [] {
    const Audio3dLibrary library;
    RequireEqual(sceAudio3dObjectUnreserve(0, 1), invalidPort, "unreserve needs an open port");
}};

const Case reservePortOne{"Audio3dObjectReserve_PortOne_ReturnsInvalidPortAndInvalidId", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    std::uint32_t object = 7;
    RequireEqual(sceAudio3dObjectReserve(1, &object), invalidPort, "reserve on port 1");
    RequireEqual(object, objectInvalid, "object id after a failed reserve");
}};

const Case reserveFresh{"Audio3dObjectReserve_FreshPort_HandsOutOneThenTwo", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object is 1");
    RequireReserved(2, "second object is 2");
}};

const Case reserveBeyondLimit{"Audio3dObjectReserve_BeyondMaxObjects_ReturnsOutOfResourcesAndInvalidId", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object");
    RequireReserved(2, "second object");
    std::uint32_t object = 7;
    RequireEqual(sceAudio3dObjectReserve(0, &object), outOfResources, "max_objects is the limit");
    RequireEqual(object, objectInvalid, "object id after a failed reserve");
}};

const Case unreservePortOne{"Audio3dObjectUnreserve_PortOne_ReturnsInvalidPort", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object");
    RequireReserved(2, "second object");
    RequireEqual(sceAudio3dObjectUnreserve(1, 1), invalidPort, "unreserve on port 1");
}};

const Case unreserveUnknown{"Audio3dObjectUnreserve_UnknownObject_ReturnsInvalidObject", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object");
    RequireReserved(2, "second object");
    RequireEqual(sceAudio3dObjectUnreserve(0, 3), invalidObject, "unreserve of an unknown object");
    RequireEqual(sceAudio3dObjectUnreserve(0, objectInvalid), invalidObject, "unreserve of the invalid object");
}};

const Case unreserveReserved{"Audio3dObjectUnreserve_ReservedObject_Succeeds", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object");
    RequireReserved(2, "second object");
    RequireEqual(sceAudio3dObjectUnreserve(0, 1), 0, "unreserve");
}};

const Case unreserveTwice{"Audio3dObjectUnreserve_Twice_ReturnsInvalidObject", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object");
    RequireReserved(2, "second object");
    RequireEqual(sceAudio3dObjectUnreserve(0, 1), 0, "unreserve");
    RequireEqual(sceAudio3dObjectUnreserve(0, 1), invalidObject, "double unreserve");
}};

const Case reserveAfterUnreserve{"Audio3dObjectReserve_AfterUnreserve_DoesNotReuseId", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object");
    RequireReserved(2, "second object");
    RequireEqual(sceAudio3dObjectUnreserve(0, 1), 0, "unreserve");
    RequireReserved(3, "ids are not reused while the port is open");
}};

const Case reserveAfterRefill{"Audio3dObjectReserve_AfterRefill_ReturnsOutOfResources", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object");
    RequireReserved(2, "second object");
    RequireEqual(sceAudio3dObjectUnreserve(0, 1), 0, "unreserve");
    RequireReserved(3, "refill");
    std::uint32_t object = 7;
    RequireEqual(sceAudio3dObjectReserve(0, &object), outOfResources, "full again");
}};

const Case closeWithObjects{"Audio3dPortClose_WithReservedObjects_Succeeds", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object");
    RequireReserved(2, "second object");
    RequireEqual(sceAudio3dPortClose(0), 0, "close");
}};

const Case reopenDropsObjects{"Audio3dPortOpen_AfterClose_DropsObjects", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object");
    RequireReserved(2, "second object");
    RequireEqual(sceAudio3dPortClose(0), 0, "close");
    OpenPort(TwoObjectParameters());
    RequireEqual(sceAudio3dObjectUnreserve(0, 2), invalidObject, "close drops the objects");
}};

const Case reopenRestartsIds{"Audio3dPortOpen_AfterClose_RestartsObjectIdsWithFullLimit", [] {
    const Audio3dLibrary library;
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "first object");
    RequireReserved(2, "second object");
    RequireEqual(sceAudio3dPortClose(0), 0, "close");
    OpenPort(TwoObjectParameters());
    RequireReserved(1, "ids restart on a new port");
    RequireReserved(2, "a new port has the full limit");
}};

const Case attributeNull{"Audio3dPortSetAttribute_NullValue_ReturnsInvalidParameter", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    RequireEqual(sceAudio3dPortSetAttribute(0, lateReverbLevel, nullptr, 4), invalidParameter, "null attribute");
}};

const Case attributePortOne{"Audio3dPortSetAttribute_PortOne_ReturnsInvalidPort", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    float value = 0.5f;
    RequireEqual(sceAudio3dPortSetAttribute(1, lateReverbLevel, &value, 4), invalidPort, "attribute on port 1");
}};

const Case attributeKnown{"Audio3dPortSetAttribute_KnownAttributes_Succeed", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    float value = 0.5f;
    int flag = 1;
    RequireEqual(sceAudio3dPortSetAttribute(0, 0x10001, &value, 4), 0, "late reverb level");
    RequireEqual(sceAudio3dPortSetAttribute(0, 0x10002, &value, 4), 0, "downmix spread radius");
    RequireEqual(sceAudio3dPortSetAttribute(0, 0x10003, &flag, 4), 0, "downmix spread height aware");
}};

const Case attributeUnknown{"Audio3dPortSetAttribute_UnknownAttribute_Throws", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    float value = 0.5f;
    RequireThrows<std::runtime_error>([&] { sceAudio3dPortSetAttribute(0, 0x10004, &value, 4); }, "unknown attribute must throw");
}};

const Case attributeWrongSize{"Audio3dPortSetAttribute_WrongSize_Throws", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    float value = 0.5f;
    RequireThrows<std::runtime_error>([&] { sceAudio3dPortSetAttribute(0, lateReverbLevel, &value, 8); }, "wrong attribute size must throw");
}};

const Case levelBothNull{"Audio3dPortGetQueueLevel_BothOutputsNull_ReturnsInvalidParameter", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    RequireEqual(sceAudio3dPortGetQueueLevel(0, nullptr, nullptr), invalidParameter, "both outputs null");
}};

const Case levelNullLevel{"Audio3dPortGetQueueLevel_NullLevel_ReportsAvailable", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    std::uint32_t available = 0;
    RequireEqual(sceAudio3dPortGetQueueLevel(0, nullptr, &available), 0, "level pointer is optional");
    RequireEqual(available, 2u, "queue available");
}};

const Case levelFresh{"Audio3dPortGetQueueLevel_FreshPort_IsEmpty", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    RequireLevel(0, 2, "empty queue");
}};

const Case pushEmpty{"Audio3dPortPush_EmptyQueue_QueuesNothing", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    RequireEqual(sceAudio3dPortPush(0, 1), 0, "push of an empty queue");
    RequireLevel(0, 2, "empty push queues nothing");
}};

const Case advanceOnce{"Audio3dPortAdvance_Once_QueuesOneFrame", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    RequireEqual(sceAudio3dPortAdvance(0), 0, "advance 1");
    RequireLevel(1, 1, "one frame queued");
}};

const Case advanceTwice{"Audio3dPortAdvance_Twice_FillsQueue", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    FillQueue();
    RequireLevel(2, 0, "queue full");
}};

const Case advanceFull{"Audio3dPortAdvance_FullQueue_Throws", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    FillQueue();
    RequireThrows<std::runtime_error>([] { sceAudio3dPortAdvance(0); }, "advance into a full queue must throw");
}};

const Case pushUnknownMode{"Audio3dPortPush_UnknownBlockingMode_Throws", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    FillQueue();
    RequireThrows<std::runtime_error>([] { sceAudio3dPortPush(0, 2); }, "unknown blocking mode must throw");
}};

const Case pushBlockingWaits{"Audio3dPortPush_BlockingOnFullQueue_WaitsForOneFrame", [] {
    using namespace std::chrono;
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    FillQueue();
    const auto start = steady_clock::now();
    RequireEqual(sceAudio3dPortPush(0, 1), 0, "blocking push");
    const auto waited = steady_clock::now() - start;
    Require(waited >= queueFrame - milliseconds(2),
            "blocking push waits until one frame has played, waited " + std::to_string(duration_cast<microseconds>(waited).count()) + " us");
}};

const Case pushBlockingFreesSlot{"Audio3dPortPush_BlockingOnFullQueue_ReturnsWithOneFreeSlot", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    FillQueue();
    RequireEqual(sceAudio3dPortPush(0, 1), 0, "blocking push");
    RequireLevel(1, 1, "blocking push returns with one free slot");
}};

const Case pushAsyncNoWait{"Audio3dPortPush_Async_DoesNotWaitForAFrame", [] {
    using namespace std::chrono;
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    QueueFrameBehindBlockingPush();
    const auto start = steady_clock::now();
    RequireEqual(sceAudio3dPortPush(0, 0), 0, "async push");
    const auto waited = steady_clock::now() - start;
    Require(waited < queueFrame, "async push does not wait for a frame, waited " + std::to_string(duration_cast<microseconds>(waited).count()) + " us");
}};

const Case pushAsyncQueued{"Audio3dPortPush_Async_KeepsFramesQueued", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    QueueFrameBehindBlockingPush();
    RequireEqual(sceAudio3dPortPush(0, 0), 0, "async push");
    std::uint32_t level = 0;
    RequireEqual(sceAudio3dPortGetQueueLevel(0, &level, nullptr), 0, "get the queue level");
    Require(level >= 1, "async push keeps frames queued, level " + std::to_string(level));
}};

const Case pushAsyncDrains{"Audio3dPortPush_Async_FramesDrainAtPortRate", [] {
    const Audio3dLibrary library;
    OpenPort(QueueParameters());
    QueueFrameBehindBlockingPush();
    RequireEqual(sceAudio3dPortPush(0, 0), 0, "async push");
    std::this_thread::sleep_for(queueFrame * 3);
    RequireLevel(0, 2, "frames drain at the port's rate");
}};

const Case flushNoPort{"Audio3dPortFlush_NoOpenPort_ReturnsInvalidPort", [] {
    const Audio3dLibrary library;
    RequireEqual(sceAudio3dPortFlush(0), invalidPort, "flush needs an open port");
}};

const Case flushPortOne{"Audio3dPortFlush_PortOne_ReturnsInvalidPort", [] {
    const Audio3dLibrary library;
    OpenPort(Defaults());
    RequireEqual(sceAudio3dPortAdvance(0), 0, "advance");
    RequireLevel(1, 1, "one frame queued");
    RequireEqual(sceAudio3dPortFlush(1), invalidPort, "flush of port 1");
}};

const Case flushQueued{"Audio3dPortFlush_QueuedFrame_DropsFrames", [] {
    const Audio3dLibrary library;
    OpenPort(Defaults());
    RequireEqual(sceAudio3dPortAdvance(0), 0, "advance");
    RequireLevel(1, 1, "one frame queued");
    RequireEqual(sceAudio3dPortFlush(0), 0, "flush");
    RequireLevel(0, 2, "flush drops the queued frames");
}};

const Case objectAttributesNoPort{"Audio3dObjectSetAttributes_NoOpenPort_ReturnsInvalidPort", [] {
    const Audio3dLibrary library;
    float value = 0.5f;
    const Audio3dAttribute attribute{lateReverbLevel, 0, &value, sizeof(value)};
    RequireEqual(sceAudio3dObjectSetAttributes(0, 1, 1, &attribute), invalidPort, "attributes need an open port");
}};

const Case objectAttributesInvalid{"Audio3dObjectSetAttributes_InvalidArguments_AreRejected", [] {
    const Audio3dLibrary library;
    OpenPort(Defaults());
    RequireReserved(1, "reserve");
    float value = 0.5f;
    const Audio3dAttribute attribute{lateReverbLevel, 0, &value, sizeof(value)};
    RequireEqual(sceAudio3dObjectSetAttributes(1, 1, 1, &attribute), invalidPort, "attributes on port 1");
    RequireEqual(sceAudio3dObjectSetAttributes(0, 1, 0, &attribute), invalidParameter, "zero attributes");
    RequireEqual(sceAudio3dObjectSetAttributes(0, 1, 1, nullptr), invalidParameter, "null array");
    RequireEqual(sceAudio3dObjectSetAttributes(0, 2, 1, &attribute), invalidObject, "unreserved object");
}};

const Case objectAttributesOne{"Audio3dObjectSetAttributes_OneAttribute_Succeeds", [] {
    const Audio3dLibrary library;
    OpenPort(Defaults());
    RequireReserved(1, "reserve");
    float value = 0.5f;
    const Audio3dAttribute attribute{lateReverbLevel, 0, &value, sizeof(value)};
    RequireEqual(sceAudio3dObjectSetAttributes(0, 1, 1, &attribute), 0, "set one attribute");
}};

const Case objectAttributesReset{"Audio3dObjectSetAttributes_ResetStateWithNullValue_Succeeds", [] {
    const Audio3dLibrary library;
    OpenPort(Defaults());
    RequireReserved(1, "reserve");
    const Audio3dAttribute reset{0x20000, 0, nullptr, 0};
    RequireEqual(sceAudio3dObjectSetAttributes(0, 1, 1, &reset), 0, "reset state takes a null value");
}};

const Case objectAttributesMissing{"Audio3dObjectSetAttributes_MissingValue_ReturnsInvalidParameter", [] {
    const Audio3dLibrary library;
    OpenPort(Defaults());
    RequireReserved(1, "reserve");
    const Audio3dAttribute missing{lateReverbLevel, 0, nullptr, 0};
    RequireEqual(sceAudio3dObjectSetAttributes(0, 1, 1, &missing), invalidParameter, "a value is required");
}};

const Case terminateOpenPort{"Audio3dTerminate_WithOpenPort_ReturnsNotReady", [] {
    const Audio3dLibrary library;
    OpenPort(Defaults());
    RequireEqual(sceAudio3dTerminate(), notReady, "terminate with an open port");
}};

const Case terminateInitialized{"Audio3dTerminate_AfterClose_Succeeds", [] {
    const Audio3dLibrary library;
    OpenPort(Defaults());
    RequireEqual(sceAudio3dPortClose(0), 0, "close before terminate");
    RequireEqual(sceAudio3dTerminate(), 0, "terminate");
}};

const Case terminateTwice{"Audio3dTerminate_Twice_ReturnsNotReady", [] {
    const Audio3dLibrary library;
    RequireEqual(sceAudio3dTerminate(), 0, "terminate");
    RequireEqual(sceAudio3dTerminate(), notReady, "double terminate");
}};

const Case openAfterTerminate{"Audio3dPortOpen_AfterTerminate_ReturnsNotReadyAndKeepsId", [] {
    const Audio3dLibrary library;
    RequireEqual(sceAudio3dTerminate(), 0, "terminate");
    std::uint32_t id = 7;
    RequireEqual(Open(Defaults(), &id), notReady, "open after terminate");
    RequireEqual(id, 7u, "a failed open must not write the port id");
}};

const Case reinitialize{"Audio3dInitialize_AfterTerminate_AllowsAPortAgain", [] {
    const Audio3dLibrary library;
    RequireEqual(sceAudio3dTerminate(), 0, "terminate");
    RequireEqual(sceAudio3dInitialize(0), 0, "initialize after terminate");
    OpenPort(Defaults());
    RequireEqual(sceAudio3dPortClose(0), 0, "close after reinitialize");
    RequireEqual(sceAudio3dTerminate(), 0, "terminate after reinitialize");
}};

} // namespace
