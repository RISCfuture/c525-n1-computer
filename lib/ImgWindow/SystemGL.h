
#if _MSC_VER                    // compiling via MS Visual Studio
#include <windows.h>            // need to make sure this is read first
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#elif defined(_WIN32)
// Windows ships the OpenGL 1.1 headers and no glext.h. This code is
// fixed-function 1.1 throughout apart from one 1.2 constant, so that constant
// is defined here rather than vendoring the Khronos extension header.
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif


