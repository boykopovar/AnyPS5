#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_trigger_strength_ext_bounds: %s\n", message);
    std::abort();
}

void TestPreInitializationBounds() {
    alignas(8) std::uint8_t buffer[256]{};
    PadTriggerInternalState* state = reinterpret_cast<PadTriggerInternalState*>(buffer);
    const PadTriggerEffectExtended* effect = reinterpret_cast<const PadTriggerEffectExtended*>(buffer);

    for (int handle : {-100, -1, 0, 1, 2, 16, 32, 100, 9999}) {
        for (std::uint32_t strength : {0u, 1u, 50u, 100u, 255u, 0xffffffffu}) {
            bool threw = false;
            try {
                scePadSetTriggerEffectStrength(handle, strength);
            } catch (const std::runtime_error&) {
                threw = true;
            }
            Require(threw, "pre-init scePadSetTriggerEffectStrength must throw");
        }

        bool threw = false;
        try {
            scePadGetTriggerEffectInternalState(handle, state);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "pre-init scePadGetTriggerEffectInternalState must throw");

        threw = false;
        try {
            scePadGetTriggerEffectInternalState(handle, nullptr);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "pre-init scePadGetTriggerEffectInternalState null must throw");

        threw = false;
        try {
            scePadSetTriggerEffectExt(handle, effect);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "pre-init scePadSetTriggerEffectExt must throw");

        threw = false;
        try {
            scePadSetTriggerEffectExt(handle, nullptr);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "pre-init scePadSetTriggerEffectExt null must throw");
    }
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK, "scePadInit must return PAD_OK");
    Require(scePadInit_nid_postfix() == PAD_OK, "repeat scePadInit must return PAD_OK");

    alignas(8) std::uint8_t buffer[256]{};
    PadTriggerInternalState* state = reinterpret_cast<PadTriggerInternalState*>(buffer);
    const PadTriggerEffectExtended* effect = reinterpret_cast<const PadTriggerEffectExtended*>(buffer);

    for (int handle : {-500, -1, 0, 1, 2, 3, 4, 16, 32, 47, 500}) {
        for (std::uint32_t strength : {0u, 1u, 10u, 128u, 255u, 65535u, 0x80000000u, 0xffffffffu}) {
            bool threw = false;
            try {
                scePadSetTriggerEffectStrength(handle, strength);
            } catch (const std::runtime_error&) {
                threw = true;
            }
            Require(threw, "unopened handle scePadSetTriggerEffectStrength must throw");
        }

        bool threw = false;
        try {
            scePadGetTriggerEffectInternalState(handle, state);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "unopened handle scePadGetTriggerEffectInternalState must throw");

        threw = false;
        try {
            scePadGetTriggerEffectInternalState(handle, nullptr);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "unopened handle scePadGetTriggerEffectInternalState null must throw");

        threw = false;
        try {
            scePadSetTriggerEffectExt(handle, effect);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "unopened handle scePadSetTriggerEffectExt must throw");

        threw = false;
        try {
            scePadSetTriggerEffectExt(handle, nullptr);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "unopened handle scePadSetTriggerEffectExt null must throw");
    }
}

void TestStandardHandleTriggerStrengthAndExt() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 > 0, "open port 0 failed");

    alignas(8) std::uint8_t buffer[256]{};
    PadTriggerInternalState* state = reinterpret_cast<PadTriggerInternalState*>(buffer);
    const PadTriggerEffectExtended* effect = reinterpret_cast<const PadTriggerEffectExtended*>(buffer);

    for (std::uint32_t strength : {0u, 1u, 2u, 10u, 50u, 100u, 128u, 200u, 255u, 500u, 1000u, 65535u, 0x80000000u, 0xffffffffu}) {
        bool threw = false;
        try {
            scePadSetTriggerEffectStrength(handle0, strength);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "open handle scePadSetTriggerEffectStrength must throw");
    }

    bool threw = false;
    try {
        scePadGetTriggerEffectInternalState(handle0, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "open handle scePadGetTriggerEffectInternalState null must throw");

    threw = false;
    try {
        scePadGetTriggerEffectInternalState(handle0, state);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "open handle scePadGetTriggerEffectInternalState must throw");

    threw = false;
    try {
        scePadSetTriggerEffectExt(handle0, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "open handle scePadSetTriggerEffectExt null must throw");

    threw = false;
    try {
        scePadSetTriggerEffectExt(handle0, effect);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "open handle scePadSetTriggerEffectExt must throw");

    Require(scePadClose_nid_postfix(handle0) == PAD_OK, "close handle0 failed");

    threw = false;
    try {
        scePadSetTriggerEffectStrength(handle0, 100);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "closed handle scePadSetTriggerEffectStrength must throw");

    threw = false;
    try {
        scePadGetTriggerEffectInternalState(handle0, state);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "closed handle scePadGetTriggerEffectInternalState must throw");

    threw = false;
    try {
        scePadSetTriggerEffectExt(handle0, effect);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "closed handle scePadSetTriggerEffectExt must throw");
}

void TestSpecialAndRemoteHandles() {
    alignas(8) std::uint8_t buffer[256]{};
    PadTriggerInternalState* state = reinterpret_cast<PadTriggerInternalState*>(buffer);
    const PadTriggerEffectExtended* effect = reinterpret_cast<const PadTriggerEffectExtended*>(buffer);

    const int specialHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(specialHandle >= PAD_SPECIAL_HANDLE, "open special failed");

    bool threw = false;
    try {
        scePadSetTriggerEffectStrength(specialHandle, 100);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "special handle scePadSetTriggerEffectStrength must throw");

    threw = false;
    try {
        scePadGetTriggerEffectInternalState(specialHandle, state);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "special handle scePadGetTriggerEffectInternalState must throw");

    threw = false;
    try {
        scePadSetTriggerEffectExt(specialHandle, effect);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "special handle scePadSetTriggerEffectExt must throw");

    Require(scePadClose_nid_postfix(specialHandle) == PAD_OK, "close special failed");

    threw = false;
    try {
        scePadSetTriggerEffectStrength(specialHandle, 100);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "closed special handle scePadSetTriggerEffectStrength must throw");

    const int remoteHandle = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remoteHandle == PAD_REMOTE_HANDLE, "open remote failed");

    threw = false;
    try {
        scePadSetTriggerEffectStrength(remoteHandle, 100);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "remote handle scePadSetTriggerEffectStrength must throw");

    threw = false;
    try {
        scePadGetTriggerEffectInternalState(remoteHandle, state);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "remote handle scePadGetTriggerEffectInternalState must throw");

    threw = false;
    try {
        scePadSetTriggerEffectExt(remoteHandle, effect);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "remote handle scePadSetTriggerEffectExt must throw");

    Require(scePadClose_nid_postfix(remoteHandle) == PAD_OK, "close remote failed");

    threw = false;
    try {
        scePadSetTriggerEffectStrength(remoteHandle, 100);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "closed remote handle scePadSetTriggerEffectStrength must throw");
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleTriggerStrengthAndExt();
    TestSpecialAndRemoteHandles();
    return 0;
}
