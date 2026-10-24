# libSceInvitationDialog technical debt

### Unknown function info

- [sceInvitationDialogGetResult](../../../core/libs/prx/libSceInvitationDialog/include/Api.hpp) - still throws; SDK field names and types at offsets 0x00 and 0x0C are unknown. In the supplied `libSceInvitationDialog.sprx`, export `8XKR6wa64iQ` at 0xFF0 takes one result pointer, checks reserved bytes 0x18..0x37 and writes the returned status at 0x08. The helper at 0x410 copies eight bytes from the saved open parameter to 0x00 and eight result bytes to 0x08, then copies 20-byte online IDs through the pointer at 0x10, after a four-byte count. Validation at 0xC20 limits recipients to 1..16. `InvitationDialogRecipients` provides the accessed 324-byte storage; any unused SDK tail is unverified. `InvitationDialogResult` provides the accessed 56 bytes, with unknown fields kept opaque; this is not a recovered SDK source declaration.
- [sceInvitationDialogClose](../../../core/libs/prx/libSceInvitationDialog/Export.cpp) (libSceInvitationDialog) - imported by PS4 titles, which do not load without it, and added without an implementation: declared without parameters, every call throws
