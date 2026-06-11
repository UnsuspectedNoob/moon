// vm.c

#include "vm.h"
#include "cast.h"
#include "lib_core.h"
#include "lib_io.h"
#include "lib_list.h"
#include "lib_math.h"
#include "lib_string.h"
#include "sigtrie.h"
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "debug.h"
#include "emitter.h"
#include "memory.h"
#include "object.h"
#include "subscript.h"
#include "table.h"
#include "value.h"

VM vm; // Define the global VM instance here
extern char *readFile(const char *path);

static void resetStack() { vm.stackTop = vm.stack; }

// ==========================================
// THE ORDEAL RUNTIME ERROR ENGINE
// ==========================================

// The Master Runtime Error function that talks to error.c
void runtimeErrorDetailed(ErrorType type, const char *hint, const char *format,
                          ...) {
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
  reportRuntimeError(function->module ? function->module->name : NULL, line,
                     type, message, hint);

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

  reportRuntimeError(function->module ? function->module->name : NULL, line,
                     ERR_RUNTIME, message, hint);

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
  freePropertySignatureTable();
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

Value peek(int distance) { return vm.stackTop[-1 - distance]; }

bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  ObjString *right = AS_STRING(peek(0));
  ObjString *left = AS_STRING(peek(1));

  // The Magic: Instantly create a pointer node instead of copying memory!
  ObjString *result = takeRope(left, right);

  pop();
  pop();
  push(OBJ_VAL(result));
}

bool valuesEqual(Value a, Value b) {
  // Fast path: Exact same memory address, or identical numbers/booleans
  if (a == b)
    return true;

  // --- THE IEEE 754 EPSILON SHIELD ---
  // If they are both numbers but failed the exact bit-match above,
  // check if they are microscopically close!
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    double numA = AS_NUMBER(a);
    double numB = AS_NUMBER(b);
    // 1e-14 is tight enough to fix 0.1+0.2=0.3, but precise enough to not break
    // real math
    return fabs(numA - numB) < 1e-14;
  }
  // -----------------------------------

  // The String Liberation: Deep character comparison
  if (IS_STRING(a) && IS_STRING(b)) {
    ObjString *aStr = AS_STRING(a);
    ObjString *bStr = AS_STRING(b);

    // If the lengths don't match, don't bother checking characters!
    if (aStr->length != bStr->length)
      return false;

    // We must ensure both strings are flattened before we compare them!
    flattenString(aStr);
    flattenString(bStr);

    // Now that they are flat, check their hashes first (O(1) comparison)
    if (aStr->hash != bStr->hash)
      return false;

    // Finally, verify the actual bytes
    return memcmp(aStr->chars, bStr->chars, aStr->length) == 0;
  }

  return false;
}

// Helper to define natives
void defineNative(const char *name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));

  // Read the top two items dynamically!
  tableSet(&vm.globals, peek(1), peek(0));

  pop();
  pop();
}

void defineModuleNative(ObjModule *module, const char *name,
                        NativeFn function) {
  // Push the name as a string constant
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  // Push the native function object
  push(OBJ_VAL(newNative(function)));

  // Directly bind into the module's hash table
  tableSet(&module->fields, peek(1), peek(0));

  pop(); // pop native function
  pop(); // pop name
}

void registerNativePhrasal(ObjModule *module, const char *root,
                           const char *path, int arity, const char *mangledName,
                           NativeFn function, ObjType **expectedTypes) {
  // 1. Build the phrasal Trie
  registerSignature(root, path, mangledName);

  // 2. Set up the MultiFunction struct
  ObjString *nameStr = copyString(mangledName, (int)strlen(mangledName));
  push(OBJ_VAL(nameStr));

  Table *targetTable = module != NULL ? &module->fields : &vm.globals;

  Value existing;
  ObjMultiFunction *multi = NULL;
  if (tableGet(targetTable, OBJ_VAL(nameStr), &existing) &&
      IS_MULTI_FUNCTION(existing)) {
    multi = AS_MULTI_FUNCTION(existing);
  } else {
    multi = newMultiFunction(nameStr, arity);
    push(OBJ_VAL(multi));
    tableSet(targetTable, OBJ_VAL(nameStr), OBJ_VAL(multi));

    // --- ROOT NAME ALIAS ---
    const char *dollar = strchr(mangledName, '$');
    if (dollar != NULL) {
      int rootLen = (int)(dollar - mangledName);
      ObjString *rootName = copyString(mangledName, rootLen);
      push(OBJ_VAL(rootName)); // GC shield
      tableSet(targetTable, OBJ_VAL(rootName), OBJ_VAL(multi));
      pop(); // pop rootName
    }
    // -----------------------

    pop(); // pop multi
  }

  // Create signatures array
  Value *signatures = NULL;
  if (arity > 0 && expectedTypes != NULL) {
    signatures = ALLOCATE(Value, arity);
    for (int i = 0; i < arity; i++) {
      signatures[i] = OBJ_VAL(expectedTypes[i]);
    }
  }

  // Protect signatures while arrays expand
  for (int i = 0; i < arity; i++) {
    if (signatures)
      push(signatures[i]);
  }

  if (multi->methodCapacity < multi->methodCount + 1) {
    int oldCap = multi->methodCapacity;
    multi->methodCapacity = GROW_CAPACITY(oldCap);
    multi->methods =
        GROW_ARRAY(Value, multi->methods, oldCap, multi->methodCapacity);
    multi->signatures =
        GROW_ARRAY(Value *, multi->signatures, oldCap, multi->methodCapacity);
  }

  ObjNative *native = newNative(function);
  push(OBJ_VAL(native));

  multi->methods[multi->methodCount] = OBJ_VAL(native);
  multi->signatures[multi->methodCount] = signatures;
  multi->methodCount++;

  pop(); // native
  for (int i = 0; i < arity; i++) {
    if (signatures)
      pop(); // signatures
  }
  pop(); // nameStr
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
  vm.sequenceCount = 0;

  initSignatureTable();
  initPropertySignatureTable();

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
  vm.moduleType = defineNativeType("Module");

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
    case OBJ_MODULE:
      return vm.moduleType;
    default:
      return vm.anyType;
    }
  }

  return vm.anyType; // Absolute fallback
}

