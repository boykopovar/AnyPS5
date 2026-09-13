# About

Converter for native execution of PlayStation 5 ELF binaries on Linux and Windows through binary format conversion and ABI compatibility. The relinker implementation uses only the C++20 standard library and performs deterministic transformation of executable binary.

Implementations of system prx libraries suitable for dynamic linking: [core/libs/prx](core/libs/prx)

Releases will be published after the first full successful launch of at least one game.

## Status

Execution reaches `_start`, stack unwinding and exception handling tables are built. All unimplemented functions throw std::runtime_error. `what()` is printed to stderr and the process terminates.
Shader initialization via `sceAgcCreate*` passes.
Audio output and video output initialization pass

Now: `sceAgcDcbResetQueue not implemented`.

On Windows, after printing an unhandled exception the process exits with STATUS_STACK_BUFFER_OVERRUN due to the difficulty of manually [implementing proper exception handling](core/libs/prx/libc/src/exception).

## Disclaimer

This project is intended for interoperability, research, preservation, and compatibility purposes. It does not include, distribute, or require copyrighted software, firmware, cryptographic keys, or proprietary libraries. Users are responsible for ensuring that any binaries used with this project are obtained and used in accordance with applicable laws and their respective license terms.

## License

This project is licensed under the GNU General Public License version 2 only.
