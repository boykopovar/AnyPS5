struct CairoSurface;
struct CairoContext;
union CairoPathData {
    struct { int type, length; } header;
    struct { double x, y; } point;
};
struct CairoPath { int status; CairoPathData* data; int num_data; };

extern "C" {
CairoSurface* cairo_image_surface_create(int, int, int);
void cairo_surface_destroy(CairoSurface*);
CairoContext* cairo_create(CairoSurface*);
void cairo_destroy(CairoContext*);
void cairo_scale(CairoContext*, double, double);
void cairo_arc(CairoContext*, double, double, double, double, double);
void cairo_arc_negative(CairoContext*, double, double, double, double, double);
CairoPath* cairo_copy_path(CairoContext*);
void cairo_path_destroy(CairoPath*);
}
