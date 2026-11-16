#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_button_remapping_bounds: %s\n", message);
    std::abort();
}

void TestStructureLayout() {
    Require(sizeof(PadButtonRemappingInfo) == 16, "PadButtonRemappingInfo must be exactly 16 bytes");

    PadButtonRemappingInfo info{};
    Require(sizeof(info.mapping) == 14, "mapping array must be 14 bytes");
    Require(sizeof(info.reserved) == 1, "reserved field must be 1 byte");
    Require(sizeof(info.mode) == 1, "mode field must be 1 byte");

    const std::uint8_t* base = reinterpret_cast<const std::uint8_t*>(&info);
    const std::uint8_t* mappingPtr = reinterpret_cast<const std::uint8_t*>(&info.mapping);
    const std::uint8_t* reservedPtr = reinterpret_cast<const std::uint8_t*>(&info.reserved);
    const std::uint8_t* modePtr = reinterpret_cast<const std::uint8_t*>(&info.mode);

    Require(mappingPtr - base == 0, "mapping must be at offset 0");
    Require(reservedPtr - base == 14, "reserved must be at offset 14");
    Require(modePtr - base == 15, "mode must be at offset 15");
}

void TestPreInitializationBounds() {
    PadButtonRemappingInfo info{};
    for (int handle : {-100, -1, 0, 1, 2, 16, 32, 100, 9999}) {
        bool threw = false;
        try {
            scePadSetButtonRemappingInfo(handle, &info);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "pre-init scePadSetButtonRemappingInfo must throw");

        threw = false;
        try {
            scePadSetButtonRemappingInfo(handle, nullptr);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "pre-init scePadSetButtonRemappingInfo null must throw");
    }

    for (bool enabled : {true, false}) {
        bool threw = false;
        try {
            scePadSetProcessPrivilegeOfButtonRemapping(enabled);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "pre-init scePadSetProcessPrivilegeOfButtonRemapping must throw");
    }

    for (bool disabled : {true, false}) {
        bool threw = false;
        try {
            scePadVirtualDeviceDisableButtonRemapping(disabled);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "pre-init scePadVirtualDeviceDisableButtonRemapping must throw");
    }
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK, "scePadInit must return PAD_OK");
    Require(scePadInit_nid_postfix() == PAD_OK, "repeat scePadInit must return PAD_OK");

    PadButtonRemappingInfo info{};
    for (int handle : {-500, -1, 0, 1, 2, 3, 4, 16, 32, 47, 500}) {
        bool threw = false;
        try {
            scePadSetButtonRemappingInfo(handle, &info);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "unopened handle scePadSetButtonRemappingInfo must throw");

        threw = false;
        try {
            scePadSetButtonRemappingInfo(handle, nullptr);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "unopened handle scePadSetButtonRemappingInfo null must throw");
    }

    for (bool enabled : {true, false}) {
        bool threw = false;
        try {
            scePadSetProcessPrivilegeOfButtonRemapping(enabled);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "post-init scePadSetProcessPrivilegeOfButtonRemapping must throw");
    }

    for (bool disabled : {true, false}) {
        bool threw = false;
        try {
            scePadVirtualDeviceDisableButtonRemapping(disabled);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "post-init scePadVirtualDeviceDisableButtonRemapping must throw");
    }
}

void TestOpenStandardHandleRemappingTableBounds() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 > 0, "open port 0 failed");

    bool threw = false;
    try {
        scePadSetButtonRemappingInfo(handle0, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "open handle null scePadSetButtonRemappingInfo must throw");

    PadButtonRemappingInfo identityInfo{};
    for (std::uint8_t i = 0; i < 14; ++i) {
        identityInfo.mapping[i] = i;
    }
    identityInfo.reserved = 0;
    identityInfo.mode = 0;

    threw = false;
    try {
        scePadSetButtonRemappingInfo(handle0, &identityInfo);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "identity remapping table must throw");

    PadButtonRemappingInfo customInfo{};
    for (std::uint8_t i = 0; i < 14; ++i) {
        customInfo.mapping[i] = static_cast<std::uint8_t>(13 - i);
    }
    customInfo.reserved = 0xFF;
    customInfo.mode = 1;

    threw = false;
    try {
        scePadSetButtonRemappingInfo(handle0, &customInfo);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "custom remapping table mode 1 must throw");

    customInfo.mode = 2;
    threw = false;
    try {
        scePadSetButtonRemappingInfo(handle0, &customInfo);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "custom remapping table mode 2 must throw");

    customInfo.mode = 255;
    threw = false;
    try {
        scePadSetButtonRemappingInfo(handle0, &customInfo);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "custom remapping table mode 255 must throw");

    for (bool enabled : {true, false}) {
        threw = false;
        try {
            scePadSetProcessPrivilegeOfButtonRemapping(enabled);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "open handle scePadSetProcessPrivilegeOfButtonRemapping must throw");
    }

    for (bool disabled : {true, false}) {
        threw = false;
        try {
            scePadVirtualDeviceDisableButtonRemapping(disabled);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "open handle scePadVirtualDeviceDisableButtonRemapping must throw");
    }

    Require(scePadClose_nid_postfix(handle0) == PAD_OK, "close handle0 failed");

    threw = false;
    try {
        scePadSetButtonRemappingInfo(handle0, &identityInfo);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "closed handle scePadSetButtonRemappingInfo must throw");
}

void TestSpecialAndRemoteHandles() {
    const int specialHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(specialHandle >= PAD_SPECIAL_HANDLE, "open special failed");

    PadButtonRemappingInfo info{};
    bool threw = false;
    try {
        scePadSetButtonRemappingInfo(specialHandle, &info);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "special handle scePadSetButtonRemappingInfo must throw");

    Require(scePadClose_nid_postfix(specialHandle) == PAD_OK, "close special failed");

    threw = false;
    try {
        scePadSetButtonRemappingInfo(specialHandle, &info);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "closed special handle scePadSetButtonRemappingInfo must throw");

    const int remoteHandle = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remoteHandle == PAD_REMOTE_HANDLE, "open remote failed");

    threw = false;
    try {
        scePadSetButtonRemappingInfo(remoteHandle, &info);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "remote handle scePadSetButtonRemappingInfo must throw");

    Require(scePadClose_nid_postfix(remoteHandle) == PAD_OK, "close remote failed");

    threw = false;
    try {
        scePadSetButtonRemappingInfo(remoteHandle, &info);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "closed remote handle scePadSetButtonRemappingInfo must throw");
}

} // namespace

int main() {
    TestStructureLayout();
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestOpenStandardHandleRemappingTableBounds();
    TestSpecialAndRemoteHandles();
    return 0;
}
