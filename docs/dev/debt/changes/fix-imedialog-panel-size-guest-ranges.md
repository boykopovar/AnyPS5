# libSceImeDialog

## change [sceImeDialogGetPanelPositionAndForm, sceImeDialogGetPanelSize]

- [sceImeDialogGetPanelPositionAndForm, sceImeDialogGetPanelSize](../../../core/libs/prx/libSceImeDialog/Export.cpp) (libSceImeDialog) - a null or unmapped form, width or height pointer returns `0x80bc0031` without writing; GetPanelSize checks both output ranges for readability and writability before either write and restores the saved width if the height write fails. A mapping change after the checks can still cause a partial write. A form region spanning an unmapped boundary may be written partially: process_vm_writev gives no atomicity for the 28-byte copy. GetPanelPositionAndForm takes the dialog mutex and returns `0x80bc0002` for StatusNone, treating Running or Finished as open like [sceImeGetPanelPositionAndForm](../../../core/libs/prx/libSceIme/Export.cpp) while its session persists; neither fallback is measured on a PS5. Closes when: measured on a PS5
