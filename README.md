# About

Converter for native execution of PlayStation 5 ELF binaries on Linux through binary format conversion and ABI compatibility. The implementation uses only the C++20 standard library and performs deterministic binary transformation without heuristics or interpretation.

## Status

Loader startup works: execution reaches `_start` and runs normally up to the first call into an external library function (from test `.prx` stubs).

## Disclaimer

This project is intended for interoperability, research, preservation, and compatibility purposes. It does not include, distribute, or require copyrighted software, firmware, cryptographic keys, or proprietary libraries. Users are responsible for ensuring that any binaries used with this project are obtained and used in accordance with applicable laws and their respective license terms.

## License

This project is licensed under the GNU General Public License version 2 only.
