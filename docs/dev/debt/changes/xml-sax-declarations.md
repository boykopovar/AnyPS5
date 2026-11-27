# libSceNKWebKitRequirements

## add Functional

- [XML push-parser SAX handlers](../../../core/libs/prx/libSceNKWebKitRequirements/Xml.cpp) use the SAX1 or SAX2 handler size selected by the caller, following [libxml2 2.9.14](https://github.com/GNOME/libxml2/blob/7846b0a677f8d3ce72486125fa281e92ac9970e8/parser.c#L12463-L12482). The four DTD declaration callbacks receive libxml2's element-content and enumeration records directly where the SAX signature includes them. Their ABI and ownership on the console are unmeasured. The guest handler's document locator and external entity resolver are still unused.
