// subscript.h
#ifndef moon_subscript_h
#define moon_subscript_h

#include "value.h"

// Returns true on success, false on failure (errors are reported internally)
bool executeGetSubscript(Value seqVal, Value indexVal, Value *result);
bool executeSetSubscript(Value collectionVal, Value indexVal, Value value,
                         Value *result);

#endif
