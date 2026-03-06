// vm.c

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "vm.h"

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

// Helper to define natives
static void defineNative(const char *name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

static Value clockNative(int argCount, Value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

void initVM() {
  resetStack();
  vm.objects = NULL;
  vm.debugMode = false;
  initTable(&vm.strings);
  initTable(&vm.globals);

  for (int i = 0; i < 256; i++) {
    char c = (char)i;
    // copyString takes a pointer to the char and the length (1)
    vm.charStrings[i] = copyString(&c, 1);
  }

  // Define native function
  defineNative("clock", clockNative);
}

void freeVM() {
  freeTable(&vm.strings);
  freeTable(&vm.globals);
  freeObjects();
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
  ObjString *b = AS_STRING(pop());
  ObjString *a = AS_STRING(pop());

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(chars, length);
  push(OBJ_VAL(result));
}

bool valuesEqual(Value a, Value b) {
  if (a.type != b.type)
    return false;
  switch (a.type) {
  case VAL_BOOL:
    return AS_BOOL(a) == AS_BOOL(b);
  case VAL_NIL:
    return true;
  case VAL_NUMBER:
    return AS_NUMBER(a) == AS_NUMBER(b);
  case VAL_OBJ:
    return AS_OBJ(a) == AS_OBJ(b);
  default:
    return false; // Unreachable.
  }
}

static ObjString *valueToString(Value value) {
  // If it's already a string, just cast and return
  if (IS_STRING(value))
    return AS_STRING(value);

  // If it's a number, format it
  if (IS_NUMBER(value)) {
    double number = AS_NUMBER(value);
    char buffer[32];
    int length = sprintf(buffer, "%.14g", number);
    return copyString(buffer, length);
  }

  // If it's boolean
  if (IS_BOOL(value)) {
    return AS_BOOL(value) ? copyString("true", 4) : copyString("false", 5);
  }

  // If it's nil
  if (IS_NIL(value)) {
    return copyString("nil", 3);
  }

  // Fallback for objects we haven't implemented yet
  return copyString("<object>", 8);
}

// Run()
static InterpretResult run() {

  CallFrame *frame = &vm.frames[vm.frameCount - 1];
  register uint8_t *ip = frame->ip;

  // Instead of vm.ip, we look at the TOP frame
  // 2. Update macros to use local 'ip' instead of 'frame->ip'
#define READ_BYTE() (*ip++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())

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

  for (;;) {
    if (vm.debugMode) {
      // 1. Visualize the Stack
      debugStack(&vm);

      disassembleInstruction(&frame->function->chunk,
                             (int)(ip - frame->function->chunk.code));
    }

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }

    case OP_ADD: {
      // 1. Fast Path: Two Numbers
      if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
      }

      // 2. Fast Path: Two Strings
      else if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatenate(); // Takes care of popping and pushing result
      }

      // 3. Mixed Types: Convert both to strings
      else if (IS_STRING(peek(0)) || IS_STRING(peek(1))) {
        // Convert top two items to strings (even if they already are)
        ObjString *b = valueToString(peek(0));
        ObjString *a = valueToString(peek(1));

        // Clean the stack
        pop(); // Pop original b
        pop(); // Pop original a

        // Push the string versions
        push(OBJ_VAL(a));
        push(OBJ_VAL(b));

        // Join them
        concatenate();
      }

      // 4. Error
      else {
        runtimeError("Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }

    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, -);
      break;

    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, *);
      break;

    case OP_DIVIDE:
      BINARY_OP(NUMBER_VAL, /);
      break;

    case OP_NEGATE: {
      if (!IS_NUMBER(peek(0))) {
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop())));
      break;
    }

    case OP_NIL:
      push(NIL_VAL);
      break;

    case OP_TRUE:
      push(BOOL_VAL(true));
      break;

    case OP_FALSE:
      push(BOOL_VAL(false));
      break;

    case OP_POP:
      pop();
      break;

    case OP_PRINT: {
      printValue(pop());
      printf("\n");
      break;
    }

    case OP_DEFINE_GLOBAL: {
      ObjString *name = READ_STRING();
      tableSet(&vm.globals, name, peek(0));
      break;
    }

    case OP_GET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value value;
      if (!tableGet(&vm.globals, name, &value)) {
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      push(value);
      break;
    }

    case OP_SET_GLOBAL: {
      ObjString *name = READ_STRING();
      if (tableSet(&vm.globals, name, peek(0))) {
        // tableSet returns true if it's a NEW key.
        // 'set' is only for EXISTING keys. Delete it and error.
        tableDelete(&vm.globals, name);
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      break;
    }

    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(frame->slots[slot]);
      break;
    }

    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(0);
      break;
    }

    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      ip += offset; // Update local ip directly
      break;
    }

    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0)))
        ip += offset; // Update local ip directly
      break;
    }

    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      ip -= offset; // Update local ip directly
      break;
    }

    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }

    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;

    case OP_NOT:
      push(BOOL_VAL(isFalsey(pop())));
      break;

    case OP_BUILD_STRING: {
      uint8_t count = READ_BYTE();

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

      break;
    }

    case OP_BUILD_LIST: {
      uint8_t itemCount = READ_BYTE();

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

      break;
    }

    case OP_GET_SUBSCRIPT: {
      Value indexVal = peek(0);
      Value seqVal = peek(1);

      if (IS_LIST(seqVal)) {
        ObjList *list = AS_LIST(seqVal);

        // --- Normal Indexing (list[2]) ---
        if (IS_NUMBER(indexVal)) {
          int index = (int)AS_NUMBER(indexVal);

          if (index < 1 || index > list->count) {
            runtimeError("List index out of bounds.");
            return INTERPRET_RUNTIME_ERROR;
          }

          pop();                        // Pop index
          pop();                        // Pop list
          push(list->items[index - 1]); // 1-based math!
        }

        // --- Slicing (list[2 to 4]) ---
        else if (IS_RANGE(indexVal)) {
          ObjRange *range = AS_RANGE(indexVal);

          // 1. Create the new list for the slice
          ObjList *slice = newList();

          // 2. GC Protection
          push(OBJ_VAL(slice));

          double start = range->start;
          double end = range->end;
          double step = range->step;

          // 3. Extract the items based on direction
          if (start <= end) {
            // Counting UP
            for (double i = start; i <= end; i += step) {
              int idx = (int)i;
              if (idx >= 1 && idx <= list->count) {
                appendList(slice, list->items[idx - 1]); // 1-based math!
              }
            }
          } else {
            // Counting DOWN (Reversing a slice!)
            for (double i = start; i >= end; i -= step) {
              int idx = (int)i;
              if (idx >= 1 && idx <= list->count) {
                appendList(slice, list->items[idx - 1]); // 1-based math!
              }
            }
          }

          // 4. Clean up the stack
          pop();                // Pop the 'slice' temporarily
          pop();                // Pop the 'range'
          pop();                // Pop the original 'list'
          push(OBJ_VAL(slice)); // Push the final slice back!
        } else {
          runtimeError("List index must be a number or a range.");
          return INTERPRET_RUNTIME_ERROR;
        }
      } else {
        runtimeError("Can only subscript lists (and strings).");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }

    case OP_SET_SUBSCRIPT: {
      // Stack Layout: [List] [Index] [Value] <--- Top
      Value value = peek(0);
      Value indexVal = peek(1);
      Value listVal = peek(2);

      // 1. Type Checks
      if (!IS_LIST(listVal)) {
        runtimeError("Can only subscript lists.");
        return INTERPRET_RUNTIME_ERROR;
      }
      if (!IS_NUMBER(indexVal)) {
        runtimeError("List index must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjList *list = AS_LIST(listVal);
      int index = (int)AS_NUMBER(indexVal);

      // Adjust for 1-based indexing
      if (index < 0) {
        index = list->count + index + 1;
      }

      // Convert to 0-based C array index
      index--;

      // Bounds Check (unchanged)
      if (index < 0 || index >= list->count) {
        runtimeError("List index out of bounds.");
        return INTERPRET_RUNTIME_ERROR;
      }

      // 4. Store the value
      list->items[index] = value;

      // 5. Cleanup
      // We leave the VALUE on the stack (so expressions like a = b[0] = 1
      // work). Stack before: [List] [Index] [Value] Stack after:  [Value]
      pop(); // value
      pop(); // index
      pop(); // list
      push(value);

      break;
    }

    case OP_GET_END_INDEX: {
      Value obj = pop(); // Pop the duplicated sequence off the stack

      if (IS_LIST(obj)) {
        // 1-BASED INDEXING: The last index is exactly the count!
        push(NUMBER_VAL(AS_LIST(obj)->count));
      } else if (IS_STRING(obj)) {
        // 1-BASED INDEXING: The last index is exactly the length!
        push(NUMBER_VAL(AS_STRING(obj)->length));
      } else {
        runtimeError("'end' can only be used on lists and strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }

    case OP_RANGE: {
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
      break;
    }

      // 1. Determine where the loop starts
    case OP_GET_ITER: {
      Value seq = peek(0); // Sequence is on stack

      // Strings, Lists, and Ranges all use the universal -1 index counter!
      if (IS_LIST(seq) || IS_RANGE(seq) || IS_STRING(seq)) {
        push(NUMBER_VAL(-1));
      } else {
        runtimeError("Can only loop over lists, ranges, and strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }

      // 2. The Loop Logic
    case OP_FOR_ITER: {
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
            push(NUMBER_VAL(nextValue)); // Push the actual loop value
            ip += 2;                     // Skip jump
          } else {
            uint16_t offset = READ_SHORT();
            ip += offset;
          }
        } else {
          // Counting DOWN
          nextValue = range->start - (nextIndex * range->step);

          if (nextValue >= range->end) {
            vm.stackTop[-1] = NUMBER_VAL(nextIndex); // Save the index
            push(NUMBER_VAL(nextValue)); // Push the actual loop value
            ip += 2;                     // Skip jump
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
      break;
    }

    case OP_CALL: {
      int argCount = READ_BYTE();

      Value callee = peek(argCount);

      if (IS_NATIVE(callee)) {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        break;
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
        break;
      }

      runtimeError("Can only call functions and classes.");
      return INTERPRET_RUNTIME_ERROR;
    }

    case OP_RETURN: {
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
      break;
    }
    }
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

  // 3. Go!
  return run();
}
