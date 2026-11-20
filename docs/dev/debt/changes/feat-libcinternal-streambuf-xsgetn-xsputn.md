# libSceLibcInternal

## remove _ZNSt15basic_streambufIcSt11char_traitsIcEE6xsgetnEPci

## remove _ZNSt15basic_streambufIcSt11char_traitsIcEE6xsputnEPKci

## remove _ZNSt15basic_streambufIwSt11char_traitsIwEE6xsgetnEPwi

## remove _ZNSt15basic_streambufIwSt11char_traitsIwEE6xsputnEPKwi

## add Functional

- [basic_streambuf<char> and basic_streambuf<wchar_t> xsgetn and xsputn](../../../core/libs/prx/libSceLibcInternal/StreambufTransfer.cpp) (libSceLibcInternal, NIDs `VpwymQiS4ck`, `sXaxl1QGorg`, `lZVehk7yFok`, `041c37QfoUc`) follow Microsoft STL's `basic_streambuf` and are not verified on a console: they copy what the get or put area holds, then move one element at a time through the virtual `uflow` (slot 8) or `overflow` (slot 4) until it returns -1, and return the count moved; a count of 0 or less moves nothing. The count is an `int`, as the mangled names give. The get and put areas are the indirect next and end pointers `time_get` and `time_put` already use (0x38 and 0x58, 0x40 and 0x60), advanced without a separate count; end-of-file is -1 for the 16-bit wide form too, as in the library's other wide stream code. A null stream buffer, or a null element range with a positive count, throws.
