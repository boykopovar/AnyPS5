#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

struct InvitationDialogRecipients {
    std::uint32_t count;
    NpOnlineId onlineIds[16];
};

struct InvitationDialogResult {
    std::uint8_t opaque00[8];
    std::int32_t errorCode;
    std::uint32_t opaque0C;
    InvitationDialogRecipients* recipients;
    std::uint8_t reserved[32];
};

static_assert(sizeof(NpOnlineId) == 20);
static_assert(offsetof(InvitationDialogRecipients, onlineIds) == 4);
static_assert(sizeof(InvitationDialogRecipients) == 324);
static_assert(offsetof(InvitationDialogResult, errorCode) == 0x08);
static_assert(offsetof(InvitationDialogResult, opaque0C) == 0x0C);
static_assert(offsetof(InvitationDialogResult, recipients) == 0x10);
static_assert(offsetof(InvitationDialogResult, reserved) == 0x18);
static_assert(sizeof(InvitationDialogResult) == 0x38);

extern "C" {

int APS5_VABI sceInvitationDialogGetResult(InvitationDialogResult* result);
int APS5_VABI sceInvitationDialogInitialize();
int APS5_VABI sceInvitationDialogOpen();
int APS5_VABI sceInvitationDialogTerminate();
int APS5_VABI sceInvitationDialogUpdateStatus();
int APS5_VABI sceInvitationDialogClose(void);

}
