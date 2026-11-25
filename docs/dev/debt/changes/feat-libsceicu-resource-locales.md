# libSceIcu

## add Functional

- [Resource bundle locale getters](../../../core/libs/prx/libSceIcu/ResourceBundle.cpp) use the ICU 68.2 resource data entry to return the borrowed actual locale name. [Resource package locale enumeration](../../../core/libs/prx/libSceIcu/AvailableLocales.cpp) uses the bundled ICU data loader and adapts the returned enumeration to the guest callbacks. The guest layout and console resource package loading remain unverified on PS5.
