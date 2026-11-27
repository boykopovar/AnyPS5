# libSceWebBrowserDialog

## add Silent stubs

- [sceWebBrowserDialogGetEvent, sceWebBrowserDialogNavigate, sceWebBrowserDialogSetZoom](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) (libSceWebBrowserDialog) - no browser runs, so no event arrives and no URL or zoom is applied. Each open clears the event record and the single request slot, and only the browser fills or empties them. So `GetEvent` copies out an all-zero event, and `Navigate` and `SetZoom` accept one request per open, then return `0x80B80006`. Prototypes, checks and their order follow firmware 9.00's `libSceWebBrowserDialog.sprx` (SHA-256 `db1b8dfc…9016`); 13.42 is not read. Unknown: what the browser writes into an event (16 bytes at `+0x0c`, two strings) and what the zoom `type` values mean.

## change import-resolution placeholder

- [Additional callable export](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp): `8r4EJ3FiX4w#H#A` is a function record in `libSceWebBrowserDialog.sprx` from images labelled 3.20 and 13.60. The dynamic metadata maps library ID 7 to `libSceWebBrowserDialogLimited` and module ID 0 to `libSceWebBrowserDialog`. The labels do not authenticate retail firmware provenance. In firmware 9.00 it has the same body as `sceWebBrowserDialogOpen`: same open parameter, checks and state. The one difference is that it passes launch mode 2 to the browser, where `Open` passes 0 and `OpenForPredeterminedContent` 1. AnyPS5 runs it as `sceWebBrowserDialogOpen`. Its name, and what mode 2 changes in the browser, are unknown.

## change sceWebBrowserDialogGetResult, sceWebBrowserDialogSetCookie

- [sceWebBrowserDialogGetResult, sceWebBrowserDialogSetCookie, sceWebBrowserDialogResetCookie, sceWebBrowserDialogOpen, sceWebBrowserDialogOpenForPredeterminedContent, sceWebBrowserDialogGetEvent, sceWebBrowserDialogNavigate, sceWebBrowserDialogSetZoom](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) (libSceWebBrowserDialog) - returning `COMMON_DIALOG_ERROR_ARG_NULL` (`0x80B8000D`) or `COMMON_DIALOG_ERROR_PARAMETER` (`0x80B8000A`) for unwritable or unreadable guest pointers is a host-safety fallback; console behaviour for unmapped memory is unmeasured. Closes when: measured on a PS5

## remove PS5 3.20 export inventory

## remove state-first check order
