#pragma once
typedef int GLint;
#define GL_VIEWPORT 0x0BA2
inline void glGetIntegerv(unsigned int, GLint* v) { v[0] = v[1] = 0; v[2] = 1920; v[3] = 1080; }
