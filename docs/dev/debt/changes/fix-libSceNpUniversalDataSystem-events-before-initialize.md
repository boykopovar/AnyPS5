# libSceNpUniversalDataSystem

## change [Event checks]

- [Event checks](../../../core/libs/prx/libSceNpUniversalDataSystem/src/Event.cpp): the event and property functions (creating, setting, destroying, sizing and describing) return 0x80553117 before `sceNpUniversalDataSystemInitialize`, even for null arguments, as the 13.42 module does, and also after `sceNpUniversalDataSystemTerminate`, whose effect that run did not cover, so an event, property object or property array created before `Terminate` cannot be destroyed until the next `Initialize`, and stays allocated if there is none; `sceNpUniversalDataSystemPostEvent` was not covered and does not check. The module's `sceNpUniversalDataSystemCreateEvent` also compares packed sizes with 0x5000 bytes when `prop` is given (read from its disassembly, not measured); this one does not. `sceNpUniversalDataSystemEventToString` writes NaN and infinite properties as `std::to_chars` does; the console's text for them is not measured.
