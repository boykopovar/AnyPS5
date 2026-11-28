# libSceLibcInternal

## remove _ZSt7_FiopenPKwNSt5_IosbIiE9_OpenmodeEi

## add Functional

- [std::_Fiopen(const wchar_t*, ios_base::openmode, int)](../../../core/libs/prx/libSceLibcInternal/Fiopen.cpp) (libSceLibcInternal, NID `aqyjhIx7jaY`) follows Dinkumware V6.50's [`fiopen.cpp`](https://github.com/bergsteiger/realwork/blob/19b446ce8ad2adf82ab8ce7988bc003221accad2/common/components/rtl/external/Embarcadero/DelphiBerlin/cpprtl/Source/dinkumware64/source/fiopen.cpp) and is not verified on a console: it converts the 16-bit name with this library's `wcstombs` into a `FILENAME_MAX` (1024-byte) buffer, returns null when the conversion fails, and otherwise opens the converted name through the narrow `_Fiopen`, so its mode table and limits apply. A name whose conversion fills the buffer throws, where Dinkumware would pass an unterminated name on, and a null name throws. This `wcstombs` encodes every 16-bit unit (lone surrogates included), so its failure path is not reached here.
