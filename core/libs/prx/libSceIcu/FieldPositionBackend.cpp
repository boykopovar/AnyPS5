// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 1997-2012, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File FORMAT.CPP
*
* Modification History:
*
*   Date        Name        Description
*   02/19/97    aliu        Converted from java.
*   03/17/97    clhuang     Implemented with new APIs.
*   03/27/97    helena      Updated to pass the simple test after code review.
*   07/20/98    stephen        Added explicit init values for Field/ParsePosition
********************************************************************************
*/

#include "unicode/fieldpos.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FieldPosition)

FieldPosition::~FieldPosition() {}

FieldPosition *
FieldPosition::clone() const {
    return new FieldPosition(*this);
}

U_NAMESPACE_END
