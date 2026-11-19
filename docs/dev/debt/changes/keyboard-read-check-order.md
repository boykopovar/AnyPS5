# libSceKeyboard

## add Functional

- [sceKeyboardRead and sceKeyboardReadState argument precedence](../../../core/libs/prx/libSceKeyboard/src/keyboard_impl.cpp) - firmware 13.42 [reading](https://github.com/boykopovar/AnyPS5/issues/21849#issuecomment-6939482475) shows Read checks initialization, then its data pointer and count, then the handle; ReadState checks its data pointer before entering the shared Read path. The host model follows this order for error returns. The driver ioctl and ReadState's writes before a bad-handle result remain unmodeled.
