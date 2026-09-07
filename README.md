# About

Converter for native execution of PlayStation 5 ELF binaries on Linux through binary format conversion and ABI compatibility. The relinker implementation uses only the C++20 standard library and performs deterministic binary transformation.

## Status

Execution reaches `_start`, stack unwinding and exception handling tables are built. All unimplemented stub functions throw std::runtime_error. `what()` is printed to stderr and the process terminates.
Now: `sceUserServiceInitialize not implemented`

## Disclaimer

This project is intended for interoperability, research, preservation, and compatibility purposes. It does not include, distribute, or require copyrighted software, firmware, cryptographic keys, or proprietary libraries. Users are responsible for ensuring that any binaries used with this project are obtained and used in accordance with applicable laws and their respective license terms.

## License

This project is licensed under the GNU General Public License version 2 only.
