# libSceXml

## change [AttributeList and Attr]

- [AttributeList and Attr](../../../core/libs/prx/libSceXml/src/AttributeList.cpp) (libSceXml) - follow `libSceXml.sprx` 13.60, read from disassembly and compared with that module run under x86-64 emulation on about 6200 call sequences over the 24 exports, with no difference in results, strings or allocations; no console comparison. Each object takes a 0x20 byte block from the initializer's allocator and a list also a 0x28 byte table, in the module's order; the blocks hold a host pointer instead of the module's layout, and the names and values the module keeps in its own pool are kept on the host. The copy the module makes in a PS4 process built with an SDK below 0x8000000 is not modelled. Strings are read from the guest a page at a time, so a size larger than the readable memory throws after reading what is readable; the module first grows its pool buffer to the given size, and when that fails it returns `0x80850001` after writing a terminator past the buffer, which is not modelled. `addAttribute` reads the value up to its terminator, as the module's `strlen` does, with no limit but the readable guest memory. Closes when: compared with the console.

## remove [AttributeList and Attr unmeasured results]

## change [AttributeList and Attr string lifetime]

- [AttributeList and Attr string lifetime](../../../core/libs/prx/libSceXml/src/AttributeList.cpp) (libSceXml) - a `XmlString` from `getName` or `getValue` points into the `Attr`'s host storage and stays valid until the next `setName`, `setValue` or assignment to that `Attr`, or its destruction. The module's buffer can move at those points too; how long a title can keep the pointer is not measured.

## add Functional

- [AttributeList and Attr faults](../../../core/libs/prx/libSceXml/src/AttributeList.cpp) (libSceXml) - throw where the module faults: a failed table allocation in `initialize`, copying a list that is not initialized or has no table, an index past the length, a string that runs into unreadable memory, and freeing a list's table or an `Attr` after its `Initializer` was terminated. Copying the list a `Sax::Parser` hands to `startElement` also throws: the module's copy shares the parser's table and later frees it through the title's allocator. Closes when: the module's faults are reproduced.
