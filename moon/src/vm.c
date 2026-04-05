// vm.c

#include "vm.h"
#include "lib_core.h"
#include "lib_io.h"
#include "lib_list.h"
#include "lib_math.h"
#include "lib_string.h"
#include "sigtrie.h"
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compiler.h"
#include "debug.h"
#include "error.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

VM vm; // Define the global VM instance here
extern char *readFile(const char *path);

static void resetStack() { vm.stackTop = vm.stack; }

// ==========================================
// THE ORDEAL RUNTIME ERROR ENGINE
// ==========================================

// The Master Runtime Error function that talks to error.c
static void runtimeErrorDetailed(ErrorType type, const char *hint,
                                 const char *format, ...) {
  // 1. Safely format the dynamic message into a string buffer
  char message[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  // 2. Extract the exact line number where the VM crashed
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
  ObjFunction *function = frame->function;

  // The IP points to the *next* instruction. We step back by 1 to find the one
  // that failed.
  size_t instruction = frame->ip - function->chunk.code - 1;
  int line = function->chunk.lines[instruction];

  // 3. Call the beautiful Error Engine!
  reportRuntimeError(function->moduleName, line, type, message, hint);

  // 4. Print the modernized Stack Trace
  fprintf(stderr, "Stack Trace:\n");
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *traceFrame = &vm.frames[i];
    ObjFunction *traceFunc = traceFrame->function;
    size_t traceInst = traceFrame->ip - traceFunc->chunk.code - 1;
    int traceLine = traceFunc->chunk.lines[traceInst];

    fprintf(stderr, "  > [line %d] in ", traceLine);
    if (traceFunc->name == NULL) {
      fprintf(stderr, "main script\n");
    } else {
      fprintf(stderr, "function %s\n", traceFunc->name->chars);
    }
  }
  fprintf(stderr, "\n");

  resetStack();
}

// The bridge for Native C-Functions to trigger MOON Panics!
// The bridge for Native C-Functions to trigger MOON Panics!
void throwNativeError(const char *hint, const char *format, ...) {
  char message[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  // --- THE BLAME SHIFT ---
  // vm.frameCount - 1 is the internal MOON wrapper (e.g., `<io>`).
  // vm.frameCount - 2 is the user's script that actually made the bad call!
  int blameIndex = (vm.frameCount > 1) ? vm.frameCount - 2 : 0;

  CallFrame *frame = &vm.frames[blameIndex];
  ObjFunction *function = frame->function;

  // Find the exact instruction in the user's script
  size_t instruction = frame->ip - function->chunk.code - 1;
  int line = function->chunk.lines[instruction];

  reportRuntimeError(function->moduleName, line, ERR_RUNTIME, message, hint);

  // --- ADD THE MISSING STACK TRACE ---
  fprintf(stderr, "Stack Trace:\n");
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *traceFrame = &vm.frames[i];
    ObjFunction *traceFunc = traceFrame->function;

    // Prevent underflow on the instruction pointer calculation
    size_t traceInst = (traceFrame->ip > traceFunc->chunk.code)
                           ? traceFrame->ip - traceFunc->chunk.code - 1
                           : 0;
    int traceLine = traceFunc->chunk.lines[traceInst];

    fprintf(stderr, "  > [line %d] in ", traceLine);
    if (traceFunc->name == NULL) {
      fprintf(stderr, "main script\n");
    } else {
      fprintf(stderr, "function %s\n", traceFunc->name->chars);
    }
  }
  fprintf(stderr, "\n");

  exit(70);
}

void freeVM() {
  freeTable(&vm.strings);
  freeTable(&vm.globals);
  freeTable(&vm.loadedModules);
  freeObjects();
  free(vm.grayStack);
  freeSignatureTable();
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
Value peek(int distance) { return vm.stackTop[-1 - distance]; }

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
void defineNative(const char *name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));

  // Read the top two items dynamically!
  tableSet(&vm.globals, peek(1), peek(0));

  pop();
  pop();
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

  initSignatureTable();

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
  initTable(&vm.loadedModules);

  // ==========================================
  // PHASE 1: BOOTSTRAP THE OBJECT UNIVERSE
  // ==========================================
  vm.anyType = defineNativeType("Any");
  vm.typeType = defineNativeType("Type");
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
  defineNativeGetter(vm.stringType, "length", stringLengthGetter);

  for (int i = 0; i < 256; i++) {
    char c = (char)i;
    // copyString takes a pointer to the char and the length (1)
    vm.charStrings[i] = copyString(&c, 1);
  }
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
      return vm.typeType; // <-- A Blueprint is an object of type 'Type'!
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
      &&TARGET_OP_BUILD_UNION,   &&TARGET_OP_GET_PROPERTY,
      &&TARGET_OP_SET_PROPERTY,  &&TARGET_OP_GET_SUBSCRIPT,
      &&TARGET_OP_SET_SUBSCRIPT, &&TARGET_OP_GET_END_INDEX,
      &&TARGET_OP_RANGE,         &&TARGET_OP_FOR_ITER,
      &&TARGET_OP_GET_ITER,      &&TARGET_OP_CALL,
      &&TARGET_OP_TYPE_DEF,      &&TARGET_OP_INSTANTIATE,
      &&TARGET_OP_DEFINE_METHOD, &&TARGET_OP_CAST,
      &&TARGET_OP_LOAD,          &&TARGET_OP_KEEP_LIST,
      &&TARGET_OP_KEEP_DICT,     &&TARGET_OP_RETURN};

