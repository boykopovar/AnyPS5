#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceFaceAgeRangeEstimate();
int APS5_VABI sceFaceAgeRangeIntegrate();
int APS5_VABI sceFaceAllParts();
int APS5_VABI sceFaceAllPartsGetWorkingMemorySize();
int APS5_VABI sceFaceAttribute();
int APS5_VABI sceFaceAttributeGetWorkingMemorySize();
int APS5_VABI sceFaceAgeGetWorkingMemorySize();
int APS5_VABI sceFaceDetection();
int APS5_VABI sceFaceDetectionEx();
int APS5_VABI sceFaceDetectionGetDefaultParam(void *param);
int APS5_VABI sceFaceDetectionGetWorkingMemorySize();
int APS5_VABI sceFaceDetectionLocal();
int APS5_VABI sceFaceEstimatePoseRegion();
int APS5_VABI sceFaceIdentifyExGetFeature();
int APS5_VABI sceFaceIdentifyExGetWorkingMemorySize();
int APS5_VABI sceFaceIdentifyExSimilarity();
int APS5_VABI sceFaceIdentifyGetFeature();
int APS5_VABI sceFaceIdentifyGetWorkingMemorySize();
int APS5_VABI sceFaceIdentifyLiteGetFeature();
int APS5_VABI sceFaceIdentifyLiteGetWorkingMemorySize();
int APS5_VABI sceFaceIdentifyLiteSimilarity();
int APS5_VABI sceFaceIdentifySimilarity();
int APS5_VABI sceFaceParts();
int APS5_VABI sceFacePartsEx();
int APS5_VABI sceFacePartsGetWorkingMemorySize();
int APS5_VABI sceFacePartsResultCheck();
int APS5_VABI sceFaceShapeFit();
int APS5_VABI sceFaceShapeGetWorkingMemorySize();
int APS5_VABI sceFaceShapeTrack();

}
