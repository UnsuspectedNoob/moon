#ifndef MOON_COMPILER_H
#define MOON_COMPILER_H

#include "object.h"
#include "vm.h"

ObjFunction *compile(const char *source);

#endif
