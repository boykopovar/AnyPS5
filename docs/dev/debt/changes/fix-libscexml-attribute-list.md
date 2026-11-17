# libSceXml

## change [AttributeList and Attr]

- [AttributeList and Attr](../../../core/libs/prx/libSceXml/src/AttributeList.cpp) (libSceXml) - not compared with the console. Follows `libSceXml.sprx` 13.60 from disassembly and matched it under x86-64 emulation on about 6200 call sequences over the 24 exports, including results, strings and allocations. The 0x20 and 0x28 byte blocks come from the initializer's allocator in the module's order, but hold a host pointer, and names and values live on the host, not in the module's pool. Not modelled: the copy the module makes in a PS4 process with an SDK below 0x8000000, and its `0x80850001` with a terminator written past the buffer when its pool cannot grow to a string's size. Strings are read a page at a time, so a size past the readable memory throws; `addAttribute` reads the value to its terminator like the module's `strlen`, bounded only by readable guest memory. Closes when: compared with the console.

## remove [AttributeList and Attr unmeasured results]

## change [AttributeList and Attr string lifetime]

- [AttributeList and Attr string lifetime](../../../core/libs/prx/libSceXml/src/AttributeList.cpp) (libSceXml) - a `XmlString` from `getName` or `getValue` points into the `Attr`'s host storage and stays valid until the next `setName`, `setValue` or assignment to that `Attr`, or its destruction. The module's buffer can move at those points too; how long a title can keep the pointer is not measured.

## add Functional

- [AttributeList and Attr faults](../../../core/libs/prx/libSceXml/src/AttributeList.cpp) (libSceXml) - throw where the module faults: a failed table allocation in `initialize`, copying a list that is not initialized or has no table, an index past the length, a string that runs into unreadable memory, and freeing a list's table or an `Attr` after its `Initializer` was terminated. Copying the list a `Sax::Parser` hands to `startElement` also throws: the module's copy shares the parser's table and later frees it through the title's allocator. Closes when: the module's faults are reproduced.
