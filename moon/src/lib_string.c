#include <ctype.h>
#include <string.h>

#include "lib_string.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

// --- THE PRIMITIVES ---

static Value upperNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL; // Arity Shield
  if (!IS_STRING(args[0]))
    return NIL_VAL; // Type Shield

  ObjString *str = AS_STRING(args[0]);
  char *heapChars = ALLOCATE(char, str->length + 1);

  for (int i = 0; i < str->length; i++) {
    heapChars[i] = toupper(str->chars[i]);
  }
  heapChars[str->length] = '\0';

  return OBJ_VAL(takeString(heapChars, str->length));
}

static Value lowerNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL;
  if (!IS_STRING(args[0]))
    return NIL_VAL;

  ObjString *str = AS_STRING(args[0]);
  char *heapChars = ALLOCATE(char, str->length + 1);

  for (int i = 0; i < str->length; i++) {
    heapChars[i] = tolower(str->chars[i]);
  }
  heapChars[str->length] = '\0';

  return OBJ_VAL(takeString(heapChars, str->length));
}

static Value trimNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL;
  if (!IS_STRING(args[0]))
    return NIL_VAL;

  ObjString *str = AS_STRING(args[0]);
  if (str->length == 0)
    return args[0]; // Empty string

  int start = 0;
  while (start < str->length && isspace(str->chars[start])) {
    start++;
  }

  int end = str->length - 1;
  while (end > start && isspace(str->chars[end])) {
    end--;
  }

  int newLength = end - start + 1;
  return OBJ_VAL(copyString(str->chars + start, newLength));
}

static Value splitNative(int argCount, Value *args) {
  if (argCount != 2)
    return NIL_VAL;
  if (!IS_STRING(args[0]) || !IS_STRING(args[1]))
    return NIL_VAL;

  ObjString *string = AS_STRING(args[0]);
  ObjString *delim = AS_STRING(args[1]);

  ObjList *list = newList();
  push(OBJ_VAL(list)); // GC Protection!

  if (delim->length == 0) {
    // Split by character if delimiter is empty ""
    for (int i = 0; i < string->length; i++) {
      ObjString *charStr = copyString(string->chars + i, 1);
      push(OBJ_VAL(charStr)); // GC Protection
      appendList(list, OBJ_VAL(charStr));
      pop();
    }
  } else {
    // Split by delimiter
    char *start = string->chars;
    char *end;
    while ((end = strstr(start, delim->chars)) != NULL) {
      ObjString *chunk = copyString(start, end - start);
      push(OBJ_VAL(chunk)); // GC Protection
      appendList(list, OBJ_VAL(chunk));
      pop();
      start = end + delim->length;
    }
    // Add the remaining tail
    ObjString *finalChunk = copyString(start, strlen(start));
    push(OBJ_VAL(finalChunk)); // GC Protection
    appendList(list, OBJ_VAL(finalChunk));
    pop();
  }

  pop(); // Remove list from VM stack
  return OBJ_VAL(list);
}

// --- THE HANDSHAKE ---

void registerStringLibrary() {
  defineNative("__str_upper", upperNative);
  defineNative("__str_lower", lowerNative);
  defineNative("__str_trim", trimNative);
  defineNative("__str_split", splitNative);
}

// --- THE MOON WRAPPERS ---

const char stringBootstrap[] = "let uppercase (s: String):\n"
                               "    give __str_upper(s)\n"
                               "end\n"
                               "\n"
                               "let lowercase (s: String):\n"
                               "    give __str_lower(s)\n"
                               "end\n"
                               "\n"
                               "let trim (s: String):\n"
                               "    give __str_trim(s)\n"
                               "end\n"
                               "\n"
                               "let split (s: String) by (delim: String):\n"
                               "    give __str_split(s, delim)\n"
                               "end\n";
