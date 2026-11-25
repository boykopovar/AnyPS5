# TechnicalDebt

## change [Quick syntax checks](../../tools/check_compile.py) do not model every compiler option's operand count.

- [Quick syntax checks](../../tools/check_compile.py) model known one-value compiler options and the common `ccache`, `sccache` and `distcc` wrapper prefixes. Safe forwarded `-Xclang` and `-Xpreprocessor` macro, include and diagnostic controls are preserved while actions and output options are rejected; `-Wp,` forwards recognized macro and include operands. Options and wrapper grammars outside these tables remain rejected rather than guessed, and response-file tokens remain rejected as operands.

## change [Quick syntax checks](../../tools/check_compile.py) reject `-save-temps=`

- [Quick syntax checks](../../tools/check_compile.py) drop every `-save-temps=` and `--save-temps=` value under `-fsyntax-only`, whatever the value. Save-temps is output-only, GCC rejects values it does not know, and newer Clang treats any value as an output prefix and writes preprocessed files even under `-fsyntax-only`; no value is passed to the compiler.
