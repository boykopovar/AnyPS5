# relinker

## change [code signature writer]

- The ad-hoc code signature of macOS output ([code signature writer](../../../core/relinker/elfpatcher/src/macos/MacOsCodeSignature.cpp)) has been verified only under Rosetta, on an M1 running macOS 27.0.1 and an M4 Pro running macOS 26.6.2 (`codesign --verify --strict`, launch and `dlopen`). Intel Macs and macOS releases before 26 have not run it. It is not marked linker-signed, so `install_name_tool` or `strip` leave it invalid until the file is signed again.
