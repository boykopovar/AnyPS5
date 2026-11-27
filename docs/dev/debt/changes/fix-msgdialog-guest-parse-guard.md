# libSceMsgDialog

## change [sceMsgDialogOpen](../../../core/libs/prx/libSceMsgDialog/Export.cpp)

- [sceMsgDialogOpen](../../../core/libs/prx/libSceMsgDialog/Export.cpp) (libSceMsgDialog) - the `SceMsgDialogParam` layout it reads is not measured on a PS5: the mode at 0x38 (1 user message, 2 progress bar, 3 system message), the user message parameter pointer at 0x40, and in that parameter the button type at 0 and the message pointer at 8. A mode outside 1 to 3, or a parameter that cannot be read, is treated as a user message. The errors `0x80b80003` (not initialized), `0x80b80007` (busy), `0x80b8000b` (not running) and `0x80b8000d` (null argument) are not measured either. PPSA21564 01.007.000 (at 0x1D8C) and PPSA30839 01.003.000 (0x19690), disassembled for this entry, write this layout: 0x88 at 0x30, mode 1 and the parameter at 0x40 with the message at 8, and PPSA21564 the button type at 0. Closes when: measured on a PS5
