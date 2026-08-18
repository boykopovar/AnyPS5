# About

Converter for native execution of PlayStation 5 ELF binaries on Linux through binary format conversion and ABI compatibility. The implementation uses only the C++20 standard library and performs deterministic binary transformation without heuristics or interpretation.

## Status

Output ELF is accepted by the Linux kernel and handed to ld.so. Crashes during ld.so startup:
e_entry contains a file offset instead of a virtual address, causing an invalid jump at loader handoff.

## Disclaimer

This project is intended for interoperability, research, preservation, and compatibility purposes. It does not include, distribute, or require copyrighted software, firmware, cryptographic keys, or proprietary libraries. Users are responsible for ensuring that any binaries used with this project are obtained and used in accordance with applicable laws and their respective license terms.

## License

This project is licensed under the GNU General Public License version 2 only.
