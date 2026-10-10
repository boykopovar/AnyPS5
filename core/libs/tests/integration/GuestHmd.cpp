#include "prx/libSceHmd/Hmd.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstring>
#include <string>

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

using InitializeFunction = std::int32_t(APS5_VABI*)(const Hmd::InitializeParam*);

struct NamedInitialize {
    const char* name;
    InitializeFunction initialize;
};

const std::array<NamedInitialize, 2> initializers{{
    {"sceHmdInitialize", sceHmdInitialize},
    {"sceHmdInitialize315", sceHmdInitialize315},
}};

class InitializedHmd {
public:
    explicit InitializedHmd(const NamedInitialize& entry) {
        const Hmd::InitializeParam param{};
        RequireEqual(entry.initialize(&param), 0, std::string(entry.name) + ": initialize");
    }

    ~InitializedHmd() {
        sceHmdTerminate();
    }

    InitializedHmd(const InitializedHmd&) = delete;
    InitializedHmd& operator=(const InitializedHmd&) = delete;
};

Hmd::DeviceInformation FilledInformation() {
    Hmd::DeviceInformation info;
    std::memset(&info, 0xa5, sizeof(info));
    return info;
}

Hmd::DeviceInformation AbsentDevice() {
    Hmd::DeviceInformation expected{};
    expected.status = Hmd::DeviceStatus::NotDetected;
    expected.userId = -1;
    return expected;
}

void RequireDisconnected(const std::string& context) {
    Hmd::DeviceStatus status = Hmd::DeviceStatus::Ready;
    RequireEqual(sceHmdInternalGetDeviceStatus(&status), 0, context + ": status query");
    RequireEqual(status, Hmd::DeviceStatus::NotDetected, context + ": device status");
    RequireEqual(sceHmdInternalGetDeviceStatus(nullptr), Hmd::ParameterNull, context + ": null status");
    RequireEqual(sceHmdInternalMmapIsConnect(), 0, context + ": mmap connection");
}

const Case statusUninitialized{"DeviceStatus_Uninitialized_ReportsNotDetected", [] {
    RequireDisconnected("before initialize");
}};

const Case callsUninitialized{"Calls_BeforeInitialize_ReturnNotInitializedWithoutWriting", [] {
    auto info = FilledInformation();
    const auto untouched = info;
    RequireEqual(sceHmdTerminate(), Hmd::NotInitialized, "terminate");
    RequireEqual(sceHmdOpen(1, 0, 0, nullptr), Hmd::NotInitialized, "open");
    RequireEqual(sceHmdClose(0), Hmd::NotInitialized, "close");
    RequireEqual(sceHmdGetDeviceInformation(&info), Hmd::NotInitialized, "device information");
    Require(std::memcmp(&info, &untouched, sizeof(info)) == 0, "failed query modified output");
    RequireEqual(sceHmdGetDeviceInformationByHandle(0, &info), Hmd::NotInitialized, "device information by handle");
}};

const Case informationNullUninitialized{"GetDeviceInformation_NullOutputBeforeInitialize_ReturnsParameterNull", [] {
    RequireEqual(sceHmdGetDeviceInformation(nullptr), Hmd::ParameterNull, "null information");
}};

const Case initializeNull{"Initialize_NullParam_ReturnsParameterNull", [] {
    for (const auto& entry : initializers) {
        RequireEqual(entry.initialize(nullptr), Hmd::ParameterNull, entry.name);
    }
}};

const Case initializeDistortion{"Initialize_DistortionRequested_ReturnsUnsupportedFeature", [] {
    Hmd::InitializeParam param{};
    param.reserved0 = &param;
    for (const auto& entry : initializers) {
        RequireEqual(entry.initialize(&param), Hmd::UnsupportedFeature, entry.name);
    }
}};

const Case initializeTwice{"Initialize_AlreadyInitialized_ReturnsAlreadyInitialized", [] {
    const Hmd::InitializeParam param{};
    for (const auto& entry : initializers) {
        const InitializedHmd hmd(entry);
        const std::string context = std::string("after ") + entry.name;
        RequireEqual(sceHmdInitialize(&param), Hmd::AlreadyInitialized, context + ": sceHmdInitialize");
        RequireEqual(sceHmdInitialize315(&param), Hmd::AlreadyInitialized, context + ": sceHmdInitialize315");
        RequireEqual(sceHmdInitialize(nullptr), Hmd::AlreadyInitialized, context + ": null param");
    }
}};

