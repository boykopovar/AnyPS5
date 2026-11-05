#ifndef CORE_LIBS_PRX_LIBSCEPLAYERINVITATIONDIALOG_LIBSCEPLAYERINVITATIONDIALOG_H
#define CORE_LIBS_PRX_LIBSCEPLAYERINVITATIONDIALOG_LIBSCEPLAYERINVITATIONDIALOG_H

#include <cstdint>
#include "SceTypes.hpp"

enum class PlayerInvitationDialogStatus : std::int32_t {
    None = 0,
    Initialized = 1,
    Running = 2,
    Finished = 3,
};

// Placeholder only: the guest ABI layout is not known and is never read by this HLE stub.
struct ScePlayerInvitationDialogParam {
    std::uint8_t reserved[1];
};

struct ScePlayerInvitationDialogResult {
    std::int32_t errorCode;
    std::int32_t result;
    std::uint8_t reserved[32];
};
static_assert(sizeof(ScePlayerInvitationDialogResult) == 40);

extern "C" {

std::int32_t APS5_VABI scePlayerInvitationDialogInitialize(void);
std::int32_t APS5_VABI scePlayerInvitationDialogOpen(const ScePlayerInvitationDialogParam* param);
std::int32_t APS5_VABI scePlayerInvitationDialogUpdateStatus(void);
std::int32_t APS5_VABI scePlayerInvitationDialogGetStatus(void);
std::int32_t APS5_VABI scePlayerInvitationDialogClose(void);
std::int32_t APS5_VABI scePlayerInvitationDialogTerminate(void);
std::int32_t APS5_VABI scePlayerInvitationDialogGetResult(ScePlayerInvitationDialogResult* result);

}

#endif