#define READ_BYTE() (*ip++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_STRING()                                                          \
  AS_STRING(frame->function->chunk.constants.values[READ_SHORT()])

#define BINARY_OP(valueType, op, opStr)                                        \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      THROW_ERROR(ERR_TYPE,                                                    \
                  "Math operations require numbers. Check if you "             \
                  "accidentally used a string or a list here.",                \
                  "You tried to use the '%s' operator on a %s and a %s.",      \
                  opStr, TYPE_NAME(peek(1)), TYPE_NAME(peek(0)));              \
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

  // --- ADD THIS NEW MACRO ---
  // 3. THE ERROR SYNC MACRO
#define THROW_ERROR(type, hint, ...)                                           \
  do {                                                                         \
    frame->ip = ip; /* Sync the fast local register back to memory! */         \
    runtimeErrorDetailed(type, hint, __VA_ARGS__);                             \
    return INTERPRET_RUNTIME_ERROR;                                            \
  } while (false)

// 4. THE STACK GUARD MACRO
#define EXPECT_STACK(expectedCount)                                            \
  do {                                                                         \
    if ((vm.stackTop - vm.stack) < (expectedCount)) {                          \
      THROW_ERROR(                                                             \
          ERR_RUNTIME,                                                         \
          "The VM attempted to pop more values than existed on the stack. "    \
          "This indicates corrupted bytecode or a compiler bug.",              \
          "Stack Underflow Error: Expected %d items.", (expectedCount));       \
    }                                                                          \
  } while (false)

// 5. THE TYPE REFLECTOR MACRO
// Instantly grabs the English name of any MOON value (e.g., "String", "List",
// "Number")
#define TYPE_NAME(val) (getObjType(val)->name->chars)

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
    THROW_ERROR(ERR_TYPE,
                "You can only add Number+Number, String+Any, or List+Any.",
                "You tried to use '+' to combine a %s and a %s.", TYPE_NAME(a),
                TYPE_NAME(b));
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

  // --- RULE 4: THE REJECTOR ---
  else {
    THROW_ERROR(ERR_TYPE,
                "You can only add Number+Number, String+Any, or List+Any.",
                "You tried to use '+' to combine a %s and a %s.", TYPE_NAME(a),
                TYPE_NAME(b));
  }
  DISPATCH();
}

TARGET_OP_SUBTRACT: {
  BINARY_OP(NUMBER_VAL, -, "-");
  DISPATCH();
}

TARGET_OP_MULTIPLY: {
  BINARY_OP(NUMBER_VAL, *, "*");
  DISPATCH();
}

TARGET_OP_GREATER: {
  BINARY_OP(BOOL_VAL, >, ">");
  DISPATCH();
}

TARGET_OP_LESS: {
  BINARY_OP(BOOL_VAL, <, "<");
  DISPATCH();
}

TARGET_OP_DIVIDE: {
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
    THROW_ERROR(ERR_TYPE,
                "Math operations require numbers. Check if you accidentally "
                "used a string or a list here.",
                "Operands must be numbers.");
  }

  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(pop());

  if (b == 0.0) {
    THROW_ERROR(ERR_RUNTIME,
                "You cannot divide by zero. Check your math logic.",
                "Division by zero.");
  }

  push(NUMBER_VAL(a / b));
  DISPATCH();
}

