/**
 * XPU - src/backend/backend_opengl_core.cpp
 *
 * Desktop OpenGL 4.5 backend stub.
 */

#include "backend.h"

#ifndef XPU_HAS_OPENGL_CORE
extern "C" int xpu_backend_opengl_core_probe(xpu_backend_vtable*, void**) { return 0; }
#else

#include <GL/gl.h>
#include <cstring>

extern "C" int xpu_backend_opengl_core_probe(xpu_backend_vtable* vt, void** state) {
    std::memset(vt, 0, sizeof(*vt));
    *state = nullptr;
    return 0;
}

#endif
