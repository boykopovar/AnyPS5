# libSceVdecwrap technical debt

### Unknown function info

- [sceVdecwrapCreateDecoder, sceVdecwrapDeleteDecoder, sceVdecwrapFlushDecodeOutput, sceVdecwrapGetDecodeOutput, sceVdecwrapMapDirectMemory, sceVdecwrapMapMemory, sceVdecwrapQueryDecoderMemoryInfo, sceVdecwrapQueryFrameBufferInfo, sceVdecwrapResetDecoder, sceVdecwrapSetDecodeInput and sceVdecwrapSyncDecode](../../../core/libs/prx/libSceVdecwrap/Export.cpp) (libSceVdecwrap) - every export throws after its argument checks: what the decoder calls behind them do, the results they write and the arguments after the checked ones are unknown. The checks follow firmware 13.42's `libSceVdecwrap.sprx` (read from disassembly, not run on a console); the `printf` line the module prints on each failed check is not reproduced. A block whose checked fields cannot be read throws where the console faults.
