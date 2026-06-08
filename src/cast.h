// cast.h
#ifndef moon_cast_h
#define moon_cast_h

#include "object.h"
#include "value.h"

// The universal router for all type conversions
// Returns true on success, false on failure (errors are reported internally)
bool executeCast(Value val, ObjType *targetType, Value *result);

#endif
