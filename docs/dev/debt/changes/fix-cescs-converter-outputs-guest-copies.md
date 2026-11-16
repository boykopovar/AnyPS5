# libSceCesCs

## change [Converter helpers]

- [Converter helpers](../../../core/libs/prx/libSceCesCs/Export.cpp) still touch guest memory in place in the string converters and the profile checks:
  - the string converters (`convertString`, `toIso2022`, `fromIso2022`) read their sources and write their outputs per character;
  - `hasSheetTag` reads the caller's profile sheet;
  - `contextProfile` and `setContextReplacementEntry` read and write the caller's context.

  Exports that reach those (for example `sceCesUtf8StrToMbcsStr`) still crash the host on an unmapped buffer instead of returning an error. `tools/guest_pointer_audit.py` does not see these accesses, because they are not on the export's own parameters. The single-character helpers (`mbcsToUtf8`, `profiledToUcs2`, `bufferToProfiled`, `utf16ToSbc` and the others) and `sceCesEucJpToUcs2`, `sceCesEucJpToUtf8` and `sceCesUtf8ToSbc` read their sources and write their outputs through `CesGuest`. They return `CES_ERROR_INVALID_SRC_BUFFER` or `CES_ERROR_INVALID_DST_BUFFER` for a buffer that cannot be read or written.
