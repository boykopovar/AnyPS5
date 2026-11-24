# libSceNKWebKit

## add Functional

- [AccessibilityObject::isARIAInput role numbers](../../../core/libs/prx/libSceNKWebKit/src/AccessibilityObjectRoles.cpp) (libSceNKWebKit) - not compared with a console module. `isARIAInput` is in the 3.20 inventory only, so it tests the role numbers of that module's WebKit, [AccessibilityObjectInterface.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/accessibility/AccessibilityObjectInterface.h#L81): CheckBox 20, RadioButton 96, SearchField 110, Switch 122, TextField 136. That 151-value enum is the same on 2020-06-13 and 2020-11-12, the window the 3.20 names allow. `isARIAControl` and `ariaRoleToWebCoreRole`, which the later inventory has, keep the later numbers. Closes when: compared with a console module.
