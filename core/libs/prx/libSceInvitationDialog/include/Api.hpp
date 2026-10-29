#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

struct InvitationDialogParam {
    CommonDialogBaseParam baseParam;
    std::uint32_t size;
    std::int32_t mode;
    std::int32_t userId;
    std::uint32_t padding;
    void* callbackArg;
    const void* dataParam;
    std::uint8_t reserved[64];
};

struct InvitationDialogRecipients {
    std::uint32_t count;
    NpOnlineId onlineIds[16];
};

struct InvitationDialogResult {
    void* callbackArg;
    std::int32_t errorCode;
    std::int32_t result;
    InvitationDialogRecipients* recipients;
    std::uint8_t reserved[32];
};

static_assert(sizeof(CommonDialogBaseParam) == 48);
static_assert(offsetof(InvitationDialogParam, size) == 48);
static_assert(offsetof(InvitationDialogParam, mode) == 52);
static_assert(offsetof(InvitationDialogParam, userId) == 56);
static_assert(offsetof(InvitationDialogParam, callbackArg) == 64);
static_assert(offsetof(InvitationDialogParam, dataParam) == 72);
static_assert(offsetof(InvitationDialogParam, reserved) == 80);
static_assert(sizeof(InvitationDialogParam) == 144);
static_assert(sizeof(NpOnlineId) == 20);
static_assert(offsetof(InvitationDialogRecipients, onlineIds) == 4);
static_assert(sizeof(InvitationDialogRecipients) == 324);
static_assert(offsetof(InvitationDialogResult, errorCode) == 0x08);
static_assert(offsetof(InvitationDialogResult, result) == 0x0C);
static_assert(offsetof(InvitationDialogResult, recipients) == 0x10);
static_assert(offsetof(InvitationDialogResult, reserved) == 0x18);
static_assert(sizeof(InvitationDialogResult) == 0x38);

extern "C" {

int APS5_VABI sceInvitationDialogGetResult(InvitationDialogResult* result);
int APS5_VABI sceInvitationDialogInitialize(void);
int APS5_VABI sceInvitationDialogOpen(const InvitationDialogParam* param);
int APS5_VABI sceInvitationDialogTerminate(void);
int APS5_VABI sceInvitationDialogUpdateStatus(void);
int APS5_VABI sceInvitationDialogClose(void);

}
