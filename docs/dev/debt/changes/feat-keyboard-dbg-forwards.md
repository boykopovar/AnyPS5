# libSceKeyboard

## remove [sceDbgKeyboardReadState]

## remove [sceDbgKeyboardGetHandle]

## remove [sceDbgKeyboardGetKey2Char and sceDbgKeyboardRead]

## add Functional

- [sceDbgKeyboardRead, sceDbgKeyboardReadState, sceDbgKeyboardGetHandle and sceDbgKeyboardGetKey2Char](../../../core/libs/prx/libSceKeyboard/Export.cpp) (libSceKeyboard) - forward to their retail names, so they share their state and gaps. In firmware 13.42's `libSceKeyboard.sprx` ([reading](https://github.com/boykopovar/AnyPS5/issues/21849#issuecomment-6939482475), disassembly only, not run on a console) `GetHandle` and `GetKey2Char` are instruction-for-instruction copies of the retail exports, and `Read` and `ReadState` run the retail path with a flag that only leaves bit 31 clear in the count of the keyboard driver `ioctl` request, where the retail call sets it. Nothing here sends that request, and what bit 31 tells the driver isn't known
