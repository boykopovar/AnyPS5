# libSceNKWebKit

## add Unknown function info

- [SymbolTable constructors](../../../core/libs/prx/libSceNKWebKit/src/SymbolTableConstructor.cpp) - every store offset, width and value is read from the module disassembly, not a layout model; the module reads the structure through a pointer at VM+0x9b00 to a slot that holds the structure pointer, where the pinned [SymbolTable.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/SymbolTable.cpp) reads vm.symbolTableStructure directly, and the member names for the zeroed words are taken from the upstream member order, not from the module. C1 and C2 are the same code in the module. Null pointers throw where the module would fault. Closes when: the SymbolTable and VM layouts are modelled from more than this one constructor.

## remove | `_ZN3JSC11SymbolTableC1ERNS_2VME` | `bORoxqBlkmc` |

## remove | `_ZN3JSC11SymbolTableC2ERNS_2VME` | `6fjpoeCJVmg` |
