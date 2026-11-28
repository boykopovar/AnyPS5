# libSceWebBrowserDialog

## add Silent stubs

- [sceWebBrowserDialogGetEvent](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) (libSceWebBrowserDialog) - no browser runs, so no event ever arrives. Each open clears the event record and only the browser fills it, so an open dialog copies out an all-zero event. What the browser writes into an event (16 bytes at `+0x0c`, two strings) is unknown. The prototype, checks and their order follow firmware 9.00's `libSceWebBrowserDialog.sprx` (SHA-256 `db1b8dfc…9016`); 13.42 is not read.

## change import-resolution placeholder

- [Additional callable export](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp): `8r4EJ3FiX4w#H#A` is a function record in `libSceWebBrowserDialog.sprx` from images labelled 3.20 and 13.60. The dynamic metadata maps library ID 7 to `libSceWebBrowserDialogLimited` and module ID 0 to `libSceWebBrowserDialog`. The labels do not authenticate retail firmware provenance. In firmware 9.00 it has the same body as `sceWebBrowserDialogOpen`: same open parameter, checks and state. The one difference is that it passes launch mode 2 to the browser, where `Open` passes 0 and `OpenForPredeterminedContent` 1. AnyPS5 runs it as `sceWebBrowserDialogOpen`. Its name, and what mode 2 changes in the browser, are unknown.

## change sceWebBrowserDialogGetResult, sceWebBrowserDialogSetCookie

- [sceWebBrowserDialogGetResult, sceWebBrowserDialogSetCookie, sceWebBrowserDialogResetCookie, sceWebBrowserDialogOpen, sceWebBrowserDialogOpenForPredeterminedContent, sceWebBrowserDialogGetEvent, sceWebBrowserDialogNavigate, sceWebBrowserDialogSetZoom](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) (libSceWebBrowserDialog) - returning `COMMON_DIALOG_ERROR_ARG_NULL` (`0x80B8000D`) or `COMMON_DIALOG_ERROR_PARAMETER` (`0x80B8000A`) for unwritable or unreadable guest pointers is a host-safety fallback; console behaviour for unmapped memory is unmeasured. Closes when: measured on a PS5

## remove PS5 3.20 export inventory

## change state-first check order

- [sceWebBrowserDialogNavigate, sceWebBrowserDialogSetZoom](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) (libSceWebBrowserDialog) - no browser runs to take a request. Both run firmware 9.00's parameter checks and then return `0x80B80003` unless a dialog is open; a request on an open dialog throws through `NotImplemented_nid_no_patch`. On the console the request goes into a single slot that the browser empties, and a second request while one is pending returns `0x80B80006`. What the zoom `type` values mean is unknown.
