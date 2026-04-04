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

  // Expose the protected functions to the MOON namespace
  defineNative("__sin", sinNative);
  defineNative("__cos", cosNative);
  defineNative("__sqrt", sqrtNative);
  defineNative("__pow", powNative);
  defineNative("__random", randomNative);
  defineNative("__floor", floorNative);
}

// Define the array here!
const char mathBootstrap[] = "let sine of (n: Number):\n"
                             "    give __sin(n)\n"
                             "end\n"
                             "let cosine of (n: Number):\n"
                             "    give __cos(n)\n"
                             "end\n"
                             "let square root of (n: Number):\n"
                             "    give __sqrt(n)\n"
                             "end\n"
                             "let power of (base: Number) to (exp: Number):\n"
                             "    give __pow(base, exp)\n"
                             "end\n"
                             "let floor of (n: Number):\n"
                             "    give __floor(n)\n"
                             "end\n"
                             "let random from (min: Number) to (max: Number):\n"
                             "    give __random(min, max)\n"
                             "end\n";
