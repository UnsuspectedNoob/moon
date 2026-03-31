// vm.c

#include "vm.h"
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"

VM vm; // Define the global VM instance here

static void resetStack() { vm.stackTop = vm.stack; }

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  // Walk the stack trace from top to bottom
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *function = frame->function;

    // The instruction pointer is currently pointing to the NEXT instruction.
    // We want the previous one (the one that failed).
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}

void freeVM() {
  freeTable(&vm.strings);
  freeTable(&vm.globals);
  freeObjects();
  free(vm.grayStack);
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

// FIX: Add peek() function
static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// FIX: Add concatenate() function for strings
static void concatenate() {
  ObjString *b = AS_STRING(peek(0));
  ObjString *a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(chars, length);

  pop();
  pop();
  push(OBJ_VAL(result));
}

bool valuesEqual(Value a, Value b) { return a == b; }

// Helper to define natives
static void defineNative(const char *name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, vm.stack[0], vm.stack[1]);
  pop();
  pop();
}

static Value clockNative(int argCount, Value *args) {
  (void)argCount; // Silence warning
  (void)args;     // Silence warning
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value showNative(int argCount, Value *args) {
  (void)argCount;

  // Phrasal 'show' only expects 1 argument (the evaluated expression)
  printValue(args[0]);
  printf(" "); // Always append the newline
  printf("%s", "\n");

  return NIL_VAL; // Native functions must return something
}

// --- NATIVE GETTER LOGIC ---
static Value listLengthGetter(int argCount, Value *args) {
  (void)argCount;
  // 'args[0]' will be the list itself, passed in by the VM later!
  ObjList *list = AS_LIST(args[0]);
  return NUMBER_VAL(list->count);
}

static Value stringLengthGetter(int argCount, Value *args) {
  (void)argCount;
  ObjString *str = AS_STRING(args[0]);
  return NUMBER_VAL(str->length);
}

// --- BOOTSTRAP HELPERS ---
static ObjType *defineNativeType(const char *name) {
  ObjString *nameStr = copyString(name, (int)strlen(name));

  push(OBJ_VAL(nameStr)); // GC Protection
  ObjType *type = newType(nameStr);
  type->isNative = true; // Lock it down! No custom instantiation.
  push(OBJ_VAL(type));   // GC Protection

  // Expose it to MOON scripts!
  tableSet(&vm.globals, OBJ_VAL(nameStr), OBJ_VAL(type));

  pop(); // pop type
  pop(); // pop name
  return type;
}

static void defineNativeGetter(ObjType *type, const char *propertyName,
                               NativeFn function) {
  push(OBJ_VAL(copyString(propertyName, (int)strlen(propertyName))));
  push(OBJ_VAL(newNative(function)));

  // Drop the C-function directly into the blueprint's property table
  tableSet(&type->properties, peek(1), peek(0));

  pop(); // pop native function
  pop(); // pop property name string
}

void initVM() {
  resetStack();
  vm.objects = NULL;
  vm.debugMode = false;

  // --- NEW GC INIT ---
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024; // Start with a 1MB threshold
  vm.grayStack = NULL;
  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.allowGC = false; // <--- GC is asleep until execution begins!
  // -------------------

  initTable(&vm.strings);
  initTable(&vm.globals);

  // ==========================================
  // PHASE 1: BOOTSTRAP THE OBJECT UNIVERSE
  // ==========================================
  vm.anyType = defineNativeType("Any");
  vm.numberType = defineNativeType("Number");
  vm.listType = defineNativeType("List");
  vm.stringType = defineNativeType("String");
  vm.dictType = defineNativeType("Dict");
  vm.boolType = defineNativeType("Bool");
  vm.rangeType = defineNativeType("Range");
  vm.functionType = defineNativeType("Function");
  vm.nilType = defineNativeType("Nil");

  // Inject Native Getters into the Blueprints!
  defineNativeGetter(vm.listType, "length", listLengthGetter);
  defineNativeGetter(vm.listType, "count", listLengthGetter);
  defineNativeGetter(vm.stringType, "length", stringLengthGetter);
  defineNativeGetter(vm.stringType, "count", stringLengthGetter);

  for (int i = 0; i < 256; i++) {
    char c = (char)i;
    // copyString takes a pointer to the char and the length (1)
    vm.charStrings[i] = copyString(&c, 1);
  }

  // Define native function
  defineNative("clock", clockNative);
  defineNative("show$1", showNative);
}

ObjType *getObjType(Value val) {
  if (IS_NUMBER(val))
    return vm.numberType;
  if (IS_BOOL(val))
    return vm.boolType;
  if (IS_NIL(val))
    return vm.nilType;

  if (IS_OBJ(val)) {
    switch (OBJ_TYPE(val)) {
    case OBJ_STRING:
      return vm.stringType;
    case OBJ_LIST:
      return vm.listType;
    case OBJ_DICT:
      return vm.dictType;
    case OBJ_RANGE:
      return vm.rangeType;
    case OBJ_FUNCTION:
    case OBJ_NATIVE:
    case OBJ_MULTI_FUNCTION:
      return vm.functionType;
    case OBJ_INSTANCE:
      return AS_INSTANCE(val)->type;
    case OBJ_TYPE_BLUEPRINT:
      return vm.anyType; // A blueprint is just an object of type 'Any' for now
    default:
      return vm.anyType;
    }
  }

  return vm.anyType; // Absolute fallback
}

// Run()
static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
  register uint8_t *ip = frame->ip;
  uint8_t instruction;

  // 1. THE DISPATCH TABLE
  // WARNING: This must exactly match the order of the OpCode enum in chunk.h!
  static void *dispatchTable[] = {
      &&TARGET_OP_CONSTANT,      &&TARGET_OP_CONSTANT_LONG,
      &&TARGET_OP_NIL,           &&TARGET_OP_TRUE,
      &&TARGET_OP_FALSE,         &&TARGET_OP_POP,
      &&TARGET_OP_GET_LOCAL,     &&TARGET_OP_SET_LOCAL,
      &&TARGET_OP_GET_GLOBAL,    &&TARGET_OP_DEFINE_GLOBAL,
      &&TARGET_OP_SET_GLOBAL,    &&TARGET_OP_ADD,
      &&TARGET_OP_ADD_INPLACE,   &&TARGET_OP_SUBTRACT,
      &&TARGET_OP_MULTIPLY,      &&TARGET_OP_DIVIDE,
      &&TARGET_OP_MOD,           &&TARGET_OP_JUMP_IF_FALSE,
      &&TARGET_OP_NEGATE,        &&TARGET_OP_JUMP,
      &&TARGET_OP_LOOP,          &&TARGET_OP_EQUAL,
      &&TARGET_OP_GREATER,       &&TARGET_OP_LESS,
      &&TARGET_OP_NOT,           &&TARGET_OP_BUILD_STRING,
      &&TARGET_OP_BUILD_LIST,    &&TARGET_OP_BUILD_DICT,
      &&TARGET_OP_GET_PROPERTY,  &&TARGET_OP_SET_PROPERTY,
      &&TARGET_OP_GET_SUBSCRIPT, &&TARGET_OP_SET_SUBSCRIPT,
      &&TARGET_OP_GET_END_INDEX, &&TARGET_OP_RANGE,
      &&TARGET_OP_FOR_ITER,      &&TARGET_OP_GET_ITER,
      &&TARGET_OP_CALL,          &&TARGET_OP_TYPE_DEF,
      &&TARGET_OP_INSTANTIATE,   &&TARGET_OP_DEFINE_METHOD,
      &&TARGET_OP_RETURN};

#define READ_BYTE() (*ip++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_STRING()                                                          \
  AS_STRING(frame->function->chunk.constants.values[READ_SHORT()])

#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands must be numbers.");                               \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(valueType(a op b));                                                   \
  } while (false)

