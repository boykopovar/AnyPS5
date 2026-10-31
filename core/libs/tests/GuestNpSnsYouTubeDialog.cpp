#include "prx/libSceNpSnsYouTubeDialog/include/Api.hpp"
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004);

}

int main() {
    Require(sceNpSnsYouTubeDialogTerminate() == kErrNotInitialized);
    Require(sceNpSnsYouTubeDialogClose() == kErrNotInitialized);
    Require(sceNpSnsYouTubeDialogGetStatus() == kStatusNone);
    Require(sceNpSnsYouTubeDialogUpdateStatus() == kStatusNone);

    Require(sceNpSnsYouTubeDialogInitialize() == 0);
    Require(sceNpSnsYouTubeDialogInitialize() == kErrAlreadyInitialized);
    Require(sceNpSnsYouTubeDialogGetStatus() == kStatusInitialized);
    Require(sceNpSnsYouTubeDialogUpdateStatus() == kStatusInitialized);

    Require(sceNpSnsYouTubeDialogClose() == 0);
    Require(sceNpSnsYouTubeDialogGetStatus() == kStatusFinished);
    Require(sceNpSnsYouTubeDialogUpdateStatus() == kStatusFinished);
    Require(sceNpSnsYouTubeDialogClose() == 0);

    Require(sceNpSnsYouTubeDialogTerminate() == 0);
    Require(sceNpSnsYouTubeDialogTerminate() == kErrNotInitialized);
    Require(sceNpSnsYouTubeDialogClose() == kErrNotInitialized);
    Require(sceNpSnsYouTubeDialogGetStatus() == kStatusNone);
    Require(sceNpSnsYouTubeDialogUpdateStatus() == kStatusNone);

    Require(sceNpSnsYouTubeDialogInitialize() == 0);
    Require(sceNpSnsYouTubeDialogUpdateStatus() == kStatusInitialized);
    Require(sceNpSnsYouTubeDialogTerminate() == 0);
    Require(sceNpSnsYouTubeDialogGetStatus() == kStatusNone);
    Require(sceNpSnsYouTubeDialogUpdateStatus() == kStatusNone);

    return 0;
}
