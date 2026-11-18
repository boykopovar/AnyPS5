// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"

extern "C" {

char16_t APS5_VABI u_asciiToUpper_nid_postfix(char16_t value) {
    return value >= u'a' && value <= u'z' ? value - (u'a' - u'A') : value;
}

}
