# libSceWebBrowserDialog

## change [sceWebBrowserDialogGetResult, sceWebBrowserDialogSetCookie, sceWebBrowserDialogResetCookie, sceWebBrowserDialogOpen, sceWebBrowserDialogOpenForPredeterminedContent]

- [sceWebBrowserDialogGetResult, sceWebBrowserDialogSetCookie, sceWebBrowserDialogResetCookie, sceWebBrowserDialogOpen, sceWebBrowserDialogOpenForPredeterminedContent](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) (libSceWebBrowserDialog) - returning `COMMON_DIALOG_ERROR_ARG_NULL` (`0x80B8000D`) or `COMMON_DIALOG_ERROR_PARAMETER` (`0x80B8000A`) for unwritable or unreadable guest pointers via safe copy helpers is a host-safety fallback; console behaviour for unmapped memory is unmeasured. Closes when: measured on a PS5
