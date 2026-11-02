#include "prx/libScePatchCheckerClient/include/Api.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

template <typename TAction>
bool ThrowsMessage(TAction action, const char* expected) {
    try {
        action();
    } catch (const std::logic_error& error) {
        return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
}

}

int main() {
    Require(ThrowsMessage([] { scePatchCheckerCancel(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerCheckPatch(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerCreateHandler(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerDestroyHandler(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerDisableAutoDownload(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerEnableAutoDownload(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerGetApplicableTick(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerGetPackageInfo(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerRequestCheckPatch(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerRequestCheckPatchByType(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerTerminate(); }, "not initialized"));

    Require(scePatchCheckerInitialize() == 0);
    Require(ThrowsMessage([] { scePatchCheckerInitialize(); }, "already initialized"));

    Require(scePatchCheckerTerminate() == 0);

    Require(ThrowsMessage([] { scePatchCheckerCancel(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerCheckPatch(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerCreateHandler(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerDestroyHandler(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerDisableAutoDownload(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerEnableAutoDownload(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerGetApplicableTick(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerGetPackageInfo(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerRequestCheckPatch(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerRequestCheckPatchByType(); }, "not initialized"));
    Require(ThrowsMessage([] { scePatchCheckerTerminate(); }, "not initialized"));

    Require(scePatchCheckerInitialize() == 0);
    Require(scePatchCheckerTerminate() == 0);

    return 0;
}