const Case statusInitialized{"DeviceStatus_Initialized_ReportsNotDetected", [] {
    for (const auto& entry : initializers) {
        const InitializedHmd hmd(entry);
        RequireDisconnected(std::string("after ") + entry.name);
    }
}};

const Case informationInitialized{"GetDeviceInformation_Initialized_ReportsAbsentDevice", [] {
    const auto expected = AbsentDevice();
    for (const auto& entry : initializers) {
        const InitializedHmd hmd(entry);
        const std::string context = std::string("after ") + entry.name;
        auto info = FilledInformation();
        RequireEqual(sceHmdGetDeviceInformation(&info), 0, context + ": device information");
        Require(std::memcmp(&info, &expected, sizeof(info)) == 0, context + ": incorrect absent-device output");
        RequireEqual(sceHmdGetDeviceInformation(nullptr), Hmd::ParameterNull, context + ": null output");
    }
}};

const Case openInvalid{"Open_InvalidParameters_ReturnsParameterInvalid", [] {
    Hmd::InitializeParam reserved{};
    for (const auto& entry : initializers) {
        const InitializedHmd hmd(entry);
        const std::string context = std::string("after ") + entry.name;
        RequireEqual(sceHmdOpen(-1, 0, 0, nullptr), Hmd::ParameterInvalid, context + ": invalid user");
        RequireEqual(sceHmdOpen(0xff, 0, 0, nullptr), Hmd::ParameterInvalid, context + ": system user");
        RequireEqual(sceHmdOpen(1, 1, 0, nullptr), Hmd::ParameterInvalid, context + ": invalid type");
        RequireEqual(sceHmdOpen(1, 0, 1, nullptr), Hmd::ParameterInvalid, context + ": invalid index");
        RequireEqual(sceHmdOpen(1, 0, 0, reinterpret_cast<Hmd::OpenParam*>(&reserved)), Hmd::ParameterInvalid, context + ": non-null reserved open parameter");
    }
}};

const Case openDisconnected{"Open_NoHeadset_ReturnsDeviceDisconnectedRepeatedly", [] {
    for (const auto& entry : initializers) {
        const InitializedHmd hmd(entry);
        const std::string context = std::string("after ") + entry.name;
        RequireEqual(sceHmdOpen(1, 0, 0, nullptr), Hmd::DeviceDisconnected, context + ": first open");
        RequireEqual(sceHmdOpen(1, 0, 0, nullptr), Hmd::DeviceDisconnected, context + ": second open");
    }
}};

const Case invalidHandles{"CloseAndQuery_InvalidHandles_ReturnInvalidHandleWithoutWriting", [] {
    const auto expected = AbsentDevice();
    for (const auto& entry : initializers) {
        const InitializedHmd hmd(entry);
        auto info = expected;
        for (const auto handle : {0, -1, 1, 0x0f000000}) {
            const std::string context = std::string("after ") + entry.name + ", handle " + std::to_string(handle);
            RequireEqual(sceHmdClose(handle), Hmd::InvalidHandle, context + ": close");
            RequireEqual(sceHmdGetDeviceInformationByHandle(handle, &info), Hmd::InvalidHandle, context + ": query");
            Require(std::memcmp(&info, &expected, sizeof(info)) == 0, context + ": invalid handle modified output");
        }
        RequireEqual(sceHmdGetDeviceInformationByHandle(0, nullptr), Hmd::InvalidHandle, std::string("after ") + entry.name + ": null query");
    }
}};

const Case terminate{"Terminate_Initialized_SucceedsOnceAndDisablesQueries", [] {
    const Hmd::InitializeParam param{};
    for (const auto& entry : initializers) {
        const std::string context = std::string("after ") + entry.name;
        auto info = FilledInformation();
        RequireEqual(entry.initialize(&param), 0, context + ": initialize");
        RequireEqual(sceHmdTerminate(), 0, context + ": terminate");
        RequireEqual(sceHmdTerminate(), Hmd::NotInitialized, context + ": second terminate");
        RequireEqual(sceHmdGetDeviceInformation(&info), Hmd::NotInitialized, context + ": query after terminate");
        RequireDisconnected(context + ", terminated");
    }
}};

} // namespace