// ==========================================
// METHOD RESOLUTION
// ==========================================

static Value resolveOverload(ObjMultiFunction *multi, int argCount, Value *args, bool *isAmbiguous) {
  Value bestMethodVal = NIL_VAL;
  int bestScore = -1;
  *isAmbiguous = false;

  for (int i = 0; i < multi->methodCount; i++) {
    Value *signature = multi->signatures[i];
    bool isMatch = true;
    int currentScore = 0;

    for (int j = 0; j < argCount; j++) {
      Value expectedVal = signature[j];
      ObjType *actualType = getObjType(args[j]);

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
          currentScore += (15 - unionObj->count);
        } else {
          isMatch = false;
          break;
        }
      } else {
        ObjType *expectedType = AS_TYPE(expectedVal);
        if (expectedType == actualType) {
          currentScore += 20;
        } else if (expectedType == vm.anyType) {
          currentScore += 1;
        } else {
          isMatch = false;
          break;
        }
      }
    }

    if (isMatch) {
      if (currentScore > bestScore) {
        bestScore = currentScore;
        bestMethodVal = multi->methods[i];
        *isAmbiguous = false;
      } else if (currentScore == bestScore) {
        *isAmbiguous = true;
      }
    }
  }

  return bestMethodVal;
}

// Run()
static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
  register uint8_t *ip = frame->ip;
  uint8_t instruction;

  // 1. THE DISPATCH TABLE
  // WARNING: This must exactly match the order of the OpCode enum in chunk.h!
  static void *dispatchTable[] = {&&TARGET_OP_CONSTANT,
                                  &&TARGET_OP_CONSTANT_LONG,
                                  &&TARGET_OP_PUSH_BYTE,
                                  &&TARGET_OP_NIL,
                                  &&TARGET_OP_TRUE,
                                  &&TARGET_OP_FALSE,
                                  &&TARGET_OP_POP,
                                  &&TARGET_OP_POP_N,
                                  &&TARGET_OP_GET_LOCAL,
                                  &&TARGET_OP_SET_LOCAL,
                                  &&TARGET_OP_GET_LOCAL_LONG,
                                  &&TARGET_OP_SET_LOCAL_LONG,
                                  &&TARGET_OP_GET_GLOBAL,
                                  &&TARGET_OP_DEFINE_GLOBAL,
                                  &&TARGET_OP_SET_GLOBAL,
                                  &&TARGET_OP_ADD,
                                  &&TARGET_OP_ADD_INPLACE,
                                  &&TARGET_OP_SUBTRACT,
                                  &&TARGET_OP_MULTIPLY,
                                  &&TARGET_OP_DIVIDE,
                                  &&TARGET_OP_MOD,
                                  &&TARGET_OP_JUMP_IF_FALSE,
                                  &&TARGET_OP_NEGATE,
                                  &&TARGET_OP_JUMP,
                                  &&TARGET_OP_LOOP,
                                  &&TARGET_OP_EQUAL,
                                  &&TARGET_OP_GREATER,
                                  &&TARGET_OP_LESS,
                                  &&TARGET_OP_NOT,
                                  &&TARGET_OP_BUILD_STRING,
                                  &&TARGET_OP_BUILD_LIST,
                                  &&TARGET_OP_BUILD_DICT,
                                  &&TARGET_OP_BUILD_UNION,
                                  &&TARGET_OP_GET_PROPERTY,
                                  &&TARGET_OP_SET_PROPERTY,
                                  &&TARGET_OP_GET_SUBSCRIPT,
                                  &&TARGET_OP_SET_SUBSCRIPT,
                                  &&TARGET_OP_GET_END_INDEX,
                                  &&TARGET_OP_RANGE,
                                  &&TARGET_OP_FOR_ITER,
                                  &&TARGET_OP_FOR_ITER_LONG,
                                  &&TARGET_OP_GET_ITER,
                                  &&TARGET_OP_GET_ITER_VALUE,
                                  &&TARGET_OP_GET_ITER_VALUE_LONG,
                                  &&TARGET_OP_CALL,
                                  &&TARGET_OP_TYPE_DEF,
                                  &&TARGET_OP_INSTANTIATE,
                                  &&TARGET_OP_DEFINE_METHOD,
                                  &&TARGET_OP_CAST,
                                  &&TARGET_OP_LOAD,
                                  &&TARGET_OP_KEEP_LIST,
                                  &&TARGET_OP_KEEP_DICT,
                                  &&TARGET_OP_RETURN,
                                  &&TARGET_OP_PUSH_SEQUENCE,
                                  &&TARGET_OP_PUSH_STICKY,
                                  &&TARGET_OP_POP_STICKY,
                                  &&TARGET_OP_SET_STICKY,
                                  &&TARGET_OP_LOAD_STICKY,
                                  &&TARGET_OP_SHOW_REPL,
                                  &&TARGET_OP_INVOKE,
                                  &&TARGET_OP_DEFINE_EXTENSION_METHOD};

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
    vm.stackTop[-1] = valueType(AS_NUMBER(vm.stackTop[-1]) op b);              \
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
                                                                               \
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

TARGET_OP_PUSH_BYTE: {
  push(NUMBER_VAL((double)READ_BYTE()));
  DISPATCH();
}

