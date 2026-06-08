#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "lib_math.h"
#include "value.h"
#include "vm.h"

// --- THE PRIMITIVES ---

static Value sinNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL; // Arity Shield
  if (!IS_NUMBER(args[0]))
    return NIL_VAL; // Type Shield
  return NUMBER_VAL(sin(AS_NUMBER(args[0])));
}

static Value cosNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL;
  if (!IS_NUMBER(args[0]))
    return NIL_VAL;
  return NUMBER_VAL(cos(AS_NUMBER(args[0])));
}

static Value sqrtNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL;
  if (!IS_NUMBER(args[0]))
    return NIL_VAL;

  double num = AS_NUMBER(args[0]);
  if (num < 0)
    return NIL_VAL; // Prevent imaginary number crashes

  return NUMBER_VAL(sqrt(num));
}

static Value powNative(int argCount, Value *args) {
  if (argCount != 2)
    return NIL_VAL;
  if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
    return NIL_VAL;
  return NUMBER_VAL(pow(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

static Value randomNative(int argCount, Value *args) {
  if (argCount != 2)
    return NIL_VAL;
  if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
    return NIL_VAL;

  double min = AS_NUMBER(args[0]);
  double max = AS_NUMBER(args[1]);

  if (min > max)
    return NIL_VAL; // Sanity check

  // Generate a random double between min and max
  double scale = rand() / (double)RAND_MAX;
  return NUMBER_VAL(min + scale * (max - min));
}

static Value floorNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL; // Arity Shield
  if (!IS_NUMBER(args[0]))
    return NIL_VAL;

  return NUMBER_VAL(floor(AS_NUMBER(args[0])));
}

// --- THE HANDSHAKE ---

void registerMathLibrary() {
  // Seed the random number generator once during boot
  srand((unsigned int)time(NULL) * time(NULL));

  REGISTER_PHRASE(NULL, "sin", "$1", 1, "sin$1", sinNative, vm.numberType);
  REGISTER_PHRASE(NULL, "cos", "$1", 1, "cos$1", cosNative, vm.numberType);
  REGISTER_PHRASE(NULL, "square", "root,of,$1", 1, "square_root_of$1", sqrtNative, vm.numberType);
  REGISTER_PHRASE(NULL, "power", "of,$1,to,$1", 2, "power_of$1_to$1", powNative, vm.numberType, vm.numberType);
  REGISTER_PHRASE(NULL, "floor", "of,$1", 1, "floor_of$1", floorNative, vm.numberType);
  REGISTER_PHRASE(NULL, "random", "from,$1,to,$1", 2, "random_from$1_to$1", randomNative, vm.numberType, vm.numberType);
}
