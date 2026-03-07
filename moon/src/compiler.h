#ifndef MOON_COMPILER_H
#define MOON_COMPILER_H

#include "object.h"

// Compiles the source string into a top-level function (the "main" script).
// Returns NULL if any syntax or compilation errors occurred.
ObjFunction *compile(const char *source);

#endif
