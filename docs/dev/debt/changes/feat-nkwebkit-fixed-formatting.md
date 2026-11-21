# libSceNKWebKit

## change [DoubleToStringConverter exponential formatting]

- [DoubleToStringConverter exponential formatting](../../../core/libs/prx/libSceNKWebKit/src/DoubleToStringConverter.cpp) (libSceNKWebKit) - console ABI and rounding are unmeasured. Assumes the 48-byte [DoubleToStringConverter](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/dtoa/double-conversion.h#L166-L176) and 24-byte `StringBuilder` layouts (`static_assert`); formats `ToExponential` via `std::to_chars` (`scientific`), rounding exact decimal ties half up to match `double-conversion` instead of WebKit's FastDtoa/BignumDtoa. `CSSConverter`, `ToShortest`, `ToFixedUncapped` and `ToPrecision` remain unimplemented. Null objects, invalid builders or overflowing buffers throw instead of faulting or overwriting memory. Closes when: measured on console.

## add Unknown function info

- [DoubleToStringConverter fixed formatting](../../../core/libs/prx/libSceNKWebKit/src/DoubleToFixed.cpp) (libSceNKWebKit) - console exports, ABI and behavior are unmeasured. `ToFixed` follows the pinned [WebKit source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/dtoa/double-conversion.cc), using the existing 48-byte converter and 24-byte builder layouts. Fixed formatting delegates to `NumberToFixedWidthString`, which uses `std::to_chars` and decimal half-up rounding instead of FastFixedDtoa/BignumDtoa. Negative precision throws where WebKit asserts; inaccessible objects, symbols and output buffers throw. Invalid builder state and insufficient capacity throw before writing instead of asserting or partially writing. Closes when: compared with a console module.
