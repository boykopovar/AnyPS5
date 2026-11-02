#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"

extern "C" {
const void* APS5_VABI WKGeolocationPositionCreate_c(
    double timestamp, double latitude, double longitude, double accuracy,
    bool providesAltitude, double altitude,
    bool providesAltitudeAccuracy, double altitudeAccuracy,
    bool providesHeading, double heading,
    bool providesSpeed, double speed,
    bool providesFloorLevel, double floorLevel) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::GeolocationPosition>(
        timestamp, latitude, longitude, accuracy,
        providesAltitude, altitude,
        providesAltitudeAccuracy, altitudeAccuracy,
        providesHeading, heading,
        providesSpeed, speed,
        providesFloorLevel, floorLevel));
}

const void* APS5_VABI WKGeolocationPositionCreate_b(
    double timestamp, double latitude, double longitude, double accuracy,
    bool providesAltitude, double altitude,
    bool providesAltitudeAccuracy, double altitudeAccuracy,
    bool providesHeading, double heading,
    bool providesSpeed, double speed) {
    return WKGeolocationPositionCreate_c(
        timestamp, latitude, longitude, accuracy,
        providesAltitude, altitude,
        providesAltitudeAccuracy, altitudeAccuracy,
        providesHeading, heading,
        providesSpeed, speed,
        false, 0.0);
}

const void* APS5_VABI WKGeolocationPositionCreate(
    double timestamp, double latitude, double longitude, double accuracy) {
    return WKGeolocationPositionCreate_c(
        timestamp, latitude, longitude, accuracy,
        false, 0.0,
        false, 0.0,
        false, 0.0,
        false, 0.0,
        false, 0.0);
}

}
