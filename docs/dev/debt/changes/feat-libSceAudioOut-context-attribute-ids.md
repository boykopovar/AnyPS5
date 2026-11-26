# libSceAudioOut

## change [sceAudioOut2ContextSetAttributes]

- [sceAudioOut2ContextSetAttributes](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Context.cpp) (libSceAudioOut) - for a valid context, returns 0 for ids 1 and 2 and ignores them; what a context attribute changes on a console is unknown. As on a PS5 (firmware 11.60, payload process; a title process may differ), it refuses with 0x80268001 a null list with count 0 or 1, ids 0, 3, 4, 0x100 and 0xffffffff, and a null value or size 0 for id 2; the console accepted ids 1 and 2 only with a 4-byte value 1, so other values and sizes are accepted unmeasured. Which entries of a list the console applies before refusing one is not measured. Other ids throw, as they are not measured. The console answered 0 for context 0, which this library refuses. PPSA25286 01.000.568 sets attribute 2 to a 4-byte 1 at start-up, and PPSA02225 passes two attributes from an object at its single call.
