# libSceWebBrowserDialog technical debt

### Silent stubs

- [sceWebBrowserDialogOpen, sceWebBrowserDialogOpenForPredeterminedContent](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) (libSceWebBrowserDialog) - with `ANYPS5_PSN_DIALOGS=show`, a successful open shows AnyPS5's generic notice that the system feature is not available; no PlayStation Network link is claimed, since no source ties this dialog to it, and the console's own display is not measured.
- [libSceWebBrowserDialog](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) - `sceWebBrowserDialogOpen` and `sceWebBrowserDialogOpenForPredeterminedContent` run the common-dialog state machine without displaying an interactive browser, and `sceWebBrowserDialogGetResult` reports that the user cancelled. Minecraft Bedrock (PPSA17221) imports `sceWebBrowserDialogOpenForPredeterminedContent` (#5744). The 424-byte allow-list parameter and its twenty optional address pointers are validated, but address filtering is not modelled without an interactive browser. Main open-parameter validation remains incomplete.
- [sceWebBrowserDialogSetCookie, sceWebBrowserDialogResetCookie](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) (libSceWebBrowserDialog) - there is no browser cookie store. Both calls check their parameters in the same order and with the same codes as the PS5 13.60 module, then return 0 without storing or clearing anything. On the console they then forward the request to the browser service. While an opened dialog has not been updated to finished, closed or terminated, they return `0x80B80006` (the console's open-dialog flag).
### Unknown function info

- [sceWebBrowserDialogGetEvent, sceWebBrowserDialogNavigate, sceWebBrowserDialogSetZoom](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) (libSceWebBrowserDialog) - their names and NIDs are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceWebBrowserDialog.c); the no-parameter prototypes follow [shadPS4 `webbrowserdialog.cpp`](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/web_browser_dialog/webbrowserdialog.cpp#L23-L85) and are not measured on a console. They are declared without parameters solely to resolve known imports; this is not an ABI claim, and every call throws.

### Additional callable exports

- [Additional callable export](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp): `8r4EJ3FiX4w#H#A` is a function record in `libSceWebBrowserDialog.sprx` from images labelled 3.20 and 13.60. The dynamic metadata maps library ID 7 to `libSceWebBrowserDialogLimited` and module ID 0 to `libSceWebBrowserDialog`. The labels do not authenticate retail firmware provenance. The name, parameter list, return type and behavior are unverified; this import-resolution placeholder throws through `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `Unknown` | `8r4EJ3FiX4w` |