TARGET_OP_MOD: {
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
    THROW_ERROR(ERR_TYPE, "The modulo operator only works on numbers.",
                "Operands must be numbers.");
  }

  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(pop());

  if (b == 0.0) {
    THROW_ERROR(ERR_RUNTIME,
                "You cannot modulo by zero. Check your math logic.",
                "Modulo by zero.");
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
    THROW_ERROR(ERR_TYPE,
                "You can only use the negative sign (-) on numeric values.",
                "You tried to negate a %s.", TYPE_NAME(peek(0)));
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
    // --- THE ORACLE INTERCEPT ---
    const char *suggestion = findVariableSuggestion(name->chars);

    if (suggestion != NULL) {
      // 1. Build the dynamic hint string first!
      char hintBuf[256];
      snprintf(hintBuf, sizeof(hintBuf),
               "Did you mean '" COLOR_CYAN "%s" COLOR_RESET "'?", suggestion);

      // 2. Throw the error with the properly aligned arguments
      THROW_ERROR(ERR_REFERENCE, hintBuf, "Undefined variable '%s'.",
                  name->chars);
    } else {
      // No close matches found. Fall back to the standard error.
      THROW_ERROR(ERR_REFERENCE,
                  "Did you misspell the variable name, or forget to declare it "
                  "with 'let'?",
                  "Undefined variable '%s'.", name->chars);
    }
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
    // --- THE ORACLE INTERCEPT ---
    const char *suggestion = findVariableSuggestion(name->chars);

    if (suggestion != NULL) {
      // 1. Build the dynamic hint string first!
      char hintBuf[256];
      snprintf(hintBuf, sizeof(hintBuf),
               "Did you mean '" COLOR_CYAN "%s" COLOR_RESET "'?", suggestion);

      // 2. Throw the error with the properly aligned arguments
      THROW_ERROR(ERR_REFERENCE, hintBuf, "Undefined variable '%s'.",
                  name->chars);
    } else {
      // No close matches found. Fall back to the standard error.
      THROW_ERROR(ERR_REFERENCE,
                  "Did you misspell the variable name, or forget to declare it "
                  "with 'let'?",
                  "Undefined variable '%s'.", name->chars);
    }
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

TARGET_OP_NOT: {
  push(BOOL_VAL(isFalsey(pop())));
  DISPATCH();
}

TARGET_OP_BUILD_STRING: {
  uint16_t count = READ_SHORT();
  EXPECT_STACK(count);

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
  EXPECT_STACK(itemCount);

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
  EXPECT_STACK(itemCount * 2);

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

TARGET_OP_BUILD_UNION: {
  uint16_t count = READ_SHORT();
  EXPECT_STACK(count);

  // 1. Create the Union object and protect it from GC
  ObjUnion *unionObj = newUnion(count);
  push(OBJ_VAL(unionObj));

  // 2. Extract the Blueprints from the stack
  Value *typesStart = vm.stackTop - 1 - count;
  for (int i = 0; i < count; i++) {
    if (!IS_TYPE(typesStart[i])) {
      THROW_ERROR(ERR_TYPE, "Unions can only contain valid Type Blueprints.",
                  "Invalid type inside Union.");
    }
    unionObj->types[i] = typesStart[i];
  }

  // 3. Cleanup: Pop the union temporarily, pop the raw types, push the finished
  // union
  pop();
  vm.stackTop -= count;
  push(OBJ_VAL(unionObj));
  DISPATCH();
}

TARGET_OP_INSTANTIATE: {
  uint16_t propCount = READ_SHORT();
  EXPECT_STACK((propCount * 2) + 1);

  // Layout: [Blueprint] [k1] [v1] [k2] [v2] <--- Top
  Value *itemsStart = vm.stackTop - (propCount * 2);
  Value targetVal = *(itemsStart - 1);

  if (!IS_TYPE(targetVal)) {
    THROW_ERROR(ERR_TYPE,
                "Make sure the name belongs to a Blueprint you created using "
                "the 'type' keyword.",
                "Can only instantiate defined types.");
  }

  ObjType *type = AS_TYPE(targetVal);
  // --- THE NATIVE LOCKDOWN FIX ---
  if (type->isNative) {
    THROW_ERROR(ERR_TYPE,
                "Use standard syntax (like '[]' for Lists, or '\"\"' for "
                "Strings) to create native objects.",
                "Cannot instantiate native types (like '%s') directly.",
                type->name->chars);
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
  THROW_ERROR(ERR_REFERENCE,
              "Check your spelling, or verify this property exists on the "
              "object's Blueprint.",
              "Undefined property '%s' on type '%s'.", name->chars,
              type->name->chars);
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
    THROW_ERROR(
        ERR_TYPE,
        "You cannot set properties on primitive values like Numbers or "
        "Strings.",
        "Only object instances and dictionaries have mutable properties.");
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
        THROW_ERROR(ERR_RUNTIME,
                    "You tried to access an index that doesn't exist. "
                    "Remember, MOON lists start at index 1!",
                    "Index out of bounds.");
      }

      int index = userIndex > 0 ? userIndex - 1 : list->count + userIndex;

      if (index < 0 || index >= list->count) {
        THROW_ERROR(ERR_RUNTIME,
                    "You tried to access an index that doesn't exist. "
                    "Remember, MOON lists start at index 1!",
                    "Index out of bounds.");
      }

      pop();
      pop();
      push(list->items[index]);
    } else if (IS_RANGE(indexVal)) {
      ObjRange *range = AS_RANGE(indexVal);
      int start = (int)range->start;
      int end = (int)range->end;
      int step = (int)range->step; // Extract the custom step size!

      // 1-based indexing rules (with negative wrap-around)
      if (start < 0)
        start = list->count + start + 1;
      if (end < 0)
        end = list->count + end + 1;
      start--;
      end--;

      ObjList *resultList = newList();
      push(OBJ_VAL(resultList)); // GC Protection

      // --- GOING FORWARD ---
      if (start <= end) {
        if (start < 0)
          start = 0;
        if (end >= list->count)
          end = list->count - 1;
        for (int i = start; i <= end; i += step) {
          appendList(resultList, list->items[i]);
        }
      }
      // --- GOING BACKWARD ---
      else {
        if (start >= list->count)
          start = list->count - 1;
        if (end < 0)
          end = 0;
        for (int i = start; i >= end; i -= step) {
          appendList(resultList, list->items[i]);
        }
      }

      pop(); // resultList
      pop(); // indexVal
      pop(); // seqVal
      push(OBJ_VAL(resultList));
    } else {
      THROW_ERROR(
          ERR_TYPE,
          "Check what type of value you are passing into the brackets [].",
          "Invalid index type for this collection.");
    }
  } else if (IS_DICT(seqVal)) {
    ObjDict *dict = AS_DICT(seqVal);
    if (!IS_STRING(indexVal)) {
      THROW_ERROR(
          ERR_TYPE,
          "Check what type of value you are passing into the brackets [].",
          "Invalid index type for this collection.");
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
  } else if (IS_STRING(seqVal)) {
    ObjString *str = AS_STRING(seqVal);

    if (IS_NUMBER(indexVal)) {
      int userIndex = (int)AS_NUMBER(indexVal);
      if (userIndex == 0) {
        THROW_ERROR(
            ERR_RUNTIME,
            "You tried to access an index that doesn't exist. Remember, "
            "MOON strings start at index 1!",
            "Index out of bounds.");
      }

      int index = userIndex > 0 ? userIndex - 1 : str->length + userIndex;

      if (index < 0 || index >= str->length) {
        THROW_ERROR(
            ERR_RUNTIME,
            "You tried to access an index that doesn't exist. Remember, "
            "MOON strings start at index 1!",
            "Index out of bounds.");
      }

      uint8_t charByte = (uint8_t)str->chars[index];
      pop();
      pop();
      push(OBJ_VAL(vm.charStrings[charByte]));
    }
    // --- NEW: STRING SLICING ---
    else if (IS_RANGE(indexVal)) {
      ObjRange *range = AS_RANGE(indexVal);
      int start = (int)range->start;
      int end = (int)range->end;
      int step = (int)range->step;

      if (start < 0)
        start = str->length + start + 1;
      if (end < 0)
        end = str->length + end + 1;
      start--;
      end--;

      // Pre-allocate the maximum possible size for the new string
      int maxLen = abs(start - end) + 1;
      char *chars = ALLOCATE(char, maxLen + 1);
      int dest = 0;

      // --- GOING FORWARD ---
      if (start <= end) {
        if (start < 0)
          start = 0;
        if (end >= str->length)
          end = str->length - 1;
        for (int i = start; i <= end; i += step) {
          chars[dest++] = str->chars[i];
        }
      }
      // --- GOING BACKWARD ---
      else {
        if (start >= str->length)
          start = str->length - 1;
        if (end < 0)
          end = 0;
        for (int i = start; i >= end; i -= step) {
          chars[dest++] = str->chars[i];
        }
      }
      chars[dest] = '\0';

      // Hand the C string to the MOON string pool
      ObjString *resultStr = takeString(chars, dest);

      pop();
      pop();
      push(OBJ_VAL(resultStr));
    } else {
      THROW_ERROR(
          ERR_TYPE,
          "Check what type of value you are passing into the brackets [].",
          "Invalid index type for this collection.");
    }
  } else {
    THROW_ERROR(ERR_TYPE,
                "You can only use brackets [] to access items in Lists, "
                "Dictionaries, or Strings.",
                "Type is not subscriptable.");
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
      THROW_ERROR(ERR_TYPE,
                  "Dictionaries require string keys. Did you accidentally use "
                  "a number or variable without quotes?",
                  "Dictionary keys must be strings.");
    }
    tableSet(&dict->fields, indexVal, value);
    pop();
    pop();
    pop();
    push(value);
  } else if (IS_LIST(collectionVal)) {
    ObjList *list = AS_LIST(collectionVal);
    if (!IS_NUMBER(indexVal)) {
      THROW_ERROR(ERR_TYPE,
                  "List indices must be numbers. Check if you accidentally "
                  "passed a string or another type inside the brackets.",
                  "List index must be a number.");
    }
    // --- THE NEGATIVE INDEX FIX (SET) ---
    int userIndex = (int)AS_NUMBER(indexVal);

    if (userIndex == 0) {
      THROW_ERROR(ERR_RUNTIME,
                  "You tried to access an index that doesn't exist. Remember, "
                  "MOON lists start at index 1!",
                  "Index out of bounds.");
    }

    int index = userIndex > 0 ? userIndex - 1 : list->count + userIndex;

    if (index < 0 || index >= list->count) {
      THROW_ERROR(ERR_RUNTIME,
                  "You tried to access an index that doesn't exist. Remember, "
                  "MOON lists start at index 1!",
                  "Index out of bounds.");
    }
    list->items[index] = value;
    pop();
    pop();
    pop();
    push(value);
  } else {
    THROW_ERROR(ERR_TYPE,
                "You can only use bracket assignment (like set target[key] to "
                "value) to modify Lists and Dictionaries.",
                "Can only assign to lists and dictionaries.");
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
    THROW_ERROR(ERR_SYNTAX,
                "The 'end' keyword can only be used inside brackets (like "
                "list[1 to end]) to represent the final item.",
                "Cannot use 'end' index here.");
  }
  DISPATCH();
}

