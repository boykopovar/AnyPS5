# libSceNKWebKit

## add Functional

- [Icon database management and URL mappings](../../../core/libs/prx/libSceNKWebKit/src/IconDatabase.cpp) (libSceNKWebKit) - not checked on a console. WKContextGetIconDatabase lazily returns the process pool's WKIconDatabase instance, WKContextSetIconDatabasePath updates the database directory path, and WKIconDatabase accessors record page-to-icon URL mappings, icon image data, retain counts, cleanup/integrity flags, and client callback pointer in memory. Closes when: verified against a title exercising icon database queries on PS5.
