# libSceAgcDriver

## change [Unexported pixel inputs]

- [Unexported pixel inputs](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderInputState.cpp) (libSceAgcDriver): a pixel input whose `SPI_PS_INPUT_CNTL` parameter the previous stage does not export is programmed as a default-value input (`OFFSET` bit 5, `DEFAULT_VAL` 0, FP16 high half too) and reads (0, 0, 0, 0), as radeonsi's `SI_PS_INPUT_CNTL_UNUSED` does. A rect-list draw builds its tessellation stages the same way for a pass-through input (`OFFSET` bit 5 with `FLAT_SHADE`) whose slot the vertex stage never exports, taking that slot's three vertices as zero, which makes PPSA28099 01.003 present (its rectangle pixel shader reads such an input at `setting` 0x421); a pass-through input of a slot the vertex stage does export still throws, because forwarding a slot's three vertices to the fragment stage is not implemented. The console's value for an unwritten parameter cache entry is not measured on hardware.
