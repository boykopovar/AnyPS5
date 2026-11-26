# relinker

## change [Needed library names]

- [Needed library names](../../../core/relinker/domain/include/domain/ImportModule.hpp) - whether a console's `libSceGnmDriver_padebug` module exports or does anything that `libSceGnmDriver` doesn't is not known: a `libSceGnmDriver_padebug` dependency, with any module suffix, is loaded as `libSceGnmDriver.prx`, and imports from module `libSceGnmDriver` match it. A `libSceSsl2` dependency is loaded as `libSceSsl.prx` in the same way: `libSceSsl2.sprx` of PS4 firmware 13.52 names both its module and its export library `libSceSsl`, with 57 exported symbols where that firmware's `libSceSsl.sprx` has 208. This is read from the dynamic tables of the two modules, not run; whether the host `libSceSsl` answers as either module does is not checked, and no PS5 module of that name was read.
