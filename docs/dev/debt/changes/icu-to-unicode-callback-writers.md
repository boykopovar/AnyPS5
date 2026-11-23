# libSceIcu

## add Functional

- [Unicode-output callback writers](../../../core/libs/prx/libSceIcu/ConverterToUnicodeWriters.cpp) have unverified console ABI and behavior. The two helpers use ICU 68.2's public 56-byte callback argument layout with the pinned ICU 78.1 backend's own converter objects; ICU 68 objects cannot be mixed with them. Checked staging preserves UTF-16 output, offsets and cursor movement, including source/target overlap. A buffered remainder exceeding the native converter's fixed 32-unit array throws before delegation; longer writes that fit the destination are supported. The legacy table-converter U+001A substitution variant has no runtime coverage. Closes when: compared with the console and its converter callbacks.
