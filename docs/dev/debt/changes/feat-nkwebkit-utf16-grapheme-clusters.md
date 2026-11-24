# libSceNKWebKit

## add Functional

- [WTF grapheme cluster counting](../../../core/libs/prx/libSceNKWebKit/src/GraphemeClusters.cpp) has no console ABI or behavior verification. UTF-16 grapheme counting uses ICU character break iteration; 8-bit strings retain the CRLF grouping behavior. Null buffers throw, and console ICU Unicode version differences remain unverified. Closes when: compared with a console module.
