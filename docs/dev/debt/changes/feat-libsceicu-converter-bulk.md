# libSceIcu

## add Functional

- [Converter bulk conversion](../../../core/libs/prx/libSceIcu/ConverterAccessors.cpp) (libSceIcu) - `ucnv_fromUnicode` and `ucnv_toUnicode` forward to the host ICU conversion loop through 4096-byte and 4096-unit staging windows, looping internally until the source is consumed or the guest target is full, so the returned status, the partial prefix and the cursor moves are ICU's own; a target that fills reports `U_BUFFER_OVERFLOW` after the bytes or units it accepted. A non-null `offsets` array reports `U_ILLEGAL_ARGUMENT_ERROR` instead of guessing the console layout, so callers that ask for per-character offsets must count instead. The converter keeps its host state, so a partial character pending at the end of one call resumes in the next.
