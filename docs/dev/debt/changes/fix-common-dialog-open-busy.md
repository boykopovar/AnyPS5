# libSceCommonDialog

## add Unknown function info

- [Open while the dialog runs](../../../core/libs/prx/libSceCommonDialog/CommonDialog.hpp) (libSceCommonDialog) - only libSceMsgDialog's path from `Open` to the shared client open `Zaa6fhiKQDE` was traced (firmware 13.42 disassembly, #15643), which returns `0x80B80006` while the dialog runs. For CdlgG2P, CdlgPlayerReview, ConnectAccount, HmdSetup, NetCtlAp, NpAuthAuthorizedApp, NpEula, NpFriendList, NpSnsFacebook, NpSnsYouTube, PlayerSelection, SocialScreen, UsbStorage, VrService and WebBrowser the same value is inferred from their modules importing it. Still `0x80B80007`, not read: libSceSigninDialog's `Open` (its module does not import the shared open), libSceWebBrowserDialog's `Terminate` while running, and libScePlayGoDialog's `Open` (its own `0x80B80006` is a process check, and the state answer after it is not read). Closes when: each library's `Open` is traced to the shared open, or measured on a PS5.
