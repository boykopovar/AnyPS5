# libSceVideoOut

## change [sceVideoOutSubmitChangeBufferAttribute2]

- [sceVideoOutSubmitChangeBufferAttribute2](../../../core/libs/prx/libSceVideoOut/src/Buffer.cpp) (libSceVideoOut) - a set index outside 0 to 3 or a set that is not registered returns `VIDEO_OUT_ERROR_INVALID_INDEX`, an `option` other than null returns `VIDEO_OUT_ERROR_INVALID_OPTION` before the registration is checked, and a closed handle returns `VIDEO_OUT_ERROR_INVALID_HANDLE`, as a PS5 (firmware 11.60, a homebrew payload process, 2026-11-15) answered with no buffer set registered. A null attribute and a port that is closing still throw because the console's answers for them are not measured, and neither is the order of the checks when more than one argument is invalid. The 16 direct call sites read in 14 titles pass set 0 and a null option; PPSA04027 was not scanned and only direct calls were read.
