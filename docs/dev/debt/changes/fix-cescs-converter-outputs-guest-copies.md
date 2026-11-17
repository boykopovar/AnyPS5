# libSceCesCs

## change [Converter helpers]

- [Converter helpers](../../../core/libs/prx/libSceCesCs/Export.cpp) still touch guest memory in place for their inputs and the string converters. The single-character helpers (`mbcsToUtf8`, `profiledToUcs2`, `profiledToUtf8`, `profiledToUtf16`, `profiledToUtf32`, `encodeProfiled`, `scalarToProfiled`, `bufferToProfiled`, `sbcToUtf16`, `sbcToUtf32`, `scalarToSbc`, `utf16ToSbc` and `rejected`) now write their outputs and length outputs through `CesGuest` and return `CES_ERROR_INVALID_DST_BUFFER` for one that cannot be written, like the exports already converted. Still in place:
  - the input decoders (`decodeMbcs`, `decodeEucJp`, `decodeScalarUtf8`, `decodeUtf16` and `loadUnit` of a UTF-32 source) read the caller's source bytes directly;
  - the string converters (`convertString`, `toIso2022`, `fromIso2022`) write their outputs per character;
  - `contextProfile` and `setContextReplacementEntry` read and write the caller's context.

  Exports that reach those (for example `sceCesBig5ToUtf8` with an unmapped source, or `sceCesUtf8StrToMbcsStr`) still crash the host on an unmapped buffer instead of returning an error. `tools/guest_pointer_audit.py` does not see these accesses, because they are not on the export's own parameters.