TARGET_OP_ADD: {
  Value b = peek(0); // Right operand
  Value a = peek(1); // Left operand

  // --- RULE 1: STRICT MATH (Number + Number) ---
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    double right = AS_NUMBER(pop());
    vm.stackTop[-1] = NUMBER_VAL(AS_NUMBER(vm.stackTop[-1]) + right);
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

    // 2. Pre-calculate precise capacity
    int newCapacity = leftList->count;
    if (IS_LIST(b)) {
      newCapacity += AS_LIST(b)->count;
    } else {
      newCapacity += 1;
    }

    if (newCapacity > 0) {
      newListObj->capacity = newCapacity;
      newListObj->items = ALLOCATE(Value, newCapacity);

      // 3. Clone the original list into the new one (O(1) block copy)
      if (leftList->count > 0) {
        memcpy(newListObj->items, leftList->items,
               sizeof(Value) * leftList->count);
      }
      newListObj->count = leftList->count;

      // 4. The Fork: Are we merging a list, or appending a single item?
      if (IS_LIST(b)) {
        ObjList *rightList = AS_LIST(b);
        if (rightList->count > 0) {
          memcpy(&newListObj->items[newListObj->count], rightList->items,
                 sizeof(Value) * rightList->count);
          newListObj->count += rightList->count;
        }
      } else {
        newListObj->items[newListObj->count++] = b;
      }
    }

    // 5. Clean up the stack
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
    vm.stackTop[-1] = NUMBER_VAL(AS_NUMBER(vm.stackTop[-1]) + right);
  }

  // RULE 2: Lists (THE O(1) MUTATION FIX)
  else if (IS_LIST(a)) {
    ObjList *list = AS_LIST(a); // We mutate this directly!

    if (IS_LIST(b)) {
      ObjList *rightList = AS_LIST(b);
      int addedCount = rightList->count;

      if (addedCount > 0) {
        if (list->capacity < list->count + addedCount) {
          int newCapacity = list->count + addedCount;
          list->items =
              GROW_ARRAY(Value, list->items, list->capacity, newCapacity);
          list->capacity = newCapacity;
        }
        memcpy(&list->items[list->count], rightList->items,
               sizeof(Value) * addedCount);
        list->count += addedCount;
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
  double a = AS_NUMBER(vm.stackTop[-1]);

  if (b == 0.0) {
    THROW_ERROR(ERR_RUNTIME,
                "Make sure the right side of the operation isn't evaluating to "
                "0 before doing this math.",
                "You cannot divide by zero. Check your math logic.");
  }

  vm.stackTop[-1] = NUMBER_VAL(a / b);
  DISPATCH();
}

TARGET_OP_MOD: {
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
    THROW_ERROR(ERR_TYPE, "The modulo operator only works on numbers.",
                "Operands must be numbers.");
  }

  double b = AS_NUMBER(pop());
  double a = AS_NUMBER(vm.stackTop[-1]);

  if (b == 0.0) {
    THROW_ERROR(ERR_RUNTIME,
                "Make sure the right side of the operation isn't evaluating to "
                "0 before doing this math.",
                "You cannot modulo by zero. Check your math logic.");
  }

  int intA = (int)a;
  int intB = (int)b;

  if (a == intA && b == intB) {
    int mod = intA % intB;
    if (mod < 0)
      mod += (intB < 0 ? -intB : intB); // Force positive wrap-around
    vm.stackTop[-1] = NUMBER_VAL(mod);
  } else {
    double mod = fmod(a, b);
    if (mod < 0.0)
      mod += (b < 0.0 ? -b : b);
    vm.stackTop[-1] = NUMBER_VAL(mod);
  }
  DISPATCH();
}

TARGET_OP_NEGATE: {
  if (!IS_NUMBER(peek(0))) {
    THROW_ERROR(ERR_TYPE,
                "You can only use the negative sign (-) on numeric values.",
                "You tried to negate a %s.", TYPE_NAME(peek(0)));
  }
  vm.stackTop[-1] = NUMBER_VAL(-AS_NUMBER(vm.stackTop[-1]));
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

TARGET_OP_POP_N: {
  vm.stackTop -= READ_BYTE();
  DISPATCH();
}

TARGET_OP_DEFINE_GLOBAL: {
  ObjString *name = READ_STRING();

  // --- THE REDECLARATION SHIELD ---
  Value dummy;
  if (tableGet(frame->globals, OBJ_VAL(name), &dummy)) {
    THROW_ERROR(ERR_REFERENCE,
                "To change the value of an existing variable, use the 'set' "
                "keyword (e.g., 'set a to 30').",
                "Variable '%s' has already been declared.", name->chars);
  }
  // --------------------------------

  tableSet(frame->globals, OBJ_VAL(name), peek(0));
  DISPATCH();
}

TARGET_OP_GET_GLOBAL: {
  ObjString *name = READ_STRING();
  Value value;

  if (!tableGet(frame->globals, OBJ_VAL(name), &value)) {
    // Fallback to Universe scope (vm.globals)
    if (!tableGet(&vm.globals, OBJ_VAL(name), &value)) {
      // --- THE ORACLE INTERCEPT ---
      const char *suggestion =
          findVariableSuggestion(frame->globals, name->chars);
      if (suggestion == NULL)
        suggestion = findVariableSuggestion(&vm.globals, name->chars);

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
        THROW_ERROR(
            ERR_REFERENCE,
            "Did you misspell the variable name, or forget to declare it "
            "with 'let'?",
            "Undefined variable '%s'.", name->chars);
      }
    }
  }

  push(value);
  DISPATCH();
}

TARGET_OP_SET_GLOBAL: {
  ObjString *name = READ_STRING();

  Value dummy;
  // Try to set in current module first
  if (tableGet(frame->globals, OBJ_VAL(name), &dummy)) {
    tableSet(frame->globals, OBJ_VAL(name), peek(0));
  } else if (tableGet(&vm.globals, OBJ_VAL(name), &dummy)) {
    // Check if it exists in Universe scope instead
    tableSet(&vm.globals, OBJ_VAL(name), peek(0));
  } else {
    // --- THE ORACLE INTERCEPT ---
    const char *suggestion =
        findVariableSuggestion(frame->globals, name->chars);
    if (suggestion == NULL)
      suggestion = findVariableSuggestion(&vm.globals, name->chars);

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

TARGET_OP_GET_LOCAL_LONG: {
  uint16_t slot = READ_SHORT();
  push(frame->slots[slot]);
  DISPATCH();
}

TARGET_OP_SET_LOCAL_LONG: {
  uint16_t slot = READ_SHORT();
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
  vm.stackTop[-1] = BOOL_VAL(valuesEqual(vm.stackTop[-1], b));
  DISPATCH();
}

TARGET_OP_NOT: {
  vm.stackTop[-1] = BOOL_VAL(isFalsey(vm.stackTop[-1]));
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
  if (itemCount > 0) {
    list->capacity = itemCount;
    list->items = ALLOCATE(Value, itemCount);
  }

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
      double step = range->step;

      // --- THE FLOATING-POINT DRIFT FIX ---
      // Calculate exact number of steps safely using round() to absorb float
      // fuzziness
      if (start <= end && step > 0) {
        int count = (int)round((end - start) / step) + 1;

        if (list->capacity < list->count + count) {
          int newCapacity = list->count + count;
          list->items =
              GROW_ARRAY(Value, list->items, list->capacity, newCapacity);
          list->capacity = newCapacity;
        }

        for (int k = 0; k < count; k++) {
          list->items[list->count++] = NUMBER_VAL(start + (k * step));
        }
      } else if (start >= end && step > 0) {
        int count = (int)round((start - end) / step) + 1;

        if (list->capacity < list->count + count) {
          int newCapacity = list->count + count;
          list->items =
              GROW_ARRAY(Value, list->items, list->capacity, newCapacity);
          list->capacity = newCapacity;
        }

        for (int k = 0; k < count; k++) {
          list->items[list->count++] = NUMBER_VAL(start - (k * step));
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

  // --- THE MODULE FIX ---
  else if (IS_MODULE(target)) {
    ObjModule *module = AS_MODULE(target);
    if (tableGet(&module->fields, OBJ_VAL(name), &value)) {
      pop();
      push(value);
      DISPATCH();
    }
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
  } else if (IS_MODULE(target)) {
    ObjModule *module = AS_MODULE(target);
    tableSet(&module->fields, OBJ_VAL(name), value);
  } else {
    THROW_ERROR(ERR_TYPE,
                "You cannot set properties on primitive values like Numbers or "
                "Strings.",
                "Only object instances, dictionaries, and modules have mutable "
                "properties.");
  }

  pop();       // Pop value
  pop();       // Pop target
  push(value); // Push value back for chained assignments!
  DISPATCH();
}

TARGET_OP_GET_SUBSCRIPT: {
  Value indexVal = pop();
  Value seqVal = pop();
  Value result;

  if (!executeGetSubscript(seqVal, indexVal, &result)) {
    frame->ip = ip; // Sync register back to memory
    return INTERPRET_RUNTIME_ERROR;
  }

  push(result);
  vm.sequenceCount--; // Clean up sequence tracking implicitly
  DISPATCH();
}

TARGET_OP_SET_SUBSCRIPT: {
  Value value = pop();
  Value indexVal = pop();
  Value collectionVal = pop();
  Value result;

  if (!executeSetSubscript(collectionVal, indexVal, value, &result)) {
    frame->ip = ip;
    return INTERPRET_RUNTIME_ERROR;
  }

  push(result);       // Push value back for chained assignments
  vm.sequenceCount--; // Clean up sequence tracking implicitly
  DISPATCH();
}

TARGET_OP_PUSH_SEQUENCE: {
  vm.sequenceStack[vm.sequenceCount++] = peek(0); // Save a copy to the tracker
  DISPATCH();
}

TARGET_OP_PUSH_STICKY: {
  vm.stickyStack[vm.stickyCount++] = frame->stickySubject;
  DISPATCH();
}

TARGET_OP_POP_STICKY: {
  frame->stickySubject = vm.stickyStack[--vm.stickyCount];
  DISPATCH();
}

TARGET_OP_SET_STICKY: {
  frame->stickySubject = peek(0);
  DISPATCH();
}

TARGET_OP_LOAD_STICKY: {
  if (IS_NIL(frame->stickySubject)) {
    THROW_ERROR(ERR_SYNTAX, "This operator is missing its left side.",
                "It looks like you used an operator (like '<', '>=', or 'is') "
                "without providing a value on its left, and there is no active "
                "subject to attach it to.");
  }
  push(frame->stickySubject);
  DISPATCH();
}

TARGET_OP_GET_END_INDEX: {
  if (vm.sequenceCount == 0) {
    THROW_ERROR(ERR_SYNTAX,
                "The 'end' keyword can only be used inside brackets (like "
                "list[1 to end]).",
                "Cannot use 'end' index here.");
  }

  // Instantly grab the active collection!
  Value seq = vm.sequenceStack[vm.sequenceCount - 1];

  if (IS_LIST(seq)) {
    push(NUMBER_VAL(AS_LIST(seq)->count));
  } else if (IS_STRING(seq)) {
    push(NUMBER_VAL(AS_STRING(seq)->length));
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
  Value seq = peek(0);

  // ADD IS_DICT TO THE ALLOW LIST!
  if (IS_LIST(seq) || IS_RANGE(seq) || IS_STRING(seq) || IS_DICT(seq)) {
    push(NUMBER_VAL(-1));
  } else {
    THROW_ERROR(ERR_TYPE,
                "The 'for' loop needs a sequence it can step through.",
                "Can only iterate over ranges, lists, strings, or dicts.");
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
  } else if (IS_DICT(seqVal)) {
    // --- THE NEW HASH TABLE TRAVERSAL ---
    ObjDict *dict = AS_DICT(seqVal);
    int index = (int)AS_NUMBER(iteratorVal);
    index++; // Move to next slot in the raw memory array

    while (index < dict->fields.capacity) {
      Entry *entry = &dict->fields.entries[index];
      // Skip empty slots and deleted tombstones!
      if (!IS_EMPTY(entry->key) && !IS_TOMB(entry->key)) {
        frame->slots[slot] = NUMBER_VAL(index); // Save internal index
        loopValue = entry->key;                 // Yield the Key!
        hasNext = true;
        break;
      }
      index++;
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

TARGET_OP_FOR_ITER_LONG: {
  uint16_t slot = READ_SHORT();
  uint16_t offset = READ_SHORT();

  Value seqVal = frame->slots[slot - 1];
  Value iteratorVal = frame->slots[slot];

  bool hasNext = false;
  Value loopValue = NIL_VAL;

  if (IS_RANGE(seqVal)) {
    ObjList *range = AS_LIST(seqVal);
    double index = AS_NUMBER(iteratorVal);
    double nextIndex = index + 1;

    if (nextIndex < range->count) {
      frame->slots[slot] = NUMBER_VAL(nextIndex);
      loopValue = range->items[(int)nextIndex];
      hasNext = true;
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
  } else if (IS_DICT(seqVal)) {
    ObjDict *dict = AS_DICT(seqVal);
    int index = (int)AS_NUMBER(iteratorVal);
    index++;

    while (index < dict->fields.capacity) {
      Entry *entry = &dict->fields.entries[index];
      if (!IS_EMPTY(entry->key) && !IS_TOMB(entry->key)) {
        frame->slots[slot] = NUMBER_VAL(index);
        loopValue = entry->key;
        hasNext = true;
        break;
      }
      index++;
    }
  }

  if (hasNext) {
    push(loopValue);
  } else {
    ip += offset;
  }

  DISPATCH();
}

TARGET_OP_GET_ITER_VALUE: {
  uint8_t slot = READ_BYTE();
  Value seqVal = frame->slots[slot - 1]; // The Collection
  Value iterVal = frame->slots[slot];    // The Raw Index

  if (IS_DICT(seqVal)) {
    // Dictionary Magic: Return the actual VALUE from the Hash Table!
    ObjDict *dict = AS_DICT(seqVal);
    int index = (int)AS_NUMBER(iterVal);
    push(dict->fields.entries[index].value);
  } else {
    // List/String/Range Magic: Add 1 to align with MOON's 1-based indexing!
    double moonIndex = AS_NUMBER(iterVal) + 1.0;
    push(NUMBER_VAL(moonIndex));
  }
  DISPATCH();
}

TARGET_OP_GET_ITER_VALUE_LONG: {
  uint16_t slot = READ_SHORT();
  Value seqVal = frame->slots[slot - 1]; // The Collection
  Value iterVal = frame->slots[slot];    // The Raw Index

  if (IS_DICT(seqVal)) {
    // Dictionary Magic: Return the actual VALUE from the Hash Table!
    ObjDict *dict = AS_DICT(seqVal);
    int index = (int)AS_NUMBER(iterVal);
    push(dict->fields.entries[index].value);
  } else {
    // List/String/Range Magic: Add 1 to align with MOON's 1-based indexing!
    double moonIndex = AS_NUMBER(iterVal) + 1.0;
    push(NUMBER_VAL(moonIndex));
  }
  DISPATCH();
}

TARGET_OP_CALL: {
  int argCount = READ_SHORT();
  EXPECT_STACK(argCount + 1);

  Value callee = peek(argCount);

  if (IS_MULTI_FUNCTION(callee)) {
    ObjMultiFunction *multi = AS_MULTI_FUNCTION(callee);

    if (argCount != multi->arity) {
      THROW_ERROR(
          ERR_RUNTIME,
          "Check the function signature to see how many values it requires.",
          "Expected %d arguments but got %d.", multi->arity, argCount);
    }

    Value *args = vm.stackTop - argCount;
    bool isAmbiguous = false;
    Value bestMethodVal = resolveOverload(multi, argCount, args, &isAmbiguous);

    if (IS_NIL(bestMethodVal)) {
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

    // Overwrite the multi-function with the resolved method
    callee = bestMethodVal;
    vm.stackTop[-1 - argCount] = callee;
  }

  // Unified Execution Block
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

    frame->ip = ip;
    CallFrame *newFrame = &vm.frames[vm.frameCount++];
    newFrame->function = function;
    newFrame->globals =
        function->homeGlobals ? function->homeGlobals : &vm.globals;
    newFrame->ip = function->chunk.code;
    newFrame->slots = vm.stackTop - argCount - 1;
    newFrame->stickySubject = NIL_VAL;

    frame = newFrame;
    ip = frame->ip;
    DISPATCH();
  }

  THROW_ERROR(
      ERR_TYPE, "You appended parentheses to a value that isn't executable.",
      "You tried to call a %s as if it were a function.", TYPE_NAME(callee));
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

  // --- THE FIX ---
  // Save the finished Blueprint to the current Module's scope!
  tableSet(frame->globals, OBJ_VAL(name),
           OBJ_VAL(type)); // <--- CHANGED FROM &vm.globals

  // 4. Clean up the stack
  pop();                              // Pop the protected ObjType
  vm.stackTop -= (propertyCount * 2); // Pop all keys and values

  DISPATCH();
}

TARGET_OP_DEFINE_METHOD: {
  ObjString *name = READ_STRING();
  ObjFunction *method = AS_FUNCTION(peek(0));
  int arity = method->arity;
  Value *typesStart = vm.stackTop - 1 - arity;

  // 1. Extract the Expected Types
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

  // Does the MultiFunction folder already exist in current scope?
  Value existing;
  ObjMultiFunction *multi = NULL;
  if (tableGet(frame->globals, OBJ_VAL(name), &existing) &&
      IS_MULTI_FUNCTION(existing)) {
    multi = AS_MULTI_FUNCTION(existing);
  } else if (tableGet(&vm.globals, OBJ_VAL(name), &existing) &&
             IS_MULTI_FUNCTION(existing)) {
    multi = AS_MULTI_FUNCTION(existing);

    // We found it in the standard library! Let's bring a reference
    // into the local scope so it's prioritized and visible locally.
    tableSet(frame->globals, OBJ_VAL(name), OBJ_VAL(multi));

    const char *dollar = strchr(name->chars, '$');
    if (dollar != NULL) {
      int rootLen = (int)(dollar - name->chars);
      ObjString *rootName = copyString(name->chars, rootLen);
      push(OBJ_VAL(rootName)); // GC shield
      tableSet(frame->globals, OBJ_VAL(rootName), OBJ_VAL(multi));
      pop(); // pop rootName
    }
  } else {
    multi = newMultiFunction(name, arity);
    push(OBJ_VAL(multi));
    // Store under mangled name (e.g. "my_func$0") for dispatch
    tableSet(frame->globals, OBJ_VAL(name), OBJ_VAL(multi));

    // --- ROOT NAME ALIAS ---
    // Also store under the unmangled root name (e.g. "my_func") so that
    // module property access via `mymod's my_func` works without knowing arity.
    const char *dollar = strchr(name->chars, '$');
    if (dollar != NULL) {
      int rootLen = (int)(dollar - name->chars);
      ObjString *rootName = copyString(name->chars, rootLen);
      push(OBJ_VAL(rootName)); // GC shield
      tableSet(frame->globals, OBJ_VAL(rootName), OBJ_VAL(multi));
      pop(); // pop rootName
    }
    // -----------------------

    pop(); // pop multi
  }

  // --- THE NEW SHIELD ---
  // We must manually push the signatures to the stack to protect them
  // from the GC while the MultiFunction arrays are expanding!
  for (int i = 0; i < arity; i++) {
    push(signatures[i]);
  }

  // Slide the method and signature into the arrays
  if (multi->methodCapacity < multi->methodCount + 1) {
    int oldCap = multi->methodCapacity;
    multi->methodCapacity = GROW_CAPACITY(oldCap);
    multi->methods =
        GROW_ARRAY(Value, multi->methods, oldCap, multi->methodCapacity);
    multi->signatures =
        GROW_ARRAY(Value *, multi->signatures, oldCap, multi->methodCapacity);
  }

  multi->methods[multi->methodCount] = OBJ_VAL(method);
  multi->signatures[multi->methodCount] = signatures;
  multi->methodCount++;

  // --- DROP THE SHIELD ---
  for (int i = 0; i < arity; i++) {
    pop();
  }

  // Clean up the stack (Pop the method and the types)
  pop();
  vm.stackTop -= arity;
  DISPATCH();
}

TARGET_OP_DEFINE_EXTENSION_METHOD: {
  ObjString *name = READ_STRING();
  ObjFunction *method = AS_FUNCTION(peek(0));
  int arity = method->arity;
  Value *typesStart = vm.stackTop - 1 - arity;

  // 1. Extract the Expected Types
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

  // 2. Extract Receiver Type
  Value receiverTypeVal = *(typesStart - 1);
  if (!IS_TYPE(receiverTypeVal) && !IS_UNION(receiverTypeVal)) {
    THROW_ERROR(
        ERR_TYPE,
        "Extension method receiver must be a concrete Type Blueprint or Union.",
        "Invalid receiver type.");
  }

  // 3. Resolve Concrete Types
  int receiverCount = 1;
  ObjType **receivers = NULL;

  if (IS_UNION(receiverTypeVal)) {
    ObjUnion *unionObj = AS_UNION(receiverTypeVal);
    receiverCount = unionObj->count;
    receivers = ALLOCATE(ObjType *, receiverCount);
    for (int i = 0; i < receiverCount; i++) {
      receivers[i] = AS_TYPE(unionObj->types[i]);
    }
  } else {
    ObjType *typeObj = AS_TYPE(receiverTypeVal);
    if (typeObj == vm.anyType) {
      THROW_ERROR(ERR_TYPE, "Cannot attach an extension method to 'Any'.",
                  "Receiver must be a concrete Type Blueprint.");
    }
    receivers = ALLOCATE(ObjType *, 1);
    receivers[0] = typeObj;
  }

  // 4. Attach Method to Each Concrete Type
  for (int t = 0; t < receiverCount; t++) {
    ObjType *targetType = receivers[t];

    Value existing;
    ObjMultiFunction *multi = NULL;
    if (tableGet(&targetType->properties, OBJ_VAL(name), &existing) &&
        IS_MULTI_FUNCTION(existing)) {
      multi = AS_MULTI_FUNCTION(existing);
    } else {
      multi = newMultiFunction(name, arity);
      push(OBJ_VAL(multi));
      tableSet(&targetType->properties, OBJ_VAL(name), OBJ_VAL(multi));
      pop();
    }

    Value *sigCopy = ALLOCATE(Value, arity);
    for (int i = 0; i < arity; i++) {
      sigCopy[i] = signatures[i];
      push(sigCopy[i]); // Shield
    }

    if (multi->methodCapacity < multi->methodCount + 1) {
      int oldCap = multi->methodCapacity;
      multi->methodCapacity = GROW_CAPACITY(oldCap);
      multi->methods =
          GROW_ARRAY(Value, multi->methods, oldCap, multi->methodCapacity);
      multi->signatures =
          GROW_ARRAY(Value *, multi->signatures, oldCap, multi->methodCapacity);
    }

    multi->methods[multi->methodCount] = OBJ_VAL(method);
    multi->signatures[multi->methodCount] = sigCopy;
    multi->methodCount++;

    for (int i = 0; i < arity; i++)
      pop(); // Drop shield
  }

  FREE_ARRAY(Value, signatures,
             arity); // Free original array since we copied it
  FREE_ARRAY(ObjType *, receivers, receiverCount);

  // Pop the method, types, and receiverType
  pop();
  vm.stackTop -= (arity + 1);
  DISPATCH();
}

TARGET_OP_INVOKE: {
  ObjString *name = READ_STRING();
  int argCount = READ_BYTE();
  EXPECT_STACK(argCount + 1);

  Value receiver = peek(argCount);
  ObjType *receiverType = getObjType(receiver);

  Value callee;
  if (!tableGet(&receiverType->properties, OBJ_VAL(name), &callee)) {
    THROW_ERROR(ERR_REFERENCE,
                "Check the name of the property you're trying to access.",
                "Property '%s' not found on type '%s'.", name->chars,
                receiverType->name->chars);
  }

  if (IS_MULTI_FUNCTION(callee)) {
    ObjMultiFunction *multi = AS_MULTI_FUNCTION(callee);

    if (argCount != multi->arity) {
      THROW_ERROR(
          ERR_RUNTIME,
          "Check the property signature to see how many values it requires.",
          "Expected %d arguments but got %d.", multi->arity, argCount);
    }

    Value *args = vm.stackTop - argCount;
    bool isAmbiguous = false;
    Value bestMethodVal = resolveOverload(multi, argCount, args, &isAmbiguous);

    if (IS_NIL(bestMethodVal)) {
      THROW_ERROR(ERR_REFERENCE,
                  "The arguments you passed don't match any overloaded version "
                  "of this property.",
                  "No matching signature found.");
    }

    if (isAmbiguous) {
      THROW_ERROR(ERR_RUNTIME,
                  "Multiple methods match this call with the exact same "
                  "specificity. Define an intersection method to clarify.",
                  "Ambiguous Dispatch Error.");
    }

    callee = bestMethodVal;
    // Note: Do NOT overwrite the receiver at stackTop[-1 - argCount].
  }

  if (IS_NATIVE(callee)) {
    NativeFn native = AS_NATIVE(callee);
    Value result = native(argCount + 1, vm.stackTop - argCount - 1);
    vm.stackTop -= argCount + 1;
    push(result);
    DISPATCH();
  } else if (IS_FUNCTION(callee)) {
    ObjFunction *function = AS_FUNCTION(callee);
    if (argCount != function->arity) {
      THROW_ERROR(
          ERR_RUNTIME,
          "Check the property signature to see how many values it requires.",
          "Expected %d arguments but got %d.", function->arity, argCount);
    }

    if (vm.frameCount == FRAMES_MAX) {
      THROW_ERROR(ERR_RUNTIME,
                  "You have too many nested function calls. Do you have "
                  "infinite recursion?",
                  "Stack overflow.");
    }

    frame->ip = ip;
    CallFrame *newFrame = &vm.frames[vm.frameCount++];
    newFrame->function = function;
    newFrame->globals =
        function->homeGlobals ? function->homeGlobals : &vm.globals;
    newFrame->ip = function->chunk.code;
    newFrame->slots = vm.stackTop - argCount - 1; // slots[0] is the Receiver!
    newFrame->stickySubject = NIL_VAL;

    frame = newFrame;
    ip = frame->ip;
    DISPATCH();
  } else {
    THROW_ERROR(ERR_RUNTIME, "Expected a callable property.",
                "Property '%s' is not callable.", name->chars);
  }
}

TARGET_OP_CAST: {
  Value typeVal = pop();
  Value val = pop();

  if (!IS_TYPE(typeVal)) {
    THROW_ERROR(ERR_TYPE,
                "You can only cast to a valid Type Blueprint (like Number, "
                "String, or a custom type).",
                "Invalid cast target.");
  }

  Value castResult;
  if (!executeCast(val, AS_TYPE(typeVal), &castResult)) {
    // If the helper failed, it already printed the error.
    // We just need to sync the register and halt the VM.
    frame->ip = ip;
    return INTERPRET_RUNTIME_ERROR;
  }

  push(castResult);
  DISPATCH();
}

TARGET_OP_LOAD: {
  ObjString *path = AS_STRING(pop());

  // 1. THE CACHE HIT
  Value cachedModule;
  if (tableGet(&vm.loadedModules, OBJ_VAL(path), &cachedModule)) {
    push(cachedModule); // Push the module object for the 'let' assignment!
    DISPATCH();
  }

  // 2. Read from OS
  char *source = readFile(path->chars);
  if (source == NULL) {
    THROW_ERROR(ERR_RUNTIME, "Make sure the file exists.",
                "Could not open module '%s'.", path->chars);
  }

  ObjString *sourceStr = copyString(source, strlen(source));
  push(OBJ_VAL(sourceStr)); // GC Shield

  // 3. Create the Module
  ObjModule *module = newModule(path);
  module->source = sourceStr; // Store source for error reporting
  push(OBJ_VAL(module));      // GC Shield

  // 4. Register immediately to prevent circular dependency infinite loops!
  tableSet(&vm.loadedModules, OBJ_VAL(path), OBJ_VAL(module));

  // 5. Compile
  currentGlobals = &module->fields; // Route compiler output to this module!
  vm.allowGC = false;
  ObjFunction *moduleFunc = compile(source, module, 1);
  vm.allowGC = true;

  free(source);

  if (moduleFunc == NULL) {
    tableDelete(&vm.loadedModules, OBJ_VAL(path)); // Remove broken module
    THROW_ERROR(ERR_RUNTIME, "Syntax error inside the module.",
                "Failed to compile '%s'.", path->chars);
  }

  // 6. Context Switch
  if (vm.frameCount == FRAMES_MAX) {
    THROW_ERROR(ERR_RUNTIME,
                "You might have a circular dependency where two files are "
                "trying to load each other in an infinite loop!",
                "Too many files loading each other.");
  }

  // POP the GC shields so they don't leak on the physical stack!
  pop(); // Pops module
  pop(); // Pops sourceStr

  // Push moduleFunc BACK onto the stack so it sits perfectly at frame->slots!
  // This mimics a 0-argument function call.
  push(OBJ_VAL(moduleFunc));

  frame->ip = ip;
  CallFrame *newFrame = &vm.frames[vm.frameCount++];
  newFrame->function = moduleFunc;
  newFrame->globals = &module->fields;
  newFrame->ip = moduleFunc->chunk.code;
  newFrame->slots = vm.stackTop - 1;
  newFrame->stickySubject = NIL_VAL;

  frame = newFrame;
  ip = frame->ip;
  DISPATCH();
}

TARGET_OP_KEEP_LIST: {
  uint8_t slot = READ_BYTE();
  Value listVal = frame->slots[slot];
  EXPECT_STACK(1);

  // 1. PEEK: Shield the item on the stack so the GC can see it!
  Value item = peek(0);

  if (!IS_LIST(listVal)) {
    THROW_ERROR(ERR_RUNTIME, "Fatal Compiler Bug: Accumulator is not a list.",
                "Internal Error");
  }

  // 2. MUTATE: Even if this triggers GC, 'item' is safely on the stack.
  appendList(AS_LIST(listVal), item);

  // 3. POP: Cleanup
  pop();
  DISPATCH();
}

TARGET_OP_KEEP_DICT: {
  uint8_t slot = READ_BYTE();
  Value dictVal = frame->slots[slot];

  EXPECT_STACK(2);

  // 1. PEEK: Shield both the value and the key!
  Value value = peek(0);
  Value key = peek(1);

  if (!IS_DICT(dictVal)) {
    THROW_ERROR(ERR_RUNTIME, "Fatal Compiler Bug: Accumulator is not a dict.",
                "Internal Error");
  }

  // 2. MUTATE: This triggers GC when the hash table resizes!
  tableSet(&AS_DICT(dictVal)->fields, key, value);

  // 3. POP: Cleanup
  pop(); // pop value
  pop(); // pop key
  DISPATCH();
}

TARGET_OP_RETURN: {
  Value result = pop();

  if (frame->function->isTopLevel && frame->function->module != NULL) {
    result = OBJ_VAL(frame->function->module);
  }

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

TARGET_OP_SHOW_REPL: {
  // 1. PEEK: Shield the value from the GC!
  Value value = peek(0);

  if (!IS_NIL(value)) {
    // 2. ALLOCATE: Even if this triggers the GC, 'value' is safe on the stack.
    ObjString *str = valueToString(value);

    printf(COLOR_DIM "< " COLOR_RESET "%s\n", str->chars);
  }

  // 3. POP: Now that we are done, safely discard it.
  pop();
  DISPATCH();
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

bool g_isBootstrappingCore = false;

void bootstrapCore() {
  g_isBootstrappingCore = true;
  registerCoreLibrary();
  registerMathLibrary();
  registerStringLibrary();
  registerListLibrary();
  registerIOLibrary();
  g_isBootstrappingCore = false;
}

bool isCoreBootstrapped = false;

InterpretResult interpret(const char *source, int startLine) {
  vm.allowGC = false; // GC sleeps while we parse/compile

  // --- THE BOOTSTRAP GUARD ---
  if (!isCoreBootstrapped) {
    bootstrapCore();           // Compile & run the core
    isCoreBootstrapped = true; // Lock the door!
  }

  // 1. Vault the Main Script
  ObjString *mainName = copyString("<main>", 6);
  push(OBJ_VAL(mainName));
  ObjString *mainSrc = copyString(source, strlen(source));
  push(OBJ_VAL(mainSrc));

  ObjModule *mainModule = NULL;
  Value existingModule;
  if (isReplMode &&
      tableGet(&vm.loadedModules, OBJ_VAL(mainName), &existingModule)) {
    mainModule = AS_MODULE(existingModule);
    // Append the new source to the persistent module's source string for error
    // reporting
    ObjString *oldSrc = mainModule->source;
    if (oldSrc != NULL) {
      int newLen = oldSrc->length + mainSrc->length;
      char *newChars = ALLOCATE(char, newLen + 1);
      memcpy(newChars, oldSrc->chars, oldSrc->length);
      memcpy(newChars + oldSrc->length, mainSrc->chars, mainSrc->length);
      newChars[newLen] = '\0';
      mainModule->source = takeString(newChars, newLen);
    } else {
      mainModule->source = mainSrc;
    }
  } else {
    mainModule = newModule(mainName);
    mainModule->source = mainSrc;
    tableSet(&vm.loadedModules, OBJ_VAL(mainName), OBJ_VAL(mainModule));
  }
  push(OBJ_VAL(mainModule));

  // Give the error engine the raw string BEFORE compilation begins!
  // In REPL, give it the accumulated source to match the line numbers!
  initErrorEngine(mainModule->source->chars);

  // 2. Compile with the tag
  currentGlobals = &mainModule->fields; // Hook up the router!
  ObjFunction *function = compile(source, mainModule, startLine);

  pop(); // pop mainModule
  pop(); // pop mainSrc
  pop(); // pop mainName

  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;

  if (noRunFlag)
    return INTERPRET_OK;

  vm.stackTop = vm.stack;
  vm.frameCount = 0;

  // Push the script function to stack so GC doesn't eat it
  push(OBJ_VAL(function));

  // 2. Set up the first Call Frame (The "Main" script)
  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->globals = &function->module->fields;
  frame->ip = function->chunk.code;

  // The script's locals start at the bottom of the stack
  frame->slots = vm.stack;
  frame->stickySubject = NIL_VAL;

  vm.allowGC = true;

  // 3. Go!
  return run();
}