// 2. THE NEW DISPATCH MACRO
// This replaces the top of the for(;;) loop and the switch statement.
#define DISPATCH()                                                             \
  do {                                                                         \
    if (vm.debugMode) {                                                        \
      debugStack(&vm);                                                         \
      disassembleInstruction(&frame->function->chunk,                          \
                             (int)(ip - frame->function->chunk.code));         \
    }                                                                          \
    instruction = READ_BYTE();                                                 \
    goto *dispatchTable[instruction];                                          \
  } while (false)

  // 3. PRIME THE PUMP
  // Fire off the very first instruction jump!
  DISPATCH();

TARGET_OP_CONSTANT: {
  Value constant = READ_CONSTANT();
  push(constant);
  DISPATCH();
}

TARGET_OP_CONSTANT_LONG: {
  uint16_t index = READ_SHORT();
  Value constant = frame->function->chunk.constants.values[index];
  push(constant);
  DISPATCH();
}

TARGET_OP_ADD: {
  Value b = peek(0); // Right operand
  Value a = peek(1); // Left operand

  // --- RULE 1: STRICT MATH (Number + Number) ---
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    double right = AS_NUMBER(pop());
    double left = AS_NUMBER(pop());
    push(NUMBER_VAL(left + right));
  }

  // --- RULE 2: THE COERCER (String + Any) ---
  else if (IS_STRING(a)) {
    ObjString *leftStr = AS_STRING(a);
    // Dynamically stringify the right side!
    ObjString *rightStr = valueToString(b);

    // Clean the stack
    pop(); // Pop b
    pop(); // Pop a

    // Push the string versions for the concatenate helper
    push(OBJ_VAL(leftStr));
    push(OBJ_VAL(rightStr));
    concatenate();
  }

  // --- RULE 3 & 4: LIST CLONING & MERGING (List + Any) ---
  else if (IS_LIST(a)) {
    ObjList *leftList = AS_LIST(a);

    // 1. Allocate the brand new clone list
    ObjList *newListObj = newList();

    // Protect it from the Garbage Collector while we build it!
    push(OBJ_VAL(newListObj));

    // 2. Clone the original list into the new one (O(N) copy)
    for (int i = 0; i < leftList->count; i++) {
      appendList(newListObj, leftList->items[i]);
    }

    // 3. The Fork: Are we merging a list, or appending a single item?
    if (IS_LIST(b)) {
      ObjList *rightList = AS_LIST(b);
      for (int i = 0; i < rightList->count; i++) {
        appendList(newListObj, rightList->items[i]);
      }
    } else {
      appendList(newListObj, b); // Just append the raw item
    }

    // 4. Clean up the stack
    Value result = pop(); // Temporarily hold the finished newList
    pop();                // Pop original b
    pop();                // Pop original a
    push(result);         // Push the brand new list back to the top!
  }

  // --- RULE 5: THE REJECTOR ---
  else {
    runtimeError("Invalid '+' operation. Must be Number+Number, "
                 "String+Any, or List+Any.");
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

TARGET_OP_ADD_INPLACE: {
  Value b = peek(0); // Right operand
  Value a = peek(1); // Left operand (The Accumulator)

  // RULE 1: Numbers (Just do normal math)
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    double right = AS_NUMBER(pop());
    double left = AS_NUMBER(pop());
    push(NUMBER_VAL(left + right));
  }

  // RULE 2: Lists (THE O(1) MUTATION FIX)
  else if (IS_LIST(a)) {
    ObjList *list = AS_LIST(a); // We mutate this directly!

    if (IS_LIST(b)) {
      ObjList *rightList = AS_LIST(b);
      for (int i = 0; i < rightList->count; i++) {
        appendList(list, rightList->items[i]);
      }
    } else {
      appendList(list, b);
    }

    // Cleanup: We pop 'b', but leave 'a' (the mutated list) right where it
    // is so that the upcoming OP_SET_LOCAL can re-assign it cleanly!
    pop();
  }

  // RULE 3: Strings (Strings are immutable in C, so we still must clone)
  else if (IS_STRING(a)) {
    ObjString *leftStr = AS_STRING(a);
    ObjString *rightStr = valueToString(b);
    pop();
    pop();
    push(OBJ_VAL(leftStr));
    push(OBJ_VAL(rightStr));
    concatenate();
  }

  else {
    runtimeError("Invalid 'add' operation.");
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

TARGET_OP_SUBTRACT: {
  BINARY_OP(NUMBER_VAL, -);
  DISPATCH();
}

TARGET_OP_MULTIPLY: {
  BINARY_OP(NUMBER_VAL, *);
  DISPATCH();
}

TARGET_OP_DIVIDE: {
  BINARY_OP(NUMBER_VAL, /);
  DISPATCH();
}

TARGET_OP_MOD: {
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
    runtimeError("Operands must be numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(pop());

  if (b == 0.0) {
    runtimeError("Modulo by zero.");
    return INTERPRET_RUNTIME_ERROR;
  }

  // --- THE FAST PATH ---
  int intA = (int)a;
  int intB = (int)b;

  // If casting them to ints didn't lose any decimal data, use the CPU
  // hardware!
  if (a == intA && b == intB) {
    push(NUMBER_VAL(intA % intB));
  } else {
    // Otherwise, fall back to the safe, slower C library
    push(NUMBER_VAL(fmod(a, b)));
  }
  DISPATCH();
}

TARGET_OP_NEGATE: {
  if (!IS_NUMBER(peek(0))) {
    runtimeError("Operand must be a number.");
    return INTERPRET_RUNTIME_ERROR;
  }
  push(NUMBER_VAL(-AS_NUMBER(pop())));
  DISPATCH();
}

TARGET_OP_NIL: {
  push(NIL_VAL);
  DISPATCH();
}

TARGET_OP_TRUE: {
  push(BOOL_VAL(true));
  DISPATCH();
}

TARGET_OP_FALSE: {
  push(BOOL_VAL(false));
  DISPATCH();
}

TARGET_OP_POP: {
  pop();
  DISPATCH();
}

TARGET_OP_DEFINE_GLOBAL: {
  ObjString *name = READ_STRING();
  tableSet(&vm.globals, OBJ_VAL(name), peek(0));
  DISPATCH();
}

TARGET_OP_GET_GLOBAL: {
  ObjString *name = READ_STRING();
  Value value;
  if (!tableGet(&vm.globals, OBJ_VAL(name), &value)) {
    runtimeError("Undefined variable '%s'.", name->chars);
    return INTERPRET_RUNTIME_ERROR;
  }

  push(value);
  DISPATCH();
}

TARGET_OP_SET_GLOBAL: {
  ObjString *name = READ_STRING();
  if (tableSet(&vm.globals, OBJ_VAL(name), peek(0))) {
    // tableSet returns true if it's a NEW key.
    // 'set' is only for EXISTING keys. Delete it and error.
    tableDelete(&vm.globals, OBJ_VAL(name));
    runtimeError("Undefined variable '%s'.", name->chars);
    return INTERPRET_RUNTIME_ERROR;
  }

  DISPATCH();
}

TARGET_OP_GET_LOCAL: {
  uint8_t slot = READ_BYTE();
  push(frame->slots[slot]);
  DISPATCH();
}

TARGET_OP_SET_LOCAL: {
  uint8_t slot = READ_BYTE();
  frame->slots[slot] = peek(0);
  DISPATCH();
}

TARGET_OP_JUMP: {
  uint16_t offset = READ_SHORT();
  ip += offset; // Update local ip directly
  DISPATCH();
}

TARGET_OP_JUMP_IF_FALSE: {
  uint16_t offset = READ_SHORT();
  if (isFalsey(peek(0)))
    ip += offset; // Update local ip directly
  DISPATCH();
}

TARGET_OP_LOOP: {
  uint16_t offset = READ_SHORT();
  ip -= offset; // Update local ip directly
  DISPATCH();
}

TARGET_OP_EQUAL: {
  Value b = pop();
  Value a = pop();
  push(BOOL_VAL(valuesEqual(a, b)));
  DISPATCH();
}

TARGET_OP_GREATER: {
  BINARY_OP(BOOL_VAL, >);
  DISPATCH();
}

TARGET_OP_LESS: {
  BINARY_OP(BOOL_VAL, <);
  DISPATCH();
}

TARGET_OP_NOT: {
  push(BOOL_VAL(isFalsey(pop())));
  DISPATCH();
}

TARGET_OP_BUILD_STRING: {
  uint16_t count = READ_SHORT();

  // Point to the first item in the sequence
  // (Stack grows up, so stackTop is past the end. We back up 'count' slots)
  Value *start = vm.stackTop - count;

  int totalLength = 0;

  // Pass 1: Verify types, convert if needed, and calculate total length
  for (int i = 0; i < count; i++) {
    // Use your helper function!
    ObjString *str = valueToString(start[i]);

    // CRITICAL: Update the stack with the string version.
    // 1. We need it for Pass 2 (memcpy).
    // 2. If GC runs (later), it needs to see these string objects to keep
    // them alive.
    start[i] = OBJ_VAL(str);

    totalLength += str->length;
  }

  // Pass 2: Allocation and Copy
  // We allocate one big buffer for the whole result
  char *chars = ALLOCATE(char, totalLength + 1);
  char *dest = chars;

  for (int i = 0; i < count; i++) {
    ObjString *str = AS_STRING(start[i]);
    memcpy(dest, str->chars, str->length);
    dest += str->length;
  }
  chars[totalLength] = '\0'; // Null terminate

  // Create the final object
  ObjString *result = takeString(chars, totalLength);

  // Cleanup: Pop the 'count' parts and push the single result
  vm.stackTop -= count;
  push(OBJ_VAL(result));

  DISPATCH();
}

TARGET_OP_BUILD_LIST: {
  uint16_t itemCount = READ_SHORT();

  // 1. Create the new list object
  ObjList *list = newList();

  // 2. GC Protection:
  // We push the list to the stack immediately. If we don't, and
  // appendList() triggers a GC (by allocating memory), the collector
  // won't see our new list and might delete it!
  push(OBJ_VAL(list));

  // 3. Locate the items on the stack
  // Stack layout: [item1] [item2] ... [itemN] [list] <--- Top
  Value *itemsStart = vm.stackTop - 1 - itemCount;

  // 4. Transfer and EXPAND items
  for (int i = 0; i < itemCount; i++) {
    Value item = itemsStart[i];

    if (IS_RANGE(item)) {
      ObjRange *range = AS_RANGE(item);
      double start = range->start;
      double end = range->end;
      double step = range->step; // The new step value!

      // Expand counting up (e.g., 1 to 10 by 2)
      if (start <= end) {
        for (double val = start; val <= end; val += step) {
          appendList(list, NUMBER_VAL(val));
        }
      }
      // Expand counting down (e.g., 10 to 1 by 2)
      else {
        for (double val = start; val >= end; val -= step) {
          appendList(list, NUMBER_VAL(val));
        }
      }
    } else {
      // Normal Item Logic
      appendList(list, item);
    }
  }

  // 5. Cleanup the stack
  pop();                    // Pop the list temporarily
  vm.stackTop -= itemCount; // Remove all the original items/ranges
  push(OBJ_VAL(list));      // Push the final expanded list back

  DISPATCH();
}

TARGET_OP_BUILD_DICT: {
  uint16_t itemCount = READ_SHORT();

  // 1. Create the dictionary and protect it from GC
  ObjDict *dict = newDict();
  push(OBJ_VAL(dict));

  // 2. Locate the keys and values on the stack
  // Layout: [k1] [v1] [k2] [v2] ... [kN] [vN] [dict] <--- Top
  Value *itemsStart = vm.stackTop - 1 - (itemCount * 2);

  // 3. Insert them into the Hash Table
  for (int i = 0; i < itemCount; i++) {
    Value key = itemsStart[i * 2];
    Value val = itemsStart[(i * 2) + 1];

    // Drop it directly into the C struct's table!
    tableSet(&dict->fields, key, val);
  }

  // 4. Cleanup
  pop();                          // Pop the dict temporarily
  vm.stackTop -= (itemCount * 2); // Remove all keys and values
  push(OBJ_VAL(dict));            // Push the finished dictionary back!

  DISPATCH();
}

TARGET_OP_INSTANTIATE: {
  uint16_t propCount = READ_SHORT();

  // Layout: [Blueprint] [k1] [v1] [k2] [v2] <--- Top
  Value *itemsStart = vm.stackTop - (propCount * 2);
  Value targetVal = *(itemsStart - 1);

  if (!IS_TYPE(targetVal)) {
    runtimeError("Can only instantiate defined types.");
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjType *type = AS_TYPE(targetVal);
  // --- THE NATIVE LOCKDOWN FIX ---
  if (type->isNative) {
    runtimeError("Cannot instantiate native types (like '%s') directly.",
                 type->name->chars);
    return INTERPRET_RUNTIME_ERROR;
  }
  // -------------------------------
  ObjInstance *instance = newInstance(type);
  push(OBJ_VAL(instance)); // GC Protection

  // 1. INHERITANCE MERGE: Copy all default properties from the Blueprint
  tableAddAll(&type->properties, &instance->fields);

  // 2. OVERRIDE MERGE: Apply the user's specific instantiation values
  for (int i = 0; i < propCount; i++) {
    Value key = itemsStart[i * 2];
    Value val = itemsStart[(i * 2) + 1];
    tableSet(&instance->fields, key, val);
  }

  // 3. Cleanup: Pop the instance, the props, and the blueprint, then push
  // the finished instance
  pop();
  vm.stackTop -= (propCount * 2) + 1;
  push(OBJ_VAL(instance));
  DISPATCH();
}

TARGET_OP_GET_PROPERTY: {
  ObjString *name = READ_STRING();
  Value target = peek(0);

  // 1. Identify the target using our Universal Resolver!
  ObjType *type = getObjType(target);
  Value value;

  // 2. INSTANCE OVERRIDES: If it's a custom clone, check its personal hash
  // table first.
  if (IS_INSTANCE(target)) {
    ObjInstance *instance = AS_INSTANCE(target);
    if (tableGet(&instance->fields, OBJ_VAL(name), &value)) {
      pop();       // Remove target
      push(value); // Push result
      DISPATCH();
    }
  }

  // --- THE DICTIONARY FIX ---
  else if (IS_DICT(target)) {
    ObjDict *dict = AS_DICT(target);
    if (tableGet(&dict->fields, OBJ_VAL(name), &value)) {
      pop();
      push(value);
      DISPATCH();
    }
    // If it's not a key, we fall through to check if it's a native
    // dictionary method on the Blueprint (like dict.keys)
  }

  // 3. BLUEPRINT LOOKUP: Check the Shared Blueprint!
  // This automatically catches native C-getters (like list.length),
  // AND it will catch shared methods for custom types later!
  if (tableGet(&type->properties, OBJ_VAL(name), &value)) {

    // --- THE NATIVE GETTER FIX ---
    // If the property is a Native C-Function, execute it instantly!
    if (IS_NATIVE(value)) {
      NativeFn native = AS_NATIVE(value);

      // Execute with target as the argument
      Value result = native(1, &target);
      pop(); // Remove target
      push(result);
      DISPATCH();
    }

    pop();       // Remove target
    push(value); // Push result
    DISPATCH();
  }

  // If we got here, the property doesn't exist anywhere.
  runtimeError("Undefined property '%s' on type '%s'.", name->chars,
               type->name->chars);
  return INTERPRET_RUNTIME_ERROR;
}

TARGET_OP_SET_PROPERTY: {
  Value value = peek(0);
  ObjString *name = READ_STRING();
  Value target = peek(1);

  if (IS_INSTANCE(target)) {
    ObjInstance *instance = AS_INSTANCE(target);
    tableSet(&instance->fields, OBJ_VAL(name), value);
  }

  // --- THE DICTIONARY FIX ---
  else if (IS_DICT(target)) {
    ObjDict *dict = AS_DICT(target);
    tableSet(&dict->fields, OBJ_VAL(name), value);
  } else {
    runtimeError(
        "Only object instances and dictionaries have mutable properties.");
    return INTERPRET_RUNTIME_ERROR;
  }

  pop();       // Pop value
  pop();       // Pop target
  push(value); // Push value back for chained assignments!
  DISPATCH();
}

TARGET_OP_GET_SUBSCRIPT: {
  Value indexVal = peek(0); // Popped from stack!
  Value seqVal = peek(1);   // Popped from stack!

  if (IS_LIST(seqVal)) {
    ObjList *list = AS_LIST(seqVal);

    if (IS_NUMBER(indexVal)) {
      int userIndex = (int)AS_NUMBER(indexVal);
      if (userIndex == 0) {
        runtimeError("List index cannot be 0. MOON uses 1-based indexing.");
        return INTERPRET_RUNTIME_ERROR;
      }

      int index = userIndex > 0 ? userIndex - 1 : list->count + userIndex;

      if (index < 0 || index >= list->count) {
        runtimeError("List index out of bounds.");
        return INTERPRET_RUNTIME_ERROR;
      }

      pop();
      pop();
      push(list->items[index]);
    } else if (IS_RANGE(indexVal)) {
      // ... (Keep your excellent slicing logic exactly the same here) ...
      ObjRange *range = AS_RANGE(indexVal);
      int start = (int)range->start;
      int end = (int)range->end;

      if (start < 0)
        start = list->count + start + 1;
      if (end < 0)
        end = list->count + end + 1;
      start--;
      end--;

      ObjList *resultList = newList();
      push(OBJ_VAL(resultList));
      if (start <= end && start < list->count && end >= 0) {
        if (start < 0)
          start = 0;
        if (end >= list->count)
          end = list->count - 1;
        for (int i = start; i <= end; i++) {
          appendList(resultList, list->items[i]);
        }
      }
      pop();
      pop();
      pop();
      push(OBJ_VAL(resultList));
    } else {
      runtimeError("List index must be a number or a range.");
      return INTERPRET_RUNTIME_ERROR;
    }
  } else if (IS_DICT(seqVal)) {
    ObjDict *dict = AS_DICT(seqVal);
    if (!IS_STRING(indexVal)) {
      runtimeError("Dictionary keys must be strings.");
      return INTERPRET_RUNTIME_ERROR;
    }
    Value result;
    if (tableGet(&dict->fields, indexVal, &result)) {
      pop();
      pop();
      push(result);
    } else {
      pop();
      pop();
      push(NIL_VAL); // Safe cache miss!
    }
  }
  // NEW: Add native string indexing! `string[2]`
  else if (IS_STRING(seqVal)) {
    if (!IS_NUMBER(indexVal)) {
      runtimeError("String index must be a number.");
      return INTERPRET_RUNTIME_ERROR;
    }
    ObjString *str = AS_STRING(seqVal);
    int userIndex = (int)AS_NUMBER(indexVal);

    if (userIndex == 0) {
      runtimeError("Like we've said, you have been rescued. First item is 1.");
      return INTERPRET_RUNTIME_ERROR;
    }
    int index = userIndex > 0 ? userIndex - 1 : str->length + userIndex;

    if (index < 0 || index >= str->length) {
      runtimeError("String index out of bounds.");
      return INTERPRET_RUNTIME_ERROR;
    }

    uint8_t charByte = (uint8_t)str->chars[index];
    pop();
    pop();
    push(OBJ_VAL(vm.charStrings[charByte]));
  } else {
    runtimeError("Type is not subscriptable.");
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

TARGET_OP_SET_SUBSCRIPT: {
  Value value = peek(0);
  Value indexVal = peek(1);
  Value collectionVal = peek(2);

  if (IS_DICT(collectionVal)) {
    ObjDict *dict = AS_DICT(collectionVal);
    if (!IS_STRING(indexVal)) {
      runtimeError("Dictionary keys must be strings.");
      return INTERPRET_RUNTIME_ERROR;
    }
    tableSet(&dict->fields, indexVal, value);
    pop();
    pop();
    pop();
    push(value);
  } else if (IS_LIST(collectionVal)) {
    ObjList *list = AS_LIST(collectionVal);
    if (!IS_NUMBER(indexVal)) {
      runtimeError("List index must be a number.");
      return INTERPRET_RUNTIME_ERROR;
    }
    // --- THE NEGATIVE INDEX FIX (SET) ---
    int userIndex = (int)AS_NUMBER(indexVal);

    if (userIndex == 0) {
      runtimeError("List index cannot be 0. MOON uses 1-based indexing.");
      return INTERPRET_RUNTIME_ERROR;
    }

    int index = userIndex > 0 ? userIndex - 1 : list->count + userIndex;

    if (index < 0 || index >= list->count) {
      runtimeError("List index out of bounds.");
      return INTERPRET_RUNTIME_ERROR;
    }
    list->items[index] = value;
    pop();
    pop();
    pop();
    push(value);
  } else {
    runtimeError("Can only assign to lists and dictionaries.");
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

TARGET_OP_GET_END_INDEX: {
  Value seq = NIL_VAL;

  // THE FIX: Scan backwards down the stack to find the collection!
  for (Value *ptr = vm.stackTop - 1; ptr >= vm.stack; ptr--) {
    if (IS_LIST(*ptr) || IS_STRING(*ptr)) {
      seq = *ptr;
      break;
    }
  }

  if (IS_LIST(seq)) {
    // 1-BASED INDEXING: The last index is exactly the count!
    push(NUMBER_VAL(AS_LIST(seq)->count));
  } else if (IS_STRING(seq)) {
    // 1-BASED INDEXING: The last index is exactly the length!
    push(NUMBER_VAL(AS_STRING(seq)->length));
  } else {
    runtimeError("Cannot use 'end' outside of a list or string subscript.");
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

TARGET_OP_RANGE: {
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)) || !IS_NUMBER(peek(2))) {
    runtimeError("Range operands must be numbers.");
    return INTERPRET_RUNTIME_ERROR;
  }

  double step = AS_NUMBER(pop());
  double end = AS_NUMBER(pop());
  double start = AS_NUMBER(pop());

  // Protect against infinite loops and negative steps
  if (step <= 0) {
    runtimeError("Range step must be a positive number greater than 0.");
    return INTERPRET_RUNTIME_ERROR;
  }

  ObjRange *range = newRange(start, end, step);
  push(OBJ_VAL(range));
  DISPATCH();
}

  // 1. Determine where the loop starts
TARGET_OP_GET_ITER: {
  Value seq = peek(0); // Sequence is on stack

  // Strings, Lists, and Ranges all use the universal -1 index counter!
  if (IS_LIST(seq) || IS_RANGE(seq) || IS_STRING(seq)) {
    push(NUMBER_VAL(-1));
  } else {
    runtimeError("Can only loop over lists, ranges, and strings.");
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

  // 2. The Loop Logic
TARGET_OP_FOR_ITER: {
  // Stack: [Sequence] [Iterator Index]
  Value iteratorVal = peek(0);
  Value seqVal = peek(1);

  // A. Range Logic
  if (IS_RANGE(seqVal)) {
    ObjRange *range = AS_RANGE(seqVal);
    double index = AS_NUMBER(iteratorVal);
    double nextIndex = index + 1;
    double nextValue;

    // Calculate the actual value based on direction and step
    if (range->start <= range->end) {
      // Counting UP
      nextValue = range->start + (nextIndex * range->step);

      if (nextValue <= range->end) {
        vm.stackTop[-1] = NUMBER_VAL(nextIndex); // Save the index
        push(NUMBER_VAL(nextValue));             // Push the actual loop value
        ip += 2;                                 // Skip jump
      } else {
        uint16_t offset = READ_SHORT();
        ip += offset;
      }
    } else {
      // Counting DOWN
      nextValue = range->start - (nextIndex * range->step);

      if (nextValue >= range->end) {
        vm.stackTop[-1] = NUMBER_VAL(nextIndex); // Save the index
        push(NUMBER_VAL(nextValue));             // Push the actual loop value
        ip += 2;                                 // Skip jump
      } else {
        uint16_t offset = READ_SHORT();
        ip += offset;
      }
    }
  }
  // B. List Logic (Remains mostly the same!)
  else if (IS_LIST(seqVal)) {
    ObjList *list = AS_LIST(seqVal);
    double index = AS_NUMBER(iteratorVal);
    double nextIndex = index + 1;

    if (nextIndex < list->count) {
      vm.stackTop[-1] = NUMBER_VAL(nextIndex); // Save the index
      push(list->items[(int)nextIndex]);       // Push the list item
      ip += 2;                                 // Skip jump
    } else {
      uint16_t offset = READ_SHORT();
      ip += offset;
    }
  } else if (IS_STRING(seqVal)) {
    ObjString *string = AS_STRING(seqVal);
    double index = AS_NUMBER(iteratorVal);
    double nextIndex = index + 1;

    if (nextIndex < string->length) {
      // 1. Save the new index
      vm.stackTop[-1] = NUMBER_VAL(nextIndex);

      // 2. Grab the raw char byte (cast to unsigned so it safely maps
      // 0-255)
      uint8_t charByte = (uint8_t)string->chars[(int)nextIndex];

      // 3. THE MAGIC: Push the pre-allocated 1-char string from our pool!
      push(OBJ_VAL(vm.charStrings[charByte]));

      // 4. Skip the jump offset
      ip += 2;
    } else {
      // Done
      uint16_t offset = READ_SHORT();
      ip += offset;
    }
  } else {
    runtimeError("Can only iterate over ranges, lists, or strings.");
    return INTERPRET_RUNTIME_ERROR;
  }
  DISPATCH();
}

TARGET_OP_CALL: {
  int argCount = READ_SHORT();

  Value callee = peek(argCount);

  if (IS_NATIVE(callee)) {
    NativeFn native = AS_NATIVE(callee);
    Value result = native(argCount, vm.stackTop - argCount);
    vm.stackTop -= argCount + 1;
    push(result);
    DISPATCH();
  } else if (IS_FUNCTION(callee)) {
    ObjFunction *function = AS_FUNCTION(callee);
    if (argCount != function->arity) {
      runtimeError("Expected %d arguments but got %d.", function->arity,
                   argCount);
      return INTERPRET_RUNTIME_ERROR;
    }

    if (vm.frameCount == FRAMES_MAX) {
      runtimeError("Stack overflow.");
      return INTERPRET_RUNTIME_ERROR;
    }

    // 1. SAVE: Store current instruction pointer back to current frame
    frame->ip = ip;

    // 2. SWITCH: Create new frame
    CallFrame *newFrame = &vm.frames[vm.frameCount++];
    newFrame->function = function;
    // Note: We don't need to set newFrame->ip yet, we'll load it into local
    // 'ip'
    newFrame->ip = function->chunk.code;
    newFrame->slots = vm.stackTop - argCount - 1;

    frame = newFrame; // Point to new frame

    // 3. LOAD: Update local 'ip' from the new frame
    ip = frame->ip;
    DISPATCH();
  } else if (IS_MULTI_FUNCTION(callee)) {
    ObjMultiFunction *multi = AS_MULTI_FUNCTION(callee);

    if (argCount != multi->arity) {
      runtimeError("Expected %d arguments but got %d.", multi->arity, argCount);
      return INTERPRET_RUNTIME_ERROR;
    }

    // Pointer to the first argument on the stack
    Value *args = vm.stackTop - argCount;

    // --- THE SYMMETRIC SCORING ENGINE ---
    ObjFunction *bestMethod = NULL;
    int bestScore = -1;
    bool isAmbiguous = false;

    for (int i = 0; i < multi->methodCount; i++) {
      ObjType **signature = multi->signatures[i];
      bool isMatch = true;
      int currentScore = 0;

      // Check every argument against the signature
      for (int j = 0; j < argCount; j++) {
        ObjType *expectedType = signature[j];
        ObjType *actualType =
            getObjType(args[j]); // <--- THE UNIVERSAL RESOLVER!

        if (expectedType == actualType) {
          currentScore += 2; // EXACT MATCH: 2 Points
        } else if (expectedType == NULL || expectedType == vm.anyType) {
          currentScore += 1; // ANY WILDCARD: 1 Point
        } else {
          isMatch = false; // TYPE MISMATCH: Hard fail
          break;
        }
      }

      // If the signature is mathematically valid, score it!
      if (isMatch) {
        if (currentScore > bestScore) {
          bestScore = currentScore;
          bestMethod = multi->methods[i];
          isAmbiguous = false; // We have a clear winner (for now)
        } else if (currentScore == bestScore) {
          isAmbiguous =
              true; // A perfect tie! We have an intersection collision.
        }
      }
    }

    // --- RESOLUTION & THE INTERSECTION MANDATE ---
    if (bestMethod == NULL) {
      runtimeError("No matching signature found for this phrasal function.");
      return INTERPRET_RUNTIME_ERROR;
    }

    if (isAmbiguous) {
      runtimeError("AMBIGUOUS DISPATCH ERROR: Multiple methods match this "
                   "call with the exact same specificity. "
                   "Please define a tie-breaker intersection method to "
                   "clarify your intent.");
      return INTERPRET_RUNTIME_ERROR;
    }

    // --- EXECUTE THE WINNING METHOD ---
    if (vm.frameCount == FRAMES_MAX) {
      runtimeError("Stack overflow.");
      return INTERPRET_RUNTIME_ERROR;
    }

    frame->ip = ip;
    CallFrame *newFrame = &vm.frames[vm.frameCount++];
    newFrame->function = bestMethod;
    newFrame->ip = bestMethod->chunk.code;
    newFrame->slots = vm.stackTop - argCount - 1;
    frame = newFrame;
    ip = frame->ip;

    DISPATCH();
  }

  runtimeError("Can only call functions, multi-functions, and classes.");
  return INTERPRET_RUNTIME_ERROR;
}

TARGET_OP_TYPE_DEF: {
  ObjString *name = READ_STRING();
  uint16_t propertyCount = READ_SHORT();

  // 1. Construct the Blueprint!
  ObjType *type = newType(name);

  // GC Protection: We push it before we loop through the stack
  push(OBJ_VAL(type));

  // 2. Extract properties from the stack
  // Stack Layout: [key1] [val1] [key2] [val2] ... [type] <--- Top
  Value *itemsStart = vm.stackTop - 1 - (propertyCount * 2);

  for (int i = 0; i < propertyCount; i++) {
    Value key = itemsStart[i * 2];
    Value val = itemsStart[(i * 2) + 1];

    // Save the default value into the Blueprint's dictionary!
    tableSet(&type->properties, key, val);
  }

  // 3. Save the finished Blueprint to Global Variables so the language
  // knows it exists!
  tableSet(&vm.globals, OBJ_VAL(name), OBJ_VAL(type));

  // 4. Clean up the stack
  pop();                              // Pop the protected ObjType
  vm.stackTop -= (propertyCount * 2); // Pop all keys and values

  DISPATCH();
}

TARGET_OP_DEFINE_METHOD: {
  ObjString *name = READ_STRING();
  ObjFunction *method =
      AS_FUNCTION(peek(0)); // The compiled method is at the top!
  int arity = method->arity;

  // The types are sitting right below the method on the stack
  Value *typesStart = vm.stackTop - 1 - arity;

  // 1. Extract the Expected Types
  ObjType **signatures = ALLOCATE(ObjType *, arity);
  for (int i = 0; i < arity; i++) {
    if (!IS_TYPE(typesStart[i])) {
      runtimeError("Type annotation must resolve to a valid Type Blueprint.");
      return INTERPRET_RUNTIME_ERROR;
    }
    signatures[i] = AS_TYPE(typesStart[i]);
  }

  // 2. Does the MultiFunction folder already exist in globals?
  Value existing;
  ObjMultiFunction *multi = NULL;
  if (tableGet(&vm.globals, OBJ_VAL(name), &existing) &&
      IS_MULTI_FUNCTION(existing)) {
    multi = AS_MULTI_FUNCTION(existing); // Yes, open the folder!
  } else {
    multi = newMultiFunction(name, arity); // No, create a brand new folder!

    push(OBJ_VAL(multi));
    tableSet(&vm.globals, OBJ_VAL(name), OBJ_VAL(multi));
    pop();
  }

  // 3. Slide the method and signature into the arrays
  if (multi->methodCapacity < multi->methodCount + 1) {
    int oldCap = multi->methodCapacity;
    multi->methodCapacity = GROW_CAPACITY(oldCap);
    multi->methods = GROW_ARRAY(ObjFunction *, multi->methods, oldCap,
                                multi->methodCapacity);
    multi->signatures = GROW_ARRAY(ObjType **, multi->signatures, oldCap,
                                   multi->methodCapacity);
  }
  multi->methods[multi->methodCount] = method;
  multi->signatures[multi->methodCount] = signatures;
  multi->methodCount++;

  // 4. Clean up the stack (Pop the method and the types)
  pop();
  vm.stackTop -= arity;
  DISPATCH();
}

TARGET_OP_RETURN: {
  Value result = pop();
  vm.frameCount--;

  if (vm.frameCount == 0) {
    pop();
    return INTERPRET_OK;
  }

  vm.stackTop = frame->slots;
  push(result);

  // 1. SWITCH: Go back to caller
  frame = &vm.frames[vm.frameCount - 1];

  // 2. LOAD: Restore caller's instruction pointer
  ip = frame->ip;
  DISPATCH();
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
  // 1. Compile returns a Function now (we will update compiler.c next)
  ObjFunction *function = compile(source);

  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;

  vm.stackTop = vm.stack;
  vm.frameCount = 0;

  // Push the script function to stack so GC doesn't eat it
  push(OBJ_VAL(function));

  // 2. Set up the first Call Frame (The "Main" script)
  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->ip = function->chunk.code;

  // The script's locals start at the bottom of the stack
  frame->slots = vm.stack;

  vm.allowGC = true;

  // 3. Go!
  return run();
}
