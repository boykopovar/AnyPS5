#pragma once

struct CairoSurface;

struct CairoPngView {
    const unsigned char* data;
    int format;
    int sourceWidth;
    int sourceHeight;
    int stride;
    double x;
    double y;
    double width;
    double height;
    bool subsurface;
};

CairoPngView CairoPngSource_nid_no_patch(CairoSurface* surface);