TARGET_OP_RANGE: {
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)) || !IS_NUMBER(peek(2))) {
    THROW_ERROR(ERR_TYPE,
                "Ranges (like 1 to 5 by 2) require numbers for the start, end, "
                "and step.",
                "Range operands must be numbers.");
  }

  double step = AS_NUMBER(pop());
  double end = AS_NUMBER(pop());
  double start = AS_NUMBER(pop());

  // Protect against infinite loops and negative steps
  if (step <= 0) {
    THROW_ERROR(ERR_RUNTIME,
                "When creating a range with a 'by' step, the step size must be "
                "greater than zero.\nThe VM will automatically handle counting "
                "up or down for you.",
                "Range step must be a positive number greater than 0.");
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
    THROW_ERROR(ERR_TYPE,
                "The 'for' loop needs a sequence it can step through.",
                "Can only iterate over ranges, lists, or strings.");
  }
  DISPATCH();
}

  // 2. The Loop Logic
TARGET_OP_FOR_ITER: {
  uint8_t slot = READ_BYTE();
  uint16_t offset = READ_SHORT();

  // The Sequence is ALWAYS stored exactly one slot before the Iterator!
  Value seqVal = frame->slots[slot - 1];
  Value iteratorVal = frame->slots[slot];

  bool hasNext = false;
  Value loopValue = NIL_VAL;

  if (IS_RANGE(seqVal)) {
    ObjRange *range = AS_RANGE(seqVal);
    double index = AS_NUMBER(iteratorVal);
    double nextIndex = index + 1;
    double nextValue;

    if (range->start <= range->end) {
      nextValue = range->start + (nextIndex * range->step);
      if (nextValue <= range->end) {
        frame->slots[slot] = NUMBER_VAL(nextIndex);
        loopValue = NUMBER_VAL(nextValue);
        hasNext = true;
      }
    } else {
      nextValue = range->start - (nextIndex * range->step);
      if (nextValue >= range->end) {
        frame->slots[slot] = NUMBER_VAL(nextIndex);
        loopValue = NUMBER_VAL(nextValue);
        hasNext = true;
      }
    }
  } else if (IS_LIST(seqVal)) {
    ObjList *list = AS_LIST(seqVal);
    double index = AS_NUMBER(iteratorVal);
    double nextIndex = index + 1;

    if (nextIndex < list->count) {
      frame->slots[slot] = NUMBER_VAL(nextIndex);
      loopValue = list->items[(int)nextIndex];
      hasNext = true;
    }
  } else if (IS_STRING(seqVal)) {
    ObjString *string = AS_STRING(seqVal);
    double index = AS_NUMBER(iteratorVal);
    double nextIndex = index + 1;

    if (nextIndex < string->length) {
      frame->slots[slot] = NUMBER_VAL(nextIndex);
      uint8_t charByte = (uint8_t)string->chars[(int)nextIndex];
      loopValue = OBJ_VAL(vm.charStrings[charByte]);
      hasNext = true;
    }
  }

  if (hasNext) {
    // We are entering the loop! Push the item for the body to use.
    push(loopValue);
  } else {
    // We are done! Jump cleanly over the loop body.
    ip += offset;
  }

  DISPATCH();
}

