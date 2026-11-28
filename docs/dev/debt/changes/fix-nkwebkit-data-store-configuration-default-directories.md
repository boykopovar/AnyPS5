# libSceNKWebKit

## add Functional

- [WKWebsiteDataStoreConfiguration default directories](../../../core/libs/prx/libSceNKWebKit/src/Configuration.hpp) (libSceNKWebKit) - not compared with a console module. A new configuration starts with the nine directories that [initializePaths](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/WebsiteData/WebsiteDataStoreConfiguration.cpp#L97-L122) sets for null base directories: the PlayStation name of each one appended to `/app0` ([WebsiteDataStorePlayStation.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/WebsiteData/playstation/WebsiteDataStorePlayStation.cpp#L44-L56)), such as `/app0/applications` and `/app0/WebKitCache`. `/app0` and the names are read from the source, not measured on a console; the directories are neither created nor used. Closes when: compared with a console module.
