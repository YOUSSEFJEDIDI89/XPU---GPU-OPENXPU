/**
 * XPU - src/backend/backend_opengl_es.cpp
 *
 * OpenGL ES 3.0 backend stub. On platforms where OpenGL ES is available
 * (Android, iOS via EAGL, embedded Linux), this backend translates XPU
 * calls to GL ES calls. On platforms without GL ES, the probe function
 * returns 0 and the XPU core falls back to another backend.
 *
 * To enable: build with -DXPU_HAS_OPENGL_ES=1 and link against EGL+GLESv3.
 * When that macro is undefined, this file compiles to a probe that
 * always reports "unavailable".
 */

#include "backend.h"

#ifndef XPU_HAS_OPENGL_ES
extern "C" int xpu_backend_opengl_es_probe(xpu_backend_vtable*, void**) { return 0; }
#else

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstring>
#include <new>

/* Real implementation would go here. For now we keep this as a stub
 * that always succeeds the probe but does nothing - it lets users
 * build with -DXPU_HAS_OPENGL_ES=1 to test that the linkage works. */

extern "C" int xpu_backend_opengl_es_probe(xpu_backend_vtable* vt, void** state) {
    std::memset(vt, 0, sizeof(*vt));
    *state = nullptr;
    /* TODO: real GL ES implementation. For now we cannot probe successfully. */
    (void)vt; (void)state;
    return 0;
}

#endif