TARGET_OP_CALL: {
  int argCount = READ_SHORT();
  EXPECT_STACK(argCount + 1);

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
      THROW_ERROR(
          ERR_RUNTIME,
          "Check the function signature to see how many values it requires.",
          "Expected %d arguments but got %d.", function->arity, argCount);
    }

    if (vm.frameCount == FRAMES_MAX) {
      THROW_ERROR(ERR_RUNTIME,
                  "You have too many nested function calls. Do you have "
                  "infinite recursion?",
                  "Stack overflow.");
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
      THROW_ERROR(
          ERR_RUNTIME,
          "Check the function signature to see how many values it requires.",
          "Expected %d arguments but got %d.", multi->arity, argCount);
    }

    // Pointer to the first argument on the stack
    Value *args = vm.stackTop - argCount;

    // --- THE SYMMETRIC SCORING ENGINE ---
    ObjFunction *bestMethod = NULL;
    int bestScore = -1;
    bool isAmbiguous = false;

    for (int i = 0; i < multi->methodCount; i++) {
      Value *signature = multi->signatures[i];
      bool isMatch = true;
      int currentScore = 0;

      // Check every argument against the signature
      // Check every argument against the signature
      for (int j = 0; j < argCount; j++) {
        Value expectedVal = signature[j];
        ObjType *actualType = getObjType(args[j]);

        // --- THE UNION CHECK ---
        if (IS_UNION(expectedVal)) {
          ObjUnion *unionObj = AS_UNION(expectedVal);
          bool matchedUnion = false;

          for (int k = 0; k < unionObj->count; k++) {
            if (AS_TYPE(unionObj->types[k]) == actualType) {
              matchedUnion = true;
              break;
            }
          }

          if (matchedUnion) {
            currentScore += 2; // EXACT MATCH inside the union!
          } else {
            isMatch = false; // Hard fail
            break;
          }
        }
        // --- THE STANDARD CHECK ---
        else {
          ObjType *expectedType = AS_TYPE(expectedVal);

          if (expectedType == actualType) {
            currentScore += 2; // EXACT MATCH
          } else if (expectedType == vm.anyType) {
            currentScore += 1; // WILDCARD
          } else {
            isMatch = false; // Hard fail
            break;
          }
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
      THROW_ERROR(ERR_REFERENCE,
                  "The arguments you passed don't match any overloaded version "
                  "of this function.",
                  "No matching signature found.");
    }

    if (isAmbiguous) {
      THROW_ERROR(ERR_RUNTIME,
                  "Multiple methods match this call with the exact same "
                  "specificity. Define an intersection method to clarify.",
                  "Ambiguous Dispatch Error.");
    }

    // --- EXECUTE THE WINNING METHOD ---
    if (vm.frameCount == FRAMES_MAX) {
      THROW_ERROR(ERR_RUNTIME,
                  "You have too many nested function calls. Do you have "
                  "infinite recursion?",
                  "Stack overflow.");
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

  THROW_ERROR(
      ERR_TYPE, "You appended parentheses to a value that isn't executable.",
      "You tried to call a %s as if it were a function.",
      TYPE_NAME(callee)); // We use the 'callee' variable you already extracted!
}

TARGET_OP_TYPE_DEF: {
  ObjString *name = READ_STRING();
  uint16_t propertyCount = READ_SHORT();
  EXPECT_STACK(propertyCount * 2);

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

  // 1. Extract the Expected Types (UPGRADED)
  Value *signatures = ALLOCATE(Value, arity);
  for (int i = 0; i < arity; i++) {
    if (!IS_TYPE(typesStart[i]) && !IS_UNION(typesStart[i])) {
      THROW_ERROR(
          ERR_TYPE,
          "Make sure the type you specified is defined before using it.",
          "Type annotation must resolve to a valid Type Blueprint or Union.");
    }
    signatures[i] = typesStart[i];
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

    // UPGRADED: Cast to Value**
    multi->signatures =
        GROW_ARRAY(Value *, multi->signatures, oldCap, multi->methodCapacity);
  }
  multi->methods[multi->methodCount] = method;
  multi->signatures[multi->methodCount] = signatures;
  multi->methodCount++;

  // 4. Clean up the stack (Pop the method and the types)
  pop();
  vm.stackTop -= arity;
  DISPATCH();
}

TARGET_OP_CAST: {
  Value typeVal = pop(); // The Target Blueprint
  Value val = pop();     // The Raw Data

  if (!IS_TYPE(typeVal)) {
    THROW_ERROR(ERR_TYPE,
                "You can only cast to a valid Type Blueprint (like Number, "
                "String, or a custom type).",
                "Invalid cast target.");
  }
  ObjType *targetType = AS_TYPE(typeVal);

  // --- THE FAST-PATH IDENTITY CHECK ---
  if (targetType == getObjType(val)) {
    push(val); // It's already the correct type! Just push it back and move on.
    DISPATCH();
  }

  // GROUP 0: Reflection (The typeof operator)
  if (targetType == vm.typeType) {
    push(OBJ_VAL(getObjType(val)));
  }
  // GROUP 1: To String (Universal)
  else if (targetType == vm.stringType) {
    if (IS_LIST(val)) {
      ObjList *list = AS_LIST(val);

      // 1. Array to hold the stringified versions of each item
      ObjString **strings = malloc(sizeof(ObjString *) * list->count);
      int totalLength = 0;

      // 2. First Pass: Convert everything to a string and protect it from the
      // GC!
      for (int i = 0; i < list->count; i++) {
        strings[i] = valueToString(list->items[i]);
        push(OBJ_VAL(strings[i])); // Pushed to VM stack to prevent GC sweep
        totalLength += strings[i]->length;
      }

      // 3. Second Pass: Allocate the exact memory needed and smash them
      // together
      char *chars = ALLOCATE(char, totalLength + 1);
      char *dest = chars;
      for (int i = 0; i < list->count; i++) {
        memcpy(dest, strings[i]->chars, strings[i]->length);
        dest += strings[i]->length;
      }
      chars[totalLength] = '\0';

      // 4. Create the final MOON string
      ObjString *result = takeString(chars, totalLength);

      // 5. Cleanup: Pop the temporary strings off the VM stack and free the C
      // array
      vm.stackTop -= list->count;
      free(strings);

      push(OBJ_VAL(result));
    } else {
      // For everything else, use the standard stringifier
      push(OBJ_VAL(valueToString(val)));
    }
  }
  // GROUP 2: To Bool
  else if (targetType == vm.boolType) {
    if (IS_STRING(val)) {
      ObjString *s = AS_STRING(val);
      if (strcmp(s->chars, "true") == 0)
        push(BOOL_VAL(true));
      else if (strcmp(s->chars, "false") == 0)
        push(BOOL_VAL(false));
      else
        THROW_ERROR(ERR_TYPE, "String must be exactly 'true' or 'false'.",
                    "Invalid string to bool cast.");
    } else {
      push(BOOL_VAL(!isFalsey(val)));
    }
  }

  // GROUP 3: To Number
  else if (targetType == vm.numberType) {
    if (IS_STRING(val)) {
      ObjString *s = AS_STRING(val);
      char *end;
      double num = strtod(s->chars, &end);

      if (*end != '\0') {
        // --- THE DESCRIPTIVE ERROR UPGRADE ---
        THROW_ERROR(ERR_TYPE,
                    "Make sure the string only contains digits, a decimal "
                    "point, or a minus sign.",
                    "Cannot cast the string '%s' to a Number.", s->chars);
      }

      push(NUMBER_VAL(num));
    } else if (IS_BOOL(val)) {
      push(NUMBER_VAL(AS_BOOL(val) ? 1.0 : 0.0));
    } else {
      THROW_ERROR(ERR_TYPE, "Can only cast Strings and Bools to Numbers.",
                  "Invalid cast to Number.");
    }
  }

  // GROUP 4a: To List
  else if (targetType == vm.listType) {
    if (IS_RANGE(val)) {
      ObjRange *r = AS_RANGE(val);
      ObjList *l = newList();
      push(OBJ_VAL(l));
      if (r->start <= r->end) {
        for (double i = r->start; i <= r->end; i += r->step)
          appendList(l, NUMBER_VAL(i));
      } else {
        for (double i = r->start; i >= r->end; i -= r->step)
          appendList(l, NUMBER_VAL(i));
      }
      pop();
      push(OBJ_VAL(l));
    } else if (IS_STRING(val)) {
      ObjString *s = AS_STRING(val);
      ObjList *l = newList();
      push(OBJ_VAL(l));
      for (int i = 0; i < s->length; i++) {
        appendList(l, OBJ_VAL(vm.charStrings[(uint8_t)s->chars[i]]));
      }
      pop();
      push(OBJ_VAL(l));
    } else if (IS_NUMBER(val)) {
      // 1. We cheat! We convert the number to a string first so we can parse
      // its characters.
      ObjString *s = valueToString(val);
      push(OBJ_VAL(s)); // GC Protection

      ObjList *l = newList();
      push(OBJ_VAL(l)); // GC Protection

      // 2. Loop through the stringified number
      for (int i = 0; i < s->length; i++) {
        char c = s->chars[i];

        if (c >= '0' && c <= '9') {
          // If it's a digit, subtract '0' (ASCII 48) to get the actual math
          // number!
          appendList(l, NUMBER_VAL(c - '0'));
        } else {
          // If it's a decimal point (or a minus sign/exponent), keep it as a
          // string
          appendList(l, OBJ_VAL(vm.charStrings[(uint8_t)c]));
        }
      }

      // 3. Clean up the stack
      pop();            // Pop list
      pop();            // Pop string
      push(OBJ_VAL(l)); // Push list back as the final result
    } else if (IS_DICT(val)) {
      ObjDict *d = AS_DICT(val);
      ObjList *l = newList();
      push(OBJ_VAL(l));
      for (int i = 0; i < d->fields.capacity; i++) {
        Entry *e = &d->fields.entries[i];
        if (!IS_EMPTY(e->key) && !IS_TOMB(e->key)) {
          ObjList *pair = newList();
          push(OBJ_VAL(pair));
          appendList(pair, e->key);
          appendList(pair, e->value);
          pop();
          appendList(l, OBJ_VAL(pair));
        }
      }
      pop();
      push(OBJ_VAL(l));
    } else {
      THROW_ERROR(ERR_TYPE, "Cannot cast this type to a List.",
                  "Invalid cast to List.");
    }
  }
  // GROUP 4b: To Dict
  else if (targetType == vm.dictType) {
    if (IS_LIST(val)) {
      ObjList *l = AS_LIST(val);
      ObjDict *d = newDict();
      push(OBJ_VAL(d));
      for (int i = 0; i < l->count; i++) {
        if (!IS_LIST(l->items[i]))
          THROW_ERROR(ERR_TYPE,
                      "List items must be key-value pairs (lists of 2) to cast "
                      "to Dict.",
                      "Invalid list to dict cast.");
        ObjList *pair = AS_LIST(l->items[i]);
        if (pair->count != 2)
          THROW_ERROR(ERR_TYPE, "Pairs must have exactly 2 items.",
                      "Invalid list to dict cast.");
        tableSet(&d->fields, pair->items[0], pair->items[1]);
      }
      pop();
      push(OBJ_VAL(d));
    } else if (IS_INSTANCE(val)) {
      ObjInstance *inst = AS_INSTANCE(val);
      ObjDict *d = newDict();
      push(OBJ_VAL(d));
      tableAddAll(&inst->fields, &d->fields);
      pop();
      push(OBJ_VAL(d));
    } else {
      THROW_ERROR(ERR_TYPE, "Cannot cast this type to a Dict.",
                  "Invalid cast to Dict.");
    }
  }
  // GROUP 5: Hydration (Dict to Custom Type)
  else if (!targetType->isNative) {
    if (!IS_DICT(val))
      THROW_ERROR(ERR_TYPE, "Can only hydrate a Blueprint using a Dictionary.",
                  "Invalid hydration cast.");
    ObjDict *d = AS_DICT(val);
    ObjInstance *inst = newInstance(targetType);
    push(OBJ_VAL(inst));

    tableAddAll(&targetType->properties, &inst->fields); // Load defaults

    // Apply overrides and enforce strictness
    for (int i = 0; i < d->fields.capacity; i++) {
      Entry *e = &d->fields.entries[i];
      if (!IS_EMPTY(e->key) && !IS_TOMB(e->key)) {
        Value dummy;
        if (!tableGet(&targetType->properties, e->key, &dummy)) {
          THROW_ERROR(ERR_TYPE,
                      "Dictionary contains a key not present on the Blueprint.",
                      "Strict hydration failure.");
        }
        tableSet(&inst->fields, e->key, e->value);
      }
    }
    pop();
    push(OBJ_VAL(inst));
  } else {
    THROW_ERROR(ERR_TYPE, "Cannot cast to this native type directly.",
                "Invalid cast target.");
  }

  DISPATCH();
}

TARGET_OP_LOAD: {
  // The compiler pushed the file path string right before calling OP_LOAD
  ObjString *path = AS_STRING(pop());

  // --- 1. THE SHIELD ---
  Value dummy;
  if (tableGet(&vm.loadedModules, OBJ_VAL(path), &dummy)) {
    // We already ran this file! Skip it to prevent infinite loops.
    DISPATCH();
  }

  // Lock the door: mark this file as loaded
  tableSet(&vm.loadedModules, OBJ_VAL(path), BOOL_VAL(true));

  // --- 2. COMPILE ON THE FLY ---
  // Read the file from the OS
  char *source = readFile(path->chars);
  if (source == NULL) {
    THROW_ERROR(ERR_RUNTIME, "Make sure the file exists.",
                "Could not open module '%s'.", path->chars);
  }

  // 1. Vault the Source Code!
  ObjString *sourceStr = copyString(source, strlen(source));
  push(OBJ_VAL(sourceStr)); // GC Protection
  tableSet(&vm.loadedModules, OBJ_VAL(path), OBJ_VAL(sourceStr));

  // 2. Compile the module with its path as the name!
  ObjFunction *moduleFunc = compile(source, path);
  free(source); // We can safely free the C-string because the Vault owns the
                // ObjString copy!

  pop(); // Pop sourceStr off the stack

  if (moduleFunc == NULL) {
    THROW_ERROR(
        ERR_RUNTIME,
        "There is a syntax error inside the module you are trying to load.",
        "Failed to compile '%s'.", path->chars);
  }

  // Protect the new function from Garbage Collection
  push(OBJ_VAL(moduleFunc));

  // --- 3. THE CONTEXT SWITCH ---
  if (vm.frameCount == FRAMES_MAX) {
    THROW_ERROR(ERR_RUNTIME, "Too many files loading each other.",
                "Stack overflow.");
  }

  // A. Save our exact spot in the CURRENT script so we can return here later!
  frame->ip = ip;

  // B. Create a brand new CallFrame for the module
  CallFrame *newFrame = &vm.frames[vm.frameCount++];
  newFrame->function = moduleFunc;
  newFrame->ip = moduleFunc->chunk.code;
  newFrame->slots = vm.stackTop - 1; // The module gets its own local variables

  // C. Hijack the VM's brain!
  frame = newFrame;
  ip = frame->ip;

  // D. Resume execution. The VM is now reading instructions from the new file!
  DISPATCH();
}

TARGET_OP_KEEP_LIST: {
  // 1. The compiler compiled the local slot index as the operand
  uint8_t slot = READ_BYTE();

  // 2. Grab the Accumulator list from the local stack slot
  Value listVal = frame->slots[slot];

  // 3. Grab the evaluated expression off the top of the stack
  EXPECT_STACK(1);
  Value item = pop();

  // Sanity check (The C-compiler should guarantee this is always true)
  if (!IS_LIST(listVal)) {
    THROW_ERROR(ERR_RUNTIME, "Fatal Compiler Bug: Accumulator is not a list.",
                "Internal Error");
  }

  // 4. Drop the item into the list!
  appendList(AS_LIST(listVal), item);

  DISPATCH();
}

TARGET_OP_KEEP_DICT: {
  uint8_t slot = READ_BYTE();
  Value dictVal = frame->slots[slot];

  EXPECT_STACK(2);
  Value value = pop(); // The value was evaluated last
  Value key = pop();   // The key was evaluated first

  if (!IS_DICT(dictVal)) {
    THROW_ERROR(ERR_RUNTIME, "Fatal Compiler Bug: Accumulator is not a dict.",
                "Internal Error");
  }

  if (!IS_STRING(key)) {
    THROW_ERROR(ERR_TYPE,
                "Dictionary keys must be strings. Did you try to 'keep' a "
                "number or a list as a key?",
                "Invalid dictionary key in comprehension.");
  }

  // Drop the Key-Value pair into the Hash Table!
  tableSet(&AS_DICT(dictVal)->fields, key, value);

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

static void compileAndRunWrapper(const char *nameStr, const char *sourceStr) {
  ObjString *name = copyString(nameStr, (int)strlen(nameStr));
  push(OBJ_VAL(name));
  ObjString *src = copyString(sourceStr, (int)strlen(sourceStr));
  push(OBJ_VAL(src));
  tableSet(&vm.loadedModules, OBJ_VAL(name), OBJ_VAL(src));

  ObjFunction *func = compile(sourceStr, name);

  if (func != NULL) {
    push(OBJ_VAL(func));
    CallFrame *frame = &vm.frames[vm.frameCount++];
    frame->function = func;
    frame->ip = func->chunk.code;
    frame->slots = vm.stackTop - 1;
    run();
  }

  pop();
  pop();
}

static void bootstrapCore() {
  MoonModule standardLibraries[] = {
      {"<core>", coreLibrary, registerCoreLibrary},
      {"<math>", mathBootstrap, registerMathLibrary},
      {"<string>", stringBootstrap, registerStringLibrary},
      {"<list>", listBootstrap, registerListLibrary},
      {"<io>", ioBootstrap, registerIOLibrary},
      {NULL, NULL, NULL}};

  bool previousDebug = vm.debugMode;
  bool previousAstFlag = printAstFlag;

  vm.debugMode = false;
  printAstFlag = false;

  for (int i = 0; standardLibraries[i].name != NULL; i++) {
    standardLibraries[i].registerCFunctions();
    compileAndRunWrapper(standardLibraries[i].name,
                         standardLibraries[i].moonWrapperSource);
  }

  vm.debugMode = previousDebug;
  printAstFlag = previousAstFlag;
}

bool isCoreBootstrapped = false;

InterpretResult interpret(const char *source) {
  // --- THE BOOTSTRAP GUARD ---
  if (!isCoreBootstrapped) {
    initErrorEngine(coreLibrary); // Point errors to the embedded string
    bootstrapCore();              // Compile & run the core
    isCoreBootstrapped = true;    // Lock the door!
  }

  // Give the error engine the raw string BEFORE compilation begins!
  initErrorEngine(source);

  // 1. Vault the Main Script
  ObjString *mainName = copyString("<main>", 6);
  push(OBJ_VAL(mainName));
  ObjString *mainSrc = copyString(source, strlen(source));
  push(OBJ_VAL(mainSrc));
  tableSet(&vm.loadedModules, OBJ_VAL(mainName), OBJ_VAL(mainSrc));

  // 2. Compile with the tag
  ObjFunction *function = compile(source, mainName);

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
