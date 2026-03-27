#ifndef MOON_VALUE_H
#define MOON_VALUE_H

#include "common.h"
#include <stdint.h>

// For now, all values are doubles.
// Phase 5 will upgrade this to a Tagged Union (for Strings/Objects).

// 1. the Core Definition
typedef uint64_t Value;

// 2. The Masks
#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN ((uint64_t)0x7ffc000000000000)

// 3. Our Custom Type Tags (Living in the low bits of the mantissa)
#define TAG_NIL 1   // 01
#define TAG_FALSE 2 // 10
#define TAG_TRUE 3  // 11

// 4. Type Punning (Tricking the C Compiler)
static inline double valueToNum(Value value) {
  union {
    uint64_t bits;
    double num;
  } data;
  data.bits = value;
  return data.num;
}

static inline Value numToValue(double num) {
  union {
    uint64_t bits;
    double num;
  } data;
  data.num = num;
  return data.bits;
}

// 5. Value Constructors (*_VAL)
#define NUMBER_VAL(num) numToValue(num)
#define NIL_VAL ((Value)(uint64_t)(QNAN | TAG_NIL))
#define FALSE_VAL ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define BOOL_VAL(b) ((b) ? TRUE_VAL : FALSE_VAL)

// Object pointers get the SIGN_BIT + QNAN mask + their actual memory address
#define OBJ_VAL(obj) ((Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj)))

// 6. Type Checkers (IS_*)
// If the bits do NOT equal the QNAN mask, it's a valid math decimal!
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define IS_NIL(value) ((value) == NIL_VAL)
#define IS_BOOL(value)                                                         \
  (((value) | 1) == TRUE_VAL) // Checks for both TAG_FALSE (2) and TAG_TRUE (3)
#define IS_OBJ(value) (((value) & (SIGN_BIT | QNAN)) == (SIGN_BIT | QNAN))

// 7. Value Extractors (AS_*)
#define AS_NUMBER(value) valueToNum(value)
#define AS_BOOL(value) ((value) == TRUE_VAL)
// To get the pointer back, we flip the bits (~) to strip the mask away!
#define AS_OBJ(value) ((Obj *)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);
void printValue(Value value);

#endif
