#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>

extern "C" {
int APS5_VABI scePlayGoDialogInitialize(void);
int APS5_VABI scePlayGoDialogTerminate(void);
int APS5_VABI scePlayGoDialogOpen(const void* param);
int APS5_VABI scePlayGoDialogClose(void);
int APS5_VABI scePlayGoDialogUpdateStatus(void);
int APS5_VABI scePlayGoDialogGetStatus(void);
int APS5_VABI scePlayGoDialogGetResult(void* result);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr std::int32_t notInitialized = static_cast<std::int32_t>(0x80ED0001);
constexpr std::int32_t alreadyInitialized = static_cast<std::int32_t>(0x80ED0002);
constexpr std::int32_t paramInvalid = static_cast<std::int32_t>(0x80ED0003);
constexpr std::int32_t invalidState = static_cast<std::int32_t>(0x80ED0005);
constexpr int statusNone = 0;
constexpr int statusInitialized = 1;
constexpr int statusFinished = 3;
constexpr int openParam = 1;

class InitializedDialog {
public:
    InitializedDialog() {
        RequireEqual(scePlayGoDialogInitialize(), 0, "initialize");
    }

    ~InitializedDialog() {
        scePlayGoDialogTerminate();
    }

    InitializedDialog(const InitializedDialog&) = delete;
    InitializedDialog& operator=(const InitializedDialog&) = delete;
};

const Case statusBeforeInit{"GetStatus_BeforeInitialize_ReturnsNone", [] {
    RequireEqual(scePlayGoDialogGetStatus(), statusNone, "status");
}};

const Case terminateBeforeInit{"Terminate_BeforeInitialize_ReturnsNotInitialized", [] {
    RequireEqual(scePlayGoDialogTerminate(), notInitialized, "terminate");
}};

const Case openNullBeforeInit{"Open_NullParamBeforeInitialize_ReturnsParamInvalid", [] {
    RequireEqual(scePlayGoDialogOpen(nullptr), paramInvalid, "open with a null param");
}};

const Case resultBeforeInit{"GetResult_BeforeInitialize_ReturnsNotInitialized", [] {
    RequireEqual(scePlayGoDialogGetResult(nullptr), notInitialized, "get result");
}};

const Case openBeforeInit{"Open_BeforeInitialize_ReturnsInvalidState", [] {
    RequireEqual(scePlayGoDialogOpen(&openParam), invalidState, "open");
}};

const Case initialize{"Initialize_FromNone_ReportsInitialized", [] {
    const InitializedDialog dialog;
    RequireEqual(scePlayGoDialogGetStatus(), statusInitialized, "status");
}};

const Case initializeTwice{"Initialize_AlreadyInitialized_ReturnsAlreadyInitialized", [] {
    const InitializedDialog dialog;
    RequireEqual(scePlayGoDialogInitialize(), alreadyInitialized, "second initialize");
}};

const Case openNullAfterInit{"Open_NullParamAfterInitialize_ReturnsParamInvalid", [] {
    const InitializedDialog dialog;
    RequireEqual(scePlayGoDialogOpen(nullptr), paramInvalid, "open with a null param");
}};

const Case updateAfterInit{"UpdateStatus_AfterInitialize_ReportsInitialized", [] {
    const InitializedDialog dialog;
    RequireEqual(scePlayGoDialogUpdateStatus(), statusInitialized, "updated status");
}};

const Case open{"Open_AfterInitialize_FinishesImmediately", [] {
    const InitializedDialog dialog;
    RequireEqual(scePlayGoDialogOpen(&openParam), 0, "open");
    RequireEqual(scePlayGoDialogGetStatus(), statusFinished, "status");
    RequireEqual(scePlayGoDialogUpdateStatus(), statusFinished, "updated status");
}};

const Case result{"GetResult_AfterOpen_ReportsResultZero", [] {
    const InitializedDialog dialog;
    RequireEqual(scePlayGoDialogOpen(&openParam), 0, "open");
    PlayGoDialogResult dialogResult{};
    RequireEqual(scePlayGoDialogGetResult(&dialogResult), 0, "get result");
    RequireEqual(dialogResult.result, 0, "dialog result");
}};

const Case closeAndTerminate{"CloseAndTerminate_AfterOpen_ReturnToNone", [] {
    RequireEqual(scePlayGoDialogInitialize(), 0, "initialize");
    RequireEqual(scePlayGoDialogOpen(&openParam), 0, "open");
    RequireEqual(scePlayGoDialogClose(), 0, "close");
    RequireEqual(scePlayGoDialogTerminate(), 0, "terminate");
    RequireEqual(scePlayGoDialogGetStatus(), statusNone, "status after terminate");
    RequireEqual(scePlayGoDialogTerminate(), notInitialized, "second terminate");
}};

} // namespace
