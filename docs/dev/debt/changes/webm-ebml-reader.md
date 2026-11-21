# libSceWebmParserMdrw

## add Functional

- [EBML reader](../../../core/libs/prx/libSceWebmParserMdrw/src/EbmlReader.cpp) is a host-only foundation, not connected to the unidentified guest exports. It reads element headers with IDs up to four bytes and sizes up to eight, bounded payloads and numeric values using [RFC 8794 sections 4–7](https://www.rfc-editor.org/rfc/rfc8794.html#section-4), with ID `0x80` permitted by [RFC 9559 section 4.2](https://www.rfc-editor.org/rfc/rfc9559.html#section-4.2). Unknown sizes remain unresolved; schema validation, document limits and defaults, master traversal, strings, metadata extraction and the console API contract remain unimplemented. `webm_ebml` tests format primitives only. Closes when: the module contract is established and a WebM metadata reader is connected and tested through guest exports.
