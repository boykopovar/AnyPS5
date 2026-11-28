# libSceCesCs

## change [MBCS string conversions]

- [MBCS string conversions](../../../core/libs/prx/libSceCesCs/Export.cpp) (libSceCesCs) - zero-filled contexts remain unverified outside the measured `sceCesMbcsStrToUtf8Str` case with valid buffers and room for a terminator. The UCS-2 forms and other profiles have limited coverage. The MBCS string conversions and their length queries follow a PS5 payload process (firmware 13.60) with Cp1252 and EUC-JP. They clear the lengths and write a terminator before checking the context, source and destination, in that order. A source limit of 0 reads up to the terminator. The output reserves space for a terminator, and errors preserve the converted text, counts and terminator. UTF-8 decoding follows `sceCesUtf8ToSbc`, and a UTF-16 surrogate returns `0x805C0015`.
